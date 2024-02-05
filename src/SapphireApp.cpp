
#include "SapphireApp.hpp"
#include "imgui.h"
#include "imGuIZMO.h"
#include "ImGuiUtils.hpp"
#include "ColorConversion.h"
#include "GraphicsAccessories.hpp"
#include "GraphicsUtilities.h"
#include "TextureUtilities.h"
#include "MapHelper.hpp"
#include "CommonlyUsedStates.h"
#include "mapbox/earcut.hpp"
#include "clipper2/clipper.h"
#include "GrimrockModelLoader.h"
//#include "LuaInterface.h"
#include "core/SapphireHash.h"
#include "grid.fxh"

#include <array>


struct PickingBuffer
{    
    uint64_t identity;
    float depth;
    
};

struct DrawCallConstants
{
    float4x4 transform;
    uint64_t identity;
    
    
};

float3 unprojectScreenCoords(float2 screen_coords, uint32_t screen_width, uint32_t screen_height, float4x4& inv_mvp_matrix)
{
    // convert to ndc -1,1 range on x and y
    float ndcx = (screen_coords.x / (float)screen_width) * 2.0f - 1.0f;
    float ndcy = (screen_coords.y / (float)screen_height) * 2.0f - 1.0f;

    float4 eye_coords = float4(ndcx, ndcy, 0.0f, 1.0f) * inv_mvp_matrix;
    float3 world_space(eye_coords.x / eye_coords.w, eye_coords.y / eye_coords.w, eye_coords.z / eye_coords.w);
    
    
    return world_space;
}

namespace mapbox {
    namespace util {

        template <>
        struct nth<0, Clipper2Lib::PointD> {
            inline static auto get(const Clipper2Lib::PointD& t) {
                return t.x;
            };
        };
        template <>
        struct nth<1, Clipper2Lib::PointD> {
            inline static auto get(const Clipper2Lib::PointD& t) {
                return t.y;
            };
        };

        template <>
        struct nth<0, Clipper2Lib::Point64> {
            inline static auto get(const Clipper2Lib::Point64& t) {
                return t.x;
            };
        };
        template <>
        struct nth<1, Clipper2Lib::Point64> {
            inline static auto get(const Clipper2Lib::Point64& t) {
                return t.y;
            };
        };

        

    } // namespace util
} // namespace mapbox

namespace Diligent
{
    #include "Shaders/Common/public/BasicStructures.fxh"
}
using namespace Diligent;

static const float physicsScale = 64.0f;

static const float2 playerMaxSpeed = { 0.1f * physicsScale,1.0f * physicsScale };
static float3 playerPos = { 0.0f, 0.0f, 0.0f };
static float2 playerSpeed = { 0.0f,0.0f };
static float2 playerAcceleration = { 0.2f * physicsScale, -0.5f * physicsScale };
static float2 playerDecceleration = { 0.3f * physicsScale,0.0f };
static int playerFacingDirection = 1;
static bool playerIsJumping = false;
static const float playerJumpStartYSpeed = 5.0f * physicsScale;



//
SampleBase* Diligent::CreateSample()
{
    return new Sapphire::SapphireApp();
}



namespace Sapphire
{
    

SapphireApp::~SapphireApp()
{
}

static void InitCommonSRBVars(IShaderResourceBinding* pSRB,
    IBuffer* pCameraAttribs,
    IBuffer* pLightAttribs)
{
    VERIFY_EXPR(pSRB != nullptr);

    if (pCameraAttribs != nullptr)
    {
        if (auto* pCameraAttribsVSVar = pSRB->GetVariableByName(SHADER_TYPE_VERTEX, "cbCameraAttribs"))
            pCameraAttribsVSVar->Set(pCameraAttribs);

        if (auto* pCameraAttribsPSVar = pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "cbCameraAttribs"))
            pCameraAttribsPSVar->Set(pCameraAttribs);
    }

    if (pLightAttribs != nullptr)
    {
        if (auto* pLightAttribsPSVar = pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "cbLightAttribs"))
            pLightAttribsPSVar->Set(pLightAttribs);
    }
}

void SapphireApp::CreateSelectionCompositPSO()
{
    GraphicsPipelineStateCreateInfo RTPSOCreateInfo;

    // Pipeline state name is used by the engine to report issues
    // It is always a good idea to give objects descriptive names
    // clang-format off
    RTPSOCreateInfo.PSODesc.Name = "Selection Composit PSO";
    // This is a graphics pipeline
    RTPSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
    // This tutorial will render to a single render target
    RTPSOCreateInfo.GraphicsPipeline.NumRenderTargets = 1;
    // Set render target format which is the format of the swap chain's color buffer
    RTPSOCreateInfo.GraphicsPipeline.RTVFormats[0] = m_pSwapChain->GetDesc().ColorBufferFormat;
    // Set depth buffer format which is the format of the swap chain's back buffer
    RTPSOCreateInfo.GraphicsPipeline.DSVFormat = m_pSwapChain->GetDesc().DepthBufferFormat;
    // Primitive topology defines what kind of primitives will be rendered by this pipeline state
    RTPSOCreateInfo.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    // Cull back faces
    RTPSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_BACK;
    // Enable depth testing
    RTPSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = False;
    RTPSOCreateInfo.GraphicsPipeline.BlendDesc = BS_AlphaBlend;
    // clang-format on

    ShaderCreateInfo ShaderCI;
    // Tell the system that the shader source code is in HLSL.
    // For OpenGL, the engine will convert this into GLSL under the hood.
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;

    // OpenGL backend requires emulated combined HLSL texture samplers (g_Texture + g_Texture_sampler combination)
    ShaderCI.Desc.UseCombinedTextureSamplers = true;

    // In this tutorial, we will load shaders from file. To be able to do that,
    // we need to create a shader source stream factory
    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;

    // Create a vertex shader
    RefCntAutoPtr<IShader> pRTVS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint = "main";
        ShaderCI.Desc.Name = "Render Target VS";
        ShaderCI.FilePath = "rendertarget.vsh";
        m_pDevice->CreateShader(ShaderCI, &pRTVS);
    }

    //ShaderMacroHelper Macros;
    //Macros.AddShaderMacro("TRANSFORM_UV", TransformUVCoords);
    //ShaderCI.Macros = Macros;

    // Create a pixel shader
    RefCntAutoPtr<IShader> pRTPS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint = "main";
        ShaderCI.Desc.Name = "Render Target PS";
        ShaderCI.FilePath = "select_composit.psh";

        m_pDevice->CreateShader(ShaderCI, &pRTPS);

    }

    RTPSOCreateInfo.pVS = pRTVS;
    RTPSOCreateInfo.pPS = pRTPS;

    // Define variable type that will be used by default
    RTPSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

    // clang-format off
    // Shader variables should typically be mutable, which means they are expected
    // to change on a per-instance basis
    ShaderResourceVariableDesc Vars[] =
    {
        { SHADER_TYPE_PIXEL, "g_SceneDepthTexture", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE },
        { SHADER_TYPE_PIXEL, "g_SelectionDepthTexture", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE },
        { SHADER_TYPE_PIXEL, "g_SelectionTexture", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE },
    };
    // clang-format on
    RTPSOCreateInfo.PSODesc.ResourceLayout.Variables = Vars;
    RTPSOCreateInfo.PSODesc.ResourceLayout.NumVariables = _countof(Vars);

    // clang-format off
    // Define immutable sampler for g_Texture. Immutable samplers should be used whenever possible
    ImmutableSamplerDesc ImtblSamplers[] =
    {
        { SHADER_TYPE_PIXEL, "g_SceneDepthTexture", Sam_LinearClamp },
        { SHADER_TYPE_PIXEL, "g_SelectionDepthTexture", Sam_LinearClamp },
        { SHADER_TYPE_PIXEL, "g_SelectionTexture", Sam_LinearClamp },
    };
    // clang-format on
    RTPSOCreateInfo.PSODesc.ResourceLayout.ImmutableSamplers = ImtblSamplers;
    RTPSOCreateInfo.PSODesc.ResourceLayout.NumImmutableSamplers = _countof(ImtblSamplers);

    m_pDevice->CreateGraphicsPipelineState(RTPSOCreateInfo, &m_pSelectionCompositPSO);


}

void SapphireApp::CreateRenderTargetPSO()
{
    GraphicsPipelineStateCreateInfo RTPSOCreateInfo;

    // Pipeline state name is used by the engine to report issues
    // It is always a good idea to give objects descriptive names
    // clang-format off
    RTPSOCreateInfo.PSODesc.Name = "Render Target PSO";
    // This is a graphics pipeline
    RTPSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
    // This tutorial will render to a single render target
    RTPSOCreateInfo.GraphicsPipeline.NumRenderTargets = 1;
    // Set render target format which is the format of the swap chain's color buffer
    RTPSOCreateInfo.GraphicsPipeline.RTVFormats[0] = m_pSwapChain->GetDesc().ColorBufferFormat;
    // Set depth buffer format which is the format of the swap chain's back buffer
    RTPSOCreateInfo.GraphicsPipeline.DSVFormat = m_pSwapChain->GetDesc().DepthBufferFormat;
    // Primitive topology defines what kind of primitives will be rendered by this pipeline state
    RTPSOCreateInfo.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    // Cull back faces
    RTPSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_BACK;
    // Enable depth testing
    RTPSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = False;
    // clang-format on

    ShaderCreateInfo ShaderCI;
    // Tell the system that the shader source code is in HLSL.
    // For OpenGL, the engine will convert this into GLSL under the hood.
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;

    // OpenGL backend requires emulated combined HLSL texture samplers (g_Texture + g_Texture_sampler combination)
    ShaderCI.Desc.UseCombinedTextureSamplers = true;

    // In this tutorial, we will load shaders from file. To be able to do that,
    // we need to create a shader source stream factory
    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;

    // Create a vertex shader
    RefCntAutoPtr<IShader> pRTVS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint = "main";
        ShaderCI.Desc.Name = "Render Target VS";
        ShaderCI.FilePath = "rendertarget.vsh";
        m_pDevice->CreateShader(ShaderCI, &pRTVS);
    }

    //ShaderMacroHelper Macros;
    //Macros.AddShaderMacro("TRANSFORM_UV", TransformUVCoords);
    //ShaderCI.Macros = Macros;

    // Create a pixel shader
    RefCntAutoPtr<IShader> pRTPS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint = "main";
        ShaderCI.Desc.Name = "Render Target PS";
        ShaderCI.FilePath = "rendertarget.psh";

        m_pDevice->CreateShader(ShaderCI, &pRTPS);

    }

    RTPSOCreateInfo.pVS = pRTVS;
    RTPSOCreateInfo.pPS = pRTPS;

    // Define variable type that will be used by default
    RTPSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

    // clang-format off
    // Shader variables should typically be mutable, which means they are expected
    // to change on a per-instance basis
    ShaderResourceVariableDesc Vars[] =
    {
        { SHADER_TYPE_PIXEL, "g_Texture", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE },        
    };
    // clang-format on
    RTPSOCreateInfo.PSODesc.ResourceLayout.Variables = Vars;
    RTPSOCreateInfo.PSODesc.ResourceLayout.NumVariables = _countof(Vars);

    // clang-format off
    // Define immutable sampler for g_Texture. Immutable samplers should be used whenever possible
    ImmutableSamplerDesc ImtblSamplers[] =
    {
        { SHADER_TYPE_PIXEL, "g_Texture", Sam_LinearClamp },        
    };
    // clang-format on
    RTPSOCreateInfo.PSODesc.ResourceLayout.ImmutableSamplers = ImtblSamplers;
    RTPSOCreateInfo.PSODesc.ResourceLayout.NumImmutableSamplers = _countof(ImtblSamplers);

    m_pDevice->CreateGraphicsPipelineState(RTPSOCreateInfo, &m_pRTPSO);

    
}

void SapphireApp::CreateSelectionRenderTargetPSO()
{

   
    // Pipeline state object encompasses configuration of all GPU stages

    GraphicsPipelineStateCreateInfo PSOCreateInfo;

    // Pipeline state name is used by the engine to report issues.
    // It is always a good idea to give objects descriptive names.
    PSOCreateInfo.PSODesc.Name = "Selection PSO";

    // This is a graphics pipeline
    PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

    // clang-format off
    // This tutorial will render to a single render target
    PSOCreateInfo.GraphicsPipeline.NumRenderTargets = 1;
    // Set render target format which is the format of the swap chain's color buffer
    PSOCreateInfo.GraphicsPipeline.RTVFormats[0] = HighlightRenderTargetFormat;
    // Set depth buffer format which is the format of the swap chain's back buffer
    PSOCreateInfo.GraphicsPipeline.DSVFormat = DepthBufferFormat;
    // Primitive topology defines what kind of primitives will be rendered by this pipeline state
    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    // Cull back faces
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_BACK;
    // Enable depth testing
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = True;
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = True;
    PSOCreateInfo.GraphicsPipeline.BlendDesc = BS_Default;
    //PSOCreateInfo.GraphicsPipeline.BlendDesc = BS_AlphaBlend;
    // clang-format on

    ShaderCreateInfo ShaderCI;
    // Tell the system that the shader source code is in HLSL.
    // For OpenGL, the engine will convert this into GLSL under the hood.
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;

    // OpenGL backend requires emulated combined HLSL texture samplers (g_Texture + g_Texture_sampler combination)
    ShaderCI.Desc.UseCombinedTextureSamplers = true;

    // Create a shader source stream factory to load shaders from files.
    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;
    // Create a vertex shader
    RefCntAutoPtr<IShader> pVS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint = "main";
        ShaderCI.Desc.Name = "Defualt VS";
        ShaderCI.FilePath = "cube.vsh";
        m_pDevice->CreateShader(ShaderCI, &pVS);
       
    }

    // Create a pixel shader
    RefCntAutoPtr<IShader> pPS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint = "main";
        ShaderCI.Desc.Name = "Defualt Selection PS";
        ShaderCI.FilePath = "cube_selection.psh";
        m_pDevice->CreateShader(ShaderCI, &pPS);
    }

    

    
    // clang-format off
    // Define vertex shader input layout
    LayoutElement LayoutElems[] =
    {
        // Attribute 0 - vertex position
        LayoutElement{0, 0, 3, VT_FLOAT32, False},
        // Attribute 0 - normals
        LayoutElement{1, 0, 3, VT_FLOAT32, False},
        // Attribute 2 - texture coordinates
        LayoutElement{2, 0, 2, VT_FLOAT32, False}
    };
    // clang-format on

    PSOCreateInfo.pVS = pVS;
    PSOCreateInfo.pPS = pPS;

    PSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems;
    PSOCreateInfo.GraphicsPipeline.InputLayout.NumElements = _countof(LayoutElems);

    // Define variable type that will be used by default
    PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

    // clang-format off
    // Shader variables should typically be mutable, which means they are expected
    // to change on a per-instance basis
    ShaderResourceVariableDesc Vars[] =
    {
        {SHADER_TYPE_VERTEX, "cbCameraAttribs", SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
        {SHADER_TYPE_VERTEX, "cbTransforms", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_PIXEL, "cbTransforms", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
    };
    // clang-format on
    PSOCreateInfo.PSODesc.ResourceLayout.Variables = Vars;
    PSOCreateInfo.PSODesc.ResourceLayout.NumVariables = _countof(Vars);


    m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pSelectionRTPSO);

    // Since we did not explcitly specify the type for 'Constants' variable, default
    // type (SHADER_RESOURCE_VARIABLE_TYPE_STATIC) will be used. Static variables
    // never change and are bound directly through the pipeline state object.
    //m_pPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_VSConstants);
    m_pSelectionRTPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "cbCameraAttribs")->Set(m_CameraAttribsCB);
    

    // Since we are using mutable variable, we must create a shader resource ``ing object
    // http://diligentgraphics.com/2016/03/23/resource-binding-model-in-diligent-engine-2-0/
    m_pSelectionRTPSO->CreateShaderResourceBinding(&m_pSelectionRTSRB, true);


    m_pSelectionRTSRB->GetVariableByName(SHADER_TYPE_VERTEX, "cbTransforms")->Set(m_VSConstants);
    m_pSelectionRTSRB->GetVariableByName(SHADER_TYPE_PIXEL, "cbTransforms")->Set(m_VSConstants);
  




}

void SapphireApp::CreatePlanePipelineState()
{
    // Pipeline state object encompasses configuration of all GPU stages

    GraphicsPipelineStateCreateInfo PSOCreateInfo;

    // Pipeline state name is used by the engine to report issues.
    // It is always a good idea to give objects descriptive names.
    PSOCreateInfo.PSODesc.Name = "GRID PSO";

    // This is a graphics pipeline
    PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

    // clang-format off
    // This tutorial will render to a single render target
    PSOCreateInfo.GraphicsPipeline.NumRenderTargets = 1;
    // Set render target format which is the format of the swap chain's color buffer
    PSOCreateInfo.GraphicsPipeline.RTVFormats[0] = RenderTargetFormat;
    // Set depth buffer format which is the format of the swap chain's back buffer
    PSOCreateInfo.GraphicsPipeline.DSVFormat = DepthBufferFormat;
    // Primitive topology defines what kind of primitives will be rendered by this pipeline state
    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    // Cull back faces
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;
    // Enable depth testing
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = True;
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = False;
    PSOCreateInfo.GraphicsPipeline.BlendDesc = BS_AlphaBlend;
    // clang-format on

    ShaderCreateInfo ShaderCI;
    // Tell the system that the shader source code is in HLSL.
    // For OpenGL, the engine will convert this into GLSL under the hood.
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;

    // OpenGL backend requires emulated combined HLSL texture samplers (g_Texture + g_Texture_sampler combination)
    ShaderCI.Desc.UseCombinedTextureSamplers = true;

    // Create a shader source stream factory to load shaders from files.
    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;
    // Create a vertex shader
    RefCntAutoPtr<IShader> pVS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint = "main";
        ShaderCI.Desc.Name = "Grid VS";
        ShaderCI.FilePath = "grid.vsh";
        m_pDevice->CreateShader(ShaderCI, &pVS);
        
    }

    // Create a pixel shader
    RefCntAutoPtr<IShader> pPS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint = "main";
        ShaderCI.Desc.Name = "Grid PS";
        ShaderCI.FilePath = "grid.psh";
        m_pDevice->CreateShader(ShaderCI, &pPS);
    }

    CreateUniformBuffer(m_pDevice, sizeof(GridAttribs), "Grid attribs buffer", &m_GridAttribsCB);
    

    // clang-format off
    StateTransitionDesc Barriers[] =
    {
        {m_GridAttribsCB,        RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_CONSTANT_BUFFER, STATE_TRANSITION_FLAG_UPDATE_STATE},
    

    };
    // clang-format on
    m_pImmediateContext->TransitionResourceStates(_countof(Barriers), Barriers);
    

    // clang-format off
    // Define vertex shader input layout
    //LayoutElement LayoutElems[] =
    //{
    //    // Attribute 0 - vertex position
    //    LayoutElement{0, 0, 3, VT_FLOAT32, False},        
    //};
    // clang-format on

    PSOCreateInfo.pVS = pVS;
    PSOCreateInfo.pPS = pPS;

    //PSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems;
    PSOCreateInfo.GraphicsPipeline.InputLayout.NumElements = 0;// _countof(LayoutElems);

    // Define variable type that will be used by default
    PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

    // clang-format off
    // Shader variables should typically be mutable, which means they are expected
    // to change on a per-instance basis
    ShaderResourceVariableDesc Vars[] =
    {
        {SHADER_TYPE_VERTEX, "cbCameraAttribs", SHADER_RESOURCE_VARIABLE_TYPE_STATIC},        
        {SHADER_TYPE_VERTEX, "cbGridAttribs", SHADER_RESOURCE_VARIABLE_TYPE_STATIC},        
    };
    // clang-format on
    PSOCreateInfo.PSODesc.ResourceLayout.Variables = Vars;
    PSOCreateInfo.PSODesc.ResourceLayout.NumVariables = _countof(Vars);

    

    m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pPlanePSO);

    // Since we did not explcitly specify the type for 'Constants' variable, default
    // type (SHADER_RESOURCE_VARIABLE_TYPE_STATIC) will be used. Static variables
    // never change and are bound directly through the pipeline state object.
    //m_pPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_VSConstants);
    if (m_pPlanePSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "cbCameraAttribs"))
    {
        m_pPlanePSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "cbCameraAttribs")->Set(m_CameraAttribsCB);
    }
        
    if (m_pPlanePSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "cbCameraAttribs"))
    {
        m_pPlanePSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "cbCameraAttribs")->Set(m_CameraAttribsCB);
    }

    if (m_pPlanePSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "cbGridAttribs"))
    {
        m_pPlanePSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "cbGridAttribs")->Set(m_GridAttribsCB);
    }

    if (m_pPlanePSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "cbGridAttribs"))
    {
        m_pPlanePSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "cbGridAttribs")->Set(m_GridAttribsCB);
    }

    m_pPlanePSO->CreateShaderResourceBinding(&m_PlaneSRB, true);
        
}

void SapphireApp::CreatePipelineState()
{
    // Pipeline state object encompasses configuration of all GPU stages

    GraphicsPipelineStateCreateInfo PSOCreateInfo;

    // Pipeline state name is used by the engine to report issues.
    // It is always a good idea to give objects descriptive names.
    PSOCreateInfo.PSODesc.Name = "PBR PSO";

    // This is a graphics pipeline
    PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

    // clang-format off
    // This tutorial will render to a single render target
    PSOCreateInfo.GraphicsPipeline.NumRenderTargets = 1;
    // Set render target format which is the format of the swap chain's color buffer
    PSOCreateInfo.GraphicsPipeline.RTVFormats[0] = RenderTargetFormat;
    // Set depth buffer format which is the format of the swap chain's back buffer
    PSOCreateInfo.GraphicsPipeline.DSVFormat = DepthBufferFormat;
    // Primitive topology defines what kind of primitives will be rendered by this pipeline state
    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    // Cull back faces
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_BACK;
    // Enable depth testing
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = True;
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = True;
    PSOCreateInfo.GraphicsPipeline.BlendDesc = BS_Default;
    //PSOCreateInfo.GraphicsPipeline.BlendDesc = BS_AlphaBlend;
    // clang-format on

    ShaderCreateInfo ShaderCI;
    // Tell the system that the shader source code is in HLSL.
    // For OpenGL, the engine will convert this into GLSL under the hood.
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;

    // OpenGL backend requires emulated combined HLSL texture samplers (g_Texture + g_Texture_sampler combination)
    ShaderCI.Desc.UseCombinedTextureSamplers = true;

    // Create a shader source stream factory to load shaders from files.
    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;
    // Create a vertex shader
    RefCntAutoPtr<IShader> pVS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint = "main";
        ShaderCI.Desc.Name = "Cube VS";
        ShaderCI.FilePath = "cube.vsh";
        m_pDevice->CreateShader(ShaderCI, &pVS);
        // Create dynamic uniform buffer that will store our transformation matrix
        // Dynamic buffers can be frequently updated by the CPU
        size_t ooo = sizeof(DrawCallConstants);
        CreateUniformBuffer(m_pDevice, ooo, "VS constants CB", &m_VSConstants);
        
        
    }

    // Create a pixel shader
    RefCntAutoPtr<IShader> pPS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint = "main";
        ShaderCI.Desc.Name = "Cube PS";
        ShaderCI.FilePath = "cube.psh";
        m_pDevice->CreateShader(ShaderCI, &pPS);
    }

    CreateUniformBuffer(m_pDevice, sizeof(CameraAttribs), "Camera attribs buffer", &m_CameraAttribsCB);
    CreateUniformBuffer(m_pDevice, sizeof(LightAttribs), "Light attribs buffer", &m_LightAttribsCB);

    // clang-format off
    StateTransitionDesc Barriers[] =
    {
        {m_CameraAttribsCB,        RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_CONSTANT_BUFFER, STATE_TRANSITION_FLAG_UPDATE_STATE},
        {m_LightAttribsCB,         RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_CONSTANT_BUFFER, STATE_TRANSITION_FLAG_UPDATE_STATE},
        {m_VSConstants,            RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_CONSTANT_BUFFER, STATE_TRANSITION_FLAG_UPDATE_STATE},
        
    };
    // clang-format on
    m_pImmediateContext->TransitionResourceStates(_countof(Barriers), Barriers);

    // clang-format off
    // Define vertex shader input layout
    LayoutElement LayoutElems[] =
    {
        // Attribute 0 - vertex position
        LayoutElement{0, 0, 3, VT_FLOAT32, False},
        // Attribute 0 - normals
        LayoutElement{1, 0, 3, VT_FLOAT32, False},
        // Attribute 2 - texture coordinates
        LayoutElement{2, 0, 2, VT_FLOAT32, False}
    };
    // clang-format on

    PSOCreateInfo.pVS = pVS;
    PSOCreateInfo.pPS = pPS;

    PSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems;
    PSOCreateInfo.GraphicsPipeline.InputLayout.NumElements = _countof(LayoutElems);

    // Define variable type that will be used by default
    PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

    // clang-format off
    // Shader variables should typically be mutable, which means they are expected
    // to change on a per-instance basis
    ShaderResourceVariableDesc Vars[] =
    {
        {SHADER_TYPE_VERTEX, "cbCameraAttribs", SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
        {SHADER_TYPE_VERTEX, "cbTransforms", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_PIXEL, "cbTransforms", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_PIXEL, "g_AlbedoTexture", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_PIXEL, "g_NormalsTexture", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_PIXEL, "g_PhysicalDescriptorMap", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_PIXEL, "PickBuffer", SHADER_RESOURCE_VARIABLE_TYPE_STATIC}
    };
    // clang-format on
    PSOCreateInfo.PSODesc.ResourceLayout.Variables = Vars;
    PSOCreateInfo.PSODesc.ResourceLayout.NumVariables = _countof(Vars);

    // clang-format off
    // Define immutable sampler for g_Texture. Immutable samplers should be used whenever possible
    SamplerDesc SamLinearClampDesc
    {
        FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR,
        TEXTURE_ADDRESS_WRAP, TEXTURE_ADDRESS_WRAP, TEXTURE_ADDRESS_WRAP
    };
    ImmutableSamplerDesc ImtblSamplers[] =
    {
        {SHADER_TYPE_PIXEL, "g_AlbedoTexture", SamLinearClampDesc},
        {SHADER_TYPE_PIXEL, "g_NormalsTexture", SamLinearClampDesc},
        {SHADER_TYPE_PIXEL, "g_PhysicalDescriptorMap", SamLinearClampDesc}
    };
    // clang-format on
    PSOCreateInfo.PSODesc.ResourceLayout.ImmutableSamplers = ImtblSamplers;
    PSOCreateInfo.PSODesc.ResourceLayout.NumImmutableSamplers = _countof(ImtblSamplers);

    m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pPSO);

    // Since we did not explcitly specify the type for 'Constants' variable, default
    // type (SHADER_RESOURCE_VARIABLE_TYPE_STATIC) will be used. Static variables
    // never change and are bound directly through the pipeline state object.
    //m_pPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_VSConstants);
    m_pPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "cbCameraAttribs")->Set(m_CameraAttribsCB);
    if (m_pPSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "cbCameraAttribs"))
    {
        m_pPSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "cbCameraAttribs")->Set(m_CameraAttribsCB);
    }
    if (m_pPSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "cbLightAttribs"))
    {
        m_pPSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "cbLightAttribs")->Set(m_LightAttribsCB);
    }

    if (m_pPSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "PickingBuffer"))
    {
        m_pPSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "PickingBuffer")->Set(m_pPickingBuffer->GetDefaultView(BUFFER_VIEW_UNORDERED_ACCESS));
    }

    // Since we are using mutable variable, we must create a shader resource ``ing object
    // http://diligentgraphics.com/2016/03/23/resource-binding-model-in-diligent-engine-2-0/
    m_pPSO->CreateShaderResourceBinding(&m_SRB[0], true);
    /*m_pPSO->CreateShaderResourceBinding(&m_SRB[1], true);
    m_pPSO->CreateShaderResourceBinding(&m_SRB[2], true);
    m_pPSO->CreateShaderResourceBinding(&m_SRB[3], true);*/

   // InitCommonSRBVars(m_SRB[0], m_CameraAttribsCB, nullptr);

    m_SRB[0]->GetVariableByName(SHADER_TYPE_VERTEX, "cbTransforms")->Set(m_VSConstants);
    m_SRB[0]->GetVariableByName(SHADER_TYPE_PIXEL, "cbTransforms")->Set(m_VSConstants);
    
}

struct Vertex
{
    float3 pos;
    float3 normal;
    float2 uv;
};

//Vertex CubeVerts[] =
//{
//    {float3(150,0,150), float3(0, 0, -1), float2(0,1)},
//    {float3(150,0,-150), float3(0, 0, -1),float2(0,0)},
//    {float3(-150,0,150), float3(0, 0, -1),float2(1,0)},
//    {float3(-150,0,-150), float3(0, 0, -1),float2(1,1)},
//
//
//};

void SapphireApp::CreateVertexBuffer()
{
    // Layout of this structure matches the one we defined in the pipeline state
    

    // Cube vertices

    //      (-1,+1,+1)________________(+1,+1,+1)
    //               /|              /|
    //              / |             / |
    //             /  |            /  |
    //            /   |           /   |
    //(-1,-1,+1) /____|__________/(+1,-1,+1)
    //           |    |__________|____|
    //           |   /(-1,+1,-1) |    /(+1,+1,-1)
    //           |  /            |   /
    //           | /             |  /
    //           |/              | /
    //           /_______________|/
    //        (-1,-1,-1)       (+1,-1,-1)
    //

    // clang-format off
    // This time we have to duplicate verices because texture coordinates cannot
    // be shared
    Vertex CubeVerts[] =
    {
        {float3(-1,-1,-1), float3(0, 0, -1), float2(0,1)},
        {float3(-1,+1,-1), float3(0, 0, -1),float2(0,0)},
        {float3(+1,+1,-1), float3(0, 0, -1),float2(1,0)},
        {float3(+1,-1,-1), float3(0, 0, -1),float2(1,1)},

        {float3(-1,-1,-1), float3(0, -1, 0),float2(0,1)},
        {float3(-1,-1,+1), float3(0, -1, 0),float2(0,0)},
        {float3(+1,-1,+1), float3(0, -1, 0),float2(1,0)},
        {float3(+1,-1,-1), float3(0, -1, 0),float2(1,1)},

        {float3(+1,-1,-1), float3(1, 0, 0),float2(0,1)},
        {float3(+1,-1,+1), float3(1, 0, 0),float2(1,1)},
        {float3(+1,+1,+1), float3(1, 0, 0),float2(1,0)},
        {float3(+1,+1,-1), float3(1, 0, 0),float2(0,0)},

        {float3(+1,+1,-1), float3(0, 1, 0), float2(0,1)},
        {float3(+1,+1,+1), float3(0, 1, 0),float2(0,0)},
        {float3(-1,+1,+1), float3(0, 1, 0),float2(1,0)},
        {float3(-1,+1,-1), float3(0, 1, 0),float2(1,1)},

        {float3(-1,+1,-1), float3(-1, 0, 0),float2(1,0)},
        {float3(-1,+1,+1), float3(-1, 0, 0),float2(0,0)},
        {float3(-1,-1,+1), float3(-1, 0, 0),float2(0,1)},
        {float3(-1,-1,-1), float3(-1, 0, 0),float2(1,1)},

        {float3(-1,-1,+1), float3(0, 0, 1),float2(1,1)},
        {float3(+1,-1,+1), float3(0, 0, 1),float2(0,1)},
        {float3(+1,+1,+1), float3(0, 0, 1),float2(0,0)},
        {float3(-1,+1,+1), float3(0, 0, 1),float2(1,0)}
    };

    
    // clang-format on

    BufferDesc VertBuffDesc;
    VertBuffDesc.Name = "Cube vertex buffer";
    VertBuffDesc.Usage = USAGE_IMMUTABLE;
    VertBuffDesc.BindFlags = BIND_VERTEX_BUFFER;
    VertBuffDesc.Size = sizeof(CubeVerts);
    BufferData VBData;
    VBData.pData = CubeVerts;
    VBData.DataSize = sizeof(CubeVerts);
    m_pDevice->CreateBuffer(VertBuffDesc, &VBData, &m_CubeVertexBuffer);

    // clang-format off
    // This time we have to duplicate verices because texture coordinates cannot
    // be shared
    Vertex QuadVerts[] =
    {
       {float3(-1,0,-1), float3(0, 0, 1),float2(0,1)},
       {float3(-1,0,-1), float3(0, 0, 1),float2(0,0)},
       {float3(+1,0,-1), float3(0, 0, 1),float2(1,0)},
       {float3(+1,1,-1), float3(0, 0, 1),float2(1,1)},
    };
    // clang-format on

    BufferDesc VertBuffDesc2;
    VertBuffDesc2.Name = "Sprite vertex buffer";
    VertBuffDesc2.Usage = USAGE_IMMUTABLE;
    VertBuffDesc2.BindFlags = BIND_VERTEX_BUFFER;
    VertBuffDesc2.Size = sizeof(QuadVerts);
    BufferData VBData2;
    VBData2.pData = QuadVerts;
    VBData2.DataSize = sizeof(QuadVerts);
    m_pDevice->CreateBuffer(VertBuffDesc2, &VBData2, &m_QuadVertexBuffer);

    float3 PlaneVerts[] =
    {
        float3(-10, 0, -10),
        float3(-10, 0, 10),
        float3(10, 0, 10),
        float3(-10, 0, -10),
        float3(10, 0, 10),
        float3(10, 0, -10),

        /*float3(0,0,0),
        float3(1, 0, 0),
        float3(1,1,1),*/
    };

    BufferDesc PlaneVertBuffDesc;
    PlaneVertBuffDesc.Name = "Plane vertex buffer";
    PlaneVertBuffDesc.Usage = USAGE_IMMUTABLE;
    PlaneVertBuffDesc.BindFlags = BIND_VERTEX_BUFFER;
    PlaneVertBuffDesc.Size = sizeof(PlaneVerts);
    BufferData PlaneVBData;
    PlaneVBData.pData = PlaneVerts;
    PlaneVBData.DataSize = sizeof(PlaneVerts);
    m_pDevice->CreateBuffer(PlaneVertBuffDesc, &PlaneVBData, &m_PlaneVertexBuffer);

    Clipper2Lib::Point64 p;
    Clipper2Lib::Paths64 op;
    Clipper2Lib::Paths64 op2;
    op.push_back(Clipper2Lib::MakePath({ -400, 200 , -400, 0, 500, 0, 700, 200, 900, 0 }));
    op2 = Clipper2Lib::InflatePaths(op, 40, Clipper2Lib::JoinType::Round, Clipper2Lib::EndType::Round, 3);

    std::vector<std::vector<Clipper2Lib::Point64>> polygon;

    std::vector<uint32_t> pol_indices = mapbox::earcut<uint32_t>(op2);

    Vertex polygonVertices[512];

    float3 centerOfMass = { 0.0f, 0.0f, 0.0f };
    size_t numPolyVertices = op2[0].size();
    float scale = 0.01f;
    for (int i = 0; i < numPolyVertices; ++i)
    {
        polygonVertices[i].pos.x = (float)op2[0][i].x * scale;
        polygonVertices[i].pos.y = (float)op2[0][i].y * scale;
        polygonVertices[i].pos.z = -1;
        polygonVertices[i].uv.u = 0;
        polygonVertices[i].uv.v = 1;
        centerOfMass.x += (float)op2[0][i].x * scale;
        centerOfMass.y += (float)op2[0][i].y * scale;
    }

    centerOfMass /= (float)numPolyVertices;

    float fillWidthWorldUnits = 1000.0f;
    float fillHeightWorldUnits = 1000.0f;
    float world2UV = 2.0f;

    for (int i = 0; i < numPolyVertices; ++i)
    {
        float2 dist = polygonVertices[i].pos - centerOfMass;
        float u = 0.5f + (dist.x / fillWidthWorldUnits) * world2UV;
        float v = 0.5f - (dist.y / fillHeightWorldUnits) * world2UV;
        polygonVertices[i].uv.u = u;
        polygonVertices[i].uv.v = v;
    }


    BufferDesc VertBuffDescTerrain;
    VertBuffDescTerrain.Name = "Sprite vertex buffer";
    VertBuffDescTerrain.Usage = USAGE_IMMUTABLE;
    VertBuffDescTerrain.BindFlags = BIND_VERTEX_BUFFER;
    VertBuffDescTerrain.Size = numPolyVertices * sizeof(Vertex);
    BufferData VBData3;
    VBData3.pData = polygonVertices;
    VBData3.DataSize = numPolyVertices * sizeof(Vertex);
    m_pDevice->CreateBuffer(VertBuffDescTerrain, &VBData3, &m_TerrainVertexBuffer);

    BufferDesc IndBuffDesc;
    IndBuffDesc.Name = "Cube index buffer";
    IndBuffDesc.Usage = USAGE_IMMUTABLE;
    IndBuffDesc.BindFlags = BIND_INDEX_BUFFER;
    IndBuffDesc.Size = pol_indices.size() * sizeof(uint32_t);
    BufferData IBData;
    IBData.pData = pol_indices.data();
    IBData.DataSize = pol_indices.size() * sizeof(uint32_t);
    m_pDevice->CreateBuffer(IndBuffDesc, &IBData, &m_TerrainIndexBuffer);


    
}


static int frame = 0;

void SapphireApp::CreateIndexBuffer()
{
    // clang-format off
   /* Uint32 Indices[] =
    {
        2,0,1,    3, 2, 1,
        
    };*/

    Uint32 Indices[] =
    {
        2,0,1,    2,3,0,
        4,6,5,    4,7,6,
        8,10,9,   8,11,10,
        12,14,13, 12,15,14,
        16,18,17, 16,19,18,
        20,21,22, 20,22,23
    };
    // clang-format on

    BufferDesc IndBuffDesc;
    IndBuffDesc.Name = "Cube index buffer";
    IndBuffDesc.Usage = USAGE_IMMUTABLE;
    IndBuffDesc.BindFlags = BIND_INDEX_BUFFER;
    IndBuffDesc.Size = sizeof(Indices);
    BufferData IBData;
    IBData.pData = Indices;
    IBData.DataSize = sizeof(Indices);
    m_pDevice->CreateBuffer(IndBuffDesc, &IBData, &m_CubeIndexBuffer);

    // clang-format off
    Uint32 Indices2[] =
    {
        0, 1, 2,    0, 2, 3
    };
    // clang-format on

    BufferDesc IndBuffDesc2;
    IndBuffDesc2.Name = "Cube index buffer";
    IndBuffDesc2.Usage = USAGE_IMMUTABLE;
    IndBuffDesc2.BindFlags = BIND_INDEX_BUFFER;
    IndBuffDesc2.Size = sizeof(Indices2);
    BufferData IBData2;
    IBData2.pData = Indices2;
    IBData2.DataSize = sizeof(Indices2);
    m_pDevice->CreateBuffer(IndBuffDesc2, &IBData2, &m_QuadIndexBuffer);
}

void SapphireApp::LoadTextures()
{
    TextureLoadInfo loadInfo;
    loadInfo.IsSRGB = true;
    
    RefCntAutoPtr<ITexture> Tex;
    //CreateTextureFromFile("pirate-gold_albedo.png", loadInfo, m_pDevice, &Tex);
    CreateTextureFromFile("dungeon_floor_dif.png", loadInfo, m_pDevice, &Tex);
    // Get shader resource view from the texture
    m_TextureSRV[0] = Tex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);

    RefCntAutoPtr<ITexture> Tex2;
    //CreateTextureFromFile("pirate-gold_normal-ogl.png", loadInfo, m_pDevice, &Tex2);
    CreateTextureFromFile("dungeon_floor_normal.png", loadInfo, m_pDevice, &Tex2);
    // Get shader resource view from the texture
    m_TextureSRV[1] = Tex2->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);

    RefCntAutoPtr<ITexture> Tex3;
    //CreateTextureFromFile("pirate-gold_mr.png", loadInfo, m_pDevice, &Tex3);
    CreateTextureFromFile("dungeon_floor_mr.png", loadInfo, m_pDevice, &Tex3);
    // Get shader resource view from the texture
    m_TextureSRV[2] = Tex3->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);

    RefCntAutoPtr<ITexture> Tex4;
    CreateTextureFromFile("run2.png", loadInfo, m_pDevice, &Tex4);
    // Get shader resource view from the texture
    m_TextureSRV[3] = Tex4->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);

    // Set texture SRV in the SRB
    if (m_SRB[0]->GetVariableByName(SHADER_TYPE_PIXEL, "g_AlbedoTexture"))
    {
        m_SRB[0]->GetVariableByName(SHADER_TYPE_PIXEL, "g_AlbedoTexture")->Set(m_TextureSRV[0]);
    }
    
    if (m_SRB[0]->GetVariableByName(SHADER_TYPE_PIXEL, "g_NormalsTexture"))
    {
        m_SRB[0]->GetVariableByName(SHADER_TYPE_PIXEL, "g_NormalsTexture")->Set(m_TextureSRV[1]);
    }

    if (m_SRB[0]->GetVariableByName(SHADER_TYPE_PIXEL, "g_PhysicalDescriptorMap"))
    {
        m_SRB[0]->GetVariableByName(SHADER_TYPE_PIXEL, "g_PhysicalDescriptorMap")->Set(m_TextureSRV[2]);
    }
    
    
    
    /*m_SRB[1]->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture")->Set(m_TextureSRV[1]);
    m_SRB[2]->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture")->Set(m_TextureSRV[2]);
    m_SRB[3]->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture")->Set(m_TextureSRV[3]);*/
    return;
}


void SapphireApp::Initialize(const SampleInitInfo& InitInfo)
{
    LOG_INFO_MESSAGE("HARA");

    /*_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    _CrtSetBreakAlloc(2570);*/

    static Sapphire::SapphireSystemAllocator s_systemAllocator;
    
   
    //if (luaL_dofile(L, "hello.lua")) {

    //Sapphire::HashMap<uint64_t, uint32_t> mapStrToIndex = {};
    
    
    //mapStrToIndex.allocator = &s_systemAllocator;
    //mapStrToIndex.default_value = 0;
    //mapStrToIndex.addValue(0, 110);
    //mapStrToIndex.addValue(1, 111);

    //uint32_t val = mapStrToIndex.hasKey(111);
    //printf("%d", val);

    SampleBase::Initialize(InitInfo);
    // Reset default colors
    ImGui::StyleColorsDark();


    /// picking buffers
    BufferDesc BuffDesc;
    BuffDesc.Name = "Statistics buffer";
    BuffDesc.Usage = USAGE_DEFAULT;
    BuffDesc.BindFlags = BIND_UNORDERED_ACCESS;
    BuffDesc.Mode = BUFFER_MODE_RAW;
    BuffDesc.Size = sizeof(PickingBuffer);

    m_pDevice->CreateBuffer(BuffDesc, nullptr, &m_pPickingBuffer);


    // Staging buffer is needed to read the data from statistics buffer.

    BuffDesc.Name = "Statistics staging buffer";
    BuffDesc.Usage = USAGE_STAGING;
    BuffDesc.BindFlags = BIND_NONE;
    BuffDesc.Mode = BUFFER_MODE_UNDEFINED;
    BuffDesc.CPUAccessFlags = CPU_ACCESS_READ;
    BuffDesc.Size = sizeof(PickingBuffer);

    m_pDevice->CreateBuffer(BuffDesc, nullptr, &m_pPickingBufferStaging);


    FenceDesc FDesc;
    FDesc.Name = "Statistics available";
    m_pDevice->CreateFence(FDesc, &m_pPickingInfoAvailable);

    CreateRenderTargetPSO();
    CreateSelectionCompositPSO();
    CreatePipelineState();
    CreateSelectionRenderTargetPSO();
    CreatePlanePipelineState();
    CreateVertexBuffer();
    CreateIndexBuffer();

    m_LightDirection = normalize(float3(0.5f, -0.6f, -0.2f));
    m_CameraDistance = 15.0f;

    LoadTextures();

    m_worldResourceManager.init();

    /*lua_State* L = initLuaContext(this);



    if (luaL_dofile(L, "C:/Programming/Sapphire/assets/materials.lua")) {
    }


    lua_close(L);*/

    SapphireMeshLoadData meshLoadData;
    loadModelFromFile("C:/Games/Legend of Grimrock/asset_pack_v2/assets/models/env/dungeon_floor_01.model", &meshLoadData);

   // m_worldResourceManager.loadMeshResouces(m_pDevice, m_pImmediateContext, "", meshLoadData);
}

void SapphireApp::UpdateUI()
{
    //const auto& SCDesc = m_pSwapChain->GetDesc();
    //uint32_t screenWidth = SCDesc.Width;
    //uint32_t screenHeight = SCDesc.Height;
    //printf("%d %d", screenWidth, screenHeight);
    //{
    //    ImVec2 window_pos = ImVec2((float)(screenWidth / 2), (float)(screenHeight / 2));
    //    ImGui::SetNextWindowPos(window_pos - ImVec2(130 / 2, 130 / 2));
    //    ImGui::SetNextWindowSize(ImVec2(500, 500));
    //    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    //    ImGui::Begin("Transp", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove); // Create a window called "Hello, world!" and append into it.        
    //    ImGui::SetCursorScreenPos(window_pos - ImVec2(130 / 2,130 /2));
    //    ImGui::mygizmo3D("Model Rotation", m_modelRotation, ImGui::GetTextLineHeight() * 10);
    //    ImGui::End();
    //    ImGui::PopStyleVar(1);
    //}

    // 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
    {
        ImGui::Begin("Hello, world!"); // Create a window called "Hello, world!" and append into it.

        ImGui::Text("This is some useful text.");          // Display some text (you can use a format strings too)
        ImGui::Checkbox("Demo Window", &m_ShowDemoWindow); // Edit bools storing our window open/close state
        ImGui::Checkbox("Another Window", &m_ShowAnotherWindow);

        static float f = 0.0f;
        ImGui::SliderFloat("float", &f, 0.0f, 1.0f);             // Edit 1 float using a slider from 0.0f to 1.0f
        ImGui::ColorEdit3("clear color", (float*)&m_ClearColor); // Edit 3 floats representing a color

        ImGui::gizmo3D("Model Rotation", m_modelRotation, ImGui::GetTextLineHeight() * 10);
        ImGui::SameLine();
        ImGui::gizmo3D("Light direction", m_LightDirection, ImGui::GetTextLineHeight() * 10);
        ImGui::ColorEdit3("Light Color", &m_LightColor.r);
        // clang-format off
        ImGui::SliderFloat("Light Intensity", &m_LightIntensity, 0.f, 50.f);
        ImGui::SliderFloat("Camera distance", &m_CameraDistance, 0.1f, 200.0f);

        static int counter = 0;
        if (ImGui::Button("Button")) // Buttons return true when clicked (most widgets return true when edited/activated)
            counter++;
        ImGui::SameLine();
        ImGui::Text("counter = %d", counter);

        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        ImGui::End();
    }

    // 3. Show another simple window.
    if (m_ShowAnotherWindow)
    {
        ImGui::Begin("Another Window", &m_ShowAnotherWindow); // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
        ImGui::Text("Hello from another window!");
        if (ImGui::Button("Close Me"))
            m_ShowAnotherWindow = false;
        ImGui::End();
    }
}

// Render a frame
void SapphireApp::Render()
{

    

    auto ClearColor = m_ClearColor;
    if (GetTextureFormatAttribs(m_pSwapChain->GetDesc().ColorBufferFormat).ComponentType == COMPONENT_TYPE_UNORM_SRGB)
    {
        ClearColor = SRGBToLinear(ClearColor);
    }
    if (GetTextureFormatAttribs(RenderTargetFormat).ComponentType == COMPONENT_TYPE_UNORM_SRGB)
    {
        ClearColor = SRGBToLinear(ClearColor);
    }

    m_pImmediateContext->SetRenderTargets(1, &m_pColorRTV, m_pDepthDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->ClearRenderTarget(m_pColorRTV, &ClearColor.x, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->ClearDepthStencil(m_pDepthDSV, CLEAR_DEPTH_FLAG, 1.0f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Reset picking buffer
    PickingBuffer picking_buffer;
    std::memset(&picking_buffer, 0, sizeof(picking_buffer));
    m_pImmediateContext->UpdateBuffer(m_pPickingBuffer, 0, sizeof(picking_buffer), &picking_buffer, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    
    
#if 0
    // Camera is at (0, 0, -15) looking along the Z axis
    //float4x4 CameraView = float4x4::Translation(0.f, -m_CameraDistance, 0) ;
    float4x4 CameraView = float4x4::Translation(0.f, 0, m_CameraDistance);

    // Get pretransform matrix that rotates the scene according the surface orientation
    auto SrfPreTransform = GetSurfacePretransformMatrix(float3{ 0, 0, 1 });

    

    // Get projection matrix adjusted to the current screen orientation
    const auto CameraProj = GetAdjustedProjectionMatrix(PI_F / 4.0f, 0.1f, 1000.f);
    const auto CameraViewProj = CameraView * CameraProj;
    float4x4 CameraWorld = CameraView.Inverse();
    float3 CameraWorldPos = float3::MakeVector(CameraWorld[3]);

    


    
    // Set the pipeline state
    m_pImmediateContext->SetPipelineState(m_pPSO);
    // Commit shader resources. RESOURCE_STATE_TRANSITION_MODE_TRANSITION mode
    // makes sure that resources are transitioned to required states.
    m_pImmediateContext->CommitShaderResources(m_SRB[0], RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    DrawIndexedAttribs DrawAttrs;     // This is an indexed draw call

    //float4x4 terrainWorldTransform = float4x4::Translation(0, -2, 0) * m_ViewProjMatrix;
    //{
    //    // Map the buffer and write current world-view-projection matrix
    //    MapHelper<float4x4> CBConstants(m_pImmediateContext, m_VSConstants, MAP_WRITE, MAP_FLAG_DISCARD);
    //    *CBConstants = terrainWorldTransform.Transpose();
    //}

    //IBuffer* pBuffs3[] = { m_TerrainVertexBuffer };
    //m_pImmediateContext->SetVertexBuffers(0, 1, pBuffs3, &offset2, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    //m_pImmediateContext->SetIndexBuffer(m_TerrainIndexBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    //DrawAttrs.NumIndices = 99;
    //DrawAttrs.IndexType = VT_UINT32; // Index type
    //m_pImmediateContext->DrawIndexed(DrawAttrs);

    /*IBuffer* pBuffs2[] = { m_QuadVertexBuffer };
    m_pImmediateContext->SetVertexBuffers(0, 1, pBuffs2, &offset2, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    m_pImmediateContext->SetIndexBuffer(m_QuadIndexBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);*/

    
    //float4x4 playerWorldTransform = m_modelRotation.ToMatrix();
    float3 world_pos = {0,1,0};
    float4x4 playerWorldTransform =  float4x4::Translation(world_pos) * m_modelRotation.ToMatrix();

    {
        // Map the buffer and write current world-view-projection matrix
        MapHelper<float4x4> CBConstants(m_pImmediateContext, m_VSConstants, MAP_WRITE, MAP_FLAG_DISCARD);
        *CBConstants = playerWorldTransform;
    }

#if 0
    const SapphireMesh* pMesh = m_worldResourceManager.GetMesh(0);

    IBuffer* pVBs[] = { m_worldResourceManager.getVertexBuffer(pMesh->m_vb) };
    m_pImmediateContext->SetVertexBuffers(0, 1, pVBs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);

    auto* pIB = m_worldResourceManager.getIndexBuffer(pMesh->m_ib);
    //auto  IBFormat = m_Mesh.GetIBFormat(meshIdx);

    m_pImmediateContext->SetIndexBuffer(pIB, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    //auto  PSOIndex = m_PSOIndex[SubMesh.VertexBuffers[0]];
    auto& pPSO = m_pPSO;//(bIsShadowPass ? m_RenderMeshShadowPSO : m_RenderMeshPSO)[PSOIndex];
    m_pImmediateContext->SetPipelineState(pPSO);

    // Draw all subsets
    for (Uint32 subsetIdx = 0; subsetIdx < pMesh->m_numSubsets; ++subsetIdx)
    {
        const SapphireSubmesh& Subset = pMesh->m_subMeshes[subsetIdx];
        m_pImmediateContext->CommitShaderResources((m_SRB)[0], RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        DrawIndexedAttribs drawAttrs(static_cast<Uint32>(Subset.m_indicesCount), VT_UINT32, DRAW_FLAG_VERIFY_ALL);
        drawAttrs.FirstIndexLocation = static_cast<Uint32>(Subset.m_indicesStart);
        m_pImmediateContext->DrawIndexed(drawAttrs);
    }
#endif

    IBuffer* pBuffs[] = { m_CubeVertexBuffer };
    m_pImmediateContext->SetVertexBuffers(0, 1, pBuffs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    m_pImmediateContext->SetIndexBuffer(m_CubeIndexBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
   // 
    DrawAttrs.IndexType = VT_UINT32; // Index type
   DrawAttrs.NumIndices = 36;
   // DrawAttrs.NumIndices = 6;
   // // Verify the state of vertex and index buffers
    DrawAttrs.Flags = DRAW_FLAG_VERIFY_ALL;
    m_pImmediateContext->DrawIndexed(DrawAttrs);


    
#else

    float4x4 CameraView = float4x4::Translation(0.f, -1, m_CameraDistance);

    // Get projection matrix adjusted to the current screen orientation
    const auto CameraProj = GetAdjustedProjectionMatrix(PI_F / 4.0f, 0.1f, 100.f);
    const auto CameraViewProj = CameraView * CameraProj;

    static bool has_pick_request = false;
    static bool has_signaled = false;
    static Uint64 pick_frame = 1;

    float4x4 CameraWorld = CameraView.Inverse();
    float3 CameraWorldPos = float3::MakeVector(CameraWorld[3]);
    {
        MapHelper<CameraAttribs> CamAttribs(m_pImmediateContext, m_CameraAttribsCB, MAP_WRITE, MAP_FLAG_DISCARD);
        CamAttribs->mViewT = CameraView.Transpose();
        CamAttribs->mProjT = CameraProj.Transpose();
        CamAttribs->mViewProjT = CameraViewProj.Transpose();
        CamAttribs->mViewProjInvT = CameraViewProj.Inverse().Transpose();
        CamAttribs->f4Position = float4(CameraWorldPos, 1);
        // store mouse position and and picking opacity threshold
        auto inputController = GetInputController();
        auto mouse_state = inputController.GetMouseState();

        if (!has_pick_request)
        {
            has_pick_request = mouse_state.ButtonFlags & MouseState::BUTTON_FLAG_LEFT;
            CamAttribs->f4ExtraData[0] = float4(mouse_state.PosX, mouse_state.PosY, 0.5, mouse_state.ButtonFlags & MouseState::BUTTON_FLAG_LEFT);
            pick_frame = m_FrameId;
        }
        
        
        
    }

    {
#if 1

        static const uint32_t grid_extent = 100;

        {
            MapHelper<GridAttribs> GridAttribs(m_pImmediateContext, m_GridAttribsCB, MAP_WRITE, MAP_FLAG_DISCARD);
            GridAttribs->origin_axis_x_color = float4(1, 0, 0, 1);
            GridAttribs->origin_axis_z_color = float4(0, 0, 1, 1);
            GridAttribs->thin_lines_color = float4(0.07f, 0.07f, 0.07f, 1);
            GridAttribs->thick_lines_color = float4(0, 0, 0, 1);
            GridAttribs->cell_size = 1;
            GridAttribs->grid_extent = grid_extent;
            GridAttribs->infinite = 1;            

        }

        static const float grid_tile_size = 100.f;
        
        uint32_t num_tiles_per_axis = (uint32_t)ceil((grid_extent * 2.f) / grid_tile_size);
        uint32_t num_vertices = num_tiles_per_axis * num_tiles_per_axis * 6;
        

        DrawAttribs draw_attribs;     // This is an indexed draw call
        draw_attribs.NumVertices = num_vertices;
        draw_attribs.Flags = DRAW_FLAG_VERIFY_ALL;

        m_pImmediateContext->SetPipelineState(m_pPlanePSO);
        m_pImmediateContext->CommitShaderResources(m_PlaneSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->SetVertexBuffers(0, 1, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_NONE, SET_VERTEX_BUFFERS_FLAG_RESET);
        m_pImmediateContext->SetIndexBuffer(nullptr, 0, RESOURCE_STATE_TRANSITION_MODE_NONE);
        //IBuffer* pPlaneBuffs[] = { m_PlaneVertexBuffer };
        //m_pImmediateContext->SetVertexBuffers(0, 1, pPlaneBuffs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
        m_pImmediateContext->Draw(draw_attribs);
#endif
        {
            MapHelper<LightAttribs> lightAttribs(m_pImmediateContext, m_LightAttribsCB, MAP_WRITE, MAP_FLAG_DISCARD);
            lightAttribs->f4Direction = m_LightDirection;
            lightAttribs->f4Direction.z *= -1.0f;
            lightAttribs->f4Intensity = m_LightColor * m_LightIntensity;
        }

        m_pImmediateContext->SetPipelineState(m_pPSO);
        m_pImmediateContext->CommitShaderResources(m_SRB[0], RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        {
            //// Draw Cube

            float3 world_pos = { 0,1,0 };
            float4x4 playerWorldTransform = m_modelRotation.ToMatrix() * float4x4::Translation(world_pos);
            {
                // Map the buffer and write current world-view-projection matrix
                MapHelper<DrawCallConstants> cbDrawCallConstants(m_pImmediateContext, m_VSConstants, MAP_WRITE, MAP_FLAG_DISCARD);
                cbDrawCallConstants->transform = playerWorldTransform;
                cbDrawCallConstants->identity = 0x1;
            }
            
            
            IBuffer* pBuffs[] = { m_CubeVertexBuffer };
            m_pImmediateContext->SetVertexBuffers(0, 1, pBuffs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
            m_pImmediateContext->SetIndexBuffer(m_CubeIndexBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            // 
            DrawIndexedAttribs DrawAttrs;     // This is an indexed draw call
            DrawAttrs.IndexType = VT_UINT32; // Index type
            DrawAttrs.NumIndices = 36;
            // DrawAttrs.NumIndices = 6;
            // // Verify the state of vertex and index buffers
            DrawAttrs.Flags = DRAW_FLAG_VERIFY_ALL;
            m_pImmediateContext->DrawIndexed(DrawAttrs);
        }

        {
            //// Draw Cube

            float3 world_pos = { 1,1,1 };
            float4x4 playerWorldTransform = m_modelRotation.ToMatrix() * float4x4::Translation(world_pos);
            {
                // Map the buffer and write current world-view-projection matrix
                MapHelper<DrawCallConstants> cbDrawCallConstants(m_pImmediateContext, m_VSConstants, MAP_WRITE, MAP_FLAG_DISCARD);
                cbDrawCallConstants->transform = playerWorldTransform;
                cbDrawCallConstants->identity = 0x8;
            }

            
            IBuffer* pBuffs[] = { m_CubeVertexBuffer };
            m_pImmediateContext->SetVertexBuffers(0, 1, pBuffs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
            m_pImmediateContext->SetIndexBuffer(m_CubeIndexBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            // 
            DrawIndexedAttribs DrawAttrs;     // This is an indexed draw call
            DrawAttrs.IndexType = VT_UINT32; // Index type
            DrawAttrs.NumIndices = 36;
            // DrawAttrs.NumIndices = 6;
            // // Verify the state of vertex and index buffers
            DrawAttrs.Flags = DRAW_FLAG_VERIFY_ALL;
            m_pImmediateContext->DrawIndexed(DrawAttrs);
        }

        //// render to selection buffer
        m_pImmediateContext->SetRenderTargets(1, &m_pSelectionColorRTV, m_pSelectionDepthDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->ClearRenderTarget(m_pSelectionColorRTV, &ClearColor.x, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->ClearDepthStencil(m_pSelectionDepthDSV, CLEAR_DEPTH_FLAG, 1.0f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->SetPipelineState(m_pSelectionRTPSO);
        m_pImmediateContext->CommitShaderResources(m_pSelectionRTSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        {
            //// Draw Cube

            float3 world_pos = { 0,1,0 };
            float4x4 playerWorldTransform = m_modelRotation.ToMatrix() * float4x4::Translation(world_pos);
            {
                // Map the buffer and write current world-view-projection matrix
                MapHelper<DrawCallConstants> cbDrawCallConstants(m_pImmediateContext, m_VSConstants, MAP_WRITE, MAP_FLAG_DISCARD);
                cbDrawCallConstants->transform = playerWorldTransform;
                cbDrawCallConstants->identity = 0x1;
            }


            IBuffer* pBuffs[] = { m_CubeVertexBuffer };
            m_pImmediateContext->SetVertexBuffers(0, 1, pBuffs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
            m_pImmediateContext->SetIndexBuffer(m_CubeIndexBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            // 
            DrawIndexedAttribs DrawAttrs;     // This is an indexed draw call
            DrawAttrs.IndexType = VT_UINT32; // Index type
            DrawAttrs.NumIndices = 36;
            // DrawAttrs.NumIndices = 6;
            // // Verify the state of vertex and index buffers
            DrawAttrs.Flags = DRAW_FLAG_VERIFY_ALL;
            m_pImmediateContext->DrawIndexed(DrawAttrs);
        }
       
    }
#endif

    if (has_pick_request)
    {
        if (!has_signaled)
        {
            m_pImmediateContext->CopyBuffer(m_pPickingBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                m_pPickingBufferStaging, 0, sizeof(PickingBuffer),
                RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            // We should use synchronizations to safely access the mapped memory.
            m_pImmediateContext->EnqueueSignal(m_pPickingInfoAvailable, m_FrameId);
            has_signaled = true;
        }
       

        // Read statistics from previous frame.
        Uint64 AvailableFrameId = m_pPickingInfoAvailable->GetCompletedValue();

        //int historySize = 10;

        //// Synchronize
        //if (m_FrameId - AvailableFrameId > historySize)
        //{
        //    // In theory we should never get here as we wait for more than enough
        //    // frames.
        //    AvailableFrameId = m_FrameId - historySize;
        //    m_pPickingInfoAvailable->Wait(AvailableFrameId);
        //}

        // Read the staging data
        if (AvailableFrameId >= pick_frame)
        {
            MapHelper<PickingBuffer> StagingData(m_pImmediateContext, m_pPickingBufferStaging, MAP_READ, MAP_FLAG_DO_NOT_WAIT);
            if (StagingData)
            {
                uint64_t hello_from_space = StagingData->identity;
                printf("%lld\n", hello_from_space);
            }
            has_pick_request = false;
            has_signaled = false;
        }
        
        ++m_FrameId;
        
    }

    // render output to render target

    auto* pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
    auto* pDSV = m_pSwapChain->GetDepthBufferDSV();
    const float Zero[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    m_pImmediateContext->SetRenderTargets(1, &pRTV, pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->ClearRenderTarget(pRTV, Zero, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Set the render target pipeline state
    m_pImmediateContext->SetPipelineState(m_pRTPSO);

    // Commit the render target shader's resources
    m_pImmediateContext->CommitShaderResources(m_pRTSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Draw the render target's vertices
    DrawAttribs RTDrawAttrs;
    RTDrawAttrs.NumVertices = 4;
    RTDrawAttrs.Flags = DRAW_FLAG_VERIFY_ALL; // Verify the state of vertex and index buffers
    m_pImmediateContext->Draw(RTDrawAttrs);

    // run composite selection pass

    // Set the render target pipeline state
    m_pImmediateContext->SetPipelineState(m_pSelectionCompositPSO);

    // Commit the render target shader's resources
    m_pImmediateContext->CommitShaderResources(m_pSelectionCompositSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Draw the render target's vertices
    
    RTDrawAttrs.NumVertices = 4;
    RTDrawAttrs.Flags = DRAW_FLAG_VERIFY_ALL; // Verify the state of vertex and index buffers
    m_pImmediateContext->Draw(RTDrawAttrs);
  

    
    // set PSO
    
    //DrawAttrs.NumIndices = 6;
    //// Verify the state of vertex and index buffers
    //DrawAttrs.Flags = DRAW_FLAG_VERIFY_ALL;
    //m_pImmediateContext->DrawIndexed(DrawAttrs);

   
}

float4x4 SapphireApp::GetOrthographicMatrix(float orthographicSize)
{
    float4x4 projMat;

    const auto& SCDesc = m_pSwapChain->GetDesc();

    projMat = float4x4::Ortho((static_cast<float>(SCDesc.Width) / static_cast<float>(SCDesc.Height)) * orthographicSize, orthographicSize, 0.1f, 100.0f, m_pDevice->GetDeviceInfo().IsGLDevice());
    return projMat;
}

void SapphireApp::UpdateLuaFrameStart(double CurrTime, double ElapsedTime)
{

}

void SapphireApp::UpdateLuaFrameEnd(double CurrTime, double ElapsedTime)
{

}

void SapphireApp::Update(double CurrTime, double ElapsedTime)
{
    SampleBase::Update(CurrTime, ElapsedTime);

    UpdateLuaFrameStart(CurrTime, ElapsedTime);


    bool isRunning = false;
    

    auto inputController = GetInputController();
    if (inputController.IsKeyFirstDown(InputKeys::MoveLeft))
    {
        LOG_INFO_MESSAGE("Key Down First!");
    }
    if (inputController.IsKeyDown(InputKeys::MoveLeft))
    {
        LOG_INFO_MESSAGE("Key Down!");
        isRunning = true;
        playerFacingDirection = -1;
        
    }
    if (inputController.IsKeyDown(InputKeys::MoveRight))
    {
        LOG_INFO_MESSAGE("Key Down!");
        isRunning = true;
        playerFacingDirection = 1;
    }
    if (inputController.IsKeyReleased(InputKeys::MoveLeft))
    {
        LOG_INFO_MESSAGE("Key released!");
    }

    
    
    // Apply rotation
    //float4x4 CubeModelTransform = float4x4::RotationZ(-PI_F * 0.0f);

   
    

    // Camera is at (0, 0, -15) looking along the Z axis
    float4x4 View = float4x4::Translation(0.f, 0.0f, 15.0f);

    // Get pretransform matrix that rotates the scene according the surface orientation
    auto SrfPreTransform = GetSurfacePretransformMatrix(float3{ 0, 0, 1 });

    // Get projection matrix adjusted to the current screen orientation
    auto Proj = GetAdjustedProjectionMatrix(PI_F / 4.0f, 0.1f, 1000.f);
   // Proj = GetOrthographicMatrix(20.0f);
    m_ViewProjMatrix = View * SrfPreTransform * Proj;
    
#if 0
 

    if (ElapsedTime > 0.25)
    {
        ElapsedTime = 0.25;
    }
    m_timeAccumulator += ElapsedTime;
    float3 prevPlayerPos = playerPos;

    const double physicsDT = 1 / 60.0;
    while (m_timeAccumulator >= physicsDT)
    {
        prevPlayerPos = playerPos;
        playerPos.x += playerSpeed.x * (float)physicsDT;
        playerPos.y += playerSpeed.y * (float)physicsDT;

        playerSpeed.x += playerAcceleration.x * (float)physicsDT * playerFacingDirection;
       // playerSpeed.y += playerAcceleration.y * (float)physicsDT;
        if (!isRunning)
        {
            if (playerSpeed.x < 0)
            {
                playerSpeed.x += playerDecceleration.x;
            }
            else if (playerSpeed.x > 0)
            {
                playerSpeed.x -= playerDecceleration.x;
            }
            if (playerSpeed.x > 0 && playerSpeed.x < playerDecceleration.x) playerSpeed.x = 0;
            if (playerSpeed.x < 0 && playerSpeed.x > -playerDecceleration.x) playerSpeed.x = 0;
        }
        
        m_timeAccumulator -= physicsDT;
    }

    float alpha = (float)(m_timeAccumulator / physicsDT);
    playerPos = alpha * prevPlayerPos + (1 - alpha) * playerPos;

    
    // Compute world-view-projection matrix
    m_ViewProjMatrix =  View * SrfPreTransform * Proj;

    static float animSpeed = 0.5;
    if (isRunning == true)
    {
        animSpeed = 0.2f;
        if (frame < 2)
        {
            frame = 2;
        }
    }
    else
    {
        animSpeed = 0.5f;
        if (frame > 1)
        {
            frame = 0;
        }
    }
    static double animTime = 0;
    animTime += ElapsedTime;
    if (animTime > animSpeed)
    {
        animTime = 0;
        if (isRunning)
        {
            frame -= 2;
        }
        frame = 1 - frame;
        if (isRunning)
        {
            frame += 2;
        }
    }
    
#endif

    UpdateUI();

    UpdateLuaFrameEnd(CurrTime, ElapsedTime);
}

void SapphireApp::WindowResize(Uint32 Width, Uint32 Height)
{
    // Create window - size offscreen render target
    TextureDesc RTColorDesc;
    RTColorDesc.Name = "Offscreen render target";
    RTColorDesc.Type = RESOURCE_DIM_TEX_2D;
    RTColorDesc.Width = m_pSwapChain->GetDesc().Width;
    RTColorDesc.Height = m_pSwapChain->GetDesc().Height;
    RTColorDesc.MipLevels = 1;
    RTColorDesc.Format = RenderTargetFormat;
    // The render target can be bound as a shader resource and as a render target
    RTColorDesc.BindFlags = BIND_SHADER_RESOURCE | BIND_RENDER_TARGET;
    // Define optimal clear value
    RTColorDesc.ClearValue.Format = RTColorDesc.Format;
    RTColorDesc.ClearValue.Color[0] = 0.350f;
    RTColorDesc.ClearValue.Color[1] = 0.350f;
    RTColorDesc.ClearValue.Color[2] = 0.350f;
    RTColorDesc.ClearValue.Color[3] = 1.f;
    RefCntAutoPtr<ITexture> pRTColor;
    m_pDevice->CreateTexture(RTColorDesc, nullptr, &pRTColor);
    // Store the render target view
    m_pColorRTV = pRTColor->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);


    // Create window-size depth buffer
    TextureDesc RTDepthDesc = RTColorDesc;
    RTDepthDesc.Name = "Offscreen depth buffer";
    RTDepthDesc.Format = DepthBufferFormat;
    RTDepthDesc.BindFlags = BIND_SHADER_RESOURCE | BIND_DEPTH_STENCIL;
    // Define optimal clear value
    RTDepthDesc.ClearValue.Format = RTDepthDesc.Format;
    RTDepthDesc.ClearValue.DepthStencil.Depth = 1;
    RTDepthDesc.ClearValue.DepthStencil.Stencil = 0;
    RefCntAutoPtr<ITexture> pRTDepth;
    m_pDevice->CreateTexture(RTDepthDesc, nullptr, &pRTDepth);
    // Store the depth-stencil view
    m_pDepthDSV = pRTDepth->GetDefaultView(TEXTURE_VIEW_DEPTH_STENCIL);

    // We need to release and create a new SRB that references new off-screen render target SRV
    m_pRTSRB.Release();
    m_pRTPSO->CreateShaderResourceBinding(&m_pRTSRB, true);
    m_pRTSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture")->Set(pRTColor->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
    // Set render target color texture SRV in the SRB
    //m_pRTSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture")->Set(pRTColor->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));

    //////////////// Selection render target

    // Create window - size offscreen render target
    
    RTColorDesc.Name = "Offscreen render target";
    RTColorDesc.Type = RESOURCE_DIM_TEX_2D;
    RTColorDesc.Width = m_pSwapChain->GetDesc().Width;
    RTColorDesc.Height = m_pSwapChain->GetDesc().Height;
    RTColorDesc.MipLevels = 1;
    RTColorDesc.Format = HighlightRenderTargetFormat;
    // The render target can be bound as a shader resource and as a render target
    RTColorDesc.BindFlags = BIND_SHADER_RESOURCE | BIND_RENDER_TARGET;
    // Define optimal clear value
    RTColorDesc.ClearValue.Format = RTColorDesc.Format;
    RTColorDesc.ClearValue.Color[0] = 0;
    RefCntAutoPtr<ITexture> pSelectionRTColor;
    m_pDevice->CreateTexture(RTColorDesc, nullptr, &pSelectionRTColor);
    // Store the render target view
    m_pSelectionColorRTV = pSelectionRTColor->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);


    // Create window-size depth buffer
    
    RTDepthDesc.Name = "Offscreen depth buffer";
    RTDepthDesc.Format = DepthBufferFormat;
    RTDepthDesc.BindFlags = BIND_SHADER_RESOURCE | BIND_DEPTH_STENCIL;
    // Define optimal clear value
    RTDepthDesc.ClearValue.Format = RTDepthDesc.Format;
    RTDepthDesc.ClearValue.DepthStencil.Depth = 1;
    RTDepthDesc.ClearValue.DepthStencil.Stencil = 0;
    RefCntAutoPtr<ITexture> pSelectionRTDepth;
    m_pDevice->CreateTexture(RTDepthDesc, nullptr, &pSelectionRTDepth);
    // Store the depth-stencil view
    m_pSelectionDepthDSV = pSelectionRTDepth->GetDefaultView(TEXTURE_VIEW_DEPTH_STENCIL);

    // We need to release and create a new SRB that references new off-screen render target SRV
    m_pSelectionRTSRB.Release();
    m_pSelectionRTPSO->CreateShaderResourceBinding(&m_pSelectionRTSRB, true);
    m_pSelectionRTSRB->GetVariableByName(SHADER_TYPE_VERTEX, "cbTransforms")->Set(m_VSConstants);
    m_pSelectionRTSRB->GetVariableByName(SHADER_TYPE_PIXEL, "cbTransforms")->Set(m_VSConstants);

    

    m_pSelectionCompositSRB.Release();
    m_pSelectionCompositPSO->CreateShaderResourceBinding(&m_pSelectionCompositSRB, true);
    m_pSelectionCompositSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_SceneDepthTexture")->Set(pRTDepth->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
    m_pSelectionCompositSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_SelectionDepthTexture")->Set(pSelectionRTDepth->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
    m_pSelectionCompositSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_SelectionTexture")->Set(pSelectionRTColor->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));

}

} // namespace Sapphire


#include "SapphireApp.hpp"
#include "imgui.h"
#include "ColorConversion.h"
#include "GraphicsAccessories.hpp"
#include "GraphicsUtilities.h"
#include "TextureUtilities.h"
#include "MapHelper.hpp"

using namespace Diligent;

SampleBase* Diligent::CreateSample()
{
    return new Sapphire::SapphireApp();
}


namespace Sapphire
{
    

SapphireApp::~SapphireApp()
{
}

void SapphireApp::CreatePipelineState()
{
    // Pipeline state object encompasses configuration of all GPU stages

    GraphicsPipelineStateCreateInfo PSOCreateInfo;

    // Pipeline state name is used by the engine to report issues.
    // It is always a good idea to give objects descriptive names.
    PSOCreateInfo.PSODesc.Name = "Cube PSO";

    // This is a graphics pipeline
    PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

    // clang-format off
    // This tutorial will render to a single render target
    PSOCreateInfo.GraphicsPipeline.NumRenderTargets = 1;
    // Set render target format which is the format of the swap chain's color buffer
    PSOCreateInfo.GraphicsPipeline.RTVFormats[0] = m_pSwapChain->GetDesc().ColorBufferFormat;
    // Set depth buffer format which is the format of the swap chain's back buffer
    PSOCreateInfo.GraphicsPipeline.DSVFormat = m_pSwapChain->GetDesc().DepthBufferFormat;
    // Primitive topology defines what kind of primitives will be rendered by this pipeline state
    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    // Cull back faces
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_BACK;
    // Enable depth testing
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = True;
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
        CreateUniformBuffer(m_pDevice, sizeof(float4x4), "VS constants CB", &m_VSConstants);
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

    // clang-format off
    // Define vertex shader input layout
    LayoutElement LayoutElems[] =
    {
        // Attribute 0 - vertex position
        LayoutElement{0, 0, 3, VT_FLOAT32, False},
        // Attribute 1 - texture coordinates
        LayoutElement{1, 0, 2, VT_FLOAT32, False}
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
        {SHADER_TYPE_PIXEL, "g_Texture", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}
    };
    // clang-format on
    PSOCreateInfo.PSODesc.ResourceLayout.Variables = Vars;
    PSOCreateInfo.PSODesc.ResourceLayout.NumVariables = _countof(Vars);

    // clang-format off
    // Define immutable sampler for g_Texture. Immutable samplers should be used whenever possible
    SamplerDesc SamLinearClampDesc
    {
        FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR,
        TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP
    };
    ImmutableSamplerDesc ImtblSamplers[] =
    {
        {SHADER_TYPE_PIXEL, "g_Texture", SamLinearClampDesc}
    };
    // clang-format on
    PSOCreateInfo.PSODesc.ResourceLayout.ImmutableSamplers = ImtblSamplers;
    PSOCreateInfo.PSODesc.ResourceLayout.NumImmutableSamplers = _countof(ImtblSamplers);

    m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pPSO);

    // Since we did not explcitly specify the type for 'Constants' variable, default
    // type (SHADER_RESOURCE_VARIABLE_TYPE_STATIC) will be used. Static variables
    // never change and are bound directly through the pipeline state object.
    m_pPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_VSConstants);

    // Since we are using mutable variable, we must create a shader resource binding object
    // http://diligentgraphics.com/2016/03/23/resource-binding-model-in-diligent-engine-2-0/
    m_pPSO->CreateShaderResourceBinding(&m_SRB, true);
}

void SapphireApp::CreateVertexBuffer()
{
    // Layout of this structure matches the one we defined in the pipeline state
    struct Vertex
    {
        float3 pos;
        float2 uv;
    };

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
        {float3(-1,-1,-1), float2(0,1)},
        {float3(-1,+1,-1), float2(0,0)},
        {float3(+1,+1,-1), float2(1,0)},
        {float3(+1,-1,-1), float2(1,1)},

        {float3(-1,-1,-1), float2(0,1)},
        {float3(-1,-1,+1), float2(0,0)},
        {float3(+1,-1,+1), float2(1,0)},
        {float3(+1,-1,-1), float2(1,1)},

        {float3(+1,-1,-1), float2(0,1)},
        {float3(+1,-1,+1), float2(1,1)},
        {float3(+1,+1,+1), float2(1,0)},
        {float3(+1,+1,-1), float2(0,0)},

        {float3(+1,+1,-1), float2(0,1)},
        {float3(+1,+1,+1), float2(0,0)},
        {float3(-1,+1,+1), float2(1,0)},
        {float3(-1,+1,-1), float2(1,1)},

        {float3(-1,+1,-1), float2(1,0)},
        {float3(-1,+1,+1), float2(0,0)},
        {float3(-1,-1,+1), float2(0,1)},
        {float3(-1,-1,-1), float2(1,1)},

        {float3(-1,-1,+1), float2(1,1)},
        {float3(+1,-1,+1), float2(0,1)},
        {float3(+1,+1,+1), float2(0,0)},
        {float3(-1,+1,+1), float2(1,0)}
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
}

void SapphireApp::CreateIndexBuffer()
{
    // clang-format off
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
}

void SapphireApp::LoadTextures()
{
    TextureLoadInfo loadInfo;
    loadInfo.IsSRGB = true;
    RefCntAutoPtr<ITexture> Tex;
    CreateTextureFromFile("DGLogo.png", loadInfo, m_pDevice, &Tex);
    // Get shader resource view from the texture
    m_TextureSRV = Tex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);

    // Set texture SRV in the SRB
    m_SRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture")->Set(m_TextureSRV);
}

void SapphireApp::Initialize(const SampleInitInfo& InitInfo)
{
    SampleBase::Initialize(InitInfo);
    // Reset default colors
    ImGui::StyleColorsDark();

    CreatePipelineState();
    CreateVertexBuffer();
    CreateIndexBuffer();
    LoadTextures();
}

void SapphireApp::UpdateUI()
{
   
   

    // 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
    {
        ImGui::Begin("Hello, world!"); // Create a window called "Hello, world!" and append into it.

        ImGui::Text("This is some useful text.");          // Display some text (you can use a format strings too)
        ImGui::Checkbox("Demo Window", &m_ShowDemoWindow); // Edit bools storing our window open/close state
        ImGui::Checkbox("Another Window", &m_ShowAnotherWindow);

        static float f = 0.0f;
        ImGui::SliderFloat("float", &f, 0.0f, 1.0f);             // Edit 1 float using a slider from 0.0f to 1.0f
        ImGui::ColorEdit3("clear color", (float*)&m_ClearColor); // Edit 3 floats representing a color

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
    auto* pRTV = m_pSwapChain->GetCurrentBackBufferRTV();

    auto ClearColor = m_ClearColor;
    if (GetTextureFormatAttribs(m_pSwapChain->GetDesc().ColorBufferFormat).ComponentType == COMPONENT_TYPE_UNORM_SRGB)
    {
        ClearColor = SRGBToLinear(ClearColor);
    }
    m_pImmediateContext->ClearRenderTarget(pRTV, &ClearColor.x, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

   
    auto* pDSV = m_pSwapChain->GetDepthBufferDSV();
  
    m_pImmediateContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    {
        // Map the buffer and write current world-view-projection matrix
        MapHelper<float4x4> CBConstants(m_pImmediateContext, m_VSConstants, MAP_WRITE, MAP_FLAG_DISCARD);
        *CBConstants = m_WorldViewProjMatrix.Transpose();
    }

    // Bind vertex and index buffers
    const Uint64 offset = 0;
    IBuffer* pBuffs[] = { m_CubeVertexBuffer };
    m_pImmediateContext->SetVertexBuffers(0, 1, pBuffs, &offset, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    m_pImmediateContext->SetIndexBuffer(m_CubeIndexBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Set the pipeline state
    m_pImmediateContext->SetPipelineState(m_pPSO);
    // Commit shader resources. RESOURCE_STATE_TRANSITION_MODE_TRANSITION mode
    // makes sure that resources are transitioned to required states.
    m_pImmediateContext->CommitShaderResources(m_SRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    DrawIndexedAttribs DrawAttrs;     // This is an indexed draw call
    DrawAttrs.IndexType = VT_UINT32; // Index type
    DrawAttrs.NumIndices = 36;
    // Verify the state of vertex and index buffers
    DrawAttrs.Flags = DRAW_FLAG_VERIFY_ALL;
    m_pImmediateContext->DrawIndexed(DrawAttrs);
}

float4x4 SapphireApp::GetOrthographicMatrix(float orthographicSize)
{
    float4x4 projMat;

    const auto& SCDesc = m_pSwapChain->GetDesc();

    projMat = float4x4::Ortho((static_cast<float>(SCDesc.Width) / static_cast<float>(SCDesc.Height)) * orthographicSize, orthographicSize, 0.1f, 100.0f, m_pDevice->GetDeviceInfo().IsGLDevice());
    return projMat;
}

void SapphireApp::Update(double CurrTime, double ElapsedTime)
{
    SampleBase::Update(CurrTime, ElapsedTime);

    auto inputController = GetInputController();
    if (inputController.IsKeyFirstDown(InputKeys::MoveLeft))
    {
        LOG_INFO_MESSAGE("Key Down First!");
    }
    if (inputController.IsKeyDown(InputKeys::MoveLeft))
    {
        LOG_INFO_MESSAGE("Key Down!");
    }
    if (inputController.IsKeyReleased(InputKeys::MoveLeft))
    {
        LOG_INFO_MESSAGE("Key released!");
    }
    // Apply rotation
    float4x4 CubeModelTransform = float4x4::RotationY(static_cast<float>(CurrTime) * 1.0f) * float4x4::RotationX(-PI_F * 0.1f);

    // Camera is at (0, 0, -15) looking along the Z axis
    float4x4 View = float4x4::Translation(0.f, 0.0f, 15.0f);

    // Get pretransform matrix that rotates the scene according the surface orientation
    auto SrfPreTransform = GetSurfacePretransformMatrix(float3{ 0, 0, 1 });

    // Get projection matrix adjusted to the current screen orientation
    auto Proj = GetAdjustedProjectionMatrix(PI_F / 4.0f, 0.1f, 100.f);
    Proj = GetOrthographicMatrix(10.0f);

    // Compute world-view-projection matrix
    m_WorldViewProjMatrix = CubeModelTransform * View * SrfPreTransform * Proj;

    UpdateUI();
}

void SapphireApp::WindowResize(Uint32 Width, Uint32 Height)
{
}

} // namespace Sapphire

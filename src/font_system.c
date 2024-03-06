#include "core/sapphire_types.h"
#include "core/sapphire_math.h"
#include "core/json.h"
#include "core/config.h"
#include "core/allocator.h"
#include "core/temp_allocator.h"
#include "core/os.h"
#include "core/murmurhash64a.h"

#include <memory.h>
#include "RenderDevice.h"
#include "SwapChain.h"
#include "DeviceContext.h"
#include "Shader.h"

#include "GraphicsUtilities.h"
#include "TextureUtilities.h"


#define MAX_FONT_VERTICES 2048
#define FONT_CONTEXT_MAX_FONTS 16

typedef struct font_vertex_t
{
    sp_vec2_t pos;
    sp_vec2_t uv;

} font_vertex_t;

#define MAX_FONT_GLYPHS 256
#define MAX_KERNING_PAIRS 256
typedef struct font_glyph_t
{
    uint32_t code;
    float u0, u1, v0, v1; // texture coordinates
    float x_advance;
    float x_top, y_top;
    float width; //?
    float height; //?
} sp_font_glyph_t;

typedef struct sp_font_t
{
    sp_font_glyph_t glyphs[MAX_FONT_GLYPHS];
    uint32_t        kerning_pairs[MAX_KERNING_PAIRS];
    int16_t         kerning_values[MAX_KERNING_PAIRS];
    uint32_t        num_gliphs;
    uint32_t        num_kerning_pairs;
    float           ascender;
    float           descender;
    float           line_height;
} sp_font_t;

typedef struct font_rendering_context_t
{
    uint32_t state_font_index;
    uint32_t state_color;
    font_vertex_t vertices[MAX_FONT_VERTICES];
    uint32_t num_vertices;
    uint32_t num_fonts;
    sp_font_t fonts[FONT_CONTEXT_MAX_FONTS];

} font_rendering_context_t;

static IRenderDevice* g_pDevice = NULL;
static ISwapChain* g_pSwapChain = NULL;

static IBuffer* g_pPSConstants = NULL;
static IPipelineState* g_pPSO = NULL;
static IShaderResourceBinding* g_pSRB = NULL;
static IBuffer* g_pFontVertexBuffer = NULL;

static float g_renderTargetWidth;
static float g_renderTargetHeight;
static font_rendering_context_t g_frc;

const char* read_file(const char* file, sp_temp_allocator_i* ta)
{
    // Read text
    const sp_file_stat_t stat = sp_os_api->file_system->stat(file);
    const uint64_t size = stat.size;
    struct sp_os_file_io_api* io = sp_os_api->file_io;
    char* text = sp_temp_alloc(ta, size + 2);
    sp_file_o f = io->open_input(file);
    io->read(f, text, size);
    io->close(f);

    // Make sure file is LF terminated.
    text[size] = '\n';
    text[size + 1] = 0;

    // Strip CR from file (normalize line endings to LF).
    if (strchr(text, '\r')) {
        const char* from = text;
        char* to = text;
        while (*from) {
            *to++ = *from++;
            if (to[-1] == '\r')
                --to;
        }
        *to = 0;
    }
    return text;
}

static sp_vec2_t add_glyph_vertices(const sp_font_t* font, int32_t code, int32_t prev_code, float x, float y, float scale, float spacing, font_vertex_t* vertices, uint32_t* buffer_index)
{
    sp_vec2_t advance = {0};

    const sp_font_glyph_t* glyph = &font->glyphs[code];
    uint32_t curr_index = *buffer_index;

    float x0 = (x + glyph->x_top * scale);
    float y0 = (y + glyph->y_top * scale);
    float x1 = (x + (glyph->x_top + glyph->width) * scale);
    float y1 = (y + (glyph->y_top + glyph->height) * scale);

    

    // first triangle
    vertices[curr_index].pos.x = x0;
    vertices[curr_index].pos.y = y0;
    vertices[curr_index].uv.x = glyph->u0;
    vertices[curr_index].uv.y = glyph->v0;
    curr_index++;

    vertices[curr_index].pos.x = x1;
    vertices[curr_index].pos.y = y1;
    vertices[curr_index].uv.x = glyph->u1;
    vertices[curr_index].uv.y = glyph->v1;
    curr_index++;

    vertices[curr_index].pos.x = x1;
    vertices[curr_index].pos.y = y0;
    vertices[curr_index].uv.x = glyph->u1;
    vertices[curr_index].uv.y = glyph->v0;
    curr_index++;

    // second triangle

    vertices[curr_index].pos.x = x0;
    vertices[curr_index].pos.y = y0;
    vertices[curr_index].uv.x = glyph->u0;
    vertices[curr_index].uv.y = glyph->v0;
    curr_index++;

    vertices[curr_index].pos.x = x0;
    vertices[curr_index].pos.y = y1;
    vertices[curr_index].uv.x = glyph->u0;
    vertices[curr_index].uv.y = glyph->v1;
    curr_index++;

    vertices[curr_index].pos.x = x1;
    vertices[curr_index].pos.y = y1;
    vertices[curr_index].uv.x = glyph->u1;
    vertices[curr_index].uv.y = glyph->v1;
    curr_index++;
    
    // update the buffer index to the new position
    *buffer_index = curr_index;

    // TODO: add pair kerning if exists
    advance.x += glyph->x_advance * scale + spacing;

    return advance;

}

static void reset_font_rendering_context(font_rendering_context_t* frc)
{
    frc->state_font_index = 0;    
    frc->num_vertices = 0;
    frc->state_color = 0xFF0000FF;
}

static void font_render_string(font_rendering_context_t* frc, float x, float y, const char* str)
{
    size_t len = strlen(str);
    const sp_font_t* font = &frc->fonts[frc->state_font_index];
    int32_t char_code;
    int32_t prev_code = -1;
    float cur_x = x;
    float cur_y = y;
    uint32_t ver_index = frc->num_vertices;
    for (uint32_t i = 0; i < len; ++i)
    {
        char_code = str[i];
        sp_vec2_t advance = add_glyph_vertices(font, char_code, prev_code, cur_x, cur_y, 1.0f, 0.0f, frc->vertices, &ver_index);
        cur_x += advance.x;
        prev_code = char_code;
    }
    frc->num_vertices = ver_index;
}

static void fill_font_buffer(IDeviceContext* pContext)
{
    reset_font_rendering_context(&g_frc);
    
    font_render_string(&g_frc, 0, 0, "ofer rundstein");    
    font_render_string(&g_frc, 0, g_frc.fonts[0].line_height, "OFER RUNDSTEIN");

    {
        // Map the buffer and write current world-view-projection matrix
        void* pCBData = NULL;
        IDeviceContext_MapBuffer(pContext, g_pFontVertexBuffer, MAP_WRITE, MAP_FLAG_DISCARD, &pCBData);
        memcpy(pCBData, g_frc.vertices, sizeof(font_vertex_t) * g_frc.num_vertices);
        
        IDeviceContext_UnmapBuffer(pContext, g_pFontVertexBuffer, MAP_WRITE);
    }
}

inline double get_attribute_as_number(sp_config_i* config, sp_config_item_t item, sp_strhash_t hash)
{
    sp_config_item_t attrib = config->object_get(config->inst, item, hash);
    double num = config->to_number(config->inst, attrib);
    return num;
}

static void load_font_file(const char* font_file)
{
    //SP_INIT_TEMP_ALLOCATOR(ta);
    SP_INIT_TEMP_ALLOCATOR_WITH_ADAPTER(ta, a);

    const char* text = read_file(font_file, ta);

    char error[256];
    const uint32_t parse_flags = SP_JSON_PARSE_EXT_ALLOW_UNQUOTED_KEYS | SP_JSON_PARSE_EXT_ALLOW_COMMENTS | SP_JSON_PARSE_EXT_IMPLICIT_ROOT_OBJECT | SP_JSON_PARSE_EXT_OPTIONAL_COMMAS | SP_JSON_PARSE_EXT_EQUALS_FOR_COLON;
    sp_config_i* fnt_config = sp_config_api->create(a);
    bool res = sp_json_api->parse(text, fnt_config, parse_flags, error);
    if (!res)
    {
        return;
    }

    sp_font_t font_o;

    sp_config_item_t root = fnt_config->root(fnt_config->inst);
    sp_strhash_t s_glyphs_hash = sp_murmur_hash_string("glyphs");
    sp_strhash_t s_scale_w_hash = sp_murmur_hash_string("scale_w");
    sp_strhash_t s_scale_h_hash = sp_murmur_hash_string("scale_h");
    sp_strhash_t s_line_height_hash = sp_murmur_hash_string("line_height");
    sp_strhash_t s_code_hash = sp_murmur_hash_string("id_code");
    sp_strhash_t s_x_hash = sp_murmur_hash_string("x");
    sp_strhash_t s_y_hash = sp_murmur_hash_string("y");
    sp_strhash_t s_width_hash = sp_murmur_hash_string("width");
    sp_strhash_t s_height_hash = sp_murmur_hash_string("height");
    sp_strhash_t s_xoffset_hash = sp_murmur_hash_string("xoffset");
    sp_strhash_t s_yoffset_hash = sp_murmur_hash_string("yoffset");
    sp_strhash_t s_xadvance_hash = sp_murmur_hash_string("xadvance");

    float scale_w = (float)get_attribute_as_number(fnt_config, root, s_scale_w_hash);
    float scale_h = (float)get_attribute_as_number(fnt_config, root, s_scale_h_hash);

    float line_height = (float)get_attribute_as_number(fnt_config, root, s_line_height_hash);

    font_o.line_height = line_height;

    sp_config_item_t glyphs = fnt_config->object_get(fnt_config->inst, root, s_glyphs_hash);
    sp_config_item_t* glyphs_array = NULL;
    uint32_t num_glyphs = fnt_config->to_array(fnt_config->inst, glyphs, &glyphs_array);
    for (uint32_t i = 0; i < num_glyphs; ++i)
    {
        sp_config_item_t glyph_item = glyphs_array[i];
        uint32_t code_id = (uint32_t)get_attribute_as_number(fnt_config, glyph_item, s_code_hash);
        float x = (float)get_attribute_as_number(fnt_config, glyph_item, s_x_hash);
        float y = (float)get_attribute_as_number(fnt_config, glyph_item, s_y_hash);
        float width = (float)get_attribute_as_number(fnt_config, glyph_item, s_width_hash);
        float height = (float)get_attribute_as_number(fnt_config, glyph_item, s_height_hash);
        float xoffset = (float)get_attribute_as_number(fnt_config, glyph_item, s_xoffset_hash);
        float yoffset = (float)get_attribute_as_number(fnt_config, glyph_item, s_yoffset_hash);
        float xadvance = (float)get_attribute_as_number(fnt_config, glyph_item, s_xadvance_hash);
        sp_font_glyph_t* font_glyph = &font_o.glyphs[code_id];
        font_glyph->code = code_id;
        font_glyph->width = width;
        font_glyph->height = height;
        font_glyph->x_top = xoffset;
        font_glyph->y_top = yoffset;
        font_glyph->x_advance = xadvance;
        font_glyph->u0 = x / scale_w;
        font_glyph->v0 = y / scale_h;
        font_glyph->u1 = (x + width) / scale_w;
        font_glyph->v1 = (y + height) / scale_h;
        
        
    }
    
    g_frc.fonts[0] = font_o;

    SP_SHUTDOWN_TEMP_ALLOCATOR(ta);
}

static void CreatePipelineState(IRenderDevice* pDevice, ISwapChain* pSwapChain)
{
    // Pipeline state object encompasses configuration of all GPU stages

    GraphicsPipelineStateCreateInfo PSOCreateInfo;

    memset(&PSOCreateInfo, 0, sizeof(PSOCreateInfo));

    PipelineStateDesc* pPSODesc = &PSOCreateInfo._PipelineStateCreateInfo.PSODesc;

    // Pipeline state name is used by the engine to report issues.
    // It is always a good idea to give objects descriptive names.
    pPSODesc->_DeviceObjectAttribs.Name = "Text Rendering PSO";

    // This is a graphics pipeline
    pPSODesc->PipelineType = PIPELINE_TYPE_GRAPHICS;

    // clang-format off

    // This tutorial will render to a single render target
    PSOCreateInfo.GraphicsPipeline.NumRenderTargets = 1;
    // Set render target format which is the format of the swap chain's color buffer
    const SwapChainDesc* pSCDesc = ISwapChain_GetDesc(pSwapChain);

    g_renderTargetWidth = (float)pSCDesc->Width;
    g_renderTargetHeight = (float)pSCDesc->Height;


    PSOCreateInfo.GraphicsPipeline.RTVFormats[0] = pSCDesc->ColorBufferFormat;
    // Set depth buffer format which is the format of the swap chain's back buffer
    PSOCreateInfo.GraphicsPipeline.DSVFormat = pSCDesc->DepthBufferFormat;
    // Primitive topology defines what kind of primitives will be rendered by this pipeline state
    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    // clang-format on

    //PSOCreateInfo.GraphicsPipeline.BlendDesc.RenderTargets[0].RenderTargetWriteMask = COLOR_MASK_ALL;

    pPSODesc->ImmediateContextMask = 1;
    PSOCreateInfo.GraphicsPipeline.SmplDesc.Count = 1;
    PSOCreateInfo.GraphicsPipeline.SampleMask = 0xFFFFFFFF;
    PSOCreateInfo.GraphicsPipeline.NumViewports = 1;

    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = False;
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = False;
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthFunc = COMPARISON_FUNC_LESS;

    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.FillMode = FILL_MODE_SOLID;
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.DepthClipEnable = True;

    BlendStateDesc bd;
    bd.AlphaToCoverageEnable = False;
    bd.IndependentBlendEnable = False;
    bd.RenderTargets[0].BlendEnable = True;
    bd.RenderTargets[0].LogicOperationEnable = False;
    bd.RenderTargets[0].BlendOpAlpha = BLEND_OPERATION_ADD;
    bd.RenderTargets[0].DestBlend = BLEND_FACTOR_INV_SRC_ALPHA;
    bd.RenderTargets[0].DestBlendAlpha = BLEND_FACTOR_INV_SRC_ALPHA;
    bd.RenderTargets[0].RenderTargetWriteMask = COLOR_MASK_ALL;
    bd.RenderTargets[0].SrcBlend = BLEND_FACTOR_SRC_ALPHA;
    bd.RenderTargets[0].BlendOp = BLEND_OPERATION_ADD;
    bd.RenderTargets[0].SrcBlendAlpha = BLEND_FACTOR_SRC_ALPHA;


    PSOCreateInfo.GraphicsPipeline.BlendDesc = bd;
   

    ShaderCreateInfo ShaderCI;
    memset(&ShaderCI, 0, sizeof(ShaderCI));

    // Tell the system that the shader source code is in HLSL.
    // For OpenGL, the engine will convert this into GLSL under the hood.
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;

    // OpenGL backend requires emulated combined HLSL texture samplers (g_Texture + g_Texture_sampler combination)
    ShaderCI.Desc.UseCombinedTextureSamplers = true;
    ShaderCI.Desc.CombinedSamplerSuffix = "_sampler";

    // Create a shader source stream factory to load shaders from files.
    IEngineFactory* pEngineFactory = IRenderDevice_GetEngineFactory(pDevice);

    IShaderSourceInputStreamFactory* pShaderSourceFactory = NULL;
    IEngineFactory_CreateDefaultShaderSourceStreamFactory(pEngineFactory, NULL, &pShaderSourceFactory);

    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;
    // Create a vertex shader
    IShader* pVS = NULL;
    {
        ShaderCI.Desc._DeviceObjectAttribs.Name = "Font VS";

        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint = "main";
        ShaderCI.FilePath = "font.vsh";
        IRenderDevice_CreateShader(pDevice, &ShaderCI, &pVS, NULL);

        
    }


    // Create a pixel shader
    IShader* pPS = NULL;
    {
        ShaderCI.Desc._DeviceObjectAttribs.Name = "Font PS";

        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint = "main";
        ShaderCI.FilePath = "font.psh";
        IRenderDevice_CreateShader(pDevice, &ShaderCI, &pPS, NULL);

        // Create dynamic uniform buffer that will store our transformation matrix
        // Dynamic buffers can be frequently updated by the CPU
        Diligent_CreateUniformBuffer(pDevice, sizeof(sp_vec4_t) * 2, "PS constants CB", &g_pPSConstants,
            USAGE_DYNAMIC, BIND_UNIFORM_BUFFER, CPU_ACCESS_WRITE, NULL);
    }

    // Define vertex shader input layout
    LayoutElement LayoutElems[2];
    LayoutElems[0].HLSLSemantic = "ATTRIB";
    LayoutElems[0].InputIndex = 0;
    LayoutElems[0].BufferSlot = 0;
    LayoutElems[0].NumComponents = 2;
    LayoutElems[0].ValueType = VT_FLOAT32;
    LayoutElems[0].IsNormalized = False;
    LayoutElems[0].RelativeOffset = LAYOUT_ELEMENT_AUTO_OFFSET;
    LayoutElems[0].Stride = LAYOUT_ELEMENT_AUTO_STRIDE;
    LayoutElems[0].Frequency = INPUT_ELEMENT_FREQUENCY_PER_VERTEX;
    LayoutElems[0].InstanceDataStepRate = 1;

    LayoutElems[1].HLSLSemantic = "ATTRIB";
    LayoutElems[1].InputIndex = 1;
    LayoutElems[1].BufferSlot = 0;
    LayoutElems[1].NumComponents = 2;
    LayoutElems[1].ValueType = VT_FLOAT32;
    LayoutElems[1].IsNormalized = False;
    LayoutElems[1].RelativeOffset = LAYOUT_ELEMENT_AUTO_OFFSET;
    LayoutElems[1].Stride = LAYOUT_ELEMENT_AUTO_STRIDE;
    LayoutElems[1].Frequency = INPUT_ELEMENT_FREQUENCY_PER_VERTEX;
    LayoutElems[1].InstanceDataStepRate = 1;

    PSOCreateInfo.pVS = pVS;
    PSOCreateInfo.pPS = pPS;

    PSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems;
    PSOCreateInfo.GraphicsPipeline.InputLayout.NumElements = 2;

    // Define variable type that will be used by default
    pPSODesc->ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

    // Shader variables should typically be mutable, which means they are expected
    // to change on a per-instance basis
    ShaderResourceVariableDesc Vars[1];
    Vars[0].ShaderStages = SHADER_TYPE_PIXEL;
    Vars[0].Name = "g_Texture";
    Vars[0].Type = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;
    Vars[0].Flags = SHADER_VARIABLE_FLAG_NONE;

    pPSODesc->ResourceLayout.Variables = Vars;
    pPSODesc->ResourceLayout.NumVariables = 1;

    // Define immutable sampler for g_Texture. Immutable samplers should be used whenever possible
    ImmutableSamplerDesc ImtblSamplers[1];

    memset(&ImtblSamplers, 0, sizeof(ImtblSamplers));
    ImtblSamplers[0].ShaderStages = SHADER_TYPE_PIXEL;
    ImtblSamplers[0].SamplerOrTextureName = "g_Texture";

    ImtblSamplers[0].Desc._DeviceObjectAttribs.Name = "Linear sampler";

    ImtblSamplers[0].Desc.MinFilter = FILTER_TYPE_LINEAR;
    ImtblSamplers[0].Desc.MagFilter = FILTER_TYPE_LINEAR;
    ImtblSamplers[0].Desc.MipFilter = FILTER_TYPE_LINEAR;
    ImtblSamplers[0].Desc.AddressU = TEXTURE_ADDRESS_CLAMP;
    ImtblSamplers[0].Desc.AddressV = TEXTURE_ADDRESS_CLAMP;
    ImtblSamplers[0].Desc.AddressW = TEXTURE_ADDRESS_CLAMP;
    ImtblSamplers[0].Desc.ComparisonFunc = COMPARISON_FUNC_NEVER,
    ImtblSamplers[0].Desc.MaxLOD = +3.402823466e+38F;

    pPSODesc->ResourceLayout.ImmutableSamplers = ImtblSamplers;
    pPSODesc->ResourceLayout.NumImmutableSamplers = 1;

    IRenderDevice_CreateGraphicsPipelineState(pDevice, &PSOCreateInfo, &g_pPSO);

    // Since we did not explcitly specify the type for 'Constants' variable, default
    // type (SHADER_RESOURCE_VARIABLE_TYPE_STATIC) will be used. Static variables
    // never change and are bound directly through the pipeline state object.
    IShaderResourceVariable* pVar = IPipelineState_GetStaticVariableByName(g_pPSO, SHADER_TYPE_VERTEX, "Constants");
    if (pVar)
    {
        IShaderResourceVariable_Set(pVar, (IDeviceObject*)g_pPSConstants, SET_SHADER_RESOURCE_FLAG_NONE);
    }
    

    // Since we are using mutable variable, we must create a shader resource binding object
    // http://diligentgraphics.com/2016/03/23/resource-binding-model-in-diligent-engine-2-0/
    IPipelineState_CreateShaderResourceBinding(g_pPSO, &g_pSRB, true);

    IObject_Release(pPS);
    IObject_Release(pVS);
    IObject_Release(pShaderSourceFactory);
}

static void CreateVertexBuffer(IRenderDevice* pDevice)
{
   

    BufferDesc VertBuffDesc;
    memset(&VertBuffDesc, 0, sizeof(VertBuffDesc));
    VertBuffDesc._DeviceObjectAttribs.Name = "Font vertex buffer";

    VertBuffDesc.Usage = USAGE_DYNAMIC;
    VertBuffDesc.BindFlags = BIND_VERTEX_BUFFER;
    VertBuffDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
    VertBuffDesc.Size = sizeof(float) * (4 * 6 * 2048);
    VertBuffDesc.ImmediateContextMask = 1;

    IRenderDevice_CreateBuffer(g_pDevice, &VertBuffDesc, NULL, &g_pFontVertexBuffer);

   
}




static void LoadTexture(IRenderDevice* pDevice)
{
    TextureLoadInfo loadInfo;
    memset(&loadInfo, 0, sizeof(loadInfo));
    loadInfo.IsSRGB = true;
    loadInfo.Usage = USAGE_IMMUTABLE;
    loadInfo.BindFlags = BIND_SHADER_RESOURCE;
    loadInfo.GenerateMips = True;

    ITexture* pTex = NULL;
    Diligent_CreateTextureFromFile("test_0.png", &loadInfo, pDevice, &pTex);
    // Get shader resource view from the texture
    ITextureView* pTextureSRV = ITexture_GetDefaultView(pTex, TEXTURE_VIEW_SHADER_RESOURCE);

    // Set texture SRV in the SRB
    IShaderResourceVariable* pVar = IShaderResourceBinding_GetVariableByName(g_pSRB, SHADER_TYPE_PIXEL, "g_Texture");
    if (pVar)
    {
        IShaderResourceVariable_Set(pVar, (IDeviceObject*)pTextureSRV, SET_SHADER_RESOURCE_FLAG_NONE);
    }
    
    IObject_Release((IObject*)pTex);
}



void CreateResources(IRenderDevice* pDevice, ISwapChain* pSwapChain)
{
    g_pDevice = pDevice;
    IObject_AddRef(g_pDevice);

    g_pSwapChain = pSwapChain;
    IObject_AddRef(g_pSwapChain);

    

    CreatePipelineState(pDevice, pSwapChain);
    CreateVertexBuffer(pDevice);
    
    LoadTexture(pDevice);

    load_font_file("C:/Users/oferr/Documents/temp/bitmapfont/first.fnt");

    
}

void ReleaseResources()
{
    
    if (g_pFontVertexBuffer)
    {
        IObject_Release(g_pFontVertexBuffer);
        g_pFontVertexBuffer = NULL;
    }

    if (g_pSRB)
    {
        IObject_Release(g_pSRB);
        g_pSRB = NULL;
    }

    if (g_pPSO)
    {
        IObject_Release(g_pPSO);
        g_pPSO = NULL;
    }

    if (g_pPSConstants)
    {
        IObject_Release(g_pPSConstants);
        g_pPSConstants = NULL;
    }

    if (g_pSwapChain)
    {
        IObject_Release(g_pSwapChain);
        g_pSwapChain = NULL;
    }

    if (g_pDevice)
    {
        IObject_Release(g_pDevice);
        g_pDevice = NULL;
    }
}

// Render a frame
void render(IDeviceContext* pContext)
{
    ITextureView* pRTV = ISwapChain_GetCurrentBackBufferRTV(g_pSwapChain);
    ITextureView* pDSV = ISwapChain_GetDepthBufferDSV(g_pSwapChain);
    IDeviceContext_SetRenderTargets(pContext, 1, &pRTV, pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    // Clear the back buffer
    const float ClearColor[] = { 0.850f, 0.350f, 0.350f, 1.0f };
    IDeviceContext_ClearRenderTarget(pContext, pRTV, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    IDeviceContext_ClearDepthStencil(pContext, pDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    fill_font_buffer(pContext);
    {
        // Map the buffer and write current world-view-projection matrix
        void* pCBData = NULL;
        IDeviceContext_MapBuffer(pContext, g_pPSConstants, MAP_WRITE, MAP_FLAG_DISCARD, &pCBData);
        float* p_floats = (float* )pCBData;
        sp_vec4_t color;
        color.w = (g_frc.state_color & 0xFF) / 255.0f;
        color.z = ((g_frc.state_color >> 8) & 0xFF) / 255.0f;
        color.y = ((g_frc.state_color >> 16) & 0xFF) / 255.0f;
        color.x = ((g_frc.state_color >> 24) & 0xFF) / 255.0f;
        memcpy(pCBData, &color, sizeof(color));
        p_floats[4] = g_renderTargetWidth;
        p_floats[5] = g_renderTargetHeight;
        IDeviceContext_UnmapBuffer(pContext, g_pPSConstants, MAP_WRITE);
    }

    // Bind vertex and index buffers
    const Uint64 offset = 0;
    IBuffer* pBuffs[1];
    pBuffs[0] = g_pFontVertexBuffer;
    IDeviceContext_SetVertexBuffers(pContext, 0, 1, pBuffs, &offset, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    //IDeviceContext_SetIndexBuffer(pContext, g_pCubeIndexBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Set the pipeline state
    IDeviceContext_SetPipelineState(pContext, g_pPSO);
    // Commit shader resources. RESOURCE_STATE_TRANSITION_MODE_TRANSITION mode
    // makes sure that resources are transitioned to required states.
    IDeviceContext_CommitShaderResources(pContext, g_pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    DrawAttribs draw_attrs;
    memset(&draw_attrs, 0, sizeof(draw_attrs));
    draw_attrs.NumVertices = g_frc.num_vertices;
    draw_attrs.NumInstances = 1;
    // Verify the state of vertex and index buffers
    draw_attrs.Flags = DRAW_FLAG_VERIFY_ALL;

 
    IDeviceContext_Draw(pContext, &draw_attrs);
}

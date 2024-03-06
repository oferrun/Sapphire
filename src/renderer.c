#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "imgui/cimgui.h"
#include "imgui/cimguizmo.h"

#include <memory.h>
#include "RenderDevice.h"
#include "SwapChain.h"
#include "DeviceContext.h"
#include "Shader.h"

#include "GraphicsUtilities.h"
#include "TextureUtilities.h"



#include "core/sapphire_types.h"
#include "core/sapphire_math.h"
#include "core/json.h"
#include "core/config.h"
#include "core/allocator.h"
#include "core/temp_allocator.h"
#include "core/os.h"
#include "core/murmurhash64a.h"
#include "core/array.h"
#include "core/hash.h"
#include "core/camera.h"
#include "core/sprintf.h"
#include "sapphire_renderer.h"

#include "renderer.h"
#include "scene.h"






///////////////
// Uniforms


typedef struct cb_light_attribs_t
{
    sp_vec4_t f4Direction;
    sp_vec4_t f4AmbientLight;
    sp_vec4_t f4Intensity;    
} cb_light_attribs_t;

typedef struct picking_buffer_t
{
    uint64_t identity;
    float depth;
    float pad;

} picking_buffer_t;

typedef struct cb_drawcall_t
{
    sp_mat4x4_t transform;
    uint64_t identity;
    uint64_t pad;


} cb_drawcall_t;

typedef struct viewer_t
{
    //tm_vec3_t damped_translation;
    //float translation_speed;
    sp_transform_t camera_transform;
    sp_camera_t camera;
    sp_mat4x4_t view_projection;
    //sp_mat4x4_t last_view_projection;
    
} viewer_t;

typedef struct cb_camera_attribs_t
{
    sp_vec4_t word_pos;     // Camera world position
    sp_vec4_t view_port_size; // (width, height, 1/width, 1/height)

    sp_vec2_t view_port_origin; // (min x, min y)
    float near_plane_z;
    float far_plane_z; // fNearPlaneZ < fFarPlaneZ

    sp_mat4x4_t view_mat_trans;
    sp_mat4x4_t proj_mat_trans;
    sp_mat4x4_t view_proj_mat_trans;
    sp_mat4x4_t view_mat_inv_trans;
    sp_mat4x4_t proj_mat_inv_trans;
    sp_mat4x4_t view_proj_mat_inv_trans;


    sp_vec4_t extra_data[5]; // Any appliation-specific data
    // Sizeof(CameraAttribs) == 256*2
} cb_camera_attribs_t;


bool read_file_stream(const char* file, sp_temp_allocator_i* ta, uint8_t** p_stream, uint64_t* out_size);
bool read_grimrock_model_from_stream(const uint8_t* p_stream, uint64_t size, sapphire_mesh_gpu_load_t* p_mesh_load);

void init_materials_manager(sapphire_materials_manager_t* materials_manager, sp_allocator_i* allocator);
void init_textures_manager(sapphire_textures_manager_t* textures_manager, sp_allocator_i* allocator);
void init_buffers_manager(sapphire_buffers_manager_t* buffers_manager);
void init_renderer(sapphire_renderer_t* p_renderer);


void destroy_buffers_manager(sapphire_buffers_manager_t* buffers_manager);
void destroy_textures_manager(sapphire_textures_manager_t* textures_manager);
void destroy_materials_manager(sapphire_materials_manager_t* materials_manager);

sp_mat_handle_t material_manager_lookup_material(sapphire_materials_manager_t* manager, sp_strhash_t mat_name_hash);

sp_mat_handle_t material_manager_lookup_material(sapphire_materials_manager_t* manager, sp_strhash_t mat_name_hash);
static IPipelineState* create_rt_pipeline_state(IRenderDevice* pDevice, ISwapChain* pSwapChain);
static void init_picking_buffers(IRenderDevice* pDevice, rendering_context_t* rendering_context_o);
static void init_uniform_buffers(IRenderDevice* pDevice, rendering_context_t* rendering_context_o);

///

rendering_context_t* g_rendering_context_o = NULL;


rendering_context_t* rendering_context_create(IRenderDevice* p_device, ISwapChain* p_swap_chain)
{
    sp_allocator_i* allocator = sp_allocator_api->system_allocator;
    g_rendering_context_o = sp_alloc(allocator, sizeof(rendering_context_t));
    memset(g_rendering_context_o, 0, sizeof(rendering_context_t));

    g_rendering_context_o->p_device = p_device;
    IObject_AddRef(g_rendering_context_o->p_device);
    g_rendering_context_o->p_swap_chain = p_swap_chain;
    IObject_AddRef(g_rendering_context_o->p_swap_chain);

    g_rendering_context_o->p_rt_pso = create_rt_pipeline_state(p_device, p_swap_chain);

    init_materials_manager(&g_rendering_context_o->materials_manager, allocator);
    init_textures_manager(&g_rendering_context_o->textures_manager, allocator);
    init_picking_buffers(p_device, g_rendering_context_o);
    init_uniform_buffers(p_device, g_rendering_context_o);
    init_buffers_manager(&g_rendering_context_o->buffers_manager);
    init_renderer(&g_rendering_context_o->renderer);

    return g_rendering_context_o;
}

void rendering_context_destroy(rendering_context_t* p_rendering_context)
{
    destroy_textures_manager(&g_rendering_context_o->textures_manager);
    destroy_materials_manager(&g_rendering_context_o->materials_manager);
    destroy_buffers_manager(&g_rendering_context_o->buffers_manager);
    if (g_rendering_context_o->picking_buffer)
    {
        IObject_Release(g_rendering_context_o->picking_buffer);
        g_rendering_context_o->picking_buffer = NULL;
    }

    if (g_rendering_context_o->picking_staging_buffer)
    {
        IObject_Release(g_rendering_context_o->picking_staging_buffer);
        g_rendering_context_o->picking_staging_buffer = NULL;
    }

    if (g_rendering_context_o->cb_camera_attribs)
    {
        IObject_Release(g_rendering_context_o->cb_camera_attribs);
        g_rendering_context_o->cb_camera_attribs = NULL;
    }

    if (g_rendering_context_o->cb_lights_attribs)
    {
        IObject_Release(g_rendering_context_o->cb_lights_attribs);
        g_rendering_context_o->cb_lights_attribs = NULL;
    }

    if (g_rendering_context_o->cb_drawcall)
    {
        IObject_Release(g_rendering_context_o->cb_drawcall);
        g_rendering_context_o->cb_drawcall = NULL;
    }

    if (g_rendering_context_o->p_rt_pso)
    {
        IObject_Release(g_rendering_context_o->p_rt_pso);
    }

    if (g_rendering_context_o->p_color_rtv)
    {
        IObject_Release(g_rendering_context_o->p_color_rtv);
    }

    if (g_rendering_context_o->p_depth_rtv)
    {
        IObject_Release(g_rendering_context_o->p_depth_rtv);
    }

    if (g_rendering_context_o->p_rt_srb)
    {
        IObject_Release(g_rendering_context_o->p_rt_srb);
    }

    if (g_rendering_context_o->p_swap_chain)
    {
        IObject_Release(g_rendering_context_o->p_swap_chain);
        g_rendering_context_o->p_swap_chain = NULL;
    }

    if (g_rendering_context_o->p_device)
    {
        IObject_Release(g_rendering_context_o->p_device);
        g_rendering_context_o->p_device = NULL;
    }

    sp_free(sp_allocator_api->system_allocator, g_rendering_context_o, sizeof(rendering_context_t));
}
/////


static void init_picking_buffers(IRenderDevice* pDevice, rendering_context_t* rendering_context_o)
{
    BufferDesc picking_buffer_desc;
    memset(&picking_buffer_desc, 0, sizeof(picking_buffer_desc));
    picking_buffer_desc._DeviceObjectAttribs.Name = "picking buffer";

    picking_buffer_desc.Usage = USAGE_DEFAULT;
    picking_buffer_desc.BindFlags = BIND_UNORDERED_ACCESS;
    picking_buffer_desc.Mode = BUFFER_MODE_RAW;

    picking_buffer_desc.Size = sizeof(picking_buffer_t);
    picking_buffer_desc.ImmediateContextMask = 1;



    IBuffer* p_buffer = NULL;
    IRenderDevice_CreateBuffer(pDevice, &picking_buffer_desc, NULL, &p_buffer);
    rendering_context_o->picking_buffer = p_buffer;

    // create staging buffer - used to read the picking gpu buffer

    memset(&picking_buffer_desc, 0, sizeof(picking_buffer_desc));
    picking_buffer_desc._DeviceObjectAttribs.Name = "picking buffer stagin";

    picking_buffer_desc.Usage = USAGE_STAGING;
    picking_buffer_desc.BindFlags = BIND_NONE;
    picking_buffer_desc.Mode = BUFFER_MODE_UNDEFINED;
    picking_buffer_desc.CPUAccessFlags = CPU_ACCESS_READ;
    picking_buffer_desc.Size = sizeof(picking_buffer_t);
    picking_buffer_desc.ImmediateContextMask = 1;

    IBuffer* p_staging_buffer = NULL;
    IRenderDevice_CreateBuffer(pDevice, &picking_buffer_desc, NULL, &p_staging_buffer);
    rendering_context_o->picking_staging_buffer = p_staging_buffer;
}

static void init_uniform_buffers(IRenderDevice* pDevice, rendering_context_t* rendering_context_o)
{  
    rendering_context_o->cb_camera_attribs = NULL;
    Diligent_CreateUniformBuffer(pDevice, sizeof(cb_camera_attribs_t), "camera CB", &rendering_context_o->cb_camera_attribs,
        USAGE_DYNAMIC, BIND_UNIFORM_BUFFER, CPU_ACCESS_WRITE, NULL);
    rendering_context_o->cb_lights_attribs = NULL;
    Diligent_CreateUniformBuffer(pDevice, sizeof(cb_light_attribs_t), "lights CB", &rendering_context_o->cb_lights_attribs,
        USAGE_DYNAMIC, BIND_UNIFORM_BUFFER, CPU_ACCESS_WRITE, NULL);
    rendering_context_o->cb_drawcall = NULL;
    Diligent_CreateUniformBuffer(pDevice, sizeof(cb_drawcall_t), "drawcall CB", &rendering_context_o->cb_drawcall,
        USAGE_DYNAMIC, BIND_UNIFORM_BUFFER, CPU_ACCESS_WRITE, NULL);
}

IBuffer* create_mesh_vertex_buffer(IRenderDevice* pDevice, const uint8_t* vertices, uint32_t size)
{
    BufferDesc vert_buffer_desc;
    memset(&vert_buffer_desc, 0, sizeof(vert_buffer_desc));
    vert_buffer_desc._DeviceObjectAttribs.Name = "mesh vertex buffer";

    vert_buffer_desc.Usage = USAGE_IMMUTABLE;
    vert_buffer_desc.BindFlags = BIND_VERTEX_BUFFER;
    vert_buffer_desc.Size = size;
    vert_buffer_desc.ImmediateContextMask = 1;

    BufferData vb_data;
    vb_data.pContext = NULL;
    vb_data.pData = vertices;
    vb_data.DataSize = size;

    IBuffer* p_buffer = NULL;
    IRenderDevice_CreateBuffer(pDevice, &vert_buffer_desc, &vb_data, &p_buffer);
    return p_buffer;
}

IBuffer* create_mesh_index_buffer(IRenderDevice* pDevice, const uint8_t* indices, uint32_t size)
{
    BufferDesc ind_buff_desc;
    memset(&ind_buff_desc, 0, sizeof(ind_buff_desc));
    ind_buff_desc._DeviceObjectAttribs.Name = "mesh Index buffer";

    ind_buff_desc.Usage = USAGE_IMMUTABLE;
    ind_buff_desc.BindFlags = BIND_INDEX_BUFFER;
    ind_buff_desc.Size = size;
    ind_buff_desc.ImmediateContextMask = 1;

    BufferData ib_data;
    ib_data.pContext = NULL;
    ib_data.pData = indices;
    ib_data.DataSize = size;

    IBuffer* p_buffer = NULL;
    IRenderDevice_CreateBuffer(pDevice, &ind_buff_desc, &ib_data, &p_buffer);
    return p_buffer;

}

sp_ib_handle_t buffers_manager_allocate_vb(IRenderDevice* pDevice, const uint8_t* vertices, uint32_t size)
{
    sapphire_buffers_manager_t* buffers_manager = &g_rendering_context_o->buffers_manager;
    IBuffer* vb = create_mesh_vertex_buffer(pDevice, vertices, size);
    sp_ib_handle_t handle = buffers_manager->num_vertex_buffers;
    buffers_manager->vertex_buffers[handle] = vb;
    ++(buffers_manager->num_vertex_buffers);
    return handle;
}

sp_ib_handle_t buffers_manager_allocate_ib(IRenderDevice* pDevice, const uint8_t* indices, uint32_t size)
{
    sapphire_buffers_manager_t* buffers_manager = &g_rendering_context_o->buffers_manager;
    IBuffer* ib = create_mesh_index_buffer(pDevice, indices, size);
    sp_ib_handle_t handle = buffers_manager->num_index_buffers;
    buffers_manager->index_buffers[handle] = ib;
    ++(buffers_manager->num_index_buffers);
    return handle;
}

inline sp_mesh_handle_t allocate_renderer_mesh(sapphire_renderer_t* renderer, sapphire_mesh_t** p_mesh)
{
    sp_mesh_handle_t mesh_handle = renderer->num_meshes;
    *p_mesh = &renderer->meshes[mesh_handle];
    ++renderer->num_meshes;
    return mesh_handle;
}

#define CENTIMETERS_TO_METERS(x) (x) *= 0.01f

void merge_vertex_streams_to_buffer(sapphire_mesh_gpu_load_t* mesh_load_data, uint32_t attribute_flags, uint8_t* vertices_data)
{
    uint8_t* p_curr = vertices_data;
    for (uint32_t i = 0; i < mesh_load_data->num_vertices; ++i)
    {
        for (uint32_t att_index = 0; att_index < MAX_VERTEX_ATTRIBUTE_STREAMS; ++att_index)
        {
            uint32_t stride = mesh_load_data->vertex_stream_stride_size[att_index];
            if (stride > 0 && (attribute_flags & (0x1 << att_index)))
            {                
                memcpy(p_curr, mesh_load_data->vertices[att_index] + stride * i, stride);                
                if (att_index == 0)
                {
                    // convert centimeters to meters
                    float* vertex_pos = (float* )p_curr;
                    CENTIMETERS_TO_METERS(vertex_pos[0]);
                    CENTIMETERS_TO_METERS(vertex_pos[1]);
                    CENTIMETERS_TO_METERS(vertex_pos[2]);
                    
                }
                p_curr += stride;
            }
            
        }
        
       
    }
}

sp_mesh_handle_t load_mesh_to_gpu(IRenderDevice* pDevice, rendering_context_t* p_rendering_context, sapphire_mesh_gpu_load_t* mesh_load_data)
{
    
    // allocate mesh
    sapphire_mesh_t* p_mesh;
    sp_mesh_handle_t mesh_handle = allocate_renderer_mesh(&p_rendering_context->renderer, &p_mesh);

    sp_allocator_i* allocator = sp_allocator_api->system_allocator;
    uint8_t* vertices_data = NULL;
    const uint8_t* p_final_vertices = NULL;
    if (mesh_load_data->flags)
    {
        uint32_t attribs_layout = 0b100011;
        mesh_load_data->vertex_stride = 32;
        mesh_load_data->vertices_data_size = mesh_load_data->num_vertices * mesh_load_data->vertex_stride;
        vertices_data = sp_alloc(allocator, mesh_load_data->vertices_data_size);
        merge_vertex_streams_to_buffer(mesh_load_data, attribs_layout, vertices_data);
        p_final_vertices = vertices_data;
    }
    else
    {
        p_final_vertices = mesh_load_data->vertices[0];
    }


    sp_vb_handle_t vb_handle = buffers_manager_allocate_vb(pDevice, p_final_vertices, mesh_load_data->vertices_data_size);
    sp_ib_handle_t ib_handle = buffers_manager_allocate_ib(pDevice, mesh_load_data->indices, mesh_load_data->indices_data_size);

    if (vertices_data)
    {
        sp_free(allocator, vertices_data, mesh_load_data->vertices_data_size);
    }

    p_mesh->ib_handle = vb_handle;
    p_mesh->vb_handle = ib_handle;
    p_mesh->num_submeshes = mesh_load_data->num_submeshes;
    for (uint32_t i = 0; i < mesh_load_data->num_submeshes; ++i)
    {
        p_mesh->sub_meshes[i].indices_start = mesh_load_data->sub_meshes[i].indices_start;
        p_mesh->sub_meshes[i].indices_count = mesh_load_data->sub_meshes[i].indices_count;
        p_mesh->sub_meshes[i].material_handle = material_manager_lookup_material(&g_rendering_context_o->materials_manager, mesh_load_data->sub_meshes[i].material_hash);// sp_hash_get_default(&p_rendering_context->materials_manager.material_name_lookup, mesh_load_data->sub_meshes[i].material_hash, 0);//mesh_load_data->sub_meshes[i].material_handle;
    }
    
    p_mesh->bounding_box_max = mesh_load_data->bounding_box_max;
    p_mesh->bounding_box_min = mesh_load_data->bounding_box_min;
    p_mesh->bounding_sphere_center = mesh_load_data->bounding_sphere_center;
    p_mesh->bounding_sphere_radius = mesh_load_data->bounding_sphere_radius;

    return mesh_handle;
}

// called when window is resized
// recreates the render target for the new window size
void renderer_window_resize(IRenderDevice* pDevice, ISwapChain* pSwapChain, uint32_t width, uint32_t height)
{
    // Create window - size offscreen render target
    TextureDesc RTColorDesc;
    RTColorDesc._DeviceObjectAttribs.Name = "Offscreen render target";
    RTColorDesc.Type = RESOURCE_DIM_TEX_2D;
    RTColorDesc.Width = width;
    RTColorDesc.Height = height;
    RTColorDesc.MipLevels = 1;
    RTColorDesc.SampleCount = 1;
    RTColorDesc.ArraySize = 1;
    RTColorDesc.Depth = 1;
    RTColorDesc.CPUAccessFlags = 0;
    RTColorDesc.MiscFlags = 0;
    RTColorDesc.Usage = USAGE_DEFAULT;
    RTColorDesc.Format = TEX_FORMAT_RGBA8_UNORM;
    // The render target can be bound as a shader resource and as a render target
    RTColorDesc.BindFlags = BIND_SHADER_RESOURCE | BIND_RENDER_TARGET;
    // Define optimal clear value
    RTColorDesc.ClearValue.Format = RTColorDesc.Format;
    RTColorDesc.ClearValue.Color[0] = 0.350f;
    RTColorDesc.ClearValue.Color[1] = 0.350f;
    RTColorDesc.ClearValue.Color[2] = 0.350f;
    RTColorDesc.ClearValue.Color[3] = 1.f;
    RTColorDesc.ImmediateContextMask = 1;

    // generate render target color texture and get the rt view
    ITexture* pRTColor = NULL;
    IRenderDevice_CreateTexture(pDevice, &RTColorDesc, NULL, &pRTColor);
    ITextureView* pColorRTV = ITexture_GetDefaultView(pRTColor, TEXTURE_VIEW_RENDER_TARGET);

    if (g_rendering_context_o->p_color_rtv)
    {
        IObject_Release(g_rendering_context_o->p_color_rtv);
    }
    g_rendering_context_o->p_color_rtv = pColorRTV;
    IObject_AddRef(g_rendering_context_o->p_color_rtv);

    //Create window - size depth buffer
    TextureDesc RTDepthDesc = RTColorDesc;    
    RTDepthDesc._DeviceObjectAttribs.Name = "Offscreen depth buffer";
    RTDepthDesc.Format = TEX_FORMAT_D32_FLOAT;
    RTDepthDesc.BindFlags = BIND_SHADER_RESOURCE | BIND_DEPTH_STENCIL;
    // Define optimal clear value
    RTDepthDesc.ClearValue.Format = RTDepthDesc.Format;
    RTDepthDesc.ClearValue.DepthStencil.Depth = 0; // TODO: ? reverse Z
    RTDepthDesc.ClearValue.DepthStencil.Stencil = 0;

    ITexture*  pRTDepth = NULL;
    IRenderDevice_CreateTexture(pDevice, &RTDepthDesc, NULL, &pRTDepth);
    
    // Store the depth-stencil view
    ITextureView* pDepthDSV = ITexture_GetDefaultView(pRTDepth, TEXTURE_VIEW_DEPTH_STENCIL);
    if (g_rendering_context_o->p_depth_rtv)
    {
        IObject_Release(g_rendering_context_o->p_depth_rtv);
    }
    g_rendering_context_o->p_depth_rtv = pDepthDSV;
    IObject_AddRef(g_rendering_context_o->p_depth_rtv);

    if (g_rendering_context_o->p_rt_srb)
    {
        IObject_Release(g_rendering_context_o->p_rt_srb);
    }

    // set the new texture as the input for the pso shader variable g_Texture in the pixel shader

    IPipelineState_CreateShaderResourceBinding(g_rendering_context_o->p_rt_pso, &g_rendering_context_o->p_rt_srb, true);
    IShaderResourceVariable* pVar = IShaderResourceBinding_GetVariableByName(g_rendering_context_o->p_rt_srb, SHADER_TYPE_PIXEL, "g_Texture");
    if (pVar)
    {
        ITextureView* pTextureSRV = ITexture_GetDefaultView(pRTColor, TEXTURE_VIEW_SHADER_RESOURCE);
        IShaderResourceVariable_Set(pVar, (IDeviceObject*)pTextureSRV, SET_SHADER_RESOURCE_FLAG_NONE);
    }

    IObject_Release(pRTColor);
    IObject_Release(pRTDepth);
         
}



////////


const char* read_file(const char* file, sp_temp_allocator_i* ta);

void get_attribute_as_string(sp_config_i* config, sp_config_item_t item, sp_strhash_t hash, char* buffer)
{
    sp_config_item_t attrib = config->object_get(config->inst, item, hash);
    const char* str = config->to_string(config->inst, attrib);
    memcpy(buffer, str, strlen(str) + 1);
}

inline bool get_attribute_as_bool(sp_config_i* config, sp_config_item_t item, sp_strhash_t hash)
{
    sp_config_item_t attrib = config->object_get(config->inst, item, hash);    
    return attrib.type == SP_CONFIG_TYPE_TRUE;
}

// create a pipepile state object for rendering a full screen quad
static IPipelineState* create_rt_pipeline_state(IRenderDevice* pDevice, ISwapChain* pSwapChain)
{
    // Pipeline state object encompasses configuration of all GPU stages

    GraphicsPipelineStateCreateInfo PSOCreateInfo;

    memset(&PSOCreateInfo, 0, sizeof(PSOCreateInfo));

    PipelineStateDesc* pPSODesc = &PSOCreateInfo._PipelineStateCreateInfo.PSODesc;

    // Pipeline state name is used by the engine to report issues.
    // It is always a good idea to give objects descriptive names.
    pPSODesc->_DeviceObjectAttribs.Name = "render_target_pso";

    // This is a graphics pipeline
    pPSODesc->PipelineType = PIPELINE_TYPE_GRAPHICS;

    // clang-format off

    // This tutorial will render to a single render target
    PSOCreateInfo.GraphicsPipeline.NumRenderTargets = 1;

    const SwapChainDesc* pSCDesc = ISwapChain_GetDesc(pSwapChain);
    PSOCreateInfo.GraphicsPipeline.RTVFormats[0] = pSCDesc->ColorBufferFormat;
    // Set depth buffer format which is the format of the swap chain's back buffer
    PSOCreateInfo.GraphicsPipeline.DSVFormat = pSCDesc->DepthBufferFormat;
    
    // Primitive topology defines what kind of primitives will be rendered by this pipeline state
    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

    // clang-format on

    pPSODesc->ImmediateContextMask = 1;
    PSOCreateInfo.GraphicsPipeline.SmplDesc.Count = 1;
    PSOCreateInfo.GraphicsPipeline.SampleMask = 0xFFFFFFFF;
    PSOCreateInfo.GraphicsPipeline.NumViewports = 1;

    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = False;
    //PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = True;
    

    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.FillMode = FILL_MODE_SOLID;
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_BACK;
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.DepthClipEnable = True;

    BlendStateDesc blend_desc = { 0 };

    blend_desc.RenderTargets[0].RenderTargetWriteMask = COLOR_MASK_ALL;    

    PSOCreateInfo.GraphicsPipeline.BlendDesc = blend_desc;


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
        ShaderCI.Desc._DeviceObjectAttribs.Name = "Render Target VS";

        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;        
        ShaderCI.EntryPoint = "main";
        ShaderCI.FilePath = "rendertarget.vsh";
        IRenderDevice_CreateShader(pDevice, &ShaderCI, &pVS, NULL);
    }

    // Create a pixel shader
    IShader* pPS = NULL;
    {
        ShaderCI.Desc._DeviceObjectAttribs.Name = "Render Target PS";

        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint = "main";
        ShaderCI.FilePath = "rendertarget.psh";
        IRenderDevice_CreateShader(pDevice, &ShaderCI, &pPS, NULL);

    }    

    PSOCreateInfo.pVS = pVS;
    PSOCreateInfo.pPS = pPS;
    
    //PSOCreateInfo.GraphicsPipeline.InputLayout.NumElements = 0;

    // Define variable type that will be used by default
    pPSODesc->ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

    // Shader variables should typically be mutable, which means they are expected
    // to change on a per-instance basis
    ShaderResourceVariableDesc Vars[] =
    {        
        {.ShaderStages = SHADER_TYPE_PIXEL, .Name = "g_Texture", .Type = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},        
    };


    pPSODesc->ResourceLayout.Variables = Vars;
    pPSODesc->ResourceLayout.NumVariables = SP_ARRAY_COUNT(Vars);

    SamplerDesc sampler_description = { 0 };
    sampler_description._DeviceObjectAttribs.Name = "Linear sampler";
    sampler_description.MinFilter = FILTER_TYPE_LINEAR;
    sampler_description.MagFilter = FILTER_TYPE_LINEAR;
    sampler_description.MipFilter = FILTER_TYPE_LINEAR;

    sampler_description.AddressU = TEXTURE_ADDRESS_CLAMP;
    sampler_description.AddressV = TEXTURE_ADDRESS_CLAMP;
    sampler_description.AddressW = TEXTURE_ADDRESS_CLAMP;
   
    sampler_description.ComparisonFunc = COMPARISON_FUNC_NEVER;
    sampler_description.MaxLOD = +3.402823466e+38F;

    // Define immutable sampler for g_Texture. Immutable samplers should be used whenever possible
    ImmutableSamplerDesc ImtblSamplers[] =
    {
        {.ShaderStages = SHADER_TYPE_PIXEL, .SamplerOrTextureName = "g_Texture", .Desc = sampler_description},        
    };

    pPSODesc->ResourceLayout.ImmutableSamplers = ImtblSamplers;
    pPSODesc->ResourceLayout.NumImmutableSamplers = SP_ARRAY_COUNT(ImtblSamplers);

    IPipelineState* pPSO = NULL;
    
    IRenderDevice_CreateGraphicsPipelineState(pDevice, &PSOCreateInfo, &pPSO);

    IObject_Release(pPS);
    IObject_Release(pVS);
    IObject_Release(pShaderSourceFactory);

    return pPSO;
}

static IPipelineState* create_pipeline_state(const char* pso_name, IRenderDevice* pDevice, TEXTURE_FORMAT color_buffer_format, TEXTURE_FORMAT depth_buffer_format, uint64_t state_flags)
{
    // Pipeline state object encompasses configuration of all GPU stages

    GraphicsPipelineStateCreateInfo PSOCreateInfo;

    memset(&PSOCreateInfo, 0, sizeof(PSOCreateInfo));

    PipelineStateDesc* pPSODesc = &PSOCreateInfo._PipelineStateCreateInfo.PSODesc;

    // Pipeline state name is used by the engine to report issues.
    // It is always a good idea to give objects descriptive names.
    pPSODesc->_DeviceObjectAttribs.Name = pso_name;

    // This is a graphics pipeline
    pPSODesc->PipelineType = PIPELINE_TYPE_GRAPHICS;

    // clang-format off

    // This tutorial will render to a single render target
    PSOCreateInfo.GraphicsPipeline.NumRenderTargets = 1;
    

    PSOCreateInfo.GraphicsPipeline.RTVFormats[0] = color_buffer_format;
    // Set depth buffer format which is the format of the swap chain's back buffer
    PSOCreateInfo.GraphicsPipeline.DSVFormat = depth_buffer_format;
    // Primitive topology defines what kind of primitives will be rendered by this pipeline state
    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    // clang-format on

    pPSODesc->ImmediateContextMask = 1;
    PSOCreateInfo.GraphicsPipeline.SmplDesc.Count = 1;
    PSOCreateInfo.GraphicsPipeline.SampleMask = 0xFFFFFFFF;
    PSOCreateInfo.GraphicsPipeline.NumViewports = 1;

    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = (state_flags & SP_MATERIAL_STATE_DEPTH_TEST_ENABLED) ? True : False;
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = (state_flags & SP_MATERIAL_STATE_DEPTH_WRITE_ENABLED) ? True : False;
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthFunc = COMPARISON_FUNC_GREATER;

    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.FillMode = FILL_MODE_SOLID;
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;// (state_flags & SP_MATERIAL_DOUBLE_SIDED) ? CULL_MODE_NONE : CULL_MODE_BACK;
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.DepthClipEnable = True;

    BlendStateDesc blend_desc = { 0 };

    blend_desc.RenderTargets[0].RenderTargetWriteMask = COLOR_MASK_ALL;
    if (state_flags & SP_MATERIAL_BLEND_MODE_TRANSPARENT)
    {
        blend_desc.AlphaToCoverageEnable = False;
        blend_desc.IndependentBlendEnable = False;
        blend_desc.RenderTargets[0].BlendEnable = True;
        blend_desc.RenderTargets[0].LogicOperationEnable = False;
        blend_desc.RenderTargets[0].BlendOpAlpha = BLEND_OPERATION_ADD;
        blend_desc.RenderTargets[0].DestBlend = BLEND_FACTOR_INV_SRC_ALPHA;
        blend_desc.RenderTargets[0].DestBlendAlpha = BLEND_FACTOR_INV_SRC_ALPHA;
        
        blend_desc.RenderTargets[0].SrcBlend = BLEND_FACTOR_SRC_ALPHA;
        blend_desc.RenderTargets[0].BlendOp = BLEND_OPERATION_ADD;
        blend_desc.RenderTargets[0].SrcBlendAlpha = BLEND_FACTOR_SRC_ALPHA;    
    }
   
    PSOCreateInfo.GraphicsPipeline.BlendDesc = blend_desc;


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
        ShaderCI.Desc._DeviceObjectAttribs.Name = "default PBR VS";

        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint = "main";
        ShaderCI.FilePath = "default_pbr.vsh";
        IRenderDevice_CreateShader(pDevice, &ShaderCI, &pVS, NULL);

    }


    // Create a pixel shader
    IShader* pPS = NULL;
    {
        ShaderCI.Desc._DeviceObjectAttribs.Name = "default PS";

        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint = "main";
        ShaderCI.FilePath = "default_pbr.psh";
        IRenderDevice_CreateShader(pDevice, &ShaderCI, &pPS, NULL);
        
    }

    // Define vertex shader input layout
    LayoutElement LayoutElems[] =
    {
        // Attribute 0 - vertex position
        {.HLSLSemantic = "ATTRIB", .InputIndex = 0, .NumComponents = 3, .ValueType = VT_FLOAT32, .IsNormalized = False, .RelativeOffset = LAYOUT_ELEMENT_AUTO_OFFSET, .Stride = LAYOUT_ELEMENT_AUTO_STRIDE, .Frequency = INPUT_ELEMENT_FREQUENCY_PER_VERTEX, .InstanceDataStepRate = 1},
        // Attribute 0 - normals
        {.HLSLSemantic = "ATTRIB", .InputIndex = 1, .NumComponents = 3, .ValueType = VT_FLOAT32, .IsNormalized = False, .RelativeOffset = LAYOUT_ELEMENT_AUTO_OFFSET, .Stride = LAYOUT_ELEMENT_AUTO_STRIDE, .Frequency = INPUT_ELEMENT_FREQUENCY_PER_VERTEX, .InstanceDataStepRate = 1},
        // Attribute 2 - texture coordinates
        {.HLSLSemantic = "ATTRIB", .InputIndex = 2, .NumComponents = 2, .ValueType = VT_FLOAT32, .IsNormalized = False, .RelativeOffset = LAYOUT_ELEMENT_AUTO_OFFSET, .Stride = LAYOUT_ELEMENT_AUTO_STRIDE, .Frequency = INPUT_ELEMENT_FREQUENCY_PER_VERTEX, .InstanceDataStepRate = 1}
    };
   
    PSOCreateInfo.pVS = pVS;
    PSOCreateInfo.pPS = pPS;

    PSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems;
    PSOCreateInfo.GraphicsPipeline.InputLayout.NumElements = SP_ARRAY_COUNT(LayoutElems);

    // Define variable type that will be used by default
    pPSODesc->ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

    // Shader variables should typically be mutable, which means they are expected
    // to change on a per-instance basis
    ShaderResourceVariableDesc Vars[] =
    {
        {.ShaderStages = SHADER_TYPE_VERTEX, .Name = "cbCameraAttribs", .Type = SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
        {.ShaderStages = SHADER_TYPE_PIXEL, .Name = "cbCameraAttribs", .Type = SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
        {.ShaderStages = SHADER_TYPE_PIXEL, .Name = "cbLightAttribs", .Type = SHADER_RESOURCE_VARIABLE_TYPE_STATIC},        
        {.ShaderStages = SHADER_TYPE_VERTEX, .Name = "cbTransforms", .Type = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {.ShaderStages = SHADER_TYPE_PIXEL, .Name = "cbTransforms", .Type = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},        
        {.ShaderStages = SHADER_TYPE_PIXEL, .Name = "g_AlbedoTexture", .Type = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {.ShaderStages = SHADER_TYPE_PIXEL, .Name = "g_NormalsTexture", .Type = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {.ShaderStages = SHADER_TYPE_PIXEL, .Name = "g_PhysicalDescriptorMap", .Type = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}
    };
    

    pPSODesc->ResourceLayout.Variables = Vars;
    pPSODesc->ResourceLayout.NumVariables = SP_ARRAY_COUNT(Vars);

    SamplerDesc sampler_description = { 0 };
    sampler_description._DeviceObjectAttribs.Name = "Linear sampler";
    sampler_description.MinFilter = FILTER_TYPE_LINEAR;
    sampler_description.MagFilter = FILTER_TYPE_LINEAR;
    sampler_description.MipFilter = FILTER_TYPE_LINEAR;

    if (state_flags & SP_MATERIAL_TEXTURE_ADDRESS_MODE_CLAMP)
    {
        sampler_description.AddressU = TEXTURE_ADDRESS_CLAMP;
        sampler_description.AddressV = TEXTURE_ADDRESS_CLAMP;
        sampler_description.AddressW = TEXTURE_ADDRESS_CLAMP;
    }
    else if (state_flags & SP_MATERIAL_TEXTURE_ADDRESS_MODE_WRAP)
    {
        sampler_description.AddressU = TEXTURE_ADDRESS_WRAP;
        sampler_description.AddressV = TEXTURE_ADDRESS_WRAP;
        sampler_description.AddressW = TEXTURE_ADDRESS_WRAP;
    }
    else
    {
        sampler_description.AddressU = TEXTURE_ADDRESS_MIRROR;
        sampler_description.AddressV = TEXTURE_ADDRESS_MIRROR;
        sampler_description.AddressW = TEXTURE_ADDRESS_MIRROR;
    }

    sampler_description.ComparisonFunc = COMPARISON_FUNC_NEVER;
    sampler_description.MaxLOD = +3.402823466e+38F;
    
    

    // Define immutable sampler for g_Texture. Immutable samplers should be used whenever possible
    ImmutableSamplerDesc ImtblSamplers[] =
    {
        {.ShaderStages = SHADER_TYPE_PIXEL, .SamplerOrTextureName = "g_AlbedoTexture", .Desc = sampler_description},
        {.ShaderStages = SHADER_TYPE_PIXEL, .SamplerOrTextureName = "g_NormalsTexture", .Desc = sampler_description},
        {.ShaderStages = SHADER_TYPE_PIXEL, .SamplerOrTextureName = "g_PhysicalDescriptorMap", .Desc = sampler_description}
    };

    pPSODesc->ResourceLayout.ImmutableSamplers = ImtblSamplers;
    pPSODesc->ResourceLayout.NumImmutableSamplers = SP_ARRAY_COUNT(ImtblSamplers);

    IPipelineState* pPSO = NULL;
    IRenderDevice_CreateGraphicsPipelineState(pDevice, &PSOCreateInfo, &pPSO);

    //// Since we did not explcitly specify the type for 'Constants' variable, default
    //// type (SHADER_RESOURCE_VARIABLE_TYPE_STATIC) will be used. Static variables
    //// never change and are bound directly through the pipeline state object.
    //IShaderResourceVariable* pVar = IPipelineState_GetStaticVariableByName(g_pPSO, SHADER_TYPE_VERTEX, "Constants");
    //if (pVar)
    //{
    //    IShaderResourceVariable_Set(pVar, (IDeviceObject*)g_pPSConstants, SET_SHADER_RESOURCE_FLAG_NONE);
    //}


    // Since we are using mutable variable, we must create a shader resource binding object
    // http://diligentgraphics.com/2016/03/23/resource-binding-model-in-diligent-engine-2-0/
    //IPipelineState_CreateShaderResourceBinding(g_pPSO, &g_pSRB, true);

    IObject_Release(pPS);
    IObject_Release(pVS);
    IObject_Release(pShaderSourceFactory);

    return pPSO;
}


inline void bind_shader_texture_variable(IShaderResourceBinding* p_srb, ITextureView* p_texture_SRV, const char* texture_var)
{
    IShaderResourceVariable* pVar = IShaderResourceBinding_GetVariableByName(p_srb, SHADER_TYPE_PIXEL, texture_var);
    if (pVar)
    {
        IShaderResourceVariable_Set(pVar, (IDeviceObject*)p_texture_SRV, SET_SHADER_RESOURCE_FLAG_NONE);
    }
}




// Render a frame
void renderer_do_rendering(IDeviceContext* pContext, viewer_t* viewer)
{
    ITextureView* pRTV = g_rendering_context_o->p_color_rtv;
    ITextureView* pDSV = g_rendering_context_o->p_depth_rtv;

    // set texture render target 
    
    IDeviceContext_SetRenderTargets(pContext, 1, &pRTV, pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    // Clear the back buffer
    const float ClearColor[] = { 0.850f, 0.350f, 0.350f, 1.0f };
    IDeviceContext_ClearRenderTarget(pContext, pRTV, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    IDeviceContext_ClearDepthStencil(pContext, pDSV, CLEAR_DEPTH_FLAG, 0.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    {
        cb_camera_attribs_t cb_camera = {
            .far_plane_z = viewer->camera.far_plane,
            .near_plane_z = viewer->camera.near_plane,
            .word_pos = {viewer->camera_transform.position.x, viewer->camera_transform.position.y, viewer->camera_transform.position.z, 1},
            .view_mat_trans = viewer->camera.view[SP_CAMERA_TRANSFORM_DEFAULT],
            .proj_mat_trans = viewer->camera.projection[SP_CAMERA_TRANSFORM_DEFAULT],
            .view_proj_mat_trans = viewer->view_projection
            
            
        };

        // Map the buffer and write current camera data
        cb_camera_attribs_t* p_cb_data = NULL;
        IDeviceContext_MapBuffer(pContext, g_rendering_context_o->cb_camera_attribs, MAP_WRITE, MAP_FLAG_DISCARD, &p_cb_data);
        *p_cb_data = cb_camera;
        IDeviceContext_UnmapBuffer(pContext, g_rendering_context_o->cb_camera_attribs, MAP_WRITE);
    }

    {
        cb_light_attribs_t cb_light = {
            .f4AmbientLight = {1,1,1,1},            
            .f4Direction = {0.5f, -0.6f, 0.2f, 0},
            .f4Intensity = {3,3,3,3}
        };

        cb_light.f4Direction = sp_vec4_normalize(cb_light.f4Direction);
        // Map the buffer and write current camera data
        cb_light_attribs_t* p_cb_data = NULL;
        IDeviceContext_MapBuffer(pContext, g_rendering_context_o->cb_lights_attribs, MAP_WRITE, MAP_FLAG_DISCARD, &p_cb_data);
        *p_cb_data = cb_light;
        IDeviceContext_UnmapBuffer(pContext, g_rendering_context_o->cb_lights_attribs, MAP_WRITE);
    }

    sapphire_renderer_t* renderer = &g_rendering_context_o->renderer;
    sapphire_buffers_manager_t* buffers_manager = &g_rendering_context_o->buffers_manager;
    sp_material_t* material_array = g_rendering_context_o->materials_manager.materials_arr;
    // render all objects
    for (uint32_t i = 0; i < renderer->num_render_objects; ++i)
    {
        sapphire_mesh_t* mesh = &renderer->meshes[renderer->mesh_handles[i]];

        // Bind vertex and index buffers
        const Uint64 offset = 0;
        IBuffer* pBuffs[1];
        pBuffs[0] = buffers_manager->vertex_buffers[mesh->vb_handle];
        IDeviceContext_SetVertexBuffers(pContext, 0, 1, pBuffs, &offset, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
        IDeviceContext_SetIndexBuffer(pContext, buffers_manager->index_buffers[mesh->ib_handle], 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        

        // set transform for mesh
        {
            // Map the buffer and write current world matrix
            cb_drawcall_t* p_cb_data = NULL;
            IDeviceContext_MapBuffer(pContext, g_rendering_context_o->cb_drawcall, MAP_WRITE, MAP_FLAG_DISCARD, &p_cb_data);
            // TODO - handle identitity 
            p_cb_data->identity = 0x1;
            p_cb_data->transform = renderer->world_matrices[i];
            IDeviceContext_UnmapBuffer(pContext, g_rendering_context_o->cb_drawcall, MAP_WRITE);
        }

        for (uint32_t sub_mesh_idx = 0; sub_mesh_idx < mesh->num_submeshes; ++sub_mesh_idx)
        {
            sapphire_sub_mesh_t* sub_mesh = &mesh->sub_meshes[sub_mesh_idx];
            sp_material_t* material = &material_array[sub_mesh->material_handle];
            IDeviceContext_SetPipelineState(pContext, material->p_pso);
            // bind textures to srb
            //// Set texture SRV in the SRB
            bind_shader_texture_variable(material->p_srb, material->texture_views[0], "g_AlbedoTexture");
            bind_shader_texture_variable(material->p_srb, material->texture_views[1], "g_NormalsTexture");
            bind_shader_texture_variable(material->p_srb, material->texture_views[2], "g_PhysicalDescriptorMap");


            IDeviceContext_CommitShaderResources(pContext, material->p_srb, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            DrawIndexedAttribs draw_attrs;
            memset(&draw_attrs, 0, sizeof(draw_attrs));

            draw_attrs.IndexType = VT_UINT32; // Index type
            draw_attrs.NumIndices = sub_mesh->indices_count;
            draw_attrs.FirstIndexLocation = sub_mesh->indices_start;
            draw_attrs.NumInstances = 1;

            // Verify the state of vertex and index buffers
            draw_attrs.Flags = DRAW_FLAG_VERIFY_ALL;

            IDeviceContext_DrawIndexed(pContext, &draw_attrs);

        }


    }

    // set swap chain render target

    pRTV = ISwapChain_GetCurrentBackBufferRTV(g_rendering_context_o->p_swap_chain);
    pDSV = ISwapChain_GetDepthBufferDSV(g_rendering_context_o->p_swap_chain);
    IDeviceContext_SetRenderTargets(pContext, 1, &pRTV, pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    const float Zero[] = { 1.0f, 0.0f, 0.0f, 1.0f };
    IDeviceContext_ClearRenderTarget(pContext, pRTV, Zero, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    IDeviceContext_ClearDepthStencil(pContext, pDSV, CLEAR_DEPTH_FLAG, 0.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Set the pipeline state
    IDeviceContext_SetPipelineState(pContext, g_rendering_context_o->p_rt_pso);
    // Commit shader resources. RESOURCE_STATE_TRANSITION_MODE_TRANSITION mode
    // makes sure that resources are transitioned to required states.
    IDeviceContext_CommitShaderResources(pContext, g_rendering_context_o->p_rt_srb, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    DrawAttribs draw_attrs;
    memset(&draw_attrs, 0, sizeof(draw_attrs));
    draw_attrs.NumVertices = 4;
    draw_attrs.NumInstances = 1;
    // Verify the state of vertex and index buffers
    draw_attrs.Flags = DRAW_FLAG_VERIFY_ALL;

    IDeviceContext_Draw(pContext, &draw_attrs);

}

void init_textures_manager(sapphire_textures_manager_t* textures_manager, sp_allocator_i* allocator)
{
    textures_manager->allocator = allocator;
    textures_manager->textures_arr = NULL;
    memset(&textures_manager->texture_path_lookup, 0, sizeof(textures_manager->texture_path_lookup));
    textures_manager->texture_path_lookup.allocator = allocator;
    
}

void destroy_textures_manager(sapphire_textures_manager_t* textures_manager)
{
    uint32_t num_textures = (uint32_t)sp_array_size(textures_manager->textures_arr);
    for (uint32_t i = 0; i < num_textures; ++i)
    {
        IObject_Release(textures_manager->textures_arr[i]);
    }
    sp_array_free(textures_manager->textures_arr, textures_manager->allocator);
    sp_hash_free(&textures_manager->texture_path_lookup);
}

static ITextureView* textures_manager_load_texture(IRenderDevice* pDevice, sapphire_textures_manager_t* textures_manager, const char* texture_file_path)
{
    sp_strhash_t texture_key = sp_murmur_hash_string(texture_file_path);
    if (sp_hash_has(&textures_manager->texture_path_lookup, texture_key))
    {
        uint32_t index = sp_hash_get(&textures_manager->texture_path_lookup, texture_key);
        return textures_manager->textures_arr[index];
    }
    TextureLoadInfo loadInfo;
    memset(&loadInfo, 0, sizeof(loadInfo));
    loadInfo.IsSRGB = true;
    loadInfo.Usage = USAGE_IMMUTABLE;
    loadInfo.BindFlags = BIND_SHADER_RESOURCE;
    loadInfo.GenerateMips = True;

    ITexture* pTex = NULL;
    Diligent_CreateTextureFromFile(texture_file_path, &loadInfo, pDevice, &pTex);
    // Get shader resource view from the texture
    ITextureView* pTextureSRV = ITexture_GetDefaultView(pTex, TEXTURE_VIEW_SHADER_RESOURCE);

    IObject_AddRef(pTextureSRV);

    //// Set texture SRV in the SRB
    //IShaderResourceVariable* pVar = IShaderResourceBinding_GetVariableByName(p_srb, SHADER_TYPE_PIXEL, texture_var);
    //if (pVar)
    //{
    //    IShaderResourceVariable_Set(pVar, (IDeviceObject*)pTextureSRV, SET_SHADER_RESOURCE_FLAG_NONE);
    //}

    IObject_Release((IObject*)pTex);
    // store texture view in array and add to lookup table
    uint32_t index = (uint32_t)sp_array_size(textures_manager->textures_arr);
    sp_array_push(textures_manager->textures_arr, pTextureSRV, textures_manager->allocator);
    sp_hash_add(&textures_manager->texture_path_lookup, texture_key, index);
    return pTextureSRV;
}

sp_material_t load_material_gpu_resources(sapphire_materials_manager_t* manager ,sp_material_def_t* material_def)
{
    TEXTURE_FORMAT  color_buffer_format  = TEX_FORMAT_RGBA8_UNORM;
    TEXTURE_FORMAT  depth_buffer_format  = TEX_FORMAT_D32_FLOAT;
        
    uint64_t pso_hash = material_def->flags;
    IPipelineState* p_pso = NULL;
    IShaderResourceBinding* p_srb = NULL;
    if (sp_hash_has(&manager->pso_srb_lookup, pso_hash))
    {
        sp_mat_gpu_resources_t gpu_res = sp_hash_get(&manager->pso_srb_lookup, pso_hash);
        p_pso = gpu_res.p_pso;
        p_srb = gpu_res.p_srb;
    }
    else
    {
        p_pso = create_pipeline_state("default_pbr_pso", g_rendering_context_o->p_device, color_buffer_format, depth_buffer_format, material_def->flags);

        // bind buffers to static shader variables
        IShaderResourceVariable* pVar = IPipelineState_GetStaticVariableByName(p_pso, SHADER_TYPE_VERTEX, "cbCameraAttribs");
        if (pVar)
        {
            IShaderResourceVariable_Set(pVar, (IDeviceObject*)g_rendering_context_o->cb_camera_attribs, SET_SHADER_RESOURCE_FLAG_NONE);
        }
        pVar = IPipelineState_GetStaticVariableByName(p_pso, SHADER_TYPE_PIXEL, "cbCameraAttribs");
        if (pVar)
        {
            IShaderResourceVariable_Set(pVar, (IDeviceObject*)g_rendering_context_o->cb_camera_attribs, SET_SHADER_RESOURCE_FLAG_NONE);
        }
        pVar = IPipelineState_GetStaticVariableByName(p_pso, SHADER_TYPE_PIXEL, "cbLightAttribs");
        if (pVar)
        {
            IShaderResourceVariable_Set(pVar, (IDeviceObject*)g_rendering_context_o->cb_lights_attribs, SET_SHADER_RESOURCE_FLAG_NONE);
        }


        pVar = IPipelineState_GetStaticVariableByName(p_pso, SHADER_TYPE_PIXEL, "PickingBuffer");
        if (pVar)
        {
            IBufferView* buffer_view = IBuffer_GetDefaultView(g_rendering_context_o->picking_buffer, BUFFER_VIEW_UNORDERED_ACCESS);
            IShaderResourceVariable_Set(pVar, (IDeviceObject*)buffer_view, SET_SHADER_RESOURCE_FLAG_NONE);
        }
        
        IPipelineState_CreateShaderResourceBinding(p_pso, &p_srb, true);

        IShaderResourceVariable* p_srb_var = IShaderResourceBinding_GetVariableByName(p_srb, SHADER_TYPE_VERTEX, "cbTransforms");
        if (p_srb_var)
        {
            IShaderResourceVariable_Set(p_srb_var, (IDeviceObject*)g_rendering_context_o->cb_drawcall, SET_SHADER_RESOURCE_FLAG_NONE);
        }

        p_srb_var = IShaderResourceBinding_GetVariableByName(p_srb, SHADER_TYPE_PIXEL, "cbTransforms");
        if (p_srb_var)
        {
            IShaderResourceVariable_Set(p_srb_var, (IDeviceObject*)g_rendering_context_o->cb_drawcall, SET_SHADER_RESOURCE_FLAG_NONE);
        }

        sp_mat_gpu_resources_t gpu_res = { .p_pso = p_pso, .p_srb = p_srb };
        sp_hash_add(&manager->pso_srb_lookup, pso_hash, gpu_res);
    }

    // TODO - store all texture views in a manager
    ITextureView* albedo_texture_view = textures_manager_load_texture(g_rendering_context_o->p_device, &g_rendering_context_o->textures_manager, material_def->albedo_map); // , "g_AlbedoTexture"
    ITextureView* normal_texture_view = textures_manager_load_texture(g_rendering_context_o->p_device, &g_rendering_context_o->textures_manager, material_def->normal_map);// , "g_NormalsTexture");
    ITextureView* arm_texture_view = textures_manager_load_texture(g_rendering_context_o->p_device, &g_rendering_context_o->textures_manager, material_def->arm_map);// , "g_PhysicalDescriptorMap");

    // TODO - store shader sampler variables name in material
    sp_material_t mat = {.p_pso = p_pso, .p_srb = p_srb };
    mat.texture_views[0] = albedo_texture_view;
    mat.texture_views[1] = normal_texture_view;
    mat.texture_views[2] = arm_texture_view;
    return mat;
}

void material_manager_add_materials(sapphire_materials_manager_t* manager , sp_material_def_t* materials_def_arr)
{
    uint32_t num_materials = (uint32_t)sp_array_size(materials_def_arr);

    sp_array_ensure(manager->materials_arr, num_materials, manager->allocator);

    for (uint32_t i = 0; i < num_materials; ++i)
    {
        sp_material_def_t* material_def = &materials_def_arr[i];
        if (sp_hash_has(&manager->material_name_lookup, material_def->name_hash) == false)
        {
            sp_material_t mat = load_material_gpu_resources(manager, material_def);
            sp_array_push(manager->materials_arr, mat, manager->allocator);
            //uint64_t hash_key = sp_murmur_hash_string("xadvance");
            uint32_t size = (uint32_t)sp_array_size(manager->materials_arr);
            sp_hash_add(&manager->material_name_lookup, material_def->name_hash, size - 1);
        }
        
    }
}

sp_mat_handle_t material_manager_lookup_material(sapphire_materials_manager_t* manager, sp_strhash_t mat_name_hash)
{
    sp_mat_handle_t mat_handle = sp_hash_get_default(&manager->material_name_lookup, mat_name_hash, 0);
    return mat_handle;
}

void load_materials(const char* materials_file, sp_material_def_t** p_materials_arr, sp_allocator_i* mats_allocator)
{
    SP_INIT_TEMP_ALLOCATOR_WITH_ADAPTER(ta, a);

    

    const char* text = read_file(materials_file, ta);

    // read json file, including all extentions
    char error[256];
    const uint32_t parse_flags = SP_JSON_PARSE_EXT_ALLOW_UNQUOTED_KEYS | SP_JSON_PARSE_EXT_ALLOW_COMMENTS | SP_JSON_PARSE_EXT_IMPLICIT_ROOT_OBJECT | SP_JSON_PARSE_EXT_OPTIONAL_COMMAS | SP_JSON_PARSE_EXT_EQUALS_FOR_COLON;
    sp_config_i* materials_config = sp_config_api->create(a);
    bool res = sp_json_api->parse(text, materials_config, parse_flags, error);
    if (!res)
    {
        return;
    }

    sp_config_item_t root = materials_config->root(materials_config->inst);
    sp_strhash_t s_materials_hash = sp_murmur_hash_string("materials");
    sp_strhash_t s_name_hash = sp_murmur_hash_string("name");
    sp_strhash_t s_albedo_map_hash = sp_murmur_hash_string("albedo_map");
    sp_strhash_t s_arm_map_hash = sp_murmur_hash_string("arm_map");
    sp_strhash_t s_normal_map_hash = sp_murmur_hash_string("normal_map");
    sp_strhash_t s_double_sided_hash = sp_murmur_hash_string("double_sided");

    sp_config_item_t materials = materials_config->object_get(materials_config->inst, root, s_materials_hash);
    sp_config_item_t* materials_items_array = NULL;
    uint32_t num_materials = materials_config->to_array(materials_config->inst, materials, &materials_items_array);

    sp_material_def_t* materials_arr = *p_materials_arr;
    sp_array_ensure(materials_arr, num_materials, mats_allocator);

    for (uint32_t i = 0; i < num_materials; ++i)
    {
        sp_config_item_t material_item = materials_items_array[i];
        sp_material_def_t* material_o = &materials_arr[i];
        material_o->flags = 0;
        get_attribute_as_string(materials_config, material_item, s_name_hash, material_o->name);
        get_attribute_as_string(materials_config, material_item, s_albedo_map_hash, material_o->albedo_map);
        get_attribute_as_string(materials_config, material_item, s_arm_map_hash, material_o->arm_map);
        get_attribute_as_string(materials_config, material_item, s_normal_map_hash, material_o->normal_map);
        bool is_double_sided = get_attribute_as_bool(materials_config, material_item, s_double_sided_hash);
        if (is_double_sided)
        {
            material_o->flags |= SP_MATERIAL_DOUBLE_SIDED;
        }
        material_o->flags |= SP_MATERIAL_BLEND_MODE_OPAQUE;
        material_o->flags |= SP_MATERIAL_TEXTURE_ADDRESS_MODE_WRAP;
        material_o->flags |= SP_MATERIAL_STATE_DEPTH_TEST_ENABLED;
        material_o->flags |= SP_MATERIAL_STATE_DEPTH_WRITE_ENABLED;

        material_o->name_hash = sp_murmur_hash_string(material_o->name);
    }
    
    sp_array_header(materials_arr)->size = num_materials;

    *p_materials_arr = materials_arr;

    SP_SHUTDOWN_TEMP_ALLOCATOR(ta);
}

void init_materials_manager(sapphire_materials_manager_t* materials_manager, sp_allocator_i* allocator)
{
    memset(materials_manager, 0, sizeof(sapphire_materials_manager_t));
    materials_manager->allocator = allocator;
    materials_manager->material_name_lookup.allocator = allocator;
    materials_manager->pso_srb_lookup.allocator = allocator;
    
}

void destroy_materials_manager(sapphire_materials_manager_t* materials_manager)
{
    // iterate over all gpu resources and destroy them
    for (uint32_t i = 0; i < materials_manager->pso_srb_lookup.num_buckets; ++i)
    {
        if (sp_hash_use_index(&materials_manager->pso_srb_lookup, i))
        {
            IObject_Release(materials_manager->pso_srb_lookup.values[i].p_srb);
            IObject_Release(materials_manager->pso_srb_lookup.values[i].p_pso);
        }
    }
             
        //         continue;
    sp_array_free(materials_manager->materials_arr, materials_manager->allocator);
    sp_hash_free(&materials_manager->material_name_lookup);
    sp_hash_free(&materials_manager->pso_srb_lookup);
}

void init_buffers_manager(sapphire_buffers_manager_t* buffers_manager)
{
    memset(buffers_manager, 0, sizeof(sapphire_buffers_manager_t));
}

void destroy_buffers_manager(sapphire_buffers_manager_t* buffers_manager)
{
    for (uint32_t i = 0; i < buffers_manager->num_vertex_buffers; ++i)
    {
        IObject_Release(buffers_manager->vertex_buffers[i]);
    }

    for (uint32_t i = 0; i < buffers_manager->num_index_buffers; ++i)
    {
        IObject_Release(buffers_manager->index_buffers[i]);
    }
}

void init_renderer(sapphire_renderer_t* p_renderer)
{
    p_renderer->num_meshes = 0;
    p_renderer->num_render_objects = 0;
}


////



void scene_load_resources(const char* root_path_str, scene_def_t* p_scene_def, IRenderDevice* p_device)
{
    SP_INIT_TEMP_ALLOCATOR_WITH_ADAPTER(ta, a);

    sp_allocator_i* allocator = sp_allocator_api->system_allocator;

    // get mesh handle by entity hash
    struct SP_HASH_T(sp_strhash_t, sp_mesh_handle_t) entity_to_mesh_handle = {.allocator = allocator};

    uint32_t num_entities_defs = (uint32_t)sp_array_size(p_scene_def->entities_def_arr);
    
    for (uint32_t i = 0; i < num_entities_defs; ++i)
    {        
        sapphire_mesh_gpu_load_t mesh_load_data;
        char mesh_file[1024];
        uint8_t* stream = NULL;
        uint64_t size;

        entity_def_t* p_entity_def = &p_scene_def->entities_def_arr[i];
        //sp_strhash_t mesh_hash = sp_murmur_hash_string(p_entity_def->model_file);
        //if (sp_hash_has(&mesh_file_to_handle, mesh_hash) == false)
        {
            sp_sprintf_api->print(mesh_file, sizeof(mesh_file), "%s/%s", root_path_str, p_entity_def->model_file);
            read_file_stream(mesh_file, ta, &stream, &size);
            read_grimrock_model_from_stream(stream, size, &mesh_load_data);

            sp_mesh_handle_t mesh_handle = load_mesh_to_gpu(p_device, g_rendering_context_o, &mesh_load_data);
            sp_hash_add(&entity_to_mesh_handle, p_entity_def->entity_hash, mesh_handle);
        }
        
    }

    uint32_t num_entity_instances = (uint32_t)sp_array_size(p_scene_def->instances_arr);
    for (uint32_t i = 0; i < num_entity_instances; ++i)
    {
        sp_strhash_t entity_hash = p_scene_def->instances_arr[i].entity_hash;
        
        sp_mesh_handle_t mesh_handle = sp_hash_get(&entity_to_mesh_handle, entity_hash);
        g_rendering_context_o->renderer.mesh_handles[i] = mesh_handle;
        sp_transform_t* p_transform = &p_scene_def->instances_arr[i].transform;
        sp_mat4x4_t inst_mat;
        sp_mat4x4_from_translation_quaternion_scale(&inst_mat, p_transform->position, p_transform->rotation, p_transform->scale);
        g_rendering_context_o->renderer.world_matrices[i] = inst_mat;
        g_rendering_context_o->renderer.num_render_objects++;
    }

    
    
    sp_hash_free(&entity_to_mesh_handle);

    SP_SHUTDOWN_TEMP_ALLOCATOR(ta);
}

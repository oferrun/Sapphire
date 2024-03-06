#pragma once

#include "core/sapphire_types.h"

typedef uint32_t sp_vb_handle_t;
typedef uint32_t sp_ib_handle_t;
typedef uint32_t sp_mat_handle_t;
typedef uint32_t sp_mesh_handle_t;
typedef uint32_t sp_render_handle_t;

typedef struct IPipelineState IPipelineState;
typedef struct IShaderResourceBinding IShaderResourceBinding;
typedef struct ITextureView ITextureView;
typedef struct IRenderDevice IRenderDevice;
typedef struct ISwapChain ISwapChain;
typedef struct IBuffer IBuffer;
typedef struct IDeviceContext IDeviceContext;

typedef struct sp_allocator_i sp_allocator_i;


typedef struct sapphire_sub_mesh_gpu_load_t
{
    uint32_t indices_count;
    uint32_t indices_start;
    sp_strhash_t material_hash;    

    
} sapphire_sub_mesh_gpu_load_t;

#define MAX_SUB_MESHES 4
#define MAX_VERTEX_ATTRIBUTE_STREAMS 15

/*
        VERTEX_ARRAY_POSITION = 0
        VERTEX_ARRAY_NORMAL = 1
        VERTEX_ARRAY_TANGENT = 2
        VERTEX_ARRAY_BITANGENT = 3
        VERTEX_ARRAY_COLOR = 4
        VERTEX_ARRAY_TEXCOORD0 = 5
        VERTEX_ARRAY_TEXCOORD1 = 6
        VERTEX_ARRAY_TEXCOORD2 = 7
        VERTEX_ARRAY_TEXCOORD3 = 8
        VERTEX_ARRAY_TEXCOORD4 = 9
        VERTEX_ARRAY_TEXCOORD5 = 10
        VERTEX_ARRAY_TEXCOORD6 = 11
        VERTEX_ARRAY_TEXCOORD7 = 12
        VERTEX_ARRAY_BONE_INDEX = 13
        VERTEX_ARRAY_BONE_WEIGHT = 14
 */
typedef struct sapphire_mesh_gpu_load_t
{    
    uint32_t num_vertices;
    uint32_t num_indices;
    uint32_t num_submeshes;
    uint32_t vertex_stride;
    uint32_t vertices_data_size; // total vertices data size for all streams
    uint32_t indices_data_size;
    uint32_t flags; // 0 - interleaved stream, 1 - separate streams
    uint32_t vertex_stream_stride_size[MAX_VERTEX_ATTRIBUTE_STREAMS];
    const uint8_t* vertices[MAX_VERTEX_ATTRIBUTE_STREAMS];
    const uint8_t* indices;

    sapphire_sub_mesh_gpu_load_t sub_meshes[MAX_SUB_MESHES];

    sp_vec3_t bounding_box_min;
    sp_vec3_t bounding_box_max;
    sp_vec3_t bounding_sphere_center;
    float bounding_sphere_radius;

} sapphire_mesh_gpu_load_t;



typedef struct sapphire_sub_mesh_t
{
    uint32_t indices_count;
    uint32_t indices_start;
    sp_mat_handle_t material_handle;
    
} sapphire_sub_mesh_t;

typedef struct sapphire_mesh_t
{    
    uint32_t num_submeshes;    
    sp_vb_handle_t vb_handle;
    sp_ib_handle_t ib_handle;
    
    sapphire_sub_mesh_t sub_meshes[MAX_SUB_MESHES];

    sp_vec3_t bounding_box_min;
    sp_vec3_t bounding_box_max;
    sp_vec3_t bounding_sphere_center;
    float bounding_sphere_radius;

} sapphire_mesh_t;

#define MAX_RENDERING_MESHES 1024
#define MAX_RENDERING_OBJECTS 0xFFFF

typedef struct sapphire_renderer_t
{
    sapphire_mesh_t meshes[MAX_RENDERING_MESHES];
    uint32_t num_meshes;
    // render objects
    sp_mat4x4_t world_matrices[MAX_RENDERING_OBJECTS];
    sp_mesh_handle_t mesh_handles[MAX_RENDERING_OBJECTS];
    uint32_t num_render_objects;
    

} sapphire_renderer_t;




#define MATERIAL_MAX_PATH_LEN 64
#define MATERIAL_MAX_NAME_LEN 64

// holds material defintion as loaded from disk
// used to genereaet runtime material data structures
typedef struct sp_material_def_t
{
    char name[MATERIAL_MAX_NAME_LEN];
    char albedo_map[MATERIAL_MAX_PATH_LEN];
    char arm_map[MATERIAL_MAX_PATH_LEN];
    char normal_map[MATERIAL_MAX_PATH_LEN];
    sp_strhash_t name_hash;
    sp_strhash_t albedo_texture_hash;
    sp_strhash_t normal_texture_hash;
    sp_strhash_t arm_texture_hash;


    uint64_t flags;
} sp_material_def_t;

// Bit flags that turn on JSON parser extensions. Use `0` for standard JSON.
enum sp_material_flags {

    SP_MATERIAL_DOUBLE_SIDED = 0x1,
    SP_MATERIAL_ALPHA_TEST = 0x2,
    SP_MATERIAL_BLEND_MODE_OPAQUE = 0x4,
    SP_MATERIAL_BLEND_MODE_TRANSPARENT = 0x8,
    SP_MATERIAL_TEXTURE_ADDRESS_MODE_WRAP = 0x100,
    SP_MATERIAL_TEXTURE_ADDRESS_MODE_CLAMP = 0x200,


    SP_MATERIAL_STATE_DEPTH_TEST_ENABLED = 0x400,
    SP_MATERIAL_STATE_DEPTH_WRITE_ENABLED = 0x800,
};

#define MAX_MATERIAL_TEXTURE_VIEWS 3



typedef struct sp_mat_gpu_resources_t
{
    IPipelineState* p_pso;
    IShaderResourceBinding* p_srb;
} sp_mat_gpu_resources_t;

typedef struct sp_material_t
{
    IPipelineState* p_pso;
    IShaderResourceBinding* p_srb;
    ITextureView* texture_views[MAX_MATERIAL_TEXTURE_VIEWS];
} sp_material_t;

typedef struct sapphire_materials_manager_t
{
    sp_allocator_i* allocator;
    // array of materials
    sp_material_t* materials_arr;
    // look up material index by name
    struct SP_HASH_T(sp_strhash_t, uint32_t) material_name_lookup;
    // Pipeline state hash to pso lookup
    struct SP_HASH_T(uint64_t, sp_mat_gpu_resources_t) pso_srb_lookup;

} sapphire_materials_manager_t;

#define MAX_VERTEX_BUFFERS 1024
#define MAX_INDEX_BUFFERS 1024

typedef struct sapphire_buffers_manager
{
    IBuffer* vertex_buffers[MAX_VERTEX_BUFFERS];
    IBuffer* index_buffers[MAX_INDEX_BUFFERS];
    uint32_t num_vertex_buffers;
    uint32_t num_index_buffers;
} sapphire_buffers_manager_t;


#define FILE_MAX_PATH_LEN 64
#define MATERIAL_MAX_NAME_LEN 64


typedef struct sapphire_textures_manager_t
{
    sp_allocator_i* allocator;
    ITextureView** textures_arr;
    struct SP_HASH_T(sp_strhash_t, uint32_t) texture_path_lookup;
}sapphire_textures_manager_t;

typedef struct rendering_context_t
{
    IRenderDevice* p_device;
    ISwapChain* p_swap_chain;
    // render target gpu objects
    ITextureView* p_color_rtv;
    ITextureView* p_depth_rtv;
    IPipelineState* p_rt_pso;
    IShaderResourceBinding* p_rt_srb;
    sapphire_renderer_t renderer;
    sapphire_materials_manager_t materials_manager;
    sapphire_textures_manager_t textures_manager;
    sapphire_buffers_manager_t buffers_manager;

    IBuffer* cb_camera_attribs;
    IBuffer* cb_lights_attribs;
    IBuffer* cb_drawcall;

    // picking
    IBuffer* picking_buffer;
    IBuffer* picking_staging_buffer;

} rendering_context_t;



typedef struct viewer_t viewer_t;

typedef struct scene_def_t scene_def_t;

rendering_context_t* rendering_context_create(IRenderDevice* p_device, ISwapChain* p_swap_chain);
void rendering_context_destroy(rendering_context_t* p_rendering_context);

void renderer_do_rendering(IDeviceContext* pContext, viewer_t* viewer);
void renderer_window_resize(IRenderDevice* pDevice, ISwapChain* pSwapChain, uint32_t width, uint32_t height);

// TODO - remove from here
void load_materials(const char* materials_file, sp_material_def_t** p_materials_arr, sp_allocator_i* mats_allocator);
void scene_load_resources(const char* root_path_str, scene_def_t* p_scene_def, IRenderDevice* p_device);
void material_manager_add_materials(sapphire_materials_manager_t* manager, sp_material_def_t* materials_def_arr);
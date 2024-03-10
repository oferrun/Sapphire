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
#include "scene.h"

void sapphire_render(IDeviceContext* pContext);
void sapphire_init(IRenderDevice* p_device, ISwapChain* p_swap_chain);
void sapphire_destroy();
void sapphire_window_resize(IRenderDevice* pDevice, ISwapChain* pSwapChain, uint32_t width, uint32_t height);
void sapphire_update(double curr_time, double elapsed_time);







////
bool read_file_stream(const char* file, sp_temp_allocator_i* ta, uint8_t** p_stream, uint64_t* out_size);
bool read_grimrock_model_from_stream(const uint8_t* p_stream, uint64_t size, sapphire_mesh_gpu_load_t* p_mesh_load);




/////
typedef struct viewer_t
{
    //tm_vec3_t damped_translation;
    //float translation_speed;
    sp_transform_t camera_transform;
    sp_camera_t camera;
    sp_mat4x4_t view_projection;
    //sp_mat4x4_t last_view_projection;

} viewer_t;

static viewer_t g_viewer;
static rendering_context_t* g_rendering_context_o;






// called when window is resized
// recreates the render target for the new window size
void sapphire_window_resize(IRenderDevice* pDevice, ISwapChain* pSwapChain, uint32_t width, uint32_t height)
{
    renderer_window_resize(pDevice, pSwapChain, width, height);         
}





inline bool get_attribute_as_bool(sp_config_i* config, sp_config_item_t item, sp_strhash_t hash)
{
    sp_config_item_t attrib = config->object_get(config->inst, item, hash);    
    return attrib.type == SP_CONFIG_TYPE_TRUE;
}


void sapphire_destroy()
{
    rendering_context_destroy(g_rendering_context_o);
    
}

void sapphire_update(double curr_time, double elapsed_time)
{

}


// Render a frame
void sapphire_render(IDeviceContext* pContext)
{
    renderer_do_rendering(pContext, &g_viewer);
}


void sapphire_init(IRenderDevice* p_device, ISwapChain* p_swap_chain)
{
    scene_def_t scene_def;
    memset(&scene_def, 0, sizeof(scene_def));
    sp_allocator_i* allocator = sp_allocator_api->system_allocator;

    
    SP_INIT_TEMP_ALLOCATOR_WITH_ADAPTER(ta, a);

   
    g_rendering_context_o = rendering_context_create(p_device, p_swap_chain);

    g_viewer = (viewer_t){ .camera = {.near_plane = 0.1f, .far_plane = 100.f, .vertical_fov =  (SP_PI / 4.0f) } };
    viewer_t* viewer = &g_viewer;

    viewer->camera_transform = (sp_transform_t){ .position = {0, 1, -15}, .rotation = {0,0,0,1}, .scale = {1,1,1} };

    const SwapChainDesc* pSCDesc = ISwapChain_GetDesc(p_swap_chain);
    float aspect = (float)pSCDesc->Width / (float)pSCDesc->Height;
    
    // calculate view from transform
    sp_camera_api->view_from_transform(&viewer->camera.view[SP_CAMERA_TRANSFORM_DEFAULT], &viewer->camera_transform);
    // calculate projection matrix
    sp_camera_api->projection_from_fov(&viewer->camera.projection[SP_CAMERA_TRANSFORM_DEFAULT], viewer->camera.near_plane, viewer->camera.far_plane, viewer->camera.vertical_fov, aspect);

    sp_mat4x4_mul(&viewer->view_projection, &viewer->camera.view[SP_CAMERA_TRANSFORM_DEFAULT], &viewer->camera.projection[SP_CAMERA_TRANSFORM_DEFAULT]);


    sp_material_def_t* mat_defs_array = NULL;
    
    // load materials definitions from file    
    load_materials("C:/Programming/Sapphire/assets/materials.mat" , &mat_defs_array, allocator);
    // create GPU resources for materials
    material_manager_add_materials(&g_rendering_context_o->materials_manager, mat_defs_array);

    sp_array_free(mat_defs_array, allocator);

    scene_load_file("C:/Programming/Sapphire/assets/test.scene", &scene_def, allocator);
    scene_load_resources("C:/Programming/Sapphire/assets", &scene_def, p_device);
    scene_free(&scene_def, allocator);

#if 0
   
    
    
#if 1
    sapphire_mesh_gpu_load_t mesh_load_data;
    sapphire_mesh_gpu_load_t mesh_load_data2;
    const char* mesh_file = "C:/Programming/Sapphire/assets/dungeon_wall_01.model";
    const char* mesh_file2 = "C:/Programming/Sapphire/assets/barrel_crate_block.model";
    
    uint8_t* stream = NULL;
    uint64_t size;
    read_file_stream(mesh_file, ta, &stream, &size);
    read_grimrock_model_from_stream(stream, size, &mesh_load_data);

    read_file_stream(mesh_file2, ta, &stream, &size);
    read_grimrock_model_from_stream(stream, size, &mesh_load_data2);
#else
    // cube test vertices

    typedef struct Vertex
    {
        sp_vec3_t pos;
        sp_vec3_t normal;
        sp_vec2_t uv;
    } Vertex;

    Vertex CubeVerts[] =
    {
        {.pos = {-1,-1,-1}, .normal = {0, 0, -1}, .uv = {0,1}},
        {.pos = {-1,+1,-1}, .normal = {0, 0, -1},.uv = {0,0}},
        {.pos = {+1,+1,-1}, .normal = {0, 0, -1},.uv = {1,0}},
        {.pos = {+1,-1,-1}, .normal = {0, 0, -1},.uv = {1,1}},

        {.pos = {-1,-1,-1}, .normal = {0, -1, 0},.uv = {0,1}},
        {.pos = {-1,-1,+1}, .normal = {0, -1, 0},.uv = {0,0}},
        {.pos = {+1,-1,+1}, .normal = {0, -1, 0},.uv = {1,0}},
        {.pos = {+1,-1,-1}, .normal = {0, -1, 0},.uv = {1,1}},

        {.pos = {+1,-1,-1}, .normal = {1, 0, 0},.uv = {0,1} },
        {.pos = {+1,-1,+1}, .normal = {1, 0, 0},.uv = {1,1}},
        {.pos = {+1,+1,+1}, .normal = {1, 0, 0},.uv = {1,0}},
        {.pos = {+1,+1,-1}, .normal = {1, 0, 0},.uv = {0,0}},

        {.pos = {+1,+1,-1}, .normal = {0, 1, 0}, .uv = {0,1}},
        {.pos = {+1,+1,+1}, .normal = {0, 1, 0},.uv = {0,0}},
        {.pos = {-1,+1,+1}, .normal = {0, 1, 0},.uv = {1,0}},
        {.pos = {-1,+1,-1}, .normal = {0, 1, 0},.uv = {1,1}},

        {.pos = {-1,+1,-1}, .normal = {-1, 0, 0},.uv = {1,0}},
        {.pos = {-1,+1,+1}, .normal = {-1, 0, 0},.uv = {0,0}},
        {.pos = {-1,-1,+1}, .normal = {-1, 0, 0},.uv = {0,1}},
        {.pos = {-1,-1,-1}, .normal = {-1, 0, 0},.uv = {1,1}},

        {.pos = {-1,-1,+1}, .normal = {0, 0, 1},.uv = {1,1}},
        {.pos = {+1,-1,+1}, .normal = {0, 0, 1},.uv = {0,1}},
        {.pos = {+1,+1,+1}, .normal = {0, 0, 1},.uv = {0,0}},
        {.pos = {-1,+1,+1}, .normal = {0, 0, 1},.uv = {1,0}}
    };

    Uint32 Indices[] =
    {
        2,0,1,    2,3,0,
        4,6,5,    4,7,6,
        8,10,9,   8,11,10,
        12,14,13, 12,15,14,
        16,18,17, 16,19,18,
        20,21,22, 20,22,23
    };
    memset(&mesh_load_data, 0, sizeof(mesh_load_data));
    mesh_load_data.num_indices = 36;
    mesh_load_data.num_submeshes = 1;
    mesh_load_data.num_vertices = 24;
    mesh_load_data.vertex_stride = 32;
    mesh_load_data.vertices_data_size = mesh_load_data.num_vertices * mesh_load_data.vertex_stride;
    mesh_load_data.indices_data_size = sizeof(uint32_t) * mesh_load_data.num_indices;
    mesh_load_data.indices = (const uint8_t*)Indices;
    mesh_load_data.vertices[0] = (const uint8_t*)CubeVerts;
    mesh_load_data.vertex_stream_stride_size[0] = mesh_load_data.vertex_stride;
    mesh_load_data.sub_meshes[0].indices_start = 0;
    mesh_load_data.sub_meshes[0].indices_count = 36;

    sp_strhash_t mat_hash = sp_murmur_hash_string("test_mat_1");
    mesh_load_data.sub_meshes[0].material_hash = mat_hash; 

    
#endif
    sp_mesh_handle_t mesh_handle = load_mesh_to_gpu(p_device, g_rendering_context_o, &mesh_load_data);
    sp_mesh_handle_t mesh_handle2 = load_mesh_to_gpu(p_device, g_rendering_context_o, &mesh_load_data2);
    // add test instances
    sp_vec3_t axis = { 0, 1, 0 };
    sp_vec4_t q = sp_quaternion_from_rotation(axis, 0);
    sp_mat4x4_t inst_mat;
    sp_mat4x4_from_quaternion(&inst_mat, q);
    inst_mat.wz = 0.0f;
    inst_mat.wx = 0.0f;
    g_rendering_context_o->renderer.mesh_handles[0] = mesh_handle;
    g_rendering_context_o->renderer.world_matrices[0] = inst_mat;
    g_rendering_context_o->renderer.mesh_handles[1] = mesh_handle2;
    inst_mat.wz = 0.0f;
    inst_mat.wx = 2.0f;
    g_rendering_context_o->renderer.world_matrices[1] = inst_mat;
    g_rendering_context_o->renderer.num_render_objects = 2;

#endif

    SP_SHUTDOWN_TEMP_ALLOCATOR(ta);

}



void testcimgui()
{
	//input_scheme_t is = {.scheme_id = "oferdklfjsd;fjs"};

    //sp_allocator_i* allocator = sp_allocator_api->system_allocator;
    //material_manager_t mm = { .material_name_lookup = {.allocator = allocator } };
    //uint64_t hash_key = sp_murmur_hash_string("xadvance");
    //sp_hash_add(&mm.material_name_lookup, hash_key, 10);
    //uint32_t val = sp_hash_get(&mm.material_name_lookup, hash_key);
    //printf("%d", val);
    
    
	ImVec2 v = { 0 };
	if (im_Button("kaki", v))
	{
		//printf(is.scheme_id);
	}
}
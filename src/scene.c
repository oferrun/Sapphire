#include "core/sapphire_types.h"
#include "core/sapphire_math.h"
#include "core/json.h"
#include "core/config.h"
#include "core/allocator.h"
#include "core/temp_allocator.h"
#include "core/os.h"
#include "core/murmurhash64a.h"
#include "core/array.h"
#include "core/sapphire_math.h"
#include "scene.h"
#include "config_utils.h"

#include <memory.h>



const char* read_file(const char* file, sp_temp_allocator_i* ta);

void get_attribute_as_string(sp_config_i* config, sp_config_item_t item, sp_strhash_t hash, char* buffer);
inline double get_attribute_as_number(sp_config_i* config, sp_config_item_t item, sp_strhash_t hash);

void get_attribute_as_vec3(sp_config_i* config, sp_config_item_t item, sp_strhash_t hash, sp_vec3_t* vec3)
{
    sp_config_item_t vec3_item = config->object_get(config->inst, item, hash);
    sp_config_item_t* float_array = NULL;
    uint32_t num_floats = config->to_array(config->inst, vec3_item, &float_array);
    
    if (num_floats == 3)
    {
        sp_config_item_t float_item;
        float_item = float_array[0];
        vec3->x = (float)config->to_number(config->inst, float_item);
        float_item = float_array[1];
        vec3->y = (float)config->to_number(config->inst, float_item);
        float_item = float_array[2];
        vec3->z = (float)config->to_number(config->inst, float_item);
    }
}

void scene_free(scene_def_t* p_scene_def, sp_allocator_i* allocator)
{
    sp_array_free(p_scene_def->entities_def_arr, allocator);
    sp_array_free(p_scene_def->instances_arr, allocator);
}

void scene_load_file(const char* scene_file, scene_def_t* p_scene_def, sp_allocator_i* allocator)
{
    //SP_INIT_TEMP_ALLOCATOR(ta);
    SP_INIT_TEMP_ALLOCATOR_WITH_ADAPTER(ta, a);

    const char* text = read_file(scene_file, ta);

    char error[256];
    const uint32_t parse_flags = SP_JSON_PARSE_EXT_ALLOW_UNQUOTED_KEYS | SP_JSON_PARSE_EXT_ALLOW_COMMENTS | SP_JSON_PARSE_EXT_IMPLICIT_ROOT_OBJECT | SP_JSON_PARSE_EXT_OPTIONAL_COMMAS | SP_JSON_PARSE_EXT_EQUALS_FOR_COLON;
    sp_config_i* scene_config = sp_config_api->create(a);
    bool res = sp_json_api->parse(text, scene_config, parse_flags, error);
    if (!res)
    {
        return;
    }

    sp_config_item_t root = scene_config->root(scene_config->inst);
    sp_strhash_t s_name_hash = sp_murmur_hash_string("name");
    sp_strhash_t s_model_hash = sp_murmur_hash_string("model");
//    sp_strhash_t s_predefined_shape_hash = sp_murmur_hash_string("shape");
    sp_strhash_t s_material_hash = sp_murmur_hash_string("material");
    sp_strhash_t s_entities_hash = sp_murmur_hash_string("entities");
    sp_strhash_t s_instances_hash = sp_murmur_hash_string("instances");
    sp_strhash_t s_entity_hash = sp_murmur_hash_string("entity");
    sp_strhash_t s_transform_hash = sp_murmur_hash_string("transform");
    sp_strhash_t s_position_hash = sp_murmur_hash_string("position");
    sp_strhash_t s_rotation_hash = sp_murmur_hash_string("rotation");
    sp_strhash_t s_scale_hash = sp_murmur_hash_string("scale");
    sp_strhash_t s_flags_hash = sp_murmur_hash_string("flags");
    

    
    sp_config_item_t entities = scene_config->object_get(scene_config->inst, root, s_entities_hash);
    sp_config_item_t* entities_array = NULL;
    uint32_t num_entities = scene_config->to_array(scene_config->inst, entities, &entities_array);
    
    sp_array_ensure(p_scene_def->entities_def_arr, num_entities, allocator);    
    char entity_name_id[64];
    
    for (uint32_t i = 0; i < num_entities; ++i)
    {
        sp_config_item_t entity_item = entities_array[i];
        entity_def_t* p_entity_def = &p_scene_def->entities_def_arr[i];
        p_entity_def->flags = 0;
        get_attribute_as_string(scene_config, entity_item, s_name_hash, entity_name_id);
        p_entity_def->entity_hash = sp_murmur_hash_string(entity_name_id);
        get_attribute_as_string(scene_config, entity_item, s_model_hash, p_entity_def->model_file);
        get_attribute_as_string(scene_config, entity_item, s_material_hash, p_entity_def->material_file);
        p_entity_def->flags = (uint32_t)get_attribute_as_number(scene_config, entity_item, s_flags_hash);
    
    }
    sp_array_header(p_scene_def->entities_def_arr)->size = num_entities;

    sp_config_item_t instances = scene_config->object_get(scene_config->inst, root, s_instances_hash);
    sp_config_item_t* instances_array = NULL;
    uint32_t num_instances = scene_config->to_array(scene_config->inst, instances, &instances_array);
    sp_array_ensure(p_scene_def->instances_arr, num_instances, allocator);
    for (uint32_t i = 0; i < num_instances; ++i)
    {
        sp_config_item_t instance_item = instances_array[i];
        entity_instance_t* p_instance = &p_scene_def->instances_arr[i];

        get_attribute_as_string(scene_config, instance_item, s_entity_hash, entity_name_id);
        p_instance->entity_hash = sp_murmur_hash_string(entity_name_id);
        sp_config_item_t transform_item = scene_config->object_get(scene_config->inst, instance_item, s_transform_hash);
        sp_vec3_t position;
        sp_vec3_t rotation;
        sp_vec3_t scale;
        get_attribute_as_vec3(scene_config, transform_item, s_position_hash, &position);
        get_attribute_as_vec3(scene_config, transform_item, s_rotation_hash, &rotation);
        get_attribute_as_vec3(scene_config, transform_item, s_scale_hash, &scale);
        p_instance->transform.position = position;
        p_instance->transform.scale = scale;
        // convert angles to radians, than convert euler angles to quaternion
#define DEG_TO_RAD(a) ((a) * SP_PI / 180.0f)
        rotation.x = DEG_TO_RAD(rotation.x);
        rotation.y = DEG_TO_RAD(rotation.y);
        rotation.z = DEG_TO_RAD(rotation.z);
        p_instance->transform.rotation = sp_euler_to_quaternion(rotation);      
    }
    sp_array_header(p_scene_def->instances_arr)->size = num_instances;
    


    SP_SHUTDOWN_TEMP_ALLOCATOR(ta);
}

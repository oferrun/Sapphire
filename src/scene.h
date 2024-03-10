#pragma once

#include "core/sapphire_types.h"

#ifndef FILE_MAX_PATH_LEN
#define FILE_MAX_PATH_LEN 64
#endif

typedef struct entity_instance_t
{
    sp_strhash_t entity_hash;
    sp_transform_t transform;
} entity_instance_t;

typedef struct entity_def_t
{
    sp_strhash_t entity_hash;
    char model_file[FILE_MAX_PATH_LEN];
    char material_file[FILE_MAX_PATH_LEN];
    uint32_t flags; // use predefined shape = 1
} entity_def_t;

typedef struct scene_def_t
{
    entity_def_t*       entities_def_arr;
    entity_instance_t*  instances_arr;
} scene_def_t;

void scene_load_file(const char* scene_file, scene_def_t* p_scene_def, sp_allocator_i* allocator);
void scene_free(scene_def_t* p_scene_def, sp_allocator_i* allocator);
#pragma once

inline void get_attribute_as_string(sp_config_i* config, sp_config_item_t item, sp_strhash_t hash, char* buffer)
{
    sp_config_item_t attrib = config->object_get(config->inst, item, hash);
    if (attrib.type != sp_config_api->c_null.type)
    {
        const char* str = config->to_string(config->inst, attrib);
        memcpy(buffer, str, strlen(str) + 1);
    }

}

inline bool get_attribute_as_bool(sp_config_i* config, sp_config_item_t item, sp_strhash_t hash)
{
    sp_config_item_t attrib = config->object_get(config->inst, item, hash);
    return attrib.type == SP_CONFIG_TYPE_TRUE;
}

inline double get_attribute_as_number(sp_config_i* config, sp_config_item_t item, sp_strhash_t hash)
{
    sp_config_item_t attrib = config->object_get(config->inst, item, hash);
    if (attrib.type != sp_config_api->c_null.type)
    {
        double num = config->to_number(config->inst, attrib);
        return num;
    }
    return 0;

}
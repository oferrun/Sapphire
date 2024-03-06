


#include <memory.h>

#include "core/temp_allocator.h"
#include "core/os.h"
#include "core/hash.h"
#include "core/sapphire_macros.h"
#include "core/murmurhash64a.h"
#include "sapphire_renderer.h"

// A String is a variable length 8-bit string
typedef struct grim_string_t
{
	int32_t		length;
	char		data[64];
} grim_string_t;

// A FourCC is a four character code string used for headers
typedef struct four_cc_t
{
	uint8_t    data[4];
} four_cc_t;

// A Mat4x3 is a transformation matrix split into 3x3 rotation and 3D rotation parts
typedef struct mat_4x3_t
{
	sp_vec3_t    base_x;
	sp_vec3_t    base_y;
	sp_vec3_t    base_z;
	sp_vec3_t    translation;
} mat_4x3_t;

four_cc_t read_four_cc(const uint8_t** ptr) {
	four_cc_t fourcc;
	memcpy(fourcc.data, *ptr, 4);
	(*ptr) += 4; // Move pointer forward

	return fourcc;
}

grim_string_t read_string(const uint8_t** ptr) {
	const uint8_t* p_curr = *ptr;
	uint32_t length = *(const uint32_t*)(p_curr);
	p_curr += sizeof(uint32_t); // Move pointer forward

	grim_string_t str;


	str.length = length;
	uint32_t maxlength = sp_min(length, (uint32_t)sizeof(str.data) - 1);
	memcpy(str.data, p_curr, maxlength);
	str.data[maxlength] = '\0';
	p_curr += length; // Move pointer forward
	*ptr = p_curr;

	return str;
}

sp_vec3_t read_vec3(const uint8_t** ptr) {
	const uint8_t* p_curr = *ptr;

	sp_vec3_t vec;
	vec.x = *(const float*)(p_curr);
	p_curr += sizeof(float);
	vec.y = *(const float*)(p_curr);
	p_curr += sizeof(float);
	vec.z = *(const float*)(p_curr);
	p_curr += sizeof(float);
	*ptr = p_curr;
	return vec;
}

mat_4x3_t read_matrix4x3(const uint8_t** ptr) {
	mat_4x3_t mat;

	mat.base_x = read_vec3(ptr);
	mat.base_y = read_vec3(ptr);
	mat.base_z = read_vec3(ptr);
	mat.translation = read_vec3(ptr);
	return mat;
}

bool read_file_stream(const char* file, sp_temp_allocator_i* ta, uint8_t** p_stream, uint64_t* out_size)
{
    // Read text
    const sp_file_stat_t stat = sp_os_api->file_system->stat(file);
    const uint64_t size = stat.size;
    struct sp_os_file_io_api* io = sp_os_api->file_io;
	
    sp_file_o f = io->open_input(file);
	if (!f.valid) return false;

	uint8_t* stream = sp_temp_alloc(ta, size);

    io->read(f, stream, size);
    io->close(f);

	*p_stream = stream;
	*out_size = size;
	return true;
}

void read_mesh_segment(int32_t num_segments, sapphire_mesh_gpu_load_t* p_mesh_load, const uint8_t** ptr)
{
	for (int32_t i = 0; i < num_segments; ++i)
	{
		grim_string_t material = read_string(ptr);      // name of the material defined in Lua script
		sp_strhash_t mat_hash = sp_murmur_hash_string(material.data);
		
		//int32_t  primitiveType = *(reinterpret_cast<const int32_t*>(ptr)); // always two
		*ptr += sizeof(int32_t);
		int32_t  firstIndex = *(const int32_t*)(*ptr); // starting location in the index list
		*ptr += sizeof(int32_t);
		int32_t  count = *(int32_t*)(*ptr); // number of triangles
		*ptr += sizeof(int32_t);

		p_mesh_load->sub_meshes[i].indices_start = firstIndex;
		// count is triangles - each triangle has 3 indices so multiple by 3
		p_mesh_load->sub_meshes[i].indices_count = count * 3;
		p_mesh_load->sub_meshes[i].material_hash = mat_hash;
	}
}

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
void read_vertex_array(int32_t num_vertices, int32_t index, sapphire_mesh_gpu_load_t* p_mesh_load, const uint8_t** ptr)
{
	// 0 = byte, 1 = int16, 2 = int32, 3 = float32
	int32_t dataType = *(const int32_t*)(*ptr);
	(void)dataType;
	*ptr += sizeof(int32_t);
	// dimensions of the data type (2-4)
	int32_t dim = *(const int32_t*)(*ptr);
	(void)dim;
	*ptr += sizeof(int32_t);
	// byte offset from vertex to vertex
	int32_t stride = *(const int32_t*)(*ptr);
	*ptr += sizeof(int32_t);

	int32_t size_vertex_data = num_vertices * stride;
	if (size_vertex_data > 0)
	{
		p_mesh_load->vertices[index] = *ptr;
		p_mesh_load->vertex_stream_stride_size[index] = stride;
		*ptr += size_vertex_data;
	}
	else
	{
		p_mesh_load->vertices[index] = NULL;
	}


}

void read_mesh_data(sapphire_mesh_gpu_load_t* p_mesh_load, const uint8_t** ptr)
{
	four_cc_t			magic = read_four_cc(ptr);
	(void)magic;
	//int32_t version = *(reinterpret_cast<const int32_t*>(ptr));
	*ptr += sizeof(int32_t);
	int32_t num_vertices = *(const int32_t*)(*ptr);
	*ptr += sizeof(int32_t);
	p_mesh_load->num_vertices = num_vertices;
	p_mesh_load->vertices_data_size = 0;
	p_mesh_load->flags = 1;
	p_mesh_load->vertex_stride = 0;
	for (uint32_t i = 0; i < 15; ++i)
	{
		read_vertex_array(num_vertices, i, p_mesh_load, ptr);
	}

	int32_t num_indices = *(const int32_t*)(*ptr);
	*ptr += sizeof(int32_t);
	p_mesh_load->num_indices = num_indices;
	// store pointer to indices
	p_mesh_load->indices = *ptr;

	int32_t indicesSize = num_indices * sizeof(int32_t);
	p_mesh_load->indices_data_size = indicesSize;
	*ptr += indicesSize;

	int32_t num_segments = *(const int32_t*)(*ptr);
	*ptr += sizeof(int32_t);
	p_mesh_load->num_submeshes = num_segments;
	read_mesh_segment(num_segments, p_mesh_load, ptr);

	sp_vec3_t			boundCenter = read_vec3(ptr);    // center of the bound sphere in model space

	float			boundRadius = *(const float*)(*ptr);   // radius of the bound sphere in model space
	*ptr += sizeof(float);

	sp_vec3_t			boundMin = read_vec3(ptr);       // minimum extents of the bound box in model space
	sp_vec3_t			boundMax = read_vec3(ptr);       // maximum extents of the bound box in model space

	p_mesh_load->bounding_box_min = boundMin;
	p_mesh_load->bounding_box_max = boundMax;
	p_mesh_load->bounding_sphere_center = boundCenter;
	p_mesh_load->bounding_sphere_radius = boundRadius;

}

void read_bones(int32_t num_bones, const uint8_t** ptr)
{
	for (int32_t i = 0; i < num_bones; ++i)
	{
		int32_t boneNodeIndex = *(const int32_t*)(*ptr);
		*ptr += sizeof(int32_t);

		mat_4x3_t		invRestMatrix = read_matrix4x3(ptr);   // transform from model space to bone space
		(void)invRestMatrix;
		if (boneNodeIndex)
		{
			continue;
		}
	}
}

void read_mesh_entity(sapphire_mesh_gpu_load_t* p_mesh_load, const uint8_t** ptr)
{
	read_mesh_data(p_mesh_load, ptr);
	const uint8_t* p_curr = *ptr;
	int32_t num_bones = *(const int32_t*)(p_curr);
	p_curr += sizeof(int32_t);
	read_bones(num_bones, &p_curr);
	read_vec3(&p_curr); // ignored
	//uint8_t castShadow = *ptr;
	p_curr += sizeof(uint8_t);
	*ptr = p_curr;
}

void grimrock_read_nodes(uint32_t num_nodes, sapphire_mesh_gpu_load_t* p_mesh_load, const uint8_t* ptr)
{
	const uint8_t* p_curr = ptr;

	for (uint32_t i = 0; i < num_nodes; ++i)
	{
		// read node
		grim_string_t name = read_string(&p_curr);
		(void)name;
		mat_4x3_t localToParent = read_matrix4x3(&p_curr);
		(void)localToParent;
		int32_t parent = *(const int32_t*)(&p_curr);
		(void)parent;
		p_curr += sizeof(int32_t);

		int32_t nodeType = *(const int32_t*)(p_curr);
		p_curr += sizeof(int32_t);
		if (nodeType == 0)
		{
			read_mesh_entity(p_mesh_load, &p_curr);
		}

		
	}
}

bool read_grimrock_model_from_stream(const uint8_t* p_stream, uint64_t size, sapphire_mesh_gpu_load_t* p_mesh_load)
{
	const uint8_t* p_curr = p_stream;
	four_cc_t four_cc = read_four_cc(&p_curr);
	(void)four_cc;

	int32_t version = *(const int32_t*)(p_curr);
	(void)version;
	p_curr += sizeof(int32_t);
	
	int32_t num_nodes = *(const int32_t*)(p_curr);
	p_curr += sizeof(int32_t);
	(void)num_nodes;
	grimrock_read_nodes(num_nodes, p_mesh_load, p_curr);
	
	return true;
}

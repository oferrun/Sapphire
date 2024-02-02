#include "BasicStructures.fxh"
#include "../src/grid.fxh"


cbuffer cbCameraAttribs
{
    CameraAttribs g_CameraAttribs;
};

cbuffer cbGridAttribs
{
    GridAttribs g_GridAttribs;
}

struct VSInput
{

    uint VertexId : SV_VertexID;
};


struct PSInput 
{ 
    float4 ClipPos : SV_POSITION;
    float2 tile_pos : WORLD_POS1;
    float2 grid_pos : WORLD_POS2;
    float2 camera_grid_pos : WORLD_POS3;
    float3 view_dir : WORLD_POS4;
    
};



// Note that if separate shader objects are not supported (this is only the case for old GLES3.0 devices), vertex
// shader output variable name must match exactly the name of the pixel shader input variable.
// If the variable has structure type (like in this example), the structure declarations must also be identical.
void main(in VSInput VSIn,
          out PSInput PSIn) 
{
    bool infinite = g_GridAttribs.infinite ? true : false;

    static float2 vertices[] =
    {
        { 1.0, -1.f },
        { -1.0, -1.f },
        { -1.0, 1.f },
        { 1.0, -1.f },
        { -1.0, 1.f },
        { 1.0, 1.f }
        
    };
    
    

    
    float grid_extent = g_GridAttribs.grid_extent;

    // Note: grid_tile_size must match tile size specfied in grid.c. 
    const float grid_tile_size = 100.f;
    uint num_tiles_per_axis = ceil((grid_extent * 2.f) / grid_tile_size);
    uint tile_idx = floor(VSIn.VertexId / 6);
    // tile pos in world space
    float2 tile_pos = (float2(tile_idx % num_tiles_per_axis, tile_idx / num_tiles_per_axis) - (num_tiles_per_axis / 2)) * grid_tile_size;    
    PSIn.tile_pos = tile_pos;
    
    float3 camera_grid_pos = g_CameraAttribs.f4Position.xyz;
    float2 grid_uv = float2(vertices[VSIn.VertexId % 6].x * grid_tile_size * 0.5f, vertices[VSIn.VertexId % 6].y * grid_tile_size * 0.5f);
    PSIn.grid_pos = infinite ? grid_uv : grid_uv + tile_pos;
    
    //float4 v = float4(VSIn.Pos, 1); //
    float4 v = float4(tile_pos.x + grid_uv.x, 0, tile_pos.y + grid_uv.y, 1);
    //float4 v = float4(vertices[VSIn.VertexId].x, 0, vertices[VSIn.VertexId].y, 1);
    v.y -= infinite ? camera_grid_pos.y : 0;
    
    PSIn.camera_grid_pos = camera_grid_pos.xz + tile_pos.xy;
    PSIn.view_dir = infinite ? float3(0, 0, 0) - v.xyz : (g_CameraAttribs.f4Position.xyz - v.xyz);
    
    float4x4 view = g_CameraAttribs.mView;
    float3 vp = infinite ? mul(v.xyz, (float3x3) view).xyz : mul(v, view).xyz;
    float4 cp = mul(float4(vp, 1), g_CameraAttribs.mProj);
    
    
    
    
    
    PSIn.ClipPos = cp;
   
    

}

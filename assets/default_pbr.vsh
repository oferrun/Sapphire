#include "BasicStructures.fxh"

cbuffer cbTransforms
{
    float4x4 g_World;
    uint2 g_entityId;
    uint2 g_pad;
    
    
};

cbuffer cbCameraAttribs
{
    CameraAttribs g_CameraAttribs;
};


// Vertex shader takes two inputs: vertex position and uv coordinates.
// By convention, Diligent Engine expects vertex shader inputs to be 
// labeled 'ATTRIBn', where n is the attribute number.
struct VSInput
{
    float3 Pos : ATTRIB0;
    float3 Normal : ATTRIB1;
    float2 UV  : ATTRIB2;
    uint VertexID : SV_VertexID;
};

struct PSInput 
{ 
    float4 ClipPos : SV_POSITION;
    float3 WorldPos : WORLD_POS;
    float3 Normal : NORMAL;
    float2 UV  : TEX_COORD; 
};

struct GLTF_TransformedVertex
{
    float3 WorldPos;
    float3 Normal;
};

float3x3 InverseTranspose3x3(float3x3 M)
{
    // Note that in HLSL, M_t[0] is the first row, while in GLSL, it is the\n"
    // first column. Luckily, determinant and inverse matrix can be equally\n"
    // defined through both rows and columns.\n"
    float det = dot(cross(M[0], M[1]), M[2]);
    float3x3 adjugate = float3x3(cross(M[1], M[2]),
                                 cross(M[2], M[0]),
                                 cross(M[0], M[1]));
    return adjugate / det;
}

GLTF_TransformedVertex GLTF_TransformVertex(in float3    Pos,
                                            in float3    Normal,
                                            in float4x4  Transform)
{
    GLTF_TransformedVertex TransformedVert;

    float4 locPos = mul(Transform, float4(Pos, 1.0));
    float3x3 NormalTransform = float3x3(Transform[0].xyz, Transform[1].xyz, Transform[2].xyz);
    NormalTransform = InverseTranspose3x3(NormalTransform);
    Normal = mul(NormalTransform, Normal);
    float NormalLen = length(Normal);
    TransformedVert.Normal = Normal / max(NormalLen, 1e-5);

    TransformedVert.WorldPos = locPos.xyz / locPos.w;

    return TransformedVert;
}


// Note that if separate shader objects are not supported (this is only the case for old GLES3.0 devices), vertex
// shader output variable name must match exactly the name of the pixel shader input variable.
// If the variable has structure type (like in this example), the structure declarations must also be identical.
void main(in  VSInput VSIn,
          out PSInput PSIn) 
{
    GLTF_TransformedVertex TransformedVert = GLTF_TransformVertex(VSIn.Pos, VSIn.Normal, g_World);
    
    float4 Pos[4];
    Pos[0] = float4(-1.0, -1.0, 0.5, 1.0);
    Pos[1] = float4(-1.0, +1.0, 0.5, 1.0);
    Pos[2] = float4(+1.0, -1.0, 0.5, 1.0);
    Pos[3] = float4(+1.0, +1.0, 0.5, 1.0);

    
   
    
    //TransformedVert.WorldPos = Pos[VSIn.VertexID].xyz;
        
    // position in clipspace
   // PSIn.ClipPos = mul(float4(TransformedVert.WorldPos, 1.0), g_World);
    PSIn.ClipPos = mul(g_CameraAttribs.mViewProj, float4(TransformedVert.WorldPos, 1.0));
    //PSIn.ClipPos = mul(float4(TransformedVert.WorldPos, 1.0), g_CameraAttribs.mViewProj);
    //PSIn.ClipPos = Pos[VSIn.VertexID % 4];
   
    // position in world space
    PSIn.WorldPos = TransformedVert.WorldPos;
    // transformed normal
    PSIn.Normal = TransformedVert.Normal;
    PSIn.UV  = VSIn.UV;
}

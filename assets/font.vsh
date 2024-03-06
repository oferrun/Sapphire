#include "BasicStructures.fxh"

cbuffer Constants
{
    float4 g_color;
    float g_rt_width;
    float g_rt_height;
    float2 padding;
};


// Vertex shader takes two inputs: vertex position and uv coordinates.
// By convention, Diligent Engine expects vertex shader inputs to be 
// labeled 'ATTRIBn', where n is the attribute number.
struct VSInput
{
    float2 Pos : ATTRIB0;    
    float2 UV  : ATTRIB1;
};

struct PSInput 
{ 
    float4 ClipPos : SV_POSITION;        
    float2 UV  : TEX_COORD; 
    float4 Color : COLOR0;
};




// Note that if separate shader objects are not supported (this is only the case for old GLES3.0 devices), vertex
// shader output variable name must match exactly the name of the pixel shader input variable.
// If the variable has structure type (like in this example), the structure declarations must also be identical.
void main(in  VSInput VSIn,
          out PSInput PSIn) 
{
    float2 pos = VSIn.Pos;
    pos.x /= g_rt_width;
    pos.y = g_rt_height - pos.y;
    pos.y /= g_rt_height;
    pos *= 2.0;
    pos -= 1.0;
    
    
    PSIn.ClipPos = float4(pos.x, pos.y, 0.0, 1.0);
    PSIn.UV  = VSIn.UV;
    PSIn.Color = g_color;

}

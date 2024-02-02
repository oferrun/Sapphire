struct GridAttribs
{
    float4 origin_axis_x_color;
    float4 origin_axis_z_color;
    float4 thin_lines_color;
    float4 thick_lines_color;
    float grid_extent;
    float cell_size;
    int infinite;

#ifdef __cplusplus
    
    
    
    
#else
    
#endif

    
};
#ifdef CHECK_STRUCT_ALIGNMENT
CHECK_STRUCT_ALIGNMENT(CameraAttribs);
#endif
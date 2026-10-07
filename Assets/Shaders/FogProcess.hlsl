cbuffer Constants : register(b0)
{
    row_major matrix inv_view_proj;
    row_major matrix view;
    float3 view_position;
    float fog_density;
    float4 fog_incattering_color;
    float fog_hieght_fall_off;
    float start_distance;
    float fog_cutoff_distance;
    float fog_max_opacity;
    float fog_height;
    float3 padding;
}

struct PS_INPUT
{
	float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

Texture2D color_texture : register(t0);
Texture2D depth_texture : register(t1);

SamplerState default_sampler : register(s0);

// Vertex Shader
PS_INPUT mainVS(uint vertex_id : SV_VertexID)
{
    PS_INPUT output;
	
    float2 full_screen_quad[4] =
    {
        float2(-1.0, 1.0),
		float2(1.0, 1.0),
		float2(1.0, -1.0),
		float2(-1.0, -1.0)
    };
    
    float2 uv_coords[4] =
    {
        float2(0.0, 0.0),
        float2(1.0, 0.0),
        float2(1.0, 1.0),
        float2(0.0, 1.0)
    };
    
    uint indices[] = { 0, 1, 2, 0, 2, 3 }; // Two triangles to form a quad
	
    output.position = float4(full_screen_quad[indices[vertex_id]], 0.0, 1.0);
    output.uv = uv_coords[indices[vertex_id]];

    return output;
}

// Pixel Shader
float4 mainPS(PS_INPUT input) : SV_TARGET
{
    const int sample_count = 8;
    
    float4 color = color_texture.Sample(default_sampler, input.uv);
    float depth = depth_texture.Sample(default_sampler, input.uv).r;
	
    // D3D의 깊이 범위 0~1, row_major 행렬과 mul(vector, matrix) 기준
    float4 clip_position = float4(input.uv.x * 2.0 - 1.0, 1.0 - input.uv.y * 2.0, depth, 1.0);

    float4 world_position = mul(clip_position, inv_view_proj);
    world_position.xyz /= world_position.w;
    float4 pixel_view_position = mul(world_position, view);
    
    float3 V = world_position.xyz - view_position;
    
    float distance_to_camera = length(V);
    if (distance_to_camera <= max(start_distance, 0.0) || distance_to_camera < 0.0001 || pixel_view_position.z > fog_cutoff_distance)
    {
        return color;
    }
    
    V /= distance_to_camera;
    
    float start = clamp(start_distance, 0.0, distance_to_camera);
    float dist = distance_to_camera - start;
    
    float3 origin = view_position + V * start;
    float segment_length = dist / float(sample_count);
    
    float optical_depth = 0;
    for (int i = 0; i < sample_count; ++i)
    {
        float t = (float(i) + 0.5) / float(sample_count);
        float3 sample_position = lerp(origin, world_position.xyz, t);
        
        float density_at_height = fog_density * exp(-fog_hieght_fall_off * (sample_position.z - fog_height));
        float segment_depth = density_at_height * segment_length;
        optical_depth += segment_depth;
    }
    
    float transmittance = exp(-optical_depth);
    float fog_factor = 1.0 - transmittance;
    fog_factor = min(fog_factor, fog_max_opacity);
    
    float3 final_color = lerp(color.rgb, fog_incattering_color.rgb, fog_factor);
    
    return float4(final_color, color.a);
}

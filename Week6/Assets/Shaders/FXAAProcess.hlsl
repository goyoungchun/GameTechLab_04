cbuffer Constants : register(b0)
{
    float width;
    float height;
    float2 padding;
}

struct PS_INPUT
{
	float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

Texture2D color_texture : register(t0);

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

float get_luminance(float3 color)
{
    return dot(color, float3(0.299, 0.587, 0.114));
}

// Pixel Shader
float4 mainPS(PS_INPUT input) : SV_TARGET
{
    float2 texel_size = float2(1.0 / width, 1.0 / height);
    const float2 uvs[9] =
    {
        float2(-texel_size.x, -texel_size.y), // Top-left
        float2(0.0, -texel_size.y), // Top-center
        float2(texel_size.x, -texel_size.y), // Top-right
        float2(-texel_size.x, 0.0), // Middle-left
        float2(0.0, 0.0), // Center
        float2(texel_size.x, 0.0), // Middle-right
        float2(-texel_size.x, texel_size.y), // Bottom-left
        float2(0.0, texel_size.y), // Bottom-center
        float2(texel_size.x, texel_size.y) // Bottom-right
    };
    
    float4 texture_color = color_texture.Sample(default_sampler, input.uv);
    
    // 상하좌우 픽셀의 밝기를 통해 aa가 필요한지 판단.
    float luma_center = get_luminance(texture_color.rgb);
    float luma_t = get_luminance(color_texture.Sample(default_sampler, input.uv + uvs[1]).rgb);
    float luma_l = get_luminance(color_texture.Sample(default_sampler, input.uv + uvs[3]).rgb);
    float luma_r = get_luminance(color_texture.Sample(default_sampler, input.uv + uvs[5]).rgb);
    float luma_b = get_luminance(color_texture.Sample(default_sampler, input.uv + uvs[7]).rgb);
    
    float luma_min = min(luma_center, min(min(luma_t, luma_l), min(luma_r, luma_b)));   
    float luma_max = max(luma_center, max(max(luma_t, luma_l), max(luma_r, luma_b)));
    
    float luma_range = luma_max - luma_min;
    
    const float absolute_threshold = 0.0312;
    const float relative_threshold = 0.125;
    
    float threshold = max(absolute_threshold, relative_threshold * luma_max);
    
    bool needs = luma_range >= threshold;
    
    if (!needs)
    {
        return texture_color;
    }
    
    // 상하좌우 대각선 픽셀의 밝기를 통해 edge 방향을 판단.
    float luma_tl = get_luminance(color_texture.Sample(default_sampler, input.uv + uvs[0]).rgb);
    float luma_tr = get_luminance(color_texture.Sample(default_sampler, input.uv + uvs[2]).rgb);
    float luma_bl = get_luminance(color_texture.Sample(default_sampler, input.uv + uvs[6]).rgb);
    float luma_br = get_luminance(color_texture.Sample(default_sampler, input.uv + uvs[8]).rgb);
    
    float horizontal_edge = 
        abs(luma_tl + luma_bl - 2.0 * luma_l) +
        2.0 * abs(luma_t + luma_b - 2.0 * luma_center) +
        abs(luma_tr + luma_br - 2.0 * luma_r);
    
    float vertical_edge = 
        abs(luma_tl + luma_tr - 2.0 * luma_t) + 
        2.0 * abs(luma_l + luma_r - 2.0 * luma_center) +
        abs(luma_bl + luma_br - 2.0 * luma_b);
    
    bool is_horizontal = horizontal_edge >= vertical_edge;
    
    // 가장 큰 밝기 차이를 가진 사이드를 선택.
    float side1 = is_horizontal ? luma_t : luma_l;
    float side2 = is_horizontal ? luma_b : luma_r;
    
    float gardient1 = abs(side1 - luma_center);
    float gradient2 = abs(side2 - luma_center);
    
    bool use_side1 = gardient1 >= gradient2;
    
    // 선택된 사이드 방향으로 반 픽셀 이동한 uv 계산
    float2 side_uv_step = is_horizontal ? float2(0.0, texel_size.y) : float2(texel_size.x, 0.0);
    
    if (use_side1)
    {
        side_uv_step *= -1.0;
    }
    
    float2 edge_uv = input.uv + side_uv_step * 0.5;
    float selected_luma = use_side1 ? side1 : side2;
    float selected_gradient = use_side1 ? gardient1 : gradient2;
    float edge_luma = (luma_center + selected_luma) * 0.5;
    
    // 선택된 edge 방향으로 픽셀을 탐색하여 밝기 변화가 큰 지점을 찾음.
    float2 uv_step = is_horizontal ? float2(texel_size.x, 0.0) : float2(0.0, texel_size.y);
    
    float2 uv_negative = edge_uv;
    float delta_negative = 0.0;
    bool found_negative = false;
    
    float2 uv_positive = edge_uv;
    float delta_positive = 0.0;
    bool found_positive = false;
    
    float search_threshold = 0.25 * selected_gradient;
    
    const int max_search_steps = 12;  
    for (int i = 0; i < max_search_steps; ++i)
    {
        if (!found_negative)
        {
            uv_negative -= uv_step;
            
            float luma = get_luminance(color_texture.SampleLevel(default_sampler, uv_negative, 0).rgb);
            delta_negative = luma - edge_luma;
            found_negative = abs(delta_negative) >= search_threshold;
        }
        
        if (!found_positive)
        {
            uv_positive += uv_step;
            
            float luma = get_luminance(color_texture.SampleLevel(default_sampler, uv_positive, 0).rgb);
            delta_positive = luma - edge_luma;
            found_positive = abs(delta_positive) >= search_threshold;
        }
        
        if (found_negative && found_positive)
        {
            break;
        }
    }
    
    // 결과 색상 결정
    float dist_negative = is_horizontal ? input.uv.x - uv_negative.x : input.uv.y - uv_negative.y;
    float dist_positive = is_horizontal ? uv_positive.x - input.uv.x : uv_positive.y - input.uv.y;
    
    bool closer_to_negative = dist_negative <= dist_positive;
    
    float nearest_dist = min(dist_negative, dist_positive);
    float span_length = dist_negative + dist_positive;
    
    float edge_offst = 0.5 - nearest_dist / span_length;
    
    float nearest_delta = closer_to_negative ? delta_negative : delta_positive;
    
    bool nearest_found = closer_to_negative ? found_negative : found_positive;
    bool center_is_darker = luma_center < edge_luma;
    bool end_is_darker = nearest_delta < 0.0;
    
    bool valid_offset = nearest_found && (center_is_darker != end_is_darker);
    
    if (!valid_offset)
    {
        edge_offst = 0.0;
    }
    
    float2 final_uv = input.uv + side_uv_step * edge_offst;
    
    return color_texture.Sample(default_sampler, final_uv);
}

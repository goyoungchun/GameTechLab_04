#define PI 3.14159265359

struct LightInfo
{
    float3 position;
    int type; // 0: directional, 1: point, 2: spot
    float4 color;
    float range;
    float intensity;
    float falloff;
};

cbuffer ModelConstants : register(b0) // FConstants
{
	row_major matrix Model;
	float4 Color;
    float2 uv_offset;
	int UseVertexColor;
    int HasTexture;
    int light_count;
    int padding[3];
}

cbuffer ViewConstants : register(b1) // FConstants
{
	row_major matrix view_projection;
}

struct VS_INPUT
{
	float4 position : POSITION;
    float3 normal : NORMAL;
	float4 color : COLOR;
    float2 uv : TEXCOORD0;
};

struct PS_INPUT
{
	float4 position : SV_POSITION;
    float3 normal : NORMAL;
	float4 color : COLOR;
    float2 uv : TEXCOORD0;
    float3 world_position : TEXCOORD1;
};

Texture2D main_texture : register(t0);
StructuredBuffer<LightInfo> lights : register(t1);

SamplerState default_sampler : register(s0);

// Vertex Shader
PS_INPUT mainVS(VS_INPUT input)
{
	PS_INPUT output;
    
    float4 world_position = mul(input.position, Model);
    
    output.position = mul(world_position, view_projection);
    output.normal = normalize(mul(float4(input.normal, 0.0), Model).xyz);
	
	if (UseVertexColor != 0)
	{
		output.color = input.color;
	}
	else
	{
		output.color = Color;
	}
	
    output.uv = input.uv + uv_offset;
    
    output.world_position = world_position;
	
	return output;
}

// Pixel Shader
float4 mainPS(PS_INPUT input) : SV_TARGET
{
    float4 final_color = input.color;
    if (HasTexture != 0)
    {
        final_color *= main_texture.Sample(default_sampler, input.uv);
    }
    
    float3 N = normalize(input.normal);
	
    float3 light_color = float3(0.0, 0.0, 0.0);
    for (int i = 0; i < light_count; ++i)
    {
        LightInfo light = lights[i];
        
        if (light.type == 1)
        {
            float3 to_light = light.position - input.world_position;
            float dist = length(to_light);
            float3 L = to_light / dist;
            float NdotL = max(dot(N, L), 0.0);
            
            float fade = saturate(1.0 - dist / light.range);
            float attenuation = pow(fade, light.falloff);
            
            float3 diffuse = light.color.rgb * NdotL * light.intensity * attenuation;
            
            light_color += diffuse;
        }
    }
    
    final_color.rgb += light_color;
	
    return final_color;
}

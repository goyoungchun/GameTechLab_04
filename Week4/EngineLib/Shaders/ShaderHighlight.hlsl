
cbuffer MaskContants : register(b0)
{
    row_major float4x4 World;
    row_major float4x4 VieProjection;
};

cbuffer OutlineContants : register(b0)
{
    float4 OutlineColor;

    int2 ViewMin;
    int2 ViewMax;

    int RadiusPixels;
}

Texture2D<float> SelectionMask : register(t0);

struct VS_INPUT
{
    float3 position : POSITION;
};

struct PS_INPUT
{
    float4 position : SV_Position;
};


/* Mask Shader */
PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;

    output.position = mul(mul(float4(input.position.xyz, 1.0f), World), VieProjection);
    return output;
}

float mainPS(PS_INPUT input) : SV_TARGET
{
    return 1.0f;
}

/* Outline Shader */
// Called by Draw(3, 0) without vertex buffer
PS_INPUT outlineVS(uint vertexID : SV_VertexID)
{
    // Draw a triangle
    // (0, 0) -> (2, 0) -> (0, 2)
    float2 corner = float2(
        (vertexID << 1) & 2,
        vertexID & 2
    );

    // Convert to NDC space
    // (-1, 1) -> (3, 1) -> (-1, -3)
    PS_INPUT output;
    output.position = float4(
        corner * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f),
        0.0f,
        1.0f
    );

    return output;
}

float ReadMask(int2 pixel)
{
    if (any(pixel < ViewMin) || any(pixel >= ViewMax))
    {
        return 0.0f;
    }

    return SelectionMask.Load(int3(pixel, 0));
}

float4 outlinePS(PS_INPUT input) : SV_Target0
{
    int2 pixel = int2(input.position.xy);

    // Check if the pixel is in the selection mask
    if (ReadMask(pixel) > 0.5f)
    {
        discard;
    }

    int radius = max(RadiusPixels, 0);
    
    for (int y = -radius; y <= radius; ++y)
    {
        for (int x = -radius; x <= radius; ++x)
        {
            // Draw a outline if there is a selected pixel in the radius
            if (x * x + y * y > radius * radius)
                continue;

            if (ReadMask(pixel + int2(x, y)) > 0.5f)
                return OutlineColor;
        }

    }

    // Don't draw if there is no selected pixel in the radius
    discard;
    return float4(0, 0, 0, 0);
}

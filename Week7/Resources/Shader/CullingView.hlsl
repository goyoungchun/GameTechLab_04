// Reads the actual CS result. Constants come from FRenderer::RenderCullingView.
cbuffer CullingViewConstants : register(b0)
{
    uint TileCountX;
    uint TileSize;
    uint MaxLightsPerTile;
    uint bValid;
    float2 ViewportOrigin;
    float2 Padding;
};

StructuredBuffer<uint> TileLightList : register(t0);

float4 mainVS(uint VertexID : SV_VertexID) : SV_POSITION
{
    float2 UV = float2((VertexID << 1) & 2, VertexID & 2);
    return float4(UV * float2(2, -2) + float2(-1, 1), 0, 1);
}

float3 CountColor(uint Count)
{
    if (Count == 0) return float3(0.025, 0.035, 0.05);
    if (Count >= MaxLightsPerTile) return float3(1, 0, 1);
    if (Count <= 4) return lerp(float3(0.1, 0.25, 0.95), float3(0, 0.8, 0.9), (Count - 1) / 3.0);
    if (Count <= 8) return lerp(float3(0, 0.8, 0.9), float3(0.1, 0.85, 0.2), (Count - 4) / 4.0);
    if (Count <= 16) return lerp(float3(0.1, 0.85, 0.2), float3(1, 0.85, 0), (Count - 8) / 8.0);
    return lerp(float3(1, 0.85, 0), float3(1, 0.1, 0.05), saturate((Count - 16) / 16.0));
}

// 3x5 digits, row-major bits (top-left is bit 0); no font texture needed.
bool DigitPixel(uint Digit, uint2 Pixel)
{
    static const uint Glyphs[10] = {
        31599, 29850, 29671, 31207, 18925,
        31183, 31695, 18727, 31727, 31215
    };
    if (Pixel.x >= 3 || Pixel.y >= 5) return false;
    return ((Glyphs[Digit] >> (Pixel.y * 3 + Pixel.x)) & 1u) != 0;
}

float4 mainPS(float4 Position : SV_POSITION) : SV_Target
{
    uint2 Pixel = uint2(Position.xy - ViewportOrigin);
    // Orthographic / wireframe / disabled geometry / failed CS: never show a previous view's data.
    if (bValid == 0)
    {
        bool Stripe = ((Pixel.x + Pixel.y) / 12u) % 2u != 0;
        return float4(Stripe ? float3(0.22, 0.12, 0.04) : float3(0.08, 0.07, 0.06), 1);
    }

    uint2 Tile = Pixel / TileSize;
    uint Base = (Tile.y * TileCountX + Tile.x) * (1u + MaxLightsPerTile);
    uint Count = min(TileLightList[Base], MaxLightsPerTile);
    uint2 Local = Pixel % TileSize;
    float3 Color = CountColor(Count);
    if (Local.x == 0 || Local.y == 0) Color *= 0.35;

    uint Digits = Count >= 1000 ? 4 : (Count >= 100 ? 3 : (Count >= 10 ? 2 : 1));
    uint Left = (TileSize - (Digits * 4u - 1u)) / 2u;
    uint Top = (TileSize - 5u) / 2u;
    if (Local.x >= Left && Local.y >= Top && Local.y < Top + 5u)
    {
        uint X = Local.x - Left;
        uint Column = X / 4u;
        if (Column < Digits)
        {
            uint Divisor = 1u;
            for (uint i = Column + 1u; i < Digits; ++i) Divisor *= 10u;
            if (DigitPixel((Count / Divisor) % 10u, uint2(X % 4u, Local.y - Top)))
                Color = Count >= 8 && Count < MaxLightsPerTile ? float3(0.02, 0.02, 0.02) : float3(1, 1, 1);
        }
    }
    return float4(Color, 1);
}

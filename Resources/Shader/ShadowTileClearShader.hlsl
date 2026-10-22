float4 mainVS(uint VertexID : SV_VertexID) : SV_POSITION
{
    float2 UV = float2((VertexID << 1) & 2, VertexID & 2);
    return float4(UV * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 1.0f, 1.0f);
}

void mainPS(float4 Position : SV_POSITION) {}
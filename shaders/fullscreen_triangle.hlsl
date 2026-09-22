
// AuroraGlass P0 - Fullscreen triangle VS (no vertex buffer; SV_VertexID trick)

float4 FullscreenVS(uint vertexID : SV_VertexID) : SV_Position
{
    float2 uv = float2((vertexID << 1) & 2, vertexID & 2);
    return float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
}

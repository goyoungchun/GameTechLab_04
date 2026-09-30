// ShaderW0.hlsl
cbuffer constants : register(b0)
{
	float3 Offset;
	float Scale;
    float4 Color;
}

struct VS_INPUT
{
	float4 position : POSITION; // 정점 버퍼로부터의 위치 입력
	float4 color : COLOR; // 정점 버퍼로 부터의 색상 입력
};

struct PS_INPUT
{
	float4 position : SV_POSITION; // 픽셀 셰이더로 패스하기 위한 변환된 위치
	float4 color : COLOR; // 픽셀 셰이더로 패스하기 위한 색상
};

PS_INPUT mainVS(VS_INPUT input)
{
	PS_INPUT output;

	// 픽셀셰이더로 직접 위치를 패스 (변환 없이)
	// output.position = input.position;

	float4 scaledPosition = float4(input.position.xyz * Scale, 1.0f);
	output.position = float4(Offset, 0.0f) + scaledPosition;

	// 픽셀셰이더로 색상 패스
	//output.color = input.color;
    output.color = Color;
	
	return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
	// 색상 직접 출력
	return input.color;
}
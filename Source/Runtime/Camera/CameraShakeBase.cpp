#include "EnginePCH.h"
#include "CameraShakeBase.h"
#include <cmath>
#include <cstdint>

namespace
{
	// 정수 격자점마다 고정된 랜덤 기울기(-1 ~ 1)를 돌려준다.
	float LatticeGradient(int32_t X)
	{
		static float Table[256];
		static bool bInit = false;
		if (!bInit)
		{
			for (float& V : Table)
				V = (std::rand() / (float)RAND_MAX) * 2.0f - 1.0f;   // -1 ~ 1
			bInit = true;
		}
		return Table[X & 255];   // 0~255 범위로 접어서 꺼냄
	}

	// 1D Perlin 노이즈. 결과는 대략 -1 ~ 1이고, X가 조금 변하면 값도 조금만 변한다.
	float PerlinNoise1D(float X)
	{
		const float Floor = std::floor(X);
		const int32_t I0 = static_cast<int32_t>(Floor);
		const float F = X - Floor;

		// 양쪽 격자점의 기울기로 만든 값을 부드럽게 섞는다.
		const float N0 = LatticeGradient(I0) * F;
		const float N1 = LatticeGradient(I0 + 1) * (F - 1.0f);
		const float U = F * F * F * (F * (F * 6.0f - 15.0f) + 10.0f);

		// 1D Perlin의 최대값이 0.5라서 2배로 맞춘다.
		return (N0 + (N1 - N0) * U) * 2.0f;
	}
}

void UCameraShakeBase::Start(const FCameraShakeParams& InParams)
{
	Params = InParams;
	Elapsed = 0.0f;
	for (float& p : Phase)
	{
		p = (std::rand() / (float)RAND_MAX) * 2.0f * PI;
	}
}

float UCameraShakeBase::Wave(int32 Axis) const
{
	switch (Params.Type)
	{
	case ECameraShakeType::Perlin:
		// Phase를 노이즈 위의 시작 위치로 써서 축마다 다른 구간을 읽는다.
		return PerlinNoise1D(Params.Frequency * Elapsed + Phase[Axis] * 100.0f);

	case ECameraShakeType::Sine:
	default:
		return std::sin(2.0f * PI * Params.Frequency * Elapsed + Phase[Axis]);
	}
}

bool UCameraShakeBase::UpdateAndApply(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
	Elapsed += DeltaTime;
	if (Elapsed >= Params.Duration)
	{
		return false;
	}

	float Progress = Elapsed / Params.Duration;
	// TODO: 베지어 Evaluation 추가 필요
	float Fade = 1.0f - Progress; //Params.Ease.Evaluation(Progress);

	float Amount = Params.Strength * Fade;

	InOutPOV.Rotation.Pitch += Amount * Wave(0);
	InOutPOV.Rotation.Yaw += Amount * Wave(1);
	InOutPOV.Rotation.Roll += Amount * Wave(2);
	return true;
}

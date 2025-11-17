#pragma once

#include "CoreMinimal.h"

#include "SceneBufferAsset.generated.h"

UCLASS(BlueprintType)
class GAUSSIANSPLATTINGXRUNTIME_API USceneBufferAsset : public UObject
{
	GENERATED_BODY()

public:
	// =============================== 全局参数 ===============================
	/// 球谐函数的维度
	UPROPERTY()
	uint32 SHDim = {};

	/// 球谐函数系数的数量，等于 (SHDim-1)^2
	UPROPERTY()
	uint32 SHCoefficientsCount = {};

	UPROPERTY()
	uint32 GaussianCount = {};

	// =============================== Gaussian 参数 ===============================
	UPROPERTY()
	TArray<FVector> GaussianPositions = {};

	UPROPERTY()
	TArray<FVector> GaussianScales = {};

	UPROPERTY()
	TArray<FQuat> GaussianRotations = {};

	UPROPERTY()
	TArray<float> GaussianOpacities = {};

	/// 展开的 SH 系数数组，长度为 GaussianCount * SHCoefficientsCount
	UPROPERTY()
	TArray<FVector> GaussianSHCoefficients = {};

	void SetGaussianCount(size_t NewGaussianCount);
};

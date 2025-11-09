#pragma once

#include "GaussianSplattingXImporter.h"

constexpr uint32_t MaxSHCoefficientsCount = 1 + 3 + 5 + 7 + 9;

struct FGaussian
{
	FVector Position;
	FVector Scale;
	FVector4 Rotation; // 四元数
	uint32_t SHDim = {};
	uint32_t SHCoefficientsCount = {};
	float_t SHCoefficients[3][MaxSHCoefficientsCount] = {};
	float Opacity = {};
};

struct FGaussianScene
{
	uint32_t SHDim = {};
	uint32_t SHCoefficientsCount = {};
	std::vector<FGaussian> Gaussians = {};
};

class GAUSSIANSPLATTINGXIMPORTER_API FPlyManager
{
public:
	static void ImportPlyFile(const FString& FilePath);

private:
	static FGaussianScene ReadScene(const FString& FilePath);
	static void StoreScene(const FGaussianScene& Scene, const FString& Name);
};

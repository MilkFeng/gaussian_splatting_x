#pragma once

#include "CoreMinimal.h"

#include "SceneBufferAsset.generated.h"

/// 表示单个高斯体的数据
/// @param Position 高斯体位置
/// @param Scale 高斯体缩放
/// @param Rotation 高斯体旋转，四元数表示
/// @param SHDim 球谐函数的维度
/// @param SHCoefficientsCount 球谐函数系数的数量
/// @param SHCoefficients 球谐函数系数数组，长度为 SHCoefficientsCount，每个系数是一个 FVector，表示 RGB 三个通道的系数
/// @param Opacity 高斯体的不透明度
USTRUCT(BlueprintType)
struct GAUSSIANSPLATTINGXRUNTIME_API FGaussian
{
	GENERATED_BODY()

	UPROPERTY()
	FVector Position = {};

	UPROPERTY()
	FVector Scale = {};

	UPROPERTY()
	FQuat Rotation = {};

	UPROPERTY()
	uint32 SHDim = {};

	UPROPERTY()
	uint32 SHCoefficientsCount = {};

	UPROPERTY()
	TArray<FVector> SHCoefficients = {};

	UPROPERTY()
	float Opacity = {};
};

/// 场景缓冲区资产，包含多个高斯体的数据
/// @param SHDim 球谐函数的维度，与高斯体的 SHDim 相同
/// @param SHCoefficientsCount 球谐函数系数的数量，等于 (SHDim-1)^2，与高斯体的 SHCoefficientsCount 相同
/// @param Gaussians 高斯体数组，包含场景中的所有高斯体
UCLASS(BlueprintType)
class GAUSSIANSPLATTINGXRUNTIME_API USceneBufferAsset : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int32 SHDim = {};

	UPROPERTY()
	int32 SHCoefficientsCount = {};

	UPROPERTY()
	TArray<FGaussian> Gaussians = {};
};

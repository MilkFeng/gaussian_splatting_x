#pragma once

#include "CoreMinimal.h"

#include "SceneBufferAsset.generated.h"

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

#pragma once

#include "CoreMinimal.h"
#include "SceneBufferAsset.generated.h"

USTRUCT(BlueprintType)
struct FGaussian
{
	GENERATED_BODY()

	UPROPERTY()
	FVector Position = {};

	UPROPERTY()
	FVector Scale = {};

	UPROPERTY()
	FQuat4f Rotation = {};

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
class USceneBufferAsset : public UObject
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

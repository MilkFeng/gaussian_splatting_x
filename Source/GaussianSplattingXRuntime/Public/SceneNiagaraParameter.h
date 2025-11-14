#pragma once

#include "SceneNiagaraParameter.generated.h"

UCLASS(BlueprintType)
class GAUSSIANSPLATTINGXRUNTIME_API USceneNiagaraParameter : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	FSoftObjectPath SceneBufferAssetPath;
};

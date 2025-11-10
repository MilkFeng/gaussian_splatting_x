#pragma once

#include "NiagaraComponent.h"
#include "SceneNiagaraInterface.h"

#include "SceneActor.generated.h"

UCLASS()
class GAUSSIANSPLATTINGXRUNTIME_API ASceneActor : public AActor
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	TObjectPtr<USceneBufferAsset> SceneBuffer;
	
	UPROPERTY()
	TObjectPtr<UNiagaraComponent> NiagaraComp;

	ASceneActor();

	virtual void OnConstruction(const FTransform& Transform) override;
};

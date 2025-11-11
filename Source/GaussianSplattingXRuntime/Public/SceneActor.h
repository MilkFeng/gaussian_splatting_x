#pragma once

#include "NiagaraComponent.h"
#include "SceneNiagaraInterface.h"

#include "SceneActor.generated.h"

UCLASS(BlueprintType)
class GAUSSIANSPLATTINGXRUNTIME_API ASceneActor : public AActor
{
	GENERATED_BODY()

public:
	// 场景数据资源
	UPROPERTY()
	TObjectPtr<USceneNiagaraInterface> SceneNiagaraInterface;

	// 用来运行 Niagara System 的组件
	UPROPERTY()
	TObjectPtr<UNiagaraComponent> NiagaraComp;

	ASceneActor();

	virtual void OnConstruction(const FTransform& Transform) override;
};

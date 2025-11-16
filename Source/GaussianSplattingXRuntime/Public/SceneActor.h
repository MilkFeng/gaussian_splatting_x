#pragma once

#include "NiagaraComponent.h"
#include "SceneNiagaraParameter.h"

#include "SceneActor.generated.h"

/// 可以放入关卡中的场景 Actor，包含一个 Niagara Component 用来渲染高斯体场景
UCLASS(BlueprintType)
class GAUSSIANSPLATTINGXRUNTIME_API ASceneActor : public AActor
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	TObjectPtr<USceneNiagaraParameter> SceneNiagaraParameter;

	// 用来运行 Niagara System 的组件
	UPROPERTY()
	TObjectPtr<UNiagaraComponent> NiagaraComp;

	ASceneActor();

	virtual void OnConstruction(const FTransform& Transform) override;

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
};

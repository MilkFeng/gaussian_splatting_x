#include "SceneActor.h"
#include "NiagaraSystem.h"

ASceneActor::ASceneActor()
{
	NiagaraComp = CreateDefaultSubobject<UNiagaraComponent>(TEXT("NiagaraComp"));

	// 设置为根组件，这个 Actor 的位置、旋转、缩放 == NiagaraComp 的变换
	RootComponent = NiagaraComp;
}

void ASceneActor::OnConstruction(const FTransform& Transform)
{
	if (NiagaraComp && SceneNiagaraParameter)
	{
		// 加载 Niagara System 资源
		NiagaraComp->SetAsset(
			LoadObject<UNiagaraSystem>(
				nullptr, TEXT(
					"/Script/Niagara.NiagaraSystem'/GaussianSplattingX/FX_GaussianSplattingX.FX_GaussianSplattingX'")));
		NiagaraComp->SetVariableObject(TEXT("User.SceneNiagaraParameter"), SceneNiagaraParameter.Get());
	}
}

void ASceneActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	UE_LOG(LogTemp, Log,
	       TEXT(
		       "PostEditChangeProperty - Updating Niagara Component with new SceneNiagaraParameter, SceneNiagaraParameter is nullptr: %d"
	       ), SceneNiagaraParameter == nullptr);

	NiagaraComp->SetVariableObject(TEXT("User.SceneNiagaraParameter"), SceneNiagaraParameter.Get());
}

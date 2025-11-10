#include "SceneActor.h"
#include "NiagaraSystem.h"

ASceneActor::ASceneActor()
{
	NiagaraComp = CreateDefaultSubobject<UNiagaraComponent>(TEXT("NiagaraComp"));
	RootComponent = NiagaraComp;
}

void ASceneActor::OnConstruction(const FTransform& Transform)
{
	if (NiagaraComp && SceneBuffer)
	{
		NiagaraComp->SetAsset(LoadObject<UNiagaraSystem>(nullptr, TEXT("/GaussianSplattingX/GaussianSplattingX_FX.GaussianSplattingX_FX")));
		NiagaraComp->SetVariableObject(TEXT("SceneBuffer"), SceneBuffer);
	}
}

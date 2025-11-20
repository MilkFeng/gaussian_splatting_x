#include "SceneNiagaraRendererProperties.h"

#include "SceneNiagaraRenderer.h"

USceneNiagaraRendererProperties::USceneNiagaraRendererProperties()
{
}

void USceneNiagaraRendererProperties::InitCDOPropertiesAfterModuleStartup()
{
}

FNiagaraRenderer* USceneNiagaraRendererProperties::CreateEmitterRenderer(
	const ERHIFeatureLevel::Type FeatureLevel,
	const FNiagaraEmitterInstance* Emitter,
	const FNiagaraSystemInstanceController& InController)
{
	return new FSceneNiagaraRenderer(FeatureLevel, this, Emitter);
}

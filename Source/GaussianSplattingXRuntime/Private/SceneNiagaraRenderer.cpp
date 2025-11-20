#include "SceneNiagaraRenderer.h"

FSceneNiagaraRenderer::FSceneNiagaraRenderer(const ERHIFeatureLevel::Type FeatureLevel,
                                             const UNiagaraRendererProperties* InProps,
                                             const FNiagaraEmitterInstance* Emitter) :
	FNiagaraRenderer(FeatureLevel, InProps, Emitter)
{
}

FSceneNiagaraRenderer::~FSceneNiagaraRenderer()
{
}

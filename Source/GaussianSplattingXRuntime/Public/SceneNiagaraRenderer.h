#pragma once
#include "NiagaraRenderer.h"

class GAUSSIANSPLATTINGXRUNTIME_API FSceneNiagaraRenderer : public FNiagaraRenderer
{
public:
	FSceneNiagaraRenderer(ERHIFeatureLevel::Type FeatureLevel, const UNiagaraRendererProperties* InProps,
		const FNiagaraEmitterInstance* Emitter);

	explicit FSceneNiagaraRenderer(const FNiagaraRenderer& Other) = delete;

	virtual ~FSceneNiagaraRenderer() override;
};

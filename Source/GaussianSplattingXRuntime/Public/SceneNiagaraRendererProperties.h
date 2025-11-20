//! https://zhuanlan.zhihu.com/p/633847465

#pragma once
#include "NiagaraRendererProperties.h"
#include "SceneNiagaraRendererProperties.generated.h"

UCLASS(EditInlineNew, meta = (DisplayName = "Gaussian Splatting Renderer"))
class GAUSSIANSPLATTINGXRUNTIME_API USceneNiagaraRendererProperties : public UNiagaraRendererProperties
{
	GENERATED_BODY()

public:
	USceneNiagaraRendererProperties();

	static void InitCDOPropertiesAfterModuleStartup();

	virtual FNiagaraRenderer* CreateEmitterRenderer(
		ERHIFeatureLevel::Type FeatureLevel,
		const FNiagaraEmitterInstance* Emitter,
		const FNiagaraSystemInstanceController& InController) override;
};

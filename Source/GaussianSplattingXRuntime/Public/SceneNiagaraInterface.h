#pragma once

#include "NiagaraDataInterface.h"
#include "SceneBufferAsset.h"

#include "SceneNiagaraInterface.generated.h"

UCLASS()
class GAUSSIANSPLATTINGXRUNTIME_API USceneNiagaraInterface : public UNiagaraDataInterface
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TSoftObjectPtr<USceneBufferAsset> SceneBufferAsset;

	virtual void GetFunctionsInternal(TArray<FNiagaraFunctionSignature>& OutFunctions) const override;
	virtual void GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo,
	                                   void* InstanceData, FVMExternalFunction& OutFunc) override;

private:
	void GetGaussianCount(FVectorVMExternalFunctionContextProxy& Context) const;
	void GetGaussianData(FVectorVMExternalFunctionContextProxy& Context) const;
};

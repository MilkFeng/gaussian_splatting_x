#pragma once

#include "NiagaraDataInterface.h"
#include "SceneBufferAsset.h"

#include "SceneNiagaraInterface.generated.h"

/// 在 Niagara 中，不能直接使用 USceneBufferAsset 资源，需要通过 UNiagaraDataInterface 来桥接
/// 它会暴露一些函数，把数据从 CPU 传递到 GPU 上的 Niagara 系统中
UCLASS()
class GAUSSIANSPLATTINGXRUNTIME_API USceneNiagaraInterface : public UNiagaraDataInterface
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TSoftObjectPtr<USceneBufferAsset> SceneBufferAsset;

	/// 注册 Niagara 可以调用的函数
	virtual void GetFunctionsInternal(TArray<FNiagaraFunctionSignature>& OutFunctions) const override;

	/// 绑定具体的实现函数
	virtual void GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo,
	                                   void* InstanceData, FVMExternalFunction& OutFunc) override;

private:
	/// 获取场景中高斯数量
	void GetGaussianCount(FVectorVMExternalFunctionContextProxy& Context) const;

	/// 获取指定的高斯
	void GetGaussianData(FVectorVMExternalFunctionContextProxy& Context) const;
};

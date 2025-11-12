//! 参考实例： https://github.com/EpicGames/UnrealEngine/tree/release/Engine/Plugins/FX/ExampleCustomDataInterface

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

	BEGIN_SHADER_PARAMETER_STRUCT(FShaderParameters,)
		SHADER_PARAMETER(int32, GaussianCount)
	END_SHADER_PARAMETER_STRUCT()

public:
	UPROPERTY()
	TSoftObjectPtr<USceneBufferAsset> SceneBufferAsset;

	/// 在对象被构造完毕并初始化其 UProperty（反射属性）后调用，CDO（Class Default Object）也会调用
	virtual void PostInitProperties() override;

#if WITH_EDITORONLY_DATA
	virtual bool AppendCompileHash(FNiagaraCompileHashVisitor* InVisitor) const override;
	virtual bool GetFunctionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo,
	                             const FNiagaraDataInterfaceGeneratedFunction& FunctionInfo, int FunctionInstanceIndex,
	                             FString& OutHLSL) override;
	virtual void
	GetParameterDefinitionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL) override;
#endif
	virtual void BuildShaderParameters(FNiagaraShaderParametersBuilder& ShaderParametersBuilder) const override;
	virtual void SetShaderParameters(const FNiagaraDataInterfaceSetShaderParametersContext& Context) const override;

	virtual void GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo,
	                                   void* InstanceData, FVMExternalFunction& OutFunc) override;

protected:
#if WITH_EDITORONLY_DATA
	virtual void GetFunctionsInternal(TArray<FNiagaraFunctionSignature>& OutFunctions) const override;
#endif

private:
	void GetGaussianCount(FVectorVMExternalFunctionContextProxy& Context) const;
	void GetGaussianData(FVectorVMExternalFunctionContextProxy& Context) const;

private:
	static const FName GetGaussianCountName;
	static const FName GetGaussianDataName;
	static const FString GetGaussianCountShaderFile;
};

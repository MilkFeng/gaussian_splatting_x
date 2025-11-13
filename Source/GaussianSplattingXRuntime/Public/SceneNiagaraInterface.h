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

public:
	UPROPERTY(EditAnywhere, Category = "Scene")
	TSoftObjectPtr<USceneBufferAsset> SceneBufferAsset;

	explicit USceneNiagaraInterface(FObjectInitializer const& ObjectInitializer);

private:
	// =============================== 定义传给 HLSL（GPU）的数据结构 ===============================
	BEGIN_SHADER_PARAMETER_STRUCT(FShaderParameters,)
		SHADER_PARAMETER(int32, GaussianCount)
	END_SHADER_PARAMETER_STRUCT()

protected:
	// ============================== 定义 VM（CPU）和 HLSL（GPU）的所有函数接口 ===============================
#if WITH_EDITORONLY_DATA
	virtual void GetFunctionsInternal(TArray<FNiagaraFunctionSignature>& OutFunctions) const override;
#endif

public:
	// =============================== UObject 相关函数，管理生命周期等 ===============================
	/// 在对象被构造完毕并初始化其 UProperty（反射属性）后调用，CDO（Class Default Object）也会调用
	virtual void PostInitProperties() override;
#if WITH_EDITOR
	/// 在编辑器中，当某个 UProperty 被修改后调用
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	virtual bool Equals(const UNiagaraDataInterface* Other) const override;

	// =============================== Niagara Data Interface 的配置 ===============================
	virtual bool CanExecuteOnTarget(ENiagaraSimTarget Target) const override;

	// =============================== HLSL（GPU）接口 Bridge ===============================
#if WITH_EDITORONLY_DATA
	virtual bool AppendCompileHash(FNiagaraCompileHashVisitor* InVisitor) const override;
	virtual bool GetFunctionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo,
	                             const FNiagaraDataInterfaceGeneratedFunction& FunctionInfo, int FunctionInstanceIndex,
	                             FString& OutHLSL) override;
	virtual void
	GetParameterDefinitionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL) override;
#endif
	virtual void BuildShaderParameters(FNiagaraShaderParametersBuilder& ShaderParametersBuilder) const override;
	/// 将数据传递给 GPU，从 InstanceData 读出数据，根据 FShaderParameters 结构体的定义，传给 GPU
	virtual void SetShaderParameters(const FNiagaraDataInterfaceSetShaderParametersContext& Context) const override;

	// =============================== 数据接口，用于向 VM（CPU）和 HLSL（GPU）传输数据 ===============================
	// note: InstanceData 是每一个实例携带的数据，实例可能是 Emitter，也可能是 Particle
	virtual int PerInstanceDataSize() const override;
	virtual bool InitPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance) override;
	virtual bool PerInstanceTick(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance,
	                             float DeltaSeconds) override;
	virtual void ProvidePerInstanceDataForRenderThread(void* DataForRenderThread, void* PerInstanceData,
	                                                   const FNiagaraSystemInstanceID& SystemInstance) override;
	virtual void DestroyPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance) override;
	virtual bool HasPreSimulateTick() const override;

	// =============================== VM（CPU）接口 Bridge ===============================
	virtual void GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo,
	                                   void* InstanceData, FVMExternalFunction& OutFunc) override;

private:
	// ============================== VM（CPU）实现 ===============================
	// note: VM 也需要从 InstanceData 读取数据
	void GetGaussianCountVM(FVectorVMExternalFunctionContext& Context) const;

private:
	static const FName GetGaussianCountName;
	static const FName GetGaussianDataName;
	static const FString GetGaussianCountShaderFile;
};

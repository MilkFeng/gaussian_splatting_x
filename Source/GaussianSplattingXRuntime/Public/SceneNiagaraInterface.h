//! 参考官方实例： https://github.com/EpicGames/UnrealEngine/tree/release/Engine/Plugins/FX/ExampleCustomDataInterface
//! 参考视频：https://www.bilibili.com/video/BV1moZHY4EHz

#pragma once

#include "NiagaraDataInterface.h"

#include "SceneNiagaraInterface.generated.h"

/// 在 Niagara 中，不能直接使用 USceneBufferAsset 资源，需要通过 UNiagaraDataInterface 来桥接
/// 它会暴露一些函数，把数据从 CPU 传递到 GPU 上的 Niagara 系统中
UCLASS()
class GAUSSIANSPLATTINGXRUNTIME_API USceneNiagaraInterface : public UNiagaraDataInterface
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Scene")
	FNiagaraUserParameterBinding UserParameterBinding;

	explicit USceneNiagaraInterface(const FObjectInitializer& ObjectInitializer);

private:
	// =============================== 暴露给 HLSL（GPU）的数据结构 ===============================
	BEGIN_SHADER_PARAMETER_STRUCT(FShaderParameters,)
		SHADER_PARAMETER(int, GaussianCount)
		SHADER_PARAMETER(int, SHCoefficientsCount)
		SHADER_PARAMETER(FMatrix44f, ActorTransformMatrixInCamera)
		SHADER_PARAMETER_SRV(Buffer<FVector4f>, GaussianPositionBuffer)
		SHADER_PARAMETER_SRV(Buffer<FVector4f>, GaussianSHCoefficientsBuffer)
	END_SHADER_PARAMETER_STRUCT()

protected:
	// ============================== 定义 VM（CPU）和 HLSL（GPU）的所有函数接口 ===============================
	// note: 如果一个函数是 MemberFunction，那么也可以访问 DataInterface 的变量（VM），或者访问 ShaderParameters（HLSL）
#if WITH_EDITORONLY_DATA
	virtual void GetFunctionsInternal(TArray<FNiagaraFunctionSignature>& OutFunctions) const override;
#endif

public:
	// =============================== 管理生命周期 ===============================
	/// 在对象被构造完毕并初始化其 UProperty（反射属性）后调用，CDO（Class Default Object）也会调用
	virtual void PostInitProperties() override;
#if WITH_EDITOR
	/// 在编辑器中，当某个 UProperty 被修改后调用
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	// =============================== Niagara Data Interface 的配置 ===============================
	virtual bool CanExecuteOnTarget(ENiagaraSimTarget Target) const override;
	/// 如果设置了 MemberFunction，那么 Niagara 会将当前的 Niagara Data Interface 对象复制到 Render Thread 中
	virtual bool CopyToInternal(UNiagaraDataInterface* Destination) const override;
	/// 用于比较两个 Data Interface 是否相等，决定是否需要重新编译 Niagara 系统或者重新传递数据
	virtual bool Equals(const UNiagaraDataInterface* Other) const override;

	// =============================== HLSL（GPU）接口 Bridge ===============================
	// note: 如果函数不是 MemberFunction，那么无法访问 ShaderParameters
	// note: SetShaderParameters 在 RT 上执行
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

	// =============================== Niagara System 的数据接口 ===============================
	// note: InstanceData 是每一个 System 携带的数据（Emitter、Particle 并没有）
	// note: 除了 ProvidePerInstanceDataForRenderThread 负责将数据传递给 RT 外，其他函数都在 GT 上执行
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
	// note: 都会在 RT 上执行
	void GetGaussianCountVM(FVectorVMExternalFunctionContext& Context) const;

	// ============================== 辅助函数 ===============================
	FTransform GetCameraTransform(FNiagaraSystemInstance* SystemInstance) const;
	FTransform GetActorTransformInCamera(FNiagaraSystemInstance* SystemInstance) const;

private:
	static const FName GetGaussianCountName;
	static const FName GetGaussianDataName;
	static const FString GaussianShaderFile;
};

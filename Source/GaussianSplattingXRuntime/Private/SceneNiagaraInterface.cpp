#include "SceneNiagaraInterface.h"
#include "SceneActor.h"

#include "NiagaraCompileHashVisitor.h"
#include "NiagaraRendererProperties.h"
#include "NiagaraRenderer.h"
#include "NiagaraShaderParametersBuilder.h"
#include "NiagaraSystemInstance.h"

const FName USceneNiagaraInterface::GetGaussianCountName = TEXT("GetGaussianCount");
const FName USceneNiagaraInterface::GetGaussianDataName = TEXT("GetGaussianData");
const FString USceneNiagaraInterface::GetGaussianCountShaderFile = TEXT(
	"/Plugin/GaussianSplattingX/Private/SceneNiagaraInterface_Shader.ush");

// the struct used to store our data interface data
struct FNDIGaussianInstanceData
{
	FVector Position;
};

// this proxy is used to safely copy data between game thread and render thread
struct FNDIGaussianProxy : public FNiagaraDataInterfaceProxy
{
	virtual int32 PerInstanceDataPassedToRenderThreadSize() const override
	{
		return sizeof(FNDIGaussianInstanceData);
	}

	static void ProvidePerInstanceDataForRenderThread(void* InDataForRenderThread, void* InDataFromGameThread,
	                                                  const FNiagaraSystemInstanceID& SystemInstance)
	{
		// initialize the render thread instance data into the pre-allocated memory
		FNDIGaussianInstanceData* DataForRenderThread = new(InDataForRenderThread) FNDIGaussianInstanceData();

		// we're just copying the game thread data, but the render thread data can be initialized to anything here and can be another struct entirely
		const FNDIGaussianInstanceData* DataFromGameThread = static_cast<FNDIGaussianInstanceData*>(
			InDataFromGameThread);
		*DataForRenderThread = *DataFromGameThread;
	}

	virtual void
	ConsumePerInstanceDataFromGameThread(void* PerInstanceData, const FNiagaraSystemInstanceID& InstanceID) override
	{
		FNDIGaussianInstanceData* InstanceDataFromGT = static_cast<FNDIGaussianInstanceData*>(
			PerInstanceData);
		FNDIGaussianInstanceData& InstanceData = SystemInstancesToInstanceData_RT.FindOrAdd(InstanceID);
		InstanceData = *InstanceDataFromGT;

		// we call the destructor here to clean up the GT data. Without this we could be leaking memory.
		InstanceDataFromGT->~FNDIGaussianInstanceData();
	}

	void LoadSceneBufferAsset(const FSoftObjectPath& SceneBufferAssetPath)
	{
		SceneBufferAsset = TSoftObjectPtr<USceneBufferAsset>(SceneBufferAssetPath);
		SceneBufferAsset.LoadSynchronous();

		UE_LOG(LogTemp, Log,
		       TEXT("FNDIGaussianProxy::LoadSceneBufferAsset - Loaded SceneBufferAsset: %s"),
		       *SceneBufferAssetPath.ToString());
	}

	void PostSRV()
	{
		if (!IsValid())
		{
			return;
		}

		ENQUEUE_RENDER_COMMAND(FUpdateGaussianBuffer)(
			[this](FRHICommandListImmediate& RHICmdList)
			{
				{
					const size_t BufferSize = SceneBufferAsset->GaussianCount * sizeof(FVector);

					// 初始化 Buffer
					if (BufferSize > 0 && !GaussianPositionBuffer.NumBytes)
					{
						GaussianPositionBuffer.Initialize(RHICmdList, TEXT("PositionBuffer"), sizeof(FVector),
						                                  SceneBufferAsset->GaussianCount, PF_R32G32B32F,
						                                  BUF_Static);
					}

					// 如果初始化成功，更新数据到 Buffer
					if (GaussianPositionBuffer.NumBytes > 0)
					{
						// 上锁，准备写入数据
						float* MappedData = static_cast<float*>(
							RHICmdList.LockBuffer(GaussianPositionBuffer.Buffer, 0, BufferSize, RLM_WriteOnly));
						FPlatformMemory::Memcpy(MappedData, &SceneBufferAsset->GaussianPositions, BufferSize);
						RHICmdList.UnlockBuffer(GaussianPositionBuffer.Buffer);
					}
				}

				// after the draw we can clear out the data for the next frame
				SystemInstancesToInstanceData_RT.Empty();
			});
	}

	const USceneBufferAsset& GetSceneBufferAsset() const
	{
		return *SceneBufferAsset;
	}

	bool IsValid() const
	{
		return SceneBufferAsset.IsValid();
	}

	TMap<FNiagaraSystemInstanceID, FNDIGaussianInstanceData> SystemInstancesToInstanceData_RT;

	// =============================== Buffer ===============================
	FReadBuffer GaussianPositionBuffer;

private:
	TSoftObjectPtr<USceneBufferAsset> SceneBufferAsset;
};

USceneNiagaraInterface::USceneNiagaraInterface(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Proxy.Reset(new FNDIGaussianProxy());
}

#if WITH_EDITORONLY_DATA
void USceneNiagaraInterface::GetFunctionsInternal(TArray<FNiagaraFunctionSignature>& OutFunctions) const
{
	// 获取高斯数量
	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetGaussianCountName;
		// 如果为真，第一个参数会隐式传递 this 指针，那么在 VM（CPU）可以直接访问成员变量，在 HLSL（GPU）可以通过设置的 Parameter 访问
		Sig.bMemberFunction = true;
		// 如果为真，表示这个函数只进行读取操作，不会修改任何数据，应该是用来优化的
		Sig.bReadFunction = true;
		// 是否支持 CPU 和 GPU 执行，这个函数只在 Emitter 阶段使用，所以只支持 CPU
		Sig.bSupportsCPU = true;
		Sig.bSupportsGPU = false;
		// 指定这个函数在哪些 Niagara 脚本类型中可用，比如 Particle、Emitter、System 等等
		Sig.ModuleUsageBitmask = ENiagaraScriptUsageMask::System | ENiagaraScriptUsageMask::Emitter;
		Sig.AddInput(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("Scene Niagara Data Interface")));
		Sig.AddOutput(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Count")));
		OutFunctions.Add(Sig);
	}

	// 获取高斯数据
	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetGaussianDataName;
		Sig.bMemberFunction = true;
		Sig.bReadFunction = true;
		Sig.bSupportsCPU = false;
		Sig.bSupportsGPU = true;
		Sig.ModuleUsageBitmask = ENiagaraScriptUsageMask::Particle;
		Sig.AddInput(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("Scene Niagara Data Interface")));
		Sig.AddInput(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Index")));
		Sig.AddOutput(FNiagaraVariable(FNiagaraTypeDefinition::GetVec4Def(), TEXT("Position")));
		OutFunctions.Add(Sig);
	}

	UE_LOG(LogTemp, Log,
	       TEXT("USceneNiagaraInterface::GetFunctionsInternal - Registered %d functions."),
	       OutFunctions.Num());
}
#endif

void USceneNiagaraInterface::PostInitProperties()
{
	Super::PostInitProperties();

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		// 保证这个类只在 CDO 上注册一次
		// 每个 UClass 在类加载时，会自动构造它的 CDO，一般发生在：
		//   - 引擎启动阶段：加载核心类，如 AActor, UObject, UGameInstance, UWorld
		//   - 模块（插件）加载时，模块会注册它的所有 UCLASS，并创建它们的 CDO
		//   - 蓝图编译 / 加载时，蓝图编译会生成 UBlueprintGeneratedClass，其 CDO 会创建
		//   - 第一次调用 StaticClass() 时，如果类还没加载，会触发加载并创建 CDO

		// AllowAnyVariable：允许这个类型出现在 Niagara 变量系统中
		// AllowParameter：允许作为用户参数（User Parameter）绑定到 Niagara System
		constexpr ENiagaraTypeRegistryFlags Flags = ENiagaraTypeRegistryFlags::AllowAnyVariable |
			ENiagaraTypeRegistryFlags::AllowParameter;

		// 向 Niagara 全局类型表注册，表示这个 Data Interface 可以被作为参数拖进 Niagara Graph，用来访问外部数据
		// 一定得注册才能在 Niagara 系统的用户参数部分使用它
		FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(GetClass()), Flags);
	}
}

void USceneNiagaraInterface::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

bool USceneNiagaraInterface::CanExecuteOnTarget(ENiagaraSimTarget Target) const
{
	// CPU 和 GPU 都支持
	return true;
}

bool USceneNiagaraInterface::CopyToInternal(UNiagaraDataInterface* Destination) const
{
	if (!Super::CopyToInternal(Destination))
	{
		return false;
	}

	if (USceneNiagaraInterface* Other = Cast<USceneNiagaraInterface>(Destination))
	{
		Other->UserParameterBinding = UserParameterBinding;
	}
	return true;
}

bool USceneNiagaraInterface::Equals(const UNiagaraDataInterface* Other) const
{
	if (!Super::Equals(Other))
	{
		return false;
	}

	return UserParameterBinding == CastChecked<USceneNiagaraInterface>(Other)->UserParameterBinding;
}

bool USceneNiagaraInterface::AppendCompileHash(FNiagaraCompileHashVisitor* InVisitor) const
{
	if (!Super::AppendCompileHash(InVisitor))
	{
		return false;
	}

	InVisitor->UpdateShaderFile(*GetGaussianCountShaderFile);
	InVisitor->UpdateShaderParameters<FShaderParameters>();
	return true;
}

bool USceneNiagaraInterface::GetFunctionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo,
                                             const FNiagaraDataInterfaceGeneratedFunction& FunctionInfo,
                                             int FunctionInstanceIndex, FString& OutHLSL)
{
	return FunctionInfo.DefinitionName == GetGaussianDataName;
}

void USceneNiagaraInterface::GetParameterDefinitionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo,
                                                        FString& OutHLSL)
{
	const TMap<FString, FStringFormatArg> TemplateArgs = {
		{TEXT("ParameterName"), ParamInfo.DataInterfaceHLSLSymbol},
	};
	AppendTemplateHLSL(OutHLSL, *GetGaussianCountShaderFile, TemplateArgs);
}

void USceneNiagaraInterface::BuildShaderParameters(FNiagaraShaderParametersBuilder& ShaderParametersBuilder) const
{
	ShaderParametersBuilder.AddNestedStruct<FShaderParameters>();
}

void USceneNiagaraInterface::SetShaderParameters(const FNiagaraDataInterfaceSetShaderParametersContext& Context) const
{
	FNDIGaussianProxy& DataInterfaceProxy = Context.GetProxy<FNDIGaussianProxy>();
	FNDIGaussianInstanceData& InstanceData = DataInterfaceProxy.SystemInstancesToInstanceData_RT.FindChecked(
		Context.GetSystemInstanceID());

	FShaderParameters* ShaderParameters = Context.GetParameterNestedStruct<FShaderParameters>();

	DataInterfaceProxy.PostSRV();
	ShaderParameters->PositionBuffer = FNiagaraRenderer::GetSrvOrDefaultFloat(
		DataInterfaceProxy.GaussianPositionBuffer.SRV);
}

int USceneNiagaraInterface::PerInstanceDataSize() const
{
	return sizeof(FNDIGaussianInstanceData);
}

bool USceneNiagaraInterface::InitPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance)
{
	FNDIGaussianProxy* DataInterfaceProxy = GetProxyAs<FNDIGaussianProxy>();
	FNDIGaussianInstanceData* InstanceData = new(PerInstanceData) FNDIGaussianInstanceData();
	InstanceData->Position = FVector::ZeroVector;

	const FNiagaraUserRedirectionParameterStore* Store = SystemInstance->GetOverrideParameters();
	TArray<FNiagaraVariable> OutParameters;
	Store->GetUserParameters(OutParameters);
	for (const FNiagaraVariable& Var : OutParameters)
	{
		UE_LOG(LogTemp, Log,
		       TEXT("USceneNiagaraInterface::InitPerInstanceData - User Parameter: %s, Type: %s"),
		       *Var.GetName().ToString(), *Var.GetType().GetName());
	}
	UObject* Object = Store->GetUObject(UserParameterBinding.Parameter);
	if (Object)
	{
		UE_LOG(LogTemp, Log,
		       TEXT("USceneNiagaraInterface::InitPerInstanceData - Found bound object: %s, Type: %s"),
		       *Object->GetName(), *Object->GetClass()->GetName());
	} else
	{
		UE_LOG(LogTemp, Warning,
		       TEXT("USceneNiagaraInterface::InitPerInstanceData - No object bound to User Parameter: %s"),
		       *UserParameterBinding.Parameter.GetName().ToString());
	}
	const USceneNiagaraParameter* NiagaraParameter = Cast<USceneNiagaraParameter>(Object);
	if (NiagaraParameter)
	{
		UE_LOG(LogTemp, Log,
		       TEXT("USceneNiagaraInterface::InitPerInstanceData - Loading SceneBufferAsset from path: %s"),
		       *NiagaraParameter->SceneBufferAssetPath.ToString());
		DataInterfaceProxy->LoadSceneBufferAsset(NiagaraParameter->SceneBufferAssetPath);
	}
	return true;
}

bool USceneNiagaraInterface::PerInstanceTick(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance,
                                             const float DeltaSeconds)
{
	check(SystemInstance);
	FNDIGaussianInstanceData* InstanceData = static_cast<FNDIGaussianInstanceData*>(PerInstanceData);

	if (!InstanceData)
	{
		return true;
	}
	return false;
}

void USceneNiagaraInterface::ProvidePerInstanceDataForRenderThread(void* DataForRenderThread, void* PerInstanceData,
                                                                   const FNiagaraSystemInstanceID& SystemInstance)
{
	FNDIGaussianProxy::ProvidePerInstanceDataForRenderThread(DataForRenderThread, PerInstanceData, SystemInstance);
}

void USceneNiagaraInterface::DestroyPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance)
{
	FNDIGaussianInstanceData* InstanceData = static_cast<FNDIGaussianInstanceData*>(PerInstanceData);
	InstanceData->~FNDIGaussianInstanceData();

	ENQUEUE_RENDER_COMMAND(RemoveProxy)
	(
		[RT_Proxy=GetProxyAs<FNDIGaussianProxy>(), InstanceID=SystemInstance->GetId()](
		FRHICommandListImmediate& CmdList)
		{
			RT_Proxy->SystemInstancesToInstanceData_RT.Remove(InstanceID);
		}
	);
}

bool USceneNiagaraInterface::HasPreSimulateTick() const
{
	return true;
}

void USceneNiagaraInterface::GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo,
                                                   void* InstanceData, FVMExternalFunction& OutFunc)
{
	if (BindingInfo.Name == GetGaussianCountName)
	{
		OutFunc = FVMExternalFunction::CreateLambda([this](FVectorVMExternalFunctionContext& Context)
		{
			this->GetGaussianCountVM(Context);
		});
		return;
	}
	UE_LOG(LogTemp, Error,
	       TEXT("USceneNiagaraInterface::GetVMExternalFunction - CPU execution is not supported for function %s"),
	       *BindingInfo.Name.ToString());
}

void USceneNiagaraInterface::GetGaussianCountVM(FVectorVMExternalFunctionContext& Context) const
{
	const FNDIGaussianProxy* DataInterfaceProxy = GetProxyAs<FNDIGaussianProxy>();

	VectorVM::FUserPtrHandler<FNDIGaussianInstanceData> InstData(Context);
	FNDIOutputParam<int> OutCount(Context);

	const int Count = DataInterfaceProxy->IsValid() ? DataInterfaceProxy->GetSceneBufferAsset().GaussianCount : 0;

	for (size_t i = 0; i < Context.GetNumInstances(); ++i)
	{
		OutCount.SetAndAdvance(Count);
	}
}

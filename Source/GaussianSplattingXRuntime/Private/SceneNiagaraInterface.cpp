#include "SceneNiagaraInterface.h"

#include "NiagaraCompileHashVisitor.h"
#include "NiagaraShaderParametersBuilder.h"
#include "NiagaraSystemInstance.h"

const FName USceneNiagaraInterface::GetGaussianCountName = TEXT("GetGaussianCount");
const FName USceneNiagaraInterface::GetGaussianDataName = TEXT("GetGaussianData");
const FString USceneNiagaraInterface::GetGaussianCountShaderFile = TEXT(
	"/Plugin/GaussianSplattingX/Private/SceneNiagaraInterface_GetGaussianCount.ush");

// the struct used to store our data interface data
struct FNDIGaussianCountInstanceData
{
	int32 GaussianCount;
};

// this proxy is used to safely copy data between game thread and render thread
struct FNDIGaussianCountProxy : public FNiagaraDataInterfaceProxy
{
	virtual int32 PerInstanceDataPassedToRenderThreadSize() const override
	{
		return sizeof(FNDIGaussianCountInstanceData);
	}

	static void ProvidePerInstanceDataForRenderThread(void* InDataForRenderThread, void* InDataFromGameThread,
	                                                  const FNiagaraSystemInstanceID& SystemInstance)
	{
		// initialize the render thread instance data into the pre-allocated memory
		FNDIGaussianCountInstanceData* DataForRenderThread = new(InDataForRenderThread) FNDIGaussianCountInstanceData();

		// we're just copying the game thread data, but the render thread data can be initialized to anything here and can be another struct entirely
		const FNDIGaussianCountInstanceData* DataFromGameThread = static_cast<FNDIGaussianCountInstanceData*>(
			InDataFromGameThread);
		*DataForRenderThread = *DataFromGameThread;
	}

	virtual void
	ConsumePerInstanceDataFromGameThread(void* PerInstanceData, const FNiagaraSystemInstanceID& InstanceID) override
	{
		FNDIGaussianCountInstanceData* InstanceDataFromGT = static_cast<FNDIGaussianCountInstanceData*>(
			PerInstanceData);
		FNDIGaussianCountInstanceData& InstanceData = SystemInstancesToInstanceData_RT.FindOrAdd(InstanceID);
		InstanceData = *InstanceDataFromGT;

		// we call the destructor here to clean up the GT data. Without this we could be leaking memory.
		InstanceDataFromGT->~FNDIGaussianCountInstanceData();
	}

	TMap<FNiagaraSystemInstanceID, FNDIGaussianCountInstanceData> SystemInstancesToInstanceData_RT;
};

USceneNiagaraInterface::USceneNiagaraInterface(FObjectInitializer const& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Proxy.Reset(new FNDIGaussianCountProxy());
}

#if WITH_EDITORONLY_DATA
void USceneNiagaraInterface::GetFunctionsInternal(TArray<FNiagaraFunctionSignature>& OutFunctions) const
{
	FNiagaraFunctionSignature CountSig;
	CountSig.Name = GetGaussianCountName;
	CountSig.bMemberFunction = true;
	CountSig.AddInput(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("Scene Interface")));
	CountSig.AddOutput(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Count")));
	OutFunctions.Add(CountSig);

	// FNiagaraFunctionSignature DataSig;
	// DataSig.Name = GetGaussianDataName;
	// CountSig.bMemberFunction = true;
	// DataSig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("SceneNiagaraInterface")));
	// DataSig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Index")));
	// DataSig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Position")));
	// DataSig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Scale")));
	// OutFunctions.Add(DataSig);

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

	const USceneBufferAsset* Asset = SceneBufferAsset.LoadSynchronous();

	UE_LOG(LogTemp, Log,
	       TEXT("PostEditChangeProperty - Loaded SceneBufferAsset, Count=%d, SceneBufferAsset is nullptr:%d"),
	       Asset ? Asset->Gaussians.Num() : 0, (SceneBufferAsset == nullptr));
}

bool USceneNiagaraInterface::Equals(const UNiagaraDataInterface* Other) const
{
	if (!Super::Equals(Other))
	{
		return false;
	}

	return SceneBufferAsset == CastChecked<USceneNiagaraInterface>(Other)->SceneBufferAsset;
}

bool USceneNiagaraInterface::CanExecuteOnTarget(ENiagaraSimTarget Target) const
{
	return true;
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
	return FunctionInfo.DefinitionName == GetGaussianCountName;
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
	FNDIGaussianCountProxy& DataInterfaceProxy = Context.GetProxy<FNDIGaussianCountProxy>();
	FNDIGaussianCountInstanceData& InstanceData = DataInterfaceProxy.SystemInstancesToInstanceData_RT.FindChecked(
		Context.GetSystemInstanceID());

	FShaderParameters* ShaderParameters = Context.GetParameterNestedStruct<FShaderParameters>();

	ShaderParameters->GaussianCount = InstanceData.GaussianCount;
	UE_LOG(LogTemp, Log,
	       TEXT("USceneNiagaraInterface::SetShaderParameters - Set GaussianCount to %d for SystemInstanceID %llu"),
	       ShaderParameters->GaussianCount,
	       Context.GetSystemInstanceID());
}

int USceneNiagaraInterface::PerInstanceDataSize() const
{
	return sizeof(FNDIGaussianCountInstanceData);
}

bool USceneNiagaraInterface::InitPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance)
{
	FNDIGaussianCountInstanceData* InstanceData = new(PerInstanceData) FNDIGaussianCountInstanceData();

	const USceneBufferAsset* Asset = SceneBufferAsset.LoadSynchronous();
	InstanceData->GaussianCount = Asset ? Asset->Gaussians.Num() : 0;

	UE_LOG(LogTemp, Log,
	       TEXT("InitPerInstanceData - Loaded SceneBufferAsset, Count=%d"),
	       InstanceData->GaussianCount);

	return true;
}

bool USceneNiagaraInterface::PerInstanceTick(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance,
                                             const float DeltaSeconds)
{
	check(SystemInstance);
	FNDIGaussianCountInstanceData* InstanceData = static_cast<FNDIGaussianCountInstanceData*>(PerInstanceData);

	if (!InstanceData)
	{
		return true;
	}

	InstanceData->GaussianCount = SceneBufferAsset ? SceneBufferAsset->Gaussians.Num() : 0;
	UE_LOG(LogTemp, Log,
	       TEXT("USceneNiagaraInterface::PerInstanceTick - Updated GaussianCount to %d for SystemInstanceID %llu"),
	       InstanceData->GaussianCount,
	       SystemInstance->GetId());
	return false;
}

void USceneNiagaraInterface::ProvidePerInstanceDataForRenderThread(void* DataForRenderThread, void* PerInstanceData,
                                                                   const FNiagaraSystemInstanceID& SystemInstance)
{
	FNDIGaussianCountProxy::ProvidePerInstanceDataForRenderThread(DataForRenderThread, PerInstanceData, SystemInstance);
}

void USceneNiagaraInterface::DestroyPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance)
{
	FNDIGaussianCountInstanceData* InstanceData = static_cast<FNDIGaussianCountInstanceData*>(PerInstanceData);
	InstanceData->~FNDIGaussianCountInstanceData();

	ENQUEUE_RENDER_COMMAND(RemoveProxy)
	(
		[RT_Proxy=GetProxyAs<FNDIGaussianCountProxy>(), InstanceID=SystemInstance->GetId()](
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
	VectorVM::FUserPtrHandler<FNDIGaussianCountInstanceData> GaussianCountInstanceData(Context);
	FNDIOutputParam<int32> OutCount(Context);

	for (size_t i = 0; i < Context.GetNumInstances(); ++i)
	{
		OutCount.SetAndAdvance(GaussianCountInstanceData.Get()->GaussianCount);

		UE_LOG(LogTemp, Log,
		       TEXT(
			       "USceneNiagaraInterface::GetGaussianCountVM - Returned GaussianCount %d for instance %llu"
		       ),
		       GaussianCountInstanceData.Get()->GaussianCount,
		       i);
	}
}

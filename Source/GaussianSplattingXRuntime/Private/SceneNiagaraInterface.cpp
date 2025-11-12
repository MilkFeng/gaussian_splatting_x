#include "SceneNiagaraInterface.h"

#include "NiagaraCompileHashVisitor.h"
#include "NiagaraShaderParametersBuilder.h"

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
}

void USceneNiagaraInterface::GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo,
                                                   void* InstanceData, FVMExternalFunction& OutFunc)
{
	if (BindingInfo.Name == GetGaussianCountName)
	{
		OutFunc = FVMExternalFunction::CreateUObject(this, &USceneNiagaraInterface::GetGaussianCount);
	}
	else if (BindingInfo.Name == GetGaussianDataName)
	{
		OutFunc = FVMExternalFunction::CreateUObject(this, &USceneNiagaraInterface::GetGaussianData);
	}
	else
	{
		UE_LOG(LogTemp, Warning,
		       TEXT("USceneNiagaraInterface::GetVMExternalFunction - Unknown function name: %s"),
		       *BindingInfo.Name.ToString());
	}
}

#if WITH_EDITORONLY_DATA
void USceneNiagaraInterface::GetFunctionsInternal(TArray<FNiagaraFunctionSignature>& OutFunctions) const
{
	FNiagaraFunctionSignature CountSig;
	CountSig.Name = GetGaussianCountName;
	CountSig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("SceneNiagaraInterface")));
	CountSig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Count")));
	OutFunctions.Add(CountSig);

	FNiagaraFunctionSignature DataSig;
	DataSig.Name = GetGaussianDataName;
	DataSig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("SceneNiagaraInterface")));
	DataSig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Index")));
	DataSig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Position")));
	DataSig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Scale")));
	OutFunctions.Add(DataSig);
}
#endif

void USceneNiagaraInterface::GetGaussianCount(FVectorVMExternalFunctionContextProxy& Context) const
{
	VectorVM::FExternalFuncRegisterHandler<int32> OutCount(Context);

	for (int32 i = 0; i < Context.GetNumInstances(); ++i)
	{
		const int32 Count = SceneBufferAsset ? SceneBufferAsset->Gaussians.Num() : 0;
		*OutCount.GetDest() = Count;
	}
}

void USceneNiagaraInterface::GetGaussianData(FVectorVMExternalFunctionContextProxy& Context) const
{
	VectorVM::FExternalFuncInputHandler<int32> IndexParam(Context);
	VectorVM::FExternalFuncRegisterHandler<FVector> OutPosition(Context);
	VectorVM::FExternalFuncRegisterHandler<FVector> OutScale(Context);

	for (int32 i = 0; i < Context.GetNumInstances(); ++i)
	{
		const int32 Index = IndexParam.GetAndAdvance();
		if (SceneBufferAsset && SceneBufferAsset->Gaussians.IsValidIndex(Index))
		{
			const FGaussian& G = SceneBufferAsset->Gaussians[Index];
			*OutPosition.GetDest() = G.Position;
			*OutScale.GetDest() = G.Scale;
		}
		else
		{
			*OutPosition.GetDest() = FVector::ZeroVector;
			*OutScale.GetDest() = FVector::OneVector;
		}

		OutPosition.Advance();
		OutScale.Advance();
	}
}

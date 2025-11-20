#include "SceneNiagaraDataInterface.h"

#if WITH_EDITOR
#include "LevelEditorViewport.h"
#endif

#include "SceneActor.h"

#include "NiagaraCompileHashVisitor.h"
#include "NiagaraRendererProperties.h"
#include "NiagaraRenderer.h"
#include "NiagaraShaderParametersBuilder.h"
#include "NiagaraSystemInstance.h"
#include "SceneBufferAsset.h"

const FName USceneNiagaraDataInterface::GetGaussianCountName = TEXT("GetGaussianCount");
const FName USceneNiagaraDataInterface::GetGaussianDataName = TEXT("GetGaussianData");
const FString USceneNiagaraDataInterface::GaussianShaderFile = TEXT(
	"/Plugin/GaussianSplattingX/Private/SceneNiagaraInterface_Shader.ush");

struct FNDIGaussianInstanceData
{
	TSoftObjectPtr<USceneBufferAsset> SceneBufferAsset;

	FTransform CameraTransform;
	FTransform ActorTransform;

	void LoadSceneBufferAsset(const FSoftObjectPath& SceneBufferAssetPath)
	{
		SceneBufferAsset = TSoftObjectPtr<USceneBufferAsset>(SceneBufferAssetPath);
		SceneBufferAsset.LoadSynchronous();

		UE_LOG(LogTemp, Log,
		       TEXT("FNDIGaussianInstanceData::LoadSceneBufferAsset - Loaded SceneBufferAsset: %s, Valid: %d"),
		       *SceneBufferAssetPath.ToString(), SceneBufferAsset.IsValid());
	}

	const USceneBufferAsset& GetSceneBufferAsset() const
	{
		return *SceneBufferAsset;
	}

	bool IsValid() const
	{
		return SceneBufferAsset.IsValid();
	}

	size_t GetGaussianCount() const
	{
		return SceneBufferAsset.IsValid() ? SceneBufferAsset->GaussianCount : 0;
	}

	size_t GetSHCoefficientsCount() const
	{
		return SceneBufferAsset.IsValid() ? SceneBufferAsset->SHCoefficientsCount : 0;
	}
};

// this proxy is used to safely copy data between game thread and render thread
class FNDIGaussianProxy : public FNiagaraDataInterfaceProxy
{
public:
	virtual int32 PerInstanceDataPassedToRenderThreadSize() const override
	{
		return sizeof(FNDIGaussianInstanceData);
	}

	/// 负责将游戏线程的数据传递到渲染线程
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

	FNDIGaussianInstanceData& GetInstanceData_RT(const FNiagaraSystemInstanceID& InstanceID)
	{
		return SystemInstancesToInstanceData_RT.FindChecked(InstanceID);
	}

	void RemoveInstanceData_RT(const FNiagaraSystemInstanceID& InstanceID)
	{
		SystemInstancesToInstanceData_RT.Remove(InstanceID);
	}

	void PostSRV_RT(const FNiagaraSystemInstanceID& InstanceID)
	{
		const FNDIGaussianInstanceData& InstanceData = GetInstanceData_RT(InstanceID);
		const USceneBufferAsset& SceneBufferAsset = InstanceData.GetSceneBufferAsset();

		ENQUEUE_RENDER_COMMAND(FUpdateGaussianBuffer)(
			[this, &SceneBufferAsset](FRHICommandListImmediate& RHICmdList)
			{
				InitializeBuffer<FVector4f>(
					GaussianPositionOpacityBuffer, TEXT("PositionOpacityBuffer"),
					SceneBufferAsset.GaussianCount,
					PF_A32B32G32R32F, RHICmdList,
					[&SceneBufferAsset](const size_t Index, FVector4f& MappedData)
					{
						const FVector& Position = SceneBufferAsset.GaussianPositions[Index];
						MappedData.X = Position.X;
						MappedData.Y = Position.Y;
						MappedData.Z = Position.Z;
						MappedData.W = SceneBufferAsset.GaussianOpacities[Index];
					});

				InitializeBuffer<FVector4f>(
					GaussianSHCoefficientsBuffer, TEXT("SHCoefficientsBuffer"),
					SceneBufferAsset.GaussianCount * SceneBufferAsset.SHCoefficientsCount,
					PF_A32B32G32R32F, RHICmdList,
					[&SceneBufferAsset](const size_t Index, FVector4f& MappedData)
					{
						const FVector& Position = SceneBufferAsset.GaussianSHCoefficients[Index];
						MappedData.X = Position.X;
						MappedData.Y = Position.Y;
						MappedData.Z = Position.Z;
						MappedData.W = 1.0f; // padding
					});

				InitializeBuffer<FQuat4f>(
					GaussianRotationBuffer, TEXT("RotationBuffer"),
					SceneBufferAsset.GaussianCount,
					PF_A32B32G32R32F, RHICmdList,
					[&SceneBufferAsset](const size_t Index, FQuat4f& MappedData)
					{
						MappedData = FQuat4f(SceneBufferAsset.GaussianRotations[Index]);
					});

				InitializeBuffer<FVector4f>(
					GaussianScaleBuffer, TEXT("ScaleBuffer"),
					SceneBufferAsset.GaussianCount,
					PF_A32B32G32R32F, RHICmdList,
					[&SceneBufferAsset](const size_t Index, FVector4f& MappedData)
					{
						const FVector& Scale = SceneBufferAsset.GaussianScales[Index];
						MappedData.X = Scale.X;
						MappedData.Y = Scale.Y;
						MappedData.Z = Scale.Z;
						MappedData.W = 1.0f;
					});
			});
	}

private:
	template <typename TBufferElementType>
	static void InitializeBuffer(FReadBuffer& Buffer,
	                             const TCHAR* BufferName,
	                             const size_t BufferElementCount,
	                             const EPixelFormat BufferFormat,
	                             FRHICommandListImmediate& RHICmdList,
	                             TFunction<void(size_t Index, TBufferElementType& MappedData)> FillFunction)
	{
		const size_t BufferSize = BufferElementCount * sizeof(TBufferElementType);
		if (BufferSize > 0 && !Buffer.NumBytes)
		{
			Buffer.Initialize(RHICmdList, BufferName, sizeof(TBufferElementType), BufferElementCount, BufferFormat,
			                  BUF_Static);
		}

		if (Buffer.NumBytes > 0)
		{
			TBufferElementType* MappedData = static_cast<TBufferElementType*>(
				RHICmdList.LockBuffer(Buffer.Buffer, 0, BufferSize, RLM_WriteOnly));
			for (size_t i = 0; i < BufferElementCount; ++i)
			{
				FillFunction(i, MappedData[i]);
			}
			RHICmdList.UnlockBuffer(Buffer.Buffer);
		}
	}

private:
	// ================================ 每个 Niagara System 实例的数据 ===============================
	// note: 一定要在 InstanceData 中存储数据，不要在 Proxy 里面存，如果直接存储在 Proxy 里面，多个 Niagara System 实例会互相覆盖数据
	TMap<FNiagaraSystemInstanceID, FNDIGaussianInstanceData> SystemInstancesToInstanceData_RT;

public:
	// =============================== Buffer ===============================
	FReadBuffer GaussianPositionOpacityBuffer;
	FReadBuffer GaussianSHCoefficientsBuffer;
	FReadBuffer GaussianRotationBuffer;
	FReadBuffer GaussianScaleBuffer;
};

USceneNiagaraDataInterface::USceneNiagaraDataInterface(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Proxy.Reset(new FNDIGaussianProxy());
}

#if WITH_EDITORONLY_DATA
void USceneNiagaraDataInterface::GetFunctionsInternal(TArray<FNiagaraFunctionSignature>& OutFunctions) const
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
		Sig.AddOutput(FNiagaraVariable(FNiagaraTypeDefinition::GetVec4Def(), TEXT("Position")));
		Sig.AddOutput(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Index")));
		Sig.AddOutput(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Color")));
		OutFunctions.Add(Sig);
	}

	UE_LOG(LogTemp, Log,
	       TEXT("USceneNiagaraInterface::GetFunctionsInternal - Registered %d functions."),
	       OutFunctions.Num());
}
#endif

void USceneNiagaraDataInterface::PostInitProperties()
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

#if WITH_EDITOR
void USceneNiagaraDataInterface::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

bool USceneNiagaraDataInterface::CanExecuteOnTarget(ENiagaraSimTarget Target) const
{
	// CPU 和 GPU 都支持
	return true;
}

bool USceneNiagaraDataInterface::CopyToInternal(UNiagaraDataInterface* Destination) const
{
	if (!Super::CopyToInternal(Destination))
	{
		return false;
	}

	if (USceneNiagaraDataInterface* Other = Cast<USceneNiagaraDataInterface>(Destination))
	{
		Other->UserParameterBinding = UserParameterBinding;
	}
	return true;
}

bool USceneNiagaraDataInterface::Equals(const UNiagaraDataInterface* Other) const
{
	if (!Super::Equals(Other))
	{
		return false;
	}

	return UserParameterBinding == CastChecked<USceneNiagaraDataInterface>(Other)->UserParameterBinding;
}

#if WITH_EDITORONLY_DATA
bool USceneNiagaraDataInterface::AppendCompileHash(FNiagaraCompileHashVisitor* InVisitor) const
{
	if (!Super::AppendCompileHash(InVisitor))
	{
		return false;
	}

	InVisitor->UpdateShaderFile(*GaussianShaderFile);
	InVisitor->UpdateShaderParameters<FShaderParameters>();
	return true;
}

bool USceneNiagaraDataInterface::GetFunctionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo,
                                                 const FNiagaraDataInterfaceGeneratedFunction& FunctionInfo,
                                                 int FunctionInstanceIndex, FString& OutHLSL)
{
	return FunctionInfo.DefinitionName == GetGaussianDataName;
}

void USceneNiagaraDataInterface::GetParameterDefinitionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo,
                                                            FString& OutHLSL)
{
	const TMap<FString, FStringFormatArg> TemplateArgs = {
		{TEXT("ParameterName"), ParamInfo.DataInterfaceHLSLSymbol},
		{TEXT("GetGaussianDataName"), FStringFormatArg(GetGaussianDataName.ToString())},
	};
	AppendTemplateHLSL(OutHLSL, *GaussianShaderFile, TemplateArgs);
}
#endif

void USceneNiagaraDataInterface::BuildShaderParameters(FNiagaraShaderParametersBuilder& ShaderParametersBuilder) const
{
	ShaderParametersBuilder.AddNestedStruct<FShaderParameters>();
}

void USceneNiagaraDataInterface::SetShaderParameters(
	const FNiagaraDataInterfaceSetShaderParametersContext& Context) const
{
	FNDIGaussianProxy& DataInterfaceProxy = Context.GetProxy<FNDIGaussianProxy>();
	const FNDIGaussianInstanceData& InstanceData = DataInterfaceProxy.GetInstanceData_RT(Context.GetSystemInstanceID());

	FShaderParameters* ShaderParameters = Context.GetParameterNestedStruct<FShaderParameters>();

	// 将 SceneBufferAsset 的数据上传到 GPU Buffer
	if (InstanceData.SceneBufferAsset.IsValid())
	{
		DataInterfaceProxy.PostSRV_RT(Context.GetSystemInstanceID());
	}
	ShaderParameters->GaussianCount = InstanceData.GetGaussianCount();
	ShaderParameters->SHCoefficientsCount = InstanceData.GetSHCoefficientsCount();
	ShaderParameters->GaussianPositionOpacityBuffer = FNiagaraRenderer::GetSrvOrDefaultFloat(
		DataInterfaceProxy.GaussianPositionOpacityBuffer.SRV);
	ShaderParameters->GaussianRotationBuffer = FNiagaraRenderer::GetSrvOrDefaultFloat(
		DataInterfaceProxy.GaussianRotationBuffer.SRV);
	ShaderParameters->GaussianSHCoefficientsBuffer = FNiagaraRenderer::GetSrvOrDefaultFloat(
		DataInterfaceProxy.GaussianSHCoefficientsBuffer.SRV);
	ShaderParameters->GaussianScaleBuffer = FNiagaraRenderer::GetSrvOrDefaultFloat(
		DataInterfaceProxy.GaussianScaleBuffer.SRV);

	ShaderParameters->ActorTransformMatrix = FMatrix44f(
		InstanceData.ActorTransform.ToMatrixWithScale());
	const FVector4 CameraPosition = InstanceData.CameraTransform.GetLocation();
	ShaderParameters->CameraPosition = FVector4f(CameraPosition);
}

int USceneNiagaraDataInterface::PerInstanceDataSize() const
{
	return sizeof(FNDIGaussianInstanceData);
}

bool USceneNiagaraDataInterface::InitPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance)
{
	FNDIGaussianInstanceData* InstanceData = new(PerInstanceData) FNDIGaussianInstanceData();

	// 从 User Parameters 中获取 SceneNiagaraParameter，然后加载 SceneBufferAsset
	const FNiagaraUserRedirectionParameterStore* Store = SystemInstance->GetOverrideParameters();
	const USceneNiagaraParameter* NiagaraParameter = Cast<USceneNiagaraParameter>(
		Store->GetUObject(UserParameterBinding.Parameter));
	if (NiagaraParameter)
	{
		InstanceData->LoadSceneBufferAsset(NiagaraParameter->SceneBufferAssetPath);
	}
	else
	{
		UE_LOG(LogTemp, Warning,
		       TEXT(
			       "USceneNiagaraInterface::InitPerInstanceData - Failed to get User.SceneNiagaraParameter from User Parameters"
		       ));
	}
	return true;
}

bool USceneNiagaraDataInterface::PerInstanceTick(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance,
                                                 const float DeltaSeconds)
{
	check(SystemInstance);
	FNDIGaussianInstanceData* InstanceData = static_cast<FNDIGaussianInstanceData*>(PerInstanceData);

	if (!InstanceData)
	{
		return true;
	}

	// 得到当前 System 对象相对于相机的 Transform
	InstanceData->CameraTransform = GetCameraTransform(SystemInstance);
	InstanceData->ActorTransform = GetActorTransform(SystemInstance);
	return false;
}

void USceneNiagaraDataInterface::ProvidePerInstanceDataForRenderThread(void* DataForRenderThread, void* PerInstanceData,
                                                                       const FNiagaraSystemInstanceID& SystemInstance)
{
	FNDIGaussianProxy::ProvidePerInstanceDataForRenderThread(DataForRenderThread, PerInstanceData, SystemInstance);
}

void USceneNiagaraDataInterface::DestroyPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance)
{
	FNDIGaussianInstanceData* InstanceData = static_cast<FNDIGaussianInstanceData*>(PerInstanceData);
	InstanceData->~FNDIGaussianInstanceData();

	ENQUEUE_RENDER_COMMAND(RemoveProxy)
	(
		[RT_Proxy=GetProxyAs<FNDIGaussianProxy>(), InstanceID=SystemInstance->GetId()](
		FRHICommandListImmediate& CmdList)
		{
			RT_Proxy->RemoveInstanceData_RT(InstanceID);
		}
	);
}

bool USceneNiagaraDataInterface::HasPreSimulateTick() const
{
	return true;
}

void USceneNiagaraDataInterface::GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo,
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

void USceneNiagaraDataInterface::GetGaussianCountVM(FVectorVMExternalFunctionContext& Context) const
{
	VectorVM::FUserPtrHandler<FNDIGaussianInstanceData> InstanceData(Context);
	FNDIOutputParam<int> OutCount(Context);

	const int Count = InstanceData->GetGaussianCount();
	for (size_t i = 0; i < Context.GetNumInstances(); ++i)
	{
		OutCount.SetAndAdvance(Count);
	}
}

FTransform USceneNiagaraDataInterface::GetCameraTransform(const FNiagaraSystemInstance* SystemInstance) const
{
	const UWorld* World = SystemInstance->GetWorld();
	if (!World)
	{
		return FTransform::Identity;
	}

	if (World->IsGameWorld())
	{
		const APlayerController* PC = World->GetFirstPlayerController();
		if (PC && PC->PlayerCameraManager)
		{
			UE_LOG(LogTemp, Log,
			       TEXT(
				       "USceneNiagaraInterface::GetCameraTransform - Found PlayerCameraManager: %s"
			       ),
			       *PC->PlayerCameraManager->GetName());
			const APlayerCameraManager* CameraManager = PC->PlayerCameraManager;

			const FVector CameraLocation = CameraManager->GetCameraLocation();
			const FRotator CameraRotation = CameraManager->GetCameraRotation();

			return FTransform(CameraRotation, CameraLocation);
		}
	}
#if WITH_EDITOR
	else
	{
		for (const auto LevelVC : GEditor->GetLevelViewportClients())
		{
			if (LevelVC && LevelVC->IsPerspective())
			{
				const FVector CameraLocation = LevelVC->GetViewLocation();
				const FRotator CameraRotation = LevelVC->GetViewRotation();

				return FTransform(CameraRotation, CameraLocation);
			}
		}
	}
#endif

	return FTransform::Identity;
}

FTransform USceneNiagaraDataInterface::GetActorTransform(FNiagaraSystemInstance* SystemInstance) const
{
	if (const AActor* Owner = SystemInstance->GetAttachComponent()->GetOwner())
	{
		return Owner->GetActorTransform();
	}
	return FTransform::Identity;
}

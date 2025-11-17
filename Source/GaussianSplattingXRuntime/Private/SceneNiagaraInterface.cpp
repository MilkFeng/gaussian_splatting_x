#include "SceneNiagaraInterface.h"
#include "SceneActor.h"

#include "NiagaraCompileHashVisitor.h"
#include "NiagaraRendererProperties.h"
#include "NiagaraRenderer.h"
#include "NiagaraShaderParametersBuilder.h"
#include "NiagaraSystemInstance.h"
#include "SceneBufferAsset.h"
#include "Kismet/GameplayStatics.h"

const FName USceneNiagaraInterface::GetGaussianCountName = TEXT("GetGaussianCount");
const FName USceneNiagaraInterface::GetGaussianDataName = TEXT("GetGaussianData");
const FString USceneNiagaraInterface::GaussianShaderFile = TEXT(
	"/Plugin/GaussianSplattingX/Private/SceneNiagaraInterface_Shader.ush");

struct FNDIGaussianInstanceData
{
	TSoftObjectPtr<USceneBufferAsset> SceneBufferAsset;
	FTransform TransformInCamera;

	void LoadSceneBufferAsset(const FSoftObjectPath& SceneBufferAssetPath)
	{
		SceneBufferAsset = TSoftObjectPtr<USceneBufferAsset>(SceneBufferAssetPath);
		SceneBufferAsset.LoadSynchronous();

		UE_LOG(LogTemp, Log,
		       TEXT("FNDIGaussianInstanceData::LoadSceneBufferAsset - Loaded SceneBufferAsset: %s"),
		       *SceneBufferAssetPath.ToString());

		UE_LOG(LogTemp, Log,
		       TEXT("FNDIGaussianInstanceData::LoadSceneBufferAsset - GaussianCount: %d, SHCoefficientsCount: %d"),
		       SceneBufferAsset->GaussianCount,
		       SceneBufferAsset->SHCoefficientsCount);

		if (SceneBufferAsset->GaussianCount > 0)
		{
			UE_LOG(LogTemp, Log,
			       TEXT("FNDIGaussianInstanceData::LoadSceneBufferAsset - First Gaussian SH Coefficients: (%f, %f, %f)"
			       ),
			       SceneBufferAsset->GaussianSHCoefficients[0].X,
			       SceneBufferAsset->GaussianSHCoefficients[0].Y,
			       SceneBufferAsset->GaussianSHCoefficients[0].Z);
		}
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
		if (InstanceDataFromGT->SceneBufferAsset != InstanceData.SceneBufferAsset)
		{
		}
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

	void PostSRV(const USceneBufferAsset& SceneBufferAsset)
	{
		ENQUEUE_RENDER_COMMAND(FUpdateGaussianBuffer)(
			[this, &SceneBufferAsset](FRHICommandListImmediate& RHICmdList)
			{
				{
					const size_t BufferSize = SceneBufferAsset.GaussianCount * sizeof(FVector4f);

					// 初始化 Buffer
					if (BufferSize > 0 && !GaussianPositionBuffer.NumBytes)
					{
						GaussianPositionBuffer.Initialize(RHICmdList, TEXT("PositionBuffer"),
						                                  sizeof(FVector4f),
						                                  SceneBufferAsset.GaussianCount, PF_A32B32G32R32F,
						                                  BUF_Static);
					}

					// 如果初始化成功，更新数据到 Buffer
					if (GaussianPositionBuffer.NumBytes > 0)
					{
						// 上锁，准备写入数据
						float* MappedData = static_cast<float*>(
							RHICmdList.LockBuffer(GaussianPositionBuffer.Buffer, 0, BufferSize, RLM_WriteOnly));
						// 先将 Vector3 转换成 Vector4，再复制到 GPU Buffer
						for (size_t i = 0; i < SceneBufferAsset.GaussianCount; ++i)
						{
							const FVector& Position = SceneBufferAsset.GaussianPositions[i];
							MappedData[i * 4 + 0] = Position.X;
							MappedData[i * 4 + 1] = Position.Y;
							MappedData[i * 4 + 2] = Position.Z;
							MappedData[i * 4 + 3] = 1.0f; // padding
						}
						RHICmdList.UnlockBuffer(GaussianPositionBuffer.Buffer);
					}
				}
				{
					const size_t TotalSHCoefficientsCount = SceneBufferAsset.GaussianCount * SceneBufferAsset.
						SHCoefficientsCount;

					const size_t BufferSize = TotalSHCoefficientsCount * sizeof(FVector4f);

					if (BufferSize > 0 && !GaussianSHCoefficientsBuffer.NumBytes)
					{
						GaussianSHCoefficientsBuffer.Initialize(RHICmdList, TEXT("SHCoefficientsBuffer"),
						                                        sizeof(FVector4f),
						                                        TotalSHCoefficientsCount, PF_A32B32G32R32F,
						                                        BUF_Static);
					}

					if (GaussianSHCoefficientsBuffer.NumBytes > 0)
					{
						// 上锁，准备写入数据
						float* MappedData = static_cast<float*>(
							RHICmdList.LockBuffer(GaussianSHCoefficientsBuffer.Buffer, 0, BufferSize,
							                      RLM_WriteOnly));
						for (size_t i = 0; i < TotalSHCoefficientsCount; ++i)
						{
							const FVector& SH = SceneBufferAsset.GaussianSHCoefficients[i];
							MappedData[i * 4 + 0] = SH.X;
							MappedData[i * 4 + 1] = SH.Y;
							MappedData[i * 4 + 2] = SH.Z;
							MappedData[i * 4 + 3] = 1.0f; // padding
						}
						RHICmdList.UnlockBuffer(GaussianSHCoefficientsBuffer.Buffer);
					}
				}
			});
	}

private:
	// ================================ 每个 Niagara System 实例的数据 ===============================
	// note: 一定要在 InstanceData 中存储数据，不要在 Proxy 里面存，如果直接存储在 Proxy 里面，多个 Niagara System 实例会互相覆盖数据
	TMap<FNiagaraSystemInstanceID, FNDIGaussianInstanceData> SystemInstancesToInstanceData_RT;

public:
	// =============================== Buffer ===============================
	FReadBuffer GaussianPositionBuffer;
	FReadBuffer GaussianSHCoefficientsBuffer;
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

	InVisitor->UpdateShaderFile(*GaussianShaderFile);
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
		{TEXT("GetGaussianDataName"), FStringFormatArg(GetGaussianDataName.ToString())},
	};
	AppendTemplateHLSL(OutHLSL, *GaussianShaderFile, TemplateArgs);
}

void USceneNiagaraInterface::BuildShaderParameters(FNiagaraShaderParametersBuilder& ShaderParametersBuilder) const
{
	ShaderParametersBuilder.AddNestedStruct<FShaderParameters>();
}

void USceneNiagaraInterface::SetShaderParameters(const FNiagaraDataInterfaceSetShaderParametersContext& Context) const
{
	FNDIGaussianProxy& DataInterfaceProxy = Context.GetProxy<FNDIGaussianProxy>();
	const FNDIGaussianInstanceData& InstanceData = DataInterfaceProxy.GetInstanceData_RT(Context.GetSystemInstanceID());

	FShaderParameters* ShaderParameters = Context.GetParameterNestedStruct<FShaderParameters>();

	// 将 SceneBufferAsset 的数据上传到 GPU Buffer
	if (InstanceData.SceneBufferAsset.IsValid())
	{
		DataInterfaceProxy.PostSRV(*InstanceData.SceneBufferAsset.Get());
	}
	ShaderParameters->GaussianCount = InstanceData.GetGaussianCount();
	ShaderParameters->SHCoefficientsCount = InstanceData.GetSHCoefficientsCount();
	ShaderParameters->GaussianPositionBuffer = FNiagaraRenderer::GetSrvOrDefaultFloat(
		DataInterfaceProxy.GaussianPositionBuffer.SRV);
	ShaderParameters->GaussianSHCoefficientsBuffer = FNiagaraRenderer::GetSrvOrDefaultFloat(
		DataInterfaceProxy.GaussianSHCoefficientsBuffer.SRV);

	// 把当前 Actor 相对于相机的位置、旋转传递给 GPU
	ShaderParameters->ActorTransformMatrixInCamera = FMatrix44f(
		InstanceData.TransformInCamera.ToMatrixWithScale());

	UE_LOG(LogTemp, Error,
	       TEXT(
		       "USceneNiagaraInterface::SetShaderParameters - Update TransformInCamera Matrix [%f, %f, %f, %f; %f, %f, %f, %f; %f, %f, %f, %f; %f, %f, %f, %f]"
	       ),
	       ShaderParameters->ActorTransformMatrixInCamera.M[0][0],
	       ShaderParameters->ActorTransformMatrixInCamera.M[0][1],
	       ShaderParameters->ActorTransformMatrixInCamera.M[0][2],
	       ShaderParameters->ActorTransformMatrixInCamera.M[0][3],
	       ShaderParameters->ActorTransformMatrixInCamera.M[1][0],
	       ShaderParameters->ActorTransformMatrixInCamera.M[1][1],
	       ShaderParameters->ActorTransformMatrixInCamera.M[1][2],
	       ShaderParameters->ActorTransformMatrixInCamera.M[1][3],
	       ShaderParameters->ActorTransformMatrixInCamera.M[2][0],
	       ShaderParameters->ActorTransformMatrixInCamera.M[2][1],
	       ShaderParameters->ActorTransformMatrixInCamera.M[2][2],
	       ShaderParameters->ActorTransformMatrixInCamera.M[2][3],
	       ShaderParameters->ActorTransformMatrixInCamera.M[3][0],
	       ShaderParameters->ActorTransformMatrixInCamera.M[3][1],
	       ShaderParameters->ActorTransformMatrixInCamera.M[3][2],
	       ShaderParameters->ActorTransformMatrixInCamera.M[3][3]);
}

int USceneNiagaraInterface::PerInstanceDataSize() const
{
	return sizeof(FNDIGaussianInstanceData);
}

bool USceneNiagaraInterface::InitPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance)
{
	FNDIGaussianInstanceData* InstanceData = new(PerInstanceData) FNDIGaussianInstanceData();

	// 从 User Parameters 中获取 SceneNiagaraParameter，然后加载 SceneBufferAsset
	{
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

	UE_LOG(LogTemp, Error,
	       TEXT("USceneNiagaraInterface::PerInstanceTick - Ticking System Instance ID: %llu"),
	       SystemInstance->GetId());

	// 得到当前 System 对象相对于相机的 Transform
	InstanceData->TransformInCamera = GetActorTransformInCamera(SystemInstance);
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
			RT_Proxy->RemoveInstanceData_RT(InstanceID);
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
	VectorVM::FUserPtrHandler<FNDIGaussianInstanceData> InstanceData(Context);
	FNDIOutputParam<int> OutCount(Context);

	const int Count = InstanceData->GetGaussianCount();
	for (size_t i = 0; i < Context.GetNumInstances(); ++i)
	{
		OutCount.SetAndAdvance(Count);
	}
}

FTransform USceneNiagaraInterface::GetCameraTransform() const
{
	const UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return FTransform::Identity;
	}

	if (World->WorldType == EWorldType::Game ||
		World->WorldType == EWorldType::PIE ||
		World->WorldType == EWorldType::GameRPC)
	{
		if (const APlayerController* PC = World->GetFirstPlayerController())
		{
			if (const APlayerCameraManager* CamMgr = PC->PlayerCameraManager)
			{
				return FTransform(CamMgr->GetCameraRotation(), CamMgr->GetCameraLocation());
			}
		}
	}

#if WITH_EDITOR
	// =============================== Editor 世界（Viewport） ===============================
	if (World->WorldType == EWorldType::Editor ||
		World->WorldType == EWorldType::EditorPreview ||
		World->WorldType == EWorldType::Inactive)
	{
		// 找到当前激活的 Editor 视口
		for (const FEditorViewportClient* Client : GEditor->GetAllViewportClients())
		{
			if (Client && Client->Viewport && Client->Viewport->HasFocus())
			{
				const FVector CamLoc = Client->GetViewLocation();
				const FRotator CamRot = Client->GetViewRotation();
				return FTransform(CamRot, CamLoc);
			}
		}

		// 如果没有焦点，就取最后一个 view client
		for (const FEditorViewportClient* Client : GEditor->GetAllViewportClients())
		{
			if (Client)
			{
				const FVector CamLoc = Client->GetViewLocation();
				const FRotator CamRot = Client->GetViewRotation();
				return FTransform(CamRot, CamLoc);
			}
		}
	}
#endif

	return FTransform::Identity;
}

FTransform USceneNiagaraInterface::GetActorTransformInCamera(FNiagaraSystemInstance* SystemInstance) const
{
	const FTransform CameraTransform = GetCameraTransform();
	if (const AActor* Owner = SystemInstance->GetAttachComponent()->GetOwner())
	{
		return Owner->GetActorTransform().GetRelativeTransform(CameraTransform);
	}
	return FTransform::Identity;
}

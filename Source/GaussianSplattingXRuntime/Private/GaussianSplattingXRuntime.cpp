#include "GaussianSplattingXRuntime.h"

#include "SceneNiagaraRendererProperties.h"
#include "Interfaces/IPluginManager.h"

#if WITH_EDITOR
#include "NiagaraEditorModule.h"
#endif

#define LOCTEXT_NAMESPACE "FGaussianSplattingXRuntimeModule"

void FGaussianSplattingXRuntimeModule::StartupModule()
{
	// map the shader dir so we can use it in the data interface
	const FString PluginShaderDir = FPaths::Combine(
		IPluginManager::Get().FindPlugin(TEXT("GaussianSplattingX"))->GetBaseDir(), TEXT("Shaders"));
	AddShaderSourceDirectoryMapping(TEXT("/Plugin/GaussianSplattingX"), PluginShaderDir);

	// 初始化 Niagara 渲染器属性的 CDO 属性
	USceneNiagaraRendererProperties::InitCDOPropertiesAfterModuleStartup();
#if WITH_EDITOR
	// 注册 Niagara 渲染器
	FNiagaraEditorModule& NiagaraEditorModule = FNiagaraEditorModule::Get();
	NiagaraEditorModule.RegisterRendererCreationInfo(FNiagaraRendererCreationInfo(
		USceneNiagaraRendererProperties::StaticClass()->GetDisplayNameText(),
		FText::FromString(USceneNiagaraRendererProperties::StaticClass()->GetDescription()),
		USceneNiagaraRendererProperties::StaticClass()->GetClassPathName(),
		FNiagaraRendererCreationInfo::FRendererFactory::CreateLambda([](UObject* OuterEmitter)
		{
			USceneNiagaraRendererProperties* NewRenderer = NewObject<USceneNiagaraRendererProperties>(
				OuterEmitter, NAME_None, RF_Transactional);
			return NewRenderer;
		})));
#endif
}

void FGaussianSplattingXRuntimeModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FGaussianSplattingXRuntimeModule, GaussianSplattingXRuntime)

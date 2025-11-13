#include "GaussianSplattingXRuntime.h"

#include "Interfaces/IPluginManager.h"

#define LOCTEXT_NAMESPACE "FGaussianSplattingXRuntimeModule"

void FGaussianSplattingXRuntimeModule::StartupModule()
{
	// map the shader dir so we can use it in the data interface
	const FString PluginShaderDir = FPaths::Combine(
		IPluginManager::Get().FindPlugin(TEXT("GaussianSplattingX"))->GetBaseDir(), TEXT("Shaders"));
	AddShaderSourceDirectoryMapping(TEXT("/Plugin/GaussianSplattingX"), PluginShaderDir);
}

void FGaussianSplattingXRuntimeModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FGaussianSplattingXRuntimeModule, GaussianSplattingXRuntime)

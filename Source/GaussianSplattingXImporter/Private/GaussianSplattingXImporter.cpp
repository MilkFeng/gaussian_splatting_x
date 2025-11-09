#include "GaussianSplattingXImporter.h"
#include "tinyply.h"

#define LOCTEXT_NAMESPACE "FGaussianSplattingXImporterModule"

void FGaussianSplattingXImporterModule::StartupModule()
{
	UE_LOG(LogTemp, Log, TEXT("GaussianSplattingXImporter loaded"));
}

void FGaussianSplattingXImporterModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FGaussianSplattingXImporterModule, GaussianSplattingXImporter)

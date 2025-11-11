#pragma once

#include "GaussianSplattingXRuntime/Public/SceneBufferAsset.h"

class GAUSSIANSPLATTINGXIMPORTER_API FSceneManager
{
public:
	static void ImportScene(const FString& FilePath, TFunction<void(float)> OnProgress = {});

private:
	static FString ImportPlyFile(const FString& FilePath, TFunction<void(float)> OnProgress = {});
	static bool ReadPlyFile(const FString& FilePath, USceneBufferAsset& Scene, TFunction<void(float)> OnProgress);

	static void CreateActorInContentBrowser(const FString& SceneBufferAssetPath);
};

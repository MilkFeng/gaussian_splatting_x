#pragma once

#include "SceneBufferAsset.h"

class GAUSSIANSPLATTINGXIMPORTER_API FSceneManager
{
public:
	static void ImportPlyFile(const FString& FilePath, TFunction<void(float)> OnProgress = {});

private:
	static void ReadScene(const FString& FilePath, USceneBufferAsset& Scene, TFunction<void(float)> OnProgress);
};

#include "SceneManager.h"

#include <format>
#include <fstream>

#include "SceneActor.h"
#include "tinyply.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/SavePackage.h"

void FSceneManager::ImportScene(const FString& FilePath, TFunction<void(float)> OnProgress)
{
	OnProgress(0.0f);
	UE_LOG(LogTemp, Log, TEXT("Importing PLY file: %s"), *FilePath);
	const FString SceneBufferAssetPath = ImportPlyFile(FilePath,
	                                                   [&OnProgress](const float Progress)
	                                                   {
		                                                   OnProgress(Progress * 0.8f);
	                                                   });

	OnProgress(0.8f);
	UE_LOG(LogTemp, Log, TEXT("Creating new Scene Actor to Content Browser"));
	CreateActorInContentBrowser(SceneBufferAssetPath);

	OnProgress(1.0f);
	UE_LOG(LogTemp, Log, TEXT("Import process completed."));
}

FString FSceneManager::ImportPlyFile(const FString& FilePath, TFunction<void(float)> OnProgress)
{
	OnProgress(0.0f);
	UE_LOG(LogTemp, Log, TEXT("Starting import of PLY file: %s"), *FilePath);
	const FString Name = FPaths::GetBaseFilename(FilePath);
	const FString PackageName = FString::Format(TEXT("/Game/GaussianSplattingX/{0}"), {Name});
	const FString PackageFilename = FPackageName::LongPackageNameToFilename(
		PackageName, FPackageName::GetAssetPackageExtension());

	UPackage* Package = CreatePackage(*PackageName);
	USceneBufferAsset* SceneBufferAsset = NewObject<USceneBufferAsset>(Package, USceneBufferAsset::StaticClass(), *Name,
	                                                                   RF_Public | RF_Standalone);

	OnProgress(0.1f);
	UE_LOG(LogTemp, Log, TEXT("Reading PLY file: %s"), *FilePath);
	const bool Success = ReadPlyFile(FilePath, *SceneBufferAsset,
	                                 [&OnProgress](const float Progress)
	                                 {
		                                 OnProgress(0.1f + Progress * 0.8f);
	                                 });

	if (!Success)
	{
		OnProgress(1.0f);
		return "";
	}

	OnProgress(0.9f);
	UE_LOG(LogTemp, Log, TEXT("Saving asset to package: %s"), *PackageFilename);

	FAssetRegistryModule::AssetCreated(SceneBufferAsset);
	[[maybe_unused]] bool Suppressed = Package->MarkPackageDirty();

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	UPackage::SavePackage(Package, SceneBufferAsset, *PackageFilename, SaveArgs);

	UE_LOG(LogTemp, Log, TEXT("PLY import completed successfully."));
	OnProgress(1.0f);

	// 返回 Buffer 的引用路径
	return PackageName + TEXT(".") + Name;
}

bool FSceneManager::ReadPlyFile(const FString& FilePath, USceneBufferAsset& Scene, TFunction<void(float)> OnProgress)
{
	try
	{
		const std::string StdFilePath = std::string(TCHAR_TO_UTF8(*FilePath));
		std::ifstream FileStream(StdFilePath, std::ios::binary);

		tinyply::PlyFile File;
		File.parse_header(FileStream);

		uint32_t NumRestSHCoefficients = 0;
		const tinyply::PlyElement VertexElement = File.get_elements()[0];
		for (const auto& Prop : VertexElement.properties)
		{
			if (FString Name = FString(Prop.name.c_str()); Name.StartsWith("f_rest"))
			{
				++NumRestSHCoefficients;
			}
		}

		Scene.SHCoefficientsCount = NumRestSHCoefficients / 3 + 1;
		Scene.SHDim = round(sqrt(Scene.SHCoefficientsCount)) - 1;
		UE_LOG(LogTemp, Log, TEXT("Detected SH Dimension: %d"), Scene.SHDim);

		// 读取顶点数据
		std::vector<std::string> PropertyKeys = {
			"x", "y", "z",
			"opacity",
			"scale_0", "scale_1", "scale_2",
			"rot_0", "rot_1", "rot_2", "rot_3",
			"f_dc_0", "f_dc_1", "f_dc_2",
		};

		for (size_t i = 0; i < NumRestSHCoefficients; ++i)
		{
			PropertyKeys.push_back(std::format("f_rest_{}", i));
		}

		const std::shared_ptr<tinyply::PlyData> Vertices = File.request_properties_from_element(
			"vertex", PropertyKeys);

		File.read(FileStream);

		Scene.Gaussians.SetNum(Vertices->count);
		for (size_t i = 0; i < Vertices->count; ++i)
		{
			const float* VertexData = reinterpret_cast<const float*>(Vertices->buffer.get_const()) + i * PropertyKeys.
				size();

			FGaussian& Gaussian = Scene.Gaussians[i];
			Gaussian.Position = FVector{VertexData[0], VertexData[1], VertexData[2]};
			Gaussian.Opacity = VertexData[3];
			Gaussian.Scale = FVector{VertexData[4], VertexData[5], VertexData[6]};
			Gaussian.Rotation = FQuat{VertexData[7], VertexData[8], VertexData[9], VertexData[10]};
			Gaussian.SHDim = Scene.SHDim;
			Gaussian.SHCoefficientsCount = Scene.SHCoefficientsCount;
			Gaussian.SHCoefficients.SetNumZeroed(Gaussian.SHCoefficientsCount);
			for (size_t j = 0; j < Gaussian.SHCoefficientsCount; ++j)
			{
				for (size_t k = 0; k < 3; ++k)
				{
					Gaussian.SHCoefficients[j][k] = VertexData[11 + j * 3 + k];
				}
			}

			OnProgress(static_cast<float>(i + 1) / static_cast<float>(Vertices->count));

			UE_LOG(LogTemp, Verbose,
			       TEXT(
				       "Gaussian %llu: Pos(%.3f, %.3f, %.3f), Scale(%.3f, %.3f, %.3f), Rot(%.3f, %.3f, %.3f, %.3f), Alpha(%.3f), SH(%.3f, %.3f, %.3f, ..., %.3f, %.3f, %.3f)"
			       ),
			       i,
			       Gaussian.Position.X, Gaussian.Position.Y, Gaussian.Position.Z,
			       Gaussian.Scale.X, Gaussian.Scale.Y, Gaussian.Scale.Z,
			       Gaussian.Rotation.X, Gaussian.Rotation.Y, Gaussian.Rotation.Z, Gaussian.Rotation.W,
			       Gaussian.Opacity,
			       Gaussian.SHCoefficients[0][0], Gaussian.SHCoefficients[0][1], Gaussian.SHCoefficients[0][2],
			       Gaussian.SHCoefficients[Gaussian.SHCoefficientsCount - 1][0],
			       Gaussian.SHCoefficients[Gaussian.SHCoefficientsCount - 1][1],
			       Gaussian.SHCoefficients[Gaussian.SHCoefficientsCount - 1][2]);
		}

		UE_LOG(LogTemp, Log, TEXT("Successfully read %d Gaussians from PLY file."), Scene.Gaussians.Num());
	}
	catch (const std::exception& e)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to read PLY file: %s"), *FString(e.what()));
		return false;
	}

	return true;
}

void FSceneManager::CreateActorInContentBrowser(const FString& SceneBufferAssetPath)
{
	const FString Name = FPaths::GetBaseFilename(SceneBufferAssetPath) + TEXT("_Actor");
	const FString PackageName = FString::Format(TEXT("/Game/GaussianSplattingX/{0}"), {Name});
	const FString PackageFilename = FPackageName::LongPackageNameToFilename(
		PackageName, FPackageName::GetAssetPackageExtension());

	UPackage* Package = CreatePackage(*PackageName);

	// 创建一个蓝图，基类是 ASceneActor，这样可以保存成一个正常的资产
	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
		ASceneActor::StaticClass(), // 基类
		CreatePackage(*PackageName),
		*Name,
		EBlueprintType::BPTYPE_Normal,
		UBlueprint::StaticClass(),
		UBlueprintGeneratedClass::StaticClass()
	);

	// 为蓝图基类中的属性赋值
	ASceneActor* SceneActor = Blueprint->GeneratedClass->GetDefaultObject<ASceneActor>();
	SceneActor->SceneNiagaraInterface = NewObject<USceneNiagaraInterface>(SceneActor);
	SceneActor->SceneNiagaraInterface->SceneBufferAsset = TSoftObjectPtr<USceneBufferAsset>(
		FSoftObjectPath(SceneBufferAssetPath));

	// 注册资产
	FAssetRegistryModule::AssetCreated(Blueprint);
	[[maybe_unused]] bool Suppressed = Package->MarkPackageDirty();

	// 保存资产到包
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags = SAVE_NoError;

	UPackage::SavePackage(Package, SceneActor, *PackageFilename, SaveArgs);
	UE_LOG(LogTemp, Log, TEXT("Created Scene Actor asset at: %s"), *PackageFilename);
}

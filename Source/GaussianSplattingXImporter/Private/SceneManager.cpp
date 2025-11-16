#include "SceneManager.h"

#include <format>
#include <fstream>

#include "BlueprintCompilationManager.h"
#include "FileHelpers.h"
#include "SceneActor.h"
#include "tinyply.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/SavePackage.h"

void FSceneManager::ImportScene(const FString& FilePath, TFunction<void(float)> OnProgress)
{
	// 读取 .ply 文件，创建 SceneBufferAsset 资产
	OnProgress(0.0f);
	UE_LOG(LogTemp, Log, TEXT("Importing PLY file: %s"), *FilePath);
	const FString SceneBufferAssetPath = ImportPlyFile(FilePath,
	                                                   [&OnProgress](const float Progress)
	                                                   {
		                                                   // 进度映射到 0.0 - 0.8
		                                                   OnProgress(Progress * 0.8f);
	                                                   });

	// 创建一个 Scene Actor 蓝图资产引用它
	OnProgress(0.8f);
	UE_LOG(LogTemp, Log, TEXT("Creating new Scene Actor to Content Browser"));
	CreateActorInContentBrowser(SceneBufferAssetPath);

	OnProgress(1.0f);
	UE_LOG(LogTemp, Log, TEXT("Import process completed."));
}

FString FSceneManager::ImportPlyFile(const FString& FilePath, TFunction<void(float)> OnProgress)
{
	// 创建一个新的包和 SceneBufferAsset 资产
	OnProgress(0.0f);
	UE_LOG(LogTemp, Log, TEXT("Starting import of PLY file: %s"), *FilePath);
	const FString Name = FPaths::GetBaseFilename(FilePath);
	const FString PackageName = FString::Format(TEXT("/Game/GaussianSplattingX/{0}"), {Name});
	const FString PackageFilename = FPackageName::LongPackageNameToFilename(
		PackageName, FPackageName::GetAssetPackageExtension());

	UPackage* Package = CreatePackage(*PackageName);
	USceneBufferAsset* SceneBufferAsset = NewObject<USceneBufferAsset>(Package, USceneBufferAsset::StaticClass(), *Name,
	                                                                   RF_Public | RF_Standalone);

	// 读取 PLY 文件并填充数据
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

	// 保存资产到包中
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

		// 解析 PLY 头，获取顶点数量和属性
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

		// 读取所有的顶点
		File.read(FileStream);

		Scene.SetGaussianCount(Vertices->count);
		for (size_t i = 0; i < Scene.GaussianCount; ++i)
		{
			const float* VertexData = reinterpret_cast<const float*>(Vertices->buffer.get_const()) + i * PropertyKeys.
				size();

			Scene.GaussianPositions[i] = FVector{VertexData[0], VertexData[1], VertexData[2]};
			Scene.GaussianOpacities[i] = VertexData[3];
			Scene.GaussianScales[i] = FVector{VertexData[4], VertexData[5], VertexData[6]};
			Scene.GaussianRotations[i] = FQuat{VertexData[7], VertexData[8], VertexData[9], VertexData[10]};
			for (size_t j = 0; j < Scene.SHCoefficientsCount; ++j)
			{
				for (size_t k = 0; k < 3; ++k)
				{
					Scene.GaussianSHCoefficients[i * Scene.SHCoefficientsCount + j][k] =
						VertexData[11 + j * 3 + k];
				}
			}

			OnProgress(static_cast<float>(i + 1) / static_cast<float>(Vertices->count));

			UE_LOG(LogTemp, Verbose,
			       TEXT(
				       "Gaussian %llu: Pos(%.3f, %.3f, %.3f), Scale(%.3f, %.3f, %.3f), Rot(%.3f, %.3f, %.3f, %.3f), Alpha(%.3f), SH(%.3f, %.3f, %.3f, ..., %.3f, %.3f, %.3f)"
			       ),
			       i,
			       Scene.GaussianPositions[i].X, Scene.GaussianPositions[i].Y, Scene.GaussianPositions[i].Z,
			       Scene.GaussianScales[i].X, Scene.GaussianScales[i].Y, Scene.GaussianScales[i].Z,
			       Scene.GaussianRotations[i].X, Scene.GaussianRotations[i].Y, Scene.GaussianRotations[i].Z,
			       Scene.GaussianRotations[i].W,
			       Scene.GaussianOpacities[i],
			       Scene.GaussianSHCoefficients[i * Scene.SHCoefficientsCount + 0][0],
			       Scene.GaussianSHCoefficients[i * Scene.SHCoefficientsCount + 0][1],
			       Scene.GaussianSHCoefficients[i * Scene.SHCoefficientsCount + 0][2],
			       Scene.GaussianSHCoefficients[i * Scene.SHCoefficientsCount + Scene.SHCoefficientsCount - 1][0],
			       Scene.GaussianSHCoefficients[i * Scene.SHCoefficientsCount + Scene.SHCoefficientsCount - 1][1],
			       Scene.GaussianSHCoefficients[i * Scene.SHCoefficientsCount + Scene.SHCoefficientsCount - 1][2]);
		}

		UE_LOG(LogTemp, Log, TEXT("Successfully read %d Gaussians from PLY file."), Scene.GaussianCount);
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
		Package,
		*Name,
		EBlueprintType::BPTYPE_Normal,
		UBlueprint::StaticClass(),
		UBlueprintGeneratedClass::StaticClass()
	);

	FAssetRegistryModule::AssetCreated(Blueprint);
	[[maybe_unused]] bool Suppressed = Package->MarkPackageDirty();

	// 为蓝图基类中的属性赋值
	ASceneActor* SceneActor = Blueprint->GeneratedClass->GetDefaultObject<ASceneActor>();
	// 注意这里要指定 Outer 为 Blueprint，让它成为蓝图资产的一部分，不然保存不了
	SceneActor->SceneNiagaraParameter = NewObject<USceneNiagaraParameter>(
		Blueprint,
		USceneNiagaraParameter::StaticClass(),
		TEXT("SceneNiagaraParameter"),
		RF_Public | RF_Standalone);
	SceneActor->SceneNiagaraParameter.Get()->SceneBufferAssetPath = FSoftObjectPath(SceneBufferAssetPath);

	// 编译蓝图
	FKismetEditorUtilities::CompileBlueprint(Blueprint);

	// 保存蓝图资产
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	UEditorLoadingAndSavingUtils::SavePackages({Package}, true);
}

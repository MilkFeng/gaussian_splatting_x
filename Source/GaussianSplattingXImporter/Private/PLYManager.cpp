#include "PLYManager.h"

#include <format>
#include <fstream>

#include "tinyply.h"

void FPlyManager::ImportPlyFile(const FString& FilePath)
{
	UE_LOG(LogTemp, Log, TEXT("Importing PLY file: %s"), *FilePath);
	const FGaussianScene Scene = ReadScene(FilePath);
	StoreScene(Scene, FPaths::GetBaseFilename(FilePath));
}

FGaussianScene FPlyManager::ReadScene(const FString& FilePath)
{
	UE_LOG(LogTemp, Log, TEXT("Reading PLY file from path: %s"), *FilePath);
	FGaussianScene Scene;

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

		Scene.Gaussians.resize(Vertices->count);
		for (size_t i = 0; i < Vertices->count; ++i)
		{
			const float* VertexData = reinterpret_cast<const float*>(Vertices->buffer.get_const()) + i * PropertyKeys.
				size();

			FGaussian& Gaussian = Scene.Gaussians[i];
			Gaussian.Position = FVector(VertexData[0], VertexData[1], VertexData[2]);
			Gaussian.Opacity = VertexData[3];
			Gaussian.Scale = FVector(VertexData[4], VertexData[5], VertexData[6]);
			Gaussian.Rotation = FVector4(VertexData[7], VertexData[8], VertexData[9], VertexData[10]);
			Gaussian.SHDim = Scene.SHDim;
			Gaussian.SHCoefficientsCount = Scene.SHCoefficientsCount;
			for (size_t j = 0; j < Gaussian.SHCoefficientsCount; ++j)
			{
				for (size_t k = 0; k < 3; ++k)
				{
					Gaussian.SHCoefficients[k][j] = VertexData[11 + j * 3 + k];
				}
			}

			UE_LOG(LogTemp, Log,
			       TEXT(
				       "Gaussian %llu: Pos(%.3f, %.3f, %.3f), Scale(%.3f, %.3f, %.3f), Rot(%.3f, %.3f, %.3f, %.3f), Alpha(%.3f), SH(%.3f, %.3f, %.3f, ..., %.3f, %.3f, %.3f)"
			       ),
			       i,
			       Gaussian.Position.X, Gaussian.Position.Y, Gaussian.Position.Z,
			       Gaussian.Scale.X, Gaussian.Scale.Y, Gaussian.Scale.Z,
			       Gaussian.Rotation.X, Gaussian.Rotation.Y, Gaussian.Rotation.Z, Gaussian.Rotation.W,
			       Gaussian.Opacity,
			       Gaussian.SHCoefficients[0][0], Gaussian.SHCoefficients[1][0], Gaussian.SHCoefficients[2][0],
			       Gaussian.SHCoefficients[0][Gaussian.SHCoefficientsCount - 3],
			       Gaussian.SHCoefficients[1][Gaussian.SHCoefficientsCount - 2],
			       Gaussian.SHCoefficients[2][Gaussian.SHCoefficientsCount - 1]);
		}

		UE_LOG(LogTemp, Log, TEXT("Successfully read %llu Gaussians from PLY file."), Scene.Gaussians.size());
	}
	catch (const std::exception& e)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to read PLY file: %s"), *FString(e.what()));
	}

	return Scene;
}

void FPlyManager::StoreScene(const FGaussianScene& Scene, const FString& Name)
{
}

#pragma once

#include "GaussianSplattingXRuntime/Public/SceneBufferAsset.h"

/// 场景管理器，负责导入场景数据并创建相应的资产和 Actor
class GAUSSIANSPLATTINGXIMPORTER_API FSceneManager
{
public:
	/// 从指定的文件路径导入 3DGS 场景数据，支持 PLY 格式
	/// @note PLY 文件包含：
	///       - 顶点位置（x, y, z）
	///       - 顶点缩放（scale_0, scale_1, scale_2）
	///       - 顶点旋转（rot_0, rot_1, rot_2, rot_3）
	///       - 顶点不透明度（opacity）
	///       - 球谐函数系数（f_dc_0, f_dc_1, f_dc_2, f_rest_0, f_rest_1, ...）
	/// @param FilePath 要导入的文件路径
	/// @param OnProgress 进度回调函数，参数为当前进度（0.0 到 1.0）
	static void ImportScene(const FString& FilePath, TFunction<void(float)> OnProgress = {});

private:
	/// 从 PLY 文件导入场景数据并创建 SceneBufferAsset 资产
	/// @param FilePath 要导入的 PLY 文件路径
	/// @param OnProgress 进度回调函数，参数为导入阶段的进度（0.0 到 1.0）
	/// @return 创建的 SceneBufferAsset 资产的引用，如果导入失败则返回空字符串
	static FString ImportPlyFile(const FString& FilePath, TFunction<void(float)> OnProgress = {});

	/// 读取 PLY 文件并将数据填充到 SceneBufferAsset 中
	/// @param FilePath 要读取的 PLY 文件路径
	/// @param Scene 要填充数据的 SceneBufferAsset 资产引用
	/// @param OnProgress 进度回调函数，参数为读取阶段的进度（0.0 到 1.0）
	/// @return 如果读取成功则返回 true，否则返回 false
	static bool ReadPlyFile(const FString& FilePath, USceneBufferAsset& Scene, TFunction<void(float)> OnProgress);

	/// 创建一个新的 Scene Actor 蓝图，引用指定的 SceneBufferAsset 资产
	static void CreateActorInContentBrowser(const FString& SceneBufferAssetPath);
};

#include "SceneBufferAsset.h"

void USceneBufferAsset::SetGaussianCount(const size_t NewGaussianCount)
{
	GaussianCount = NewGaussianCount;
	GaussianPositions.SetNum(NewGaussianCount);
	GaussianScales.SetNum(NewGaussianCount);
	GaussianRotations.SetNum(NewGaussianCount);
	GaussianOpacities.SetNum(NewGaussianCount);
	GaussianSHCoefficients.SetNumZeroed(NewGaussianCount * SHCoefficientsCount);
}

#include "SceneNiagaraInterface.h"

void USceneNiagaraInterface::GetFunctionsInternal(TArray<FNiagaraFunctionSignature>& OutFunctions) const
{
	FNiagaraFunctionSignature CountSig;
	CountSig.Name = TEXT("GetGaussianCount");
	CountSig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Count")));
	OutFunctions.Add(CountSig);

	FNiagaraFunctionSignature DataSig;
	DataSig.Name = TEXT("GetGaussianData");
	DataSig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Index")));
	DataSig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Position")));
	DataSig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Scale")));
	OutFunctions.Add(DataSig);
}

void USceneNiagaraInterface::GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo,
                                                   void* InstanceData, FVMExternalFunction& OutFunc)
{
	if (BindingInfo.Name == TEXT("GetGaussianCount"))
	{
		OutFunc = FVMExternalFunction::CreateUObject(this, &USceneNiagaraInterface::GetGaussianCount);
	}
	else if (BindingInfo.Name == TEXT("GetGaussianData"))
	{
		OutFunc = FVMExternalFunction::CreateUObject(this, &USceneNiagaraInterface::GetGaussianData);
	}
}

void USceneNiagaraInterface::GetGaussianCount(FVectorVMExternalFunctionContextProxy& Context)
{
	VectorVM::FExternalFuncRegisterHandler<int32> OutCount(Context);

	for (int32 i = 0; i < Context.GetNumInstances(); ++i)
	{
		const int32 Count = SceneBufferAsset ? SceneBufferAsset->Gaussians.Num() : 0;
		*OutCount.GetDest() = Count;
	}
}

void USceneNiagaraInterface::GetGaussianData(FVectorVMExternalFunctionContextProxy& Context)
{
	VectorVM::FExternalFuncInputHandler<int32> IndexParam(Context);
	VectorVM::FExternalFuncRegisterHandler<FVector> OutPosition(Context);
	VectorVM::FExternalFuncRegisterHandler<FVector> OutScale(Context);

	for (int32 i = 0; i < Context.GetNumInstances(); ++i)
	{
		const int32 Index = IndexParam.GetAndAdvance();
		if (SceneBufferAsset && SceneBufferAsset->Gaussians.IsValidIndex(Index))
		{
			const FGaussian& G = SceneBufferAsset->Gaussians[Index];
			*OutPosition.GetDest() = G.Position;
			*OutScale.GetDest() = G.Scale;
		}
		else
		{
			*OutPosition.GetDest() = FVector::ZeroVector;
			*OutScale.GetDest() = FVector::OneVector;
		}

		OutPosition.Advance();
		OutScale.Advance();
	}
}

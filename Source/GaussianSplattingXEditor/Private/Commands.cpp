// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands.h"

#define LOCTEXT_NAMESPACE "FGaussianSplattingXModule"

void FCommands::RegisterCommands()
{
	UI_COMMAND(OpenPluginWindow, "GaussianSplattingX", "Bring up GaussianSplattingX window",
	           EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"
#include "Style.h"

class FCommands : public TCommands<FCommands>
{
public:
	FCommands()
		: TCommands<FCommands>(
			TEXT("GaussianSplattingX"), NSLOCTEXT("Contexts", "GaussianSplattingX", "GaussianSplattingX Plugin"),
			NAME_None, FStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr<FUICommandInfo> OpenPluginWindow;
};

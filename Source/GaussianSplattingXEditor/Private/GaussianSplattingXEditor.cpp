// Copyright Epic Games, Inc. All Rights Reserved.

#include "GaussianSplattingXEditor.h"
#include "Style.h"
#include "Commands.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "ToolMenus.h"
#include "GaussianSplattingXImporter/Public/PLYManager.h"

static const FName GaussianSplattingXTabName("GaussianSplattingX");

#define LOCTEXT_NAMESPACE "FGaussianSplattingXEditor"

void FGaussianSplattingXEditorModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module

	FStyle::Initialize();
	FStyle::ReloadTextures();

	FCommands::Register();

	PluginCommands = MakeShareable(new FUICommandList);
	PluginCommands->MapAction(
		FCommands::Get().OpenPluginWindow,
		FExecuteAction::CreateRaw(this, &FGaussianSplattingXEditorModule::PluginButtonClicked),
		FCanExecuteAction());

	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FGaussianSplattingXEditorModule::RegisterMenus));

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(GaussianSplattingXTabName,
	                                                  FOnSpawnTab::CreateRaw(
		                                                  this, &FGaussianSplattingXEditorModule::OnSpawnPluginTab))
	                        .SetDisplayName(LOCTEXT("FGaussianSplattingXTabTitle", "GaussianSplattingX"))
	                        .SetMenuType(ETabSpawnerMenuType::Hidden);
}

void FGaussianSplattingXEditorModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	UToolMenus::UnRegisterStartupCallback(this);

	UToolMenus::UnregisterOwner(this);

	FStyle::Shutdown();

	FCommands::Unregister();

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(GaussianSplattingXTabName);
}

TSharedRef<SDockTab> FGaussianSplattingXEditorModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{
	// 添加一个加载 .ply 文件的按钮
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SBox)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.Text(FText::FromString("Import .ply file"))
				.OnClicked(FOnClicked::CreateLambda([]() -> FReply
				{
					IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
					if (!DesktopPlatform)
					{
						return FReply::Handled();
					}

					const void* ParentWindowHandle = nullptr;
					TArray<FString> OutFiles;
					const bool bOpened = DesktopPlatform->OpenFileDialog(
						ParentWindowHandle,
						TEXT("选择一个文件"),
						FPaths::ProjectContentDir(),
						TEXT(""),
						TEXT("*.ply"),
						EFileDialogFlags::None,
						OutFiles
					);

					if (!bOpened || OutFiles.Num() == 0)
					{
						return FReply::Handled();
					}

					FPlyManager::ImportPlyFile(OutFiles[0]);
					return FReply::Handled();
				}))
			]
		];
}

void FGaussianSplattingXEditorModule::PluginButtonClicked()
{
	FGlobalTabmanager::Get()->TryInvokeTab(GaussianSplattingXTabName);
}

void FGaussianSplattingXEditorModule::RegisterMenus()
{
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	FToolMenuOwnerScoped OwnerScoped(this);

	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		{
			FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
			Section.AddMenuEntryWithCommandList(FCommands::Get().OpenPluginWindow, PluginCommands);
		}
	}

	{
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar");
		{
			FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("Settings");
			{
				FToolMenuEntry& Entry = Section.AddEntry(
					FToolMenuEntry::InitToolBarButton(FCommands::Get().OpenPluginWindow));
				Entry.SetCommandList(PluginCommands);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FGaussianSplattingXEditorModule, FGaussianSplattingXEditor)

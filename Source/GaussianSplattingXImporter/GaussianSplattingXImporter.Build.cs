using System.IO;
using UnrealBuildTool;

public class GaussianSplattingXImporter : ModuleRules
{
	public GaussianSplattingXImporter(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			[
				"Core",
				"GaussianSplattingXRuntime"
			]
		);

		PrivateDependencyModuleNames.AddRange(
			[
				"CoreUObject",
				"Engine",
				"UnrealEd",
				"Kismet"
			]
		);

		string ThirdPartyPath = Path.Combine(ModuleDirectory, "../../ThirdParty/TinyPLY");
		PublicIncludePaths.Add(Path.Combine(ThirdPartyPath, "Include"));

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicAdditionalLibraries.Add(Path.Combine(ThirdPartyPath, "Lib/Win64/tinyply.lib"));
		}
		else
		{
			// Error
			throw new BuildException("Unsupported platform for TinyPLY");
		}
	}
}
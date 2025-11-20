using UnrealBuildTool;

public class GaussianSplattingXRuntime : ModuleRules
{
	public GaussianSplattingXRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			[
				"Core",
				"Niagara",
				"NiagaraCore",
				"VectorVM",
				"RenderCore",
				"Projects",
				"RHI"
			]
		);

		PrivateDependencyModuleNames.AddRange(
			[
				"CoreUObject",
				"Engine"
			]
		);

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange([
				"UnrealEd",
				"NiagaraEditor"
			]);
		}
	}
}
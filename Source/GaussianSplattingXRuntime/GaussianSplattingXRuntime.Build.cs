using UnrealBuildTool;

public class GaussianSplattingXRuntime : ModuleRules
{
	public GaussianSplattingXRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Niagara",
				"NiagaraCore",
				"VectorVM",
				"RenderCore"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
			}
		);
	}
}
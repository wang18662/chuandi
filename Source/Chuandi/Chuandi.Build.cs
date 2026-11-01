using UnrealBuildTool;

public class Chuandi : ModuleRules
{
	public Chuandi(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",
			"UMG",
			"AIModule",
			// UDeveloperSettings 在独立模块里，不写这条编辑器目标会链接失败（LNK2019）
			"DeveloperSettings"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			// Canvas 画圆/菱形要用 GWhiteTexture（RenderCore 导出）
			"RenderCore"
		});

		// 让 L0_Core/... L1_Framework/... 这种子目录写法可以直接 include
		PublicIncludePaths.Add(ModuleDirectory);
	}
}

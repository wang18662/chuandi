using UnrealBuildTool;
using System.Collections.Generic;

public class ChuandiEditorTarget : TargetRules
{
	public ChuandiEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("Chuandi");
	}
}

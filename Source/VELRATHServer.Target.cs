using UnrealBuildTool;
using System.Collections.Generic;

public class VELRATHServerTarget : TargetRules
{
    public VELRATHServerTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Server;
        DefaultBuildSettings = BuildSettingsVersion.V6;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_7;
        ExtraModuleNames.Add("VELRATH");
    }
}


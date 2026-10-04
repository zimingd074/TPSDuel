using UnrealBuildTool;
using System.Collections.Generic;
public class TPSDuelTarget : TargetRules
{
    public TPSDuelTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V2;
        ExtraModuleNames.Add("TPSDuel");
    }
}

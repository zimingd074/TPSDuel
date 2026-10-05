using UnrealBuildTool;
using System.Collections.Generic;
public class TPSDuelEditorTarget : TargetRules
{
    public TPSDuelEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V2;
        ExtraModuleNames.Add("TPSDuel");
        ExtraModuleNames.Add("TPSDuelEditor");
    }
}

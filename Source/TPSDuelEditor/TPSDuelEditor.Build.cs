using UnrealBuildTool;
public class TPSDuelEditor : ModuleRules
{
    public TPSDuelEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
        PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd", "AnimationCore", "TPSDuel", "AnimGraph", "BlueprintGraph", "RawMesh", "AssetRegistry" });
    }
}

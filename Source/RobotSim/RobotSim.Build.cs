using UnrealBuildTool;
public class RobotSim : ModuleRules
{
    public RobotSim(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] {
            "Core", "CoreUObject", "Engine", "InputCore", "Sockets", "Networking", "Json", "ImageWrapper", "RenderCore", "UMG", "Slate", "SlateCore"
        });
    }
}

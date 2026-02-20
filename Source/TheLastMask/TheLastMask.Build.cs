// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TheLastMask : ModuleRules
{
    public TheLastMask(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[] 
        { 
            "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "AIModule", "GameplayTasks", "NavigationSystem" });

        // PrivateDependencyModuleNames.AddRange(new string[] 
        // {
        //     // Needed for UHierarchicalInstancedStaticMeshComponent (efficient mesh instancing)
        //     "Engine",
        //     
        //     // Needed for Enhanced Input (UE5 input system)
        //     "EnhancedInput"
        // });
    }
}

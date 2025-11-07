// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class FPSCore : ModuleRules
{
	public FPSCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
				"EnhancedInput",
				"NetCore",
				"OnlineSubsystem",
				"OnlineSubsystemUtils",
				"UMG",
				"ChaosVehicles",
				"GameplayTags"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Slate",
				"SlateCore"
			}
		);
	}
}

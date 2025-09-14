// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ThirdPersonCameraMan : ModuleRules
{
	public ThirdPersonCameraMan(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
			"SlateCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });


		// Include paths for all module subfolders (variants are kept in-source)
		PublicIncludePaths.AddRange(new string[] {
			"ThirdPersonCameraMan",
			"ThirdPersonCameraMan/Variant_Platforming",
			"ThirdPersonCameraMan/Variant_Platforming/Animation",
			"ThirdPersonCameraMan/Variant_Combat",
			"ThirdPersonCameraMan/Variant_Combat/AI",
			"ThirdPersonCameraMan/Variant_Combat/Animation",
			"ThirdPersonCameraMan/Variant_Combat/Gameplay",
			"ThirdPersonCameraMan/Variant_Combat/Interfaces",
			"ThirdPersonCameraMan/Variant_Combat/UI",
			"ThirdPersonCameraMan/Variant_SideScrolling",
			"ThirdPersonCameraMan/Variant_SideScrolling/AI",
			"ThirdPersonCameraMan/Variant_SideScrolling/Gameplay",
			"ThirdPersonCameraMan/Variant_SideScrolling/Interfaces",
			"ThirdPersonCameraMan/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}

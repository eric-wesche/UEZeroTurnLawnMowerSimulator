// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Mower3 : ModuleRules
{
	public Mower3(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[] { "Foliage", "Landscape", "SocketIOClient", "SIOJson" });

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", 
			"ChaosVehicles", "PhysicsCore", "ChaosVehiclesCore",
			// Rendering dependencies
			"Renderer",
			"RenderCore",
			"RHI",
			"RHICore",
			"D3D12RHI",
		});
	}
}

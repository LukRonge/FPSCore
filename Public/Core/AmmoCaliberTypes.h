// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AmmoCaliberTypes.generated.h"

/**
 * Enum for ammunition caliber types used in FPS games
 * Used for quick matching (weapon accepts this caliber type)
 *
 * Usage:
 * - Weapon has AcceptedCaliberType
 * - AmmoTypeDataAsset has CaliberType
 * - Quick check: if (Weapon->AcceptedCaliberType == Ammo->CaliberType) { CanUse(); }
 */
UENUM(BlueprintType)
enum class EAmmoCaliberType : uint8
{
	// 5.56x45mm NATO - Standard rifle (M4A1, M16, SCAR-L, HK416)
	NATO_556x45mm UMETA(DisplayName = "5.56x45mm NATO"),

	// 7.62x51mm NATO / .308 Winchester - Battle rifles, sniper rifles (G3, FAL, M14, SR-25)
	NATO_762x51mm UMETA(DisplayName = "7.62x51mm NATO / .308 Win"),

	// 7.62x39mm Soviet - AK platform (AK-47, AKM, SKS)
	Soviet_762x39mm UMETA(DisplayName = "7.62x39mm"),

	// 5.45x39mm Soviet - Modern Russian rifles (AK-74, AK-12, AN-94)
	Soviet_545x39mm UMETA(DisplayName = "5.45x39mm"),

	// 9x19mm Parabellum - Pistols and SMGs (Glock, MP5, UZI, MP9)
	Parabellum_9x19mm UMETA(DisplayName = "9x19mm Parabellum"),

	// .45 ACP - Heavy pistols and SMGs (M1911, UMP45, Thompson, Vector)
	ACP_45 UMETA(DisplayName = ".45 ACP"),

	// 12 Gauge - Shotguns (Benelli M4, Remington 870, SPAS-12, AA-12)
	Gauge_12 UMETA(DisplayName = "12 Gauge"),

	// .50 BMG - Heavy anti-materiel sniper rifles (Barrett M82, M107)
	BMG_50 UMETA(DisplayName = ".50 BMG"),

	// .338 Lapua Magnum - Long-range precision rifles (AWM, AXMC, CheyTac)
	LapuaMagnum_338 UMETA(DisplayName = ".338 Lapua Magnum"),

	// 9x39mm Soviet - Subsonic rifles (AS VAL, VSS Vintorez, SR-3 Vikhr)
	Soviet_9x39mm UMETA(DisplayName = "9x39mm"),

	// Default/Unknown
	None UMETA(DisplayName = "None")
};

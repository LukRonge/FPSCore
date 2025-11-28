# Bolt-Action Weapon System Documentation

## Overview

This document describes the bolt-action weapon system implementation for the Sako85 sniper rifle in a multiplayer FPS shooter using Unreal Engine 5.6.

**Reference Implementation:** Sako85 (.308 Winchester / 7.62x51mm NATO)
**Inspired By:** M4A1 and HKVP9 implementations (fire components, reload components, animation flow)

---

## Design Requirements

Based on original specification:

1. **Sako85** - Bolt-action sniper rifle inheriting from `ABaseWeapon`
2. **BoltActionFireComponent** - Handles bolt-action fire mode mechanics
3. **BoltActionReloadComponent** - Handles reload with bolt-action chambering at end
4. **Animation Montages:**
   - ReloadMontage - Magazine swap (like M4A1/HKVP9)
   - BoltActionMontage - Bolt cycling animation
5. **AnimNotifies** for bolt-action (similar to magazine in/out):
   - Shell ejection
   - Chamber round
6. **Weapon Socket Reattachment:**
   - During **BOTH** reload AND bolt-action: weapon reattaches to `weapon_l`
   - After completion: weapon returns to `weapon_r`
7. **BaseWeapon defaults** for attachment sockets

---

## Architecture

### Component Structure

```
ASako85 : ABaseWeapon
├── UBoltActionFireComponent (FireComponent)
│   ├── bIsCyclingBolt (Replicated)
│   ├── bChamberEmpty (Replicated)
│   ├── BoltActionMontage (Character)
│   └── WeaponBoltActionMontage (Weapon)
├── UBoltActionReloadComponent (ReloadComponent)
│   ├── bIsReloading (Replicated, inherited)
│   ├── bReattachDuringReload
│   └── bBoltActionPending (Local)
├── UBallisticsComponent (inherited)
├── FPSMesh / TPSMesh (inherited)
└── CurrentMagazine (inherited)
```

### Class Hierarchy

```
UFireComponent (Abstract)
├── USemiAutoFireComponent    ← HKVP9
├── UFullAutoFireComponent    ← M4A1
└── UBoltActionFireComponent  ← Sako85

UReloadComponent (Abstract)
└── UBoxMagazineReloadComponent  ← M4A1, HKVP9
    └── UBoltActionReloadComponent  ← Sako85
```

---

## BaseWeapon Socket Defaults

BaseWeapon provides configurable socket properties for weapon attachment:

```cpp
// BaseWeapon.h

// Default weapon position - right hand
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Attachment")
FName CharacterAttachSocket = FName("weapon_r");

// Socket during reload/bolt-action operations
// Override in subclass for weapons needing hand repositioning
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Attachment")
FName ReloadAttachSocket = FName("weapon_r");
```

**Sako85 Override:**
```cpp
// Sako85.cpp constructor
CharacterAttachSocket = FName("weapon_r");  // Normal hold
ReloadAttachSocket = FName("weapon_l");     // Left hand support during reload/bolt-action
```

**Interface Access:**
```cpp
// Via IHoldableInterface
FName GetAttachSocket();           // Returns CharacterAttachSocket
FName GetReloadAttachSocket();     // Returns ReloadAttachSocket
```

---

## State Variables

### BoltActionFireComponent

| Variable | Type | Replicated | Description |
|----------|------|------------|-------------|
| `bIsCyclingBolt` | bool | Yes (OnRep) | True during bolt-action animation. **Blocks firing.** |
| `bChamberEmpty` | bool | Yes (OnRep) | True when no round in chamber. **Requires reload.** |
| `bTriggerHeld` | bool | No | Local trigger state. |

### BoltActionReloadComponent

| Variable | Type | Replicated | Description |
|----------|------|------------|-------------|
| `bIsReloading` | bool | Yes (OnRep) | True during reload sequence (inherited). |
| `bBoltActionPending` | bool | No | Server-only: reload→bolt-action transition. |
| `bReattachDuringReload` | bool | No | Config: reattach weapon to different socket. |

---

## Fire Flow (Bolt-Action)

### Correct Flow per Specification

```
1. Fire available? (not blocked, ammo > 0)
2. Fire shot + play Shoot anim montage
3. Start BoltActionMontage
   ├── AnimNotify: Shell Ejection
   └── AnimNotify: Chamber Round (if ammo available)
4. Bolt-action complete → ready to fire again
```

### Weapon Reattachment During Bolt-Action

**Per specification:** During bolt-action after fire, weapon should reattach to `weapon_l`.

```
FIRE → Weapon stays on weapon_r during shoot montage
     → BoltAction starts → Weapon reattaches to weapon_l
     → BoltAction ends → Weapon returns to weapon_r
```

### Detailed Fire Sequence

```
CLIENT                              SERVER
──────                              ──────
UseStart_Implementation()
    │
    └──► Server_Shoot(true) ──────► Server_Shoot_Implementation()
                                        │
                                        ├── FireComponent->TriggerPulled()
                                        │       │
                                        │       ├── CanFire() checks:
                                        │       │   ├── bIsCyclingBolt == false? ✓
                                        │       │   ├── bChamberEmpty == false? ✓
                                        │       │   └── Ammo > 0? ✓
                                        │       │
                                        │       ├── Fire()
                                        │       │   ├── ConsumeAmmo(1)
                                        │       │   └── BallisticsComponent->Shoot()
                                        │       │       └── HandleShotFired()
                                        │       │           └── Multicast_PlayShootEffects()
                                        │       │
                                        │       └── StartBoltAction(false)
                                        │           ├── bIsCyclingBolt = true (REPLICATES)
                                        │           ├── ReattachWeaponToSocket(weapon_l) ← NEW
                                        │           └── PlayBoltActionMontages()
                                        │
    ◄── OnRep_IsCyclingBolt(true) ──────┤
    │                                   │
    ├── ReattachWeaponToSocket()        │
    └── PlayBoltActionMontages()        │
        │                               │
        ├── AnimNotify_BoltActionStart ─┼── AnimNotify_BoltActionStart
        │   └── PlayWeaponMontage()     │   └── PlayWeaponMontage()
        │                               │
        ├── AnimNotify_ShellEject ──────┼── AnimNotify_ShellEject
        │   └── SpawnShellCasing()      │   └── SpawnShellCasing()
        │                               │
        └── AnimNotify_ChamberRound ────┼── AnimNotify_ChamberRound
            └── (visual feedback)       │   └── Check ammo for chamber
                                        │
    Montage ends                        │   Montage ends
    │                                   │       │
    └── OnMontageEnded()                │       └── OnMontageEnded()
        └── OnBoltActionComplete()      │           └── OnBoltActionComplete()
            │                           │               ├── bIsCyclingBolt = false
            └── HasAuthority? NO        │               ├── bChamberEmpty = (ammo==0)?
                                        │               └── ReattachWeaponToSocket(weapon_r)
                                        │
    ◄── OnRep_IsCyclingBolt(false) ─────┤
    │                                   │
    └── ReattachWeaponToSocket()        │
                                        │
    (Ready to fire again)               │
```

### CanFire() Logic

```cpp
bool UBoltActionFireComponent::CanFire() const
{
    if (bIsCyclingBolt) return false;   // Cycling bolt - BLOCKED
    if (bChamberEmpty) return false;    // Empty chamber - NEED RELOAD
    return Super::CanFire();            // Base: ammo, ballistics check
}
```

---

## Reload Flow (Bolt-Action)

### Correct Flow per Specification

```
1. Start Reload (like M4A1/HKVP9)
   └── Weapon reattaches to weapon_l
2. ReloadMontage plays
   ├── AnimNotify_MagazineOut → detach magazine
   └── AnimNotify_MagazineIn → attach magazine
3. ReloadMontage blend-out starts
   └── Start BoltActionMontage (chamber first round)
       ├── AnimNotify_ShellEject (optional for empty reload)
       └── AnimNotify_ChamberRound
4. BoltAction complete
   ├── Weapon reattaches to weapon_r
   └── Ready to fire
```

### Detailed Reload Sequence

```
CLIENT                              SERVER
──────                              ──────
Reload Input
    │
    └──► Server_StartReload() ────► Server_StartReload_Implementation()
                                        │
                                        ├── bIsReloading = true (REPLICATES)
                                        └── PlayReloadMontages()
                                            ├── ReattachWeaponToSocket(weapon_l)
                                            └── Play ReloadMontage

    ◄── OnRep_IsReloading(true) ────────┤
    │                                   │
    ├── ReattachWeaponToSocket()        │
    └── PlayReloadMontages()            │
                                        │
    AnimNotify_MagazineOut              │   AnimNotify_MagazineOut
    └── Detach mag to hand              │   └── Detach mag to hand
                                        │
    AnimNotify_MagazineIn               │   AnimNotify_MagazineIn
    └── Attach mag to weapon            │   └── Attach mag to weapon
                                        │
    ReloadMontage ends                  │   ReloadMontage ends
    │                                   │       │
    └── OnMontageEnded()                │       └── OnMontageEnded()
        └── HasAuthority? NO            │           │
                                        │           ├── StartBoltAction(true)
                                        │           │   ├── bIsCyclingBolt = true
                                        │           │   └── PlayBoltActionMontages()
                                        │           │
                                        │           └── bBoltActionPending = true
                                        │
    ◄── OnRep_IsCyclingBolt(true) ──────┤
    │                                   │
    └── PlayBoltActionMontages()        │
        │                               │
        AnimNotify_ChamberRound ────────┼── AnimNotify_ChamberRound
                                        │
    BoltMontage ends                    │   BoltMontage ends
    │                                   │       │
    └── OnMontageEnded()                │       └── OnBoltActionMontageEnded()
        └── OnBoltActionComplete()      │           │
            └── HasAuthority? NO        │           └── OnBoltActionAfterReloadComplete()
                                        │               ├── OnBoltActionComplete() ← CRITICAL
                                        │               │   └── bIsCyclingBolt = false
                                        │               ├── ResetChamberState()
                                        │               │   └── bChamberEmpty = false
                                        │               ├── ReattachWeaponToSocket(weapon_r)
                                        │               └── Super::OnReloadComplete()
                                        │                   ├── Transfer ammo
                                        │                   └── bIsReloading = false
                                        │
    ◄── OnRep_IsCyclingBolt(false) ─────┤
    ◄── OnRep_IsReloading(false) ───────┤
    │                                   │
    └── StopReloadMontages()            │
        └── ReattachWeaponToSocket()    │
                                        │
    (Ready to fire)                     │
```

---

## AnimNotify System

### Comparison: M4A1/HKVP9 vs Sako85

| Feature | M4A1/HKVP9 | Sako85 |
|---------|------------|--------|
| Fire AnimNotify | None (instant) | BoltActionStart, ShellEject, ChamberRound |
| Reload AnimNotify | MagazineOut, MagazineIn | MagazineOut, MagazineIn + BoltAction notifies |
| Weapon Montage | BoltCarrierShoot / SlideShoot | WeaponBoltActionMontage |

### Required AnimNotifies for Sako85

| Notify Class | Montage | When to Place | Action |
|--------------|---------|---------------|--------|
| `AnimNotify_MagazineOut` | ReloadMontage | Hand grabs magazine | Detach magazine to hand |
| `AnimNotify_MagazineIn` | ReloadMontage | Magazine inserted | Attach magazine to weapon |
| `AnimNotify_BoltActionStart` | BoltActionMontage | Hand reaches bolt handle | Start weapon bolt montage |
| `AnimNotify_ShellEject` | BoltActionMontage | Bolt pulls back | Spawn shell casing VFX |
| `AnimNotify_ChamberRound` | BoltActionMontage | Bolt closes | Audio/visual for chambering |

### AnimNotify Implementation Pattern

All AnimNotifies follow the same pattern (interface-based, no direct casts):

```cpp
void UAnimNotify_BoltActionStart::Notify(USkeletalMeshComponent* MeshComp, ...)
{
    AActor* Owner = MeshComp->GetOwner();
    if (!Owner || !Owner->Implements<UItemCollectorInterface>()) return;

    AActor* ActiveItem = IItemCollectorInterface::Execute_GetActiveItem(Owner);
    if (!ActiveItem) return;

    // Find component via FindComponentByClass (not cast!)
    UBoltActionFireComponent* BoltComp = ActiveItem->FindComponentByClass<UBoltActionFireComponent>();
    if (!BoltComp) return;

    BoltComp->OnBoltActionStart();
}
```

---

## Weapon Socket Reattachment

### Socket Configuration

| Weapon | CharacterAttachSocket | ReloadAttachSocket |
|--------|----------------------|-------------------|
| M4A1 | weapon_r | weapon_r (no change) |
| HKVP9 | weapon_r | weapon_r (no change) |
| **Sako85** | weapon_r | **weapon_l** |

### When Reattachment Occurs

| Event | Socket Before | Socket After |
|-------|---------------|--------------|
| Equip weapon | - | weapon_r |
| Start reload | weapon_r | **weapon_l** |
| Start bolt-action (after fire) | weapon_r | **weapon_l** |
| End reload (after bolt-action) | weapon_l | **weapon_r** |
| End bolt-action (after fire) | weapon_l | **weapon_r** |
| Unequip weapon | weapon_r | - |

### Reattachment Implementation

```cpp
void UBoltActionReloadComponent::ReattachWeaponToSocket(FName SocketName)
{
    AActor* WeaponActor = GetOwnerItem();
    AActor* CharacterActor = GetOwnerCharacterActor();

    // Get meshes via interfaces (no casts!)
    USkeletalMeshComponent* BodyMesh = ICharacterMeshProviderInterface::Execute_GetBodyMesh(CharacterActor);
    USkeletalMeshComponent* ArmsMesh = ICharacterMeshProviderInterface::Execute_GetArmsMesh(CharacterActor);
    UPrimitiveComponent* FPSWeaponMesh = IHoldableInterface::Execute_GetFPSMeshComponent(WeaponActor);
    UPrimitiveComponent* TPSWeaponMesh = IHoldableInterface::Execute_GetTPSMeshComponent(WeaponActor);

    // FPS mesh → Arms, TPS mesh → Body
    if (FPSWeaponMesh && ArmsMesh)
    {
        FPSWeaponMesh->AttachToComponent(ArmsMesh,
            FAttachmentTransformRules::SnapToTargetNotIncludingScale, SocketName);
    }
    if (TPSWeaponMesh && BodyMesh)
    {
        TPSWeaponMesh->AttachToComponent(BodyMesh,
            FAttachmentTransformRules::SnapToTargetNotIncludingScale, SocketName);
    }
}
```

---

## Multiplayer Replication

### Server Authority Pattern (Same as M4A1/HKVP9)

```
SERVER                          CLIENT
──────                          ──────
State change
├── bIsCyclingBolt = true ─────► OnRep_IsCyclingBolt()
│   (gameplay state)                └── PlayBoltActionMontages()
│                                       (visual operation)
│
└── PropagateStateToAnimInstances()
    (local visual update)
```

### Replicated vs Local Operations

| Operation | Replicated? | Where Executed |
|-----------|-------------|----------------|
| `bIsCyclingBolt` | Yes | Server sets, clients via OnRep |
| `bChamberEmpty` | Yes | Server sets, clients via OnRep |
| `bIsReloading` | Yes | Server sets, clients via OnRep |
| Montage playback | No | Each machine locally |
| Weapon reattachment | No | Each machine locally |
| AnimInstance updates | No | Each machine locally |
| VFX spawning | No | Each machine locally |

### RPC Usage

| RPC | Type | Purpose |
|-----|------|---------|
| `Server_Shoot` | Server | Client requests fire |
| `Server_StartReload` | Server | Client requests reload |
| `Multicast_PlayShootEffects` | Multicast | Play shoot VFX on all clients |

---

## Comparison: M4A1 vs HKVP9 vs Sako85

| Feature | M4A1 | HKVP9 | Sako85 |
|---------|------|-------|--------|
| Fire Component | FullAutoFireComponent | SemiAutoFireComponent | **BoltActionFireComponent** |
| Reload Component | BoxMagazineReloadComponent | BoxMagazineReloadComponent | **BoltActionReloadComponent** |
| Fire Rate | 800 RPM | 450 RPM | **40 RPM** |
| Fire Mode | Full-auto | Semi-auto | **Bolt-action** |
| Weapon State | bBoltCarrierOpen | bSlideLockedBack | **bIsCyclingBolt, bChamberEmpty** |
| Post-Fire Action | None | None | **BoltActionMontage** |
| Post-Reload Action | None | None | **BoltActionMontage** |
| Socket Change | No | No | **Yes (weapon_l)** |
| Weapon Montage | BoltCarrierShootMontage | SlideShootMontage | **WeaponBoltActionMontage** |

---

## Blueprint Setup Checklist

### 1. Sako85 Blueprint

- [ ] Parent class: `ABaseWeapon`
- [ ] FPSMesh: Skeletal mesh with sockets (`muzzle`, `magazine`)
- [ ] TPSMesh: Skeletal mesh with sockets
- [ ] DefaultMagazineClass: `BP_Magazine_308`
- [ ] DefaultSightClass: Scope or iron sights
- [ ] `CharacterAttachSocket`: "weapon_r"
- [ ] `ReloadAttachSocket`: "weapon_l"

### 2. BoltActionFireComponent Settings

- [ ] `FireRate`: 40.0f
- [ ] `SpreadScale`: 0.3f
- [ ] `RecoilScale`: 1.5f
- [ ] `BoltActionMontage`: Character bolt-action animation
- [ ] `WeaponBoltActionMontage`: Weapon bolt animation

### 3. BoltActionReloadComponent Settings

- [ ] `ReloadMontage`: Character reload animation
- [ ] `MagazineOutSocketName`: "weapon_l"
- [ ] `MagazineInSocketName`: "magazine"
- [ ] `bReattachDuringReload`: true

### 4. Animation Setup

**ReloadMontage:**
- [ ] `AnimNotify_MagazineOut` at magazine grab frame
- [ ] `AnimNotify_MagazineIn` at magazine insert frame

**BoltActionMontage:**
- [ ] `AnimNotify_BoltActionStart` when hand reaches bolt
- [ ] `AnimNotify_ShellEject` when bolt pulls back
- [ ] `AnimNotify_ChamberRound` when bolt closes

### 5. Character Skeleton Sockets

- [ ] `weapon_r` - Right hand weapon socket
- [ ] `weapon_l` - Left hand / support hand socket

---

## Known Issue: Delegate Overwrite Bug

### Problem

In `BoltActionReloadComponent::OnMontageEnded()`, the montage end delegate is registered twice:

1. First in `StartBoltAction()` → `PlayBoltActionMontages()`
2. Then immediately overwritten in `OnMontageEnded()`

**Result:** `BoltActionFireComponent::OnBoltActionComplete()` is never called after reload, leaving `bIsCyclingBolt = true` permanently.

### Required Fix

In `OnBoltActionAfterReloadComplete()`, explicitly call `OnBoltActionComplete()`:

```cpp
void UBoltActionReloadComponent::OnBoltActionAfterReloadComplete()
{
    bBoltActionPending = false;
    ReattachWeaponToSocket(EquipSocket);

    UBoltActionFireComponent* BoltComp = GetBoltActionFireComponent();
    if (BoltComp)
    {
        BoltComp->OnBoltActionComplete();  // ← CRITICAL: Reset bIsCyclingBolt
        BoltComp->ResetChamberState();
    }

    Super::OnReloadComplete();
}
```

---

## Debug Logging

```cpp
// BoltActionFireComponent
UE_LOG(LogFPSCore, Log, TEXT("[BoltFire] TriggerPulled - CanFire:%d, Cycling:%d, Empty:%d"),
    CanFire(), bIsCyclingBolt, bChamberEmpty);

UE_LOG(LogFPSCore, Log, TEXT("[BoltFire] StartBoltAction - FromReload:%d, Auth:%d"),
    bFromReload, GetOwner()->HasAuthority());

UE_LOG(LogFPSCore, Log, TEXT("[BoltFire] OnBoltActionComplete - bIsCyclingBolt→false"));

// BoltActionReloadComponent
UE_LOG(LogFPSCore, Log, TEXT("[BoltReload] OnMontageEnded - Interrupted:%d, Auth:%d"),
    bInterrupted, GetOwner()->HasAuthority());

UE_LOG(LogFPSCore, Log, TEXT("[BoltReload] ReattachWeapon - Socket:%s"), *SocketName.ToString());
```

---

## File References

| File | Description |
|------|-------------|
| `Weapons/Sako85.h/.cpp` | Sako85 weapon class |
| `Components/BoltActionFireComponent.h/.cpp` | Bolt-action fire mechanics |
| `Components/BoltActionReloadComponent.h/.cpp` | Bolt-action reload mechanics |
| `Components/FireComponent.h/.cpp` | Base fire component |
| `Components/ReloadComponent.h/.cpp` | Base reload component |
| `Components/BoxMagazineReloadComponent.h/.cpp` | Box magazine reload (parent) |
| `AnimNotifies/AnimNotify_BoltActionStart.h/.cpp` | Start weapon bolt montage |
| `AnimNotifies/AnimNotify_ShellEject.h/.cpp` | Shell ejection VFX |
| `AnimNotifies/AnimNotify_ChamberRound.h/.cpp` | Chamber round feedback |
| `AnimNotifies/AnimNotify_MagazineOut.h/.cpp` | Magazine detach |
| `AnimNotifies/AnimNotify_MagazineIn.h/.cpp` | Magazine attach |
| `Interfaces/HoldableInterface.h` | Socket access interface |
| `BaseWeapon.h/.cpp` | Base weapon with socket defaults |

---

## Version History

| Date | Version | Changes |
|------|---------|---------|
| 2025-01-28 | 1.0 | Initial documentation |
| 2025-01-28 | 1.1 | Added weapon reattachment during bolt-action after fire, BaseWeapon defaults, M4A1/HKVP9 comparison |

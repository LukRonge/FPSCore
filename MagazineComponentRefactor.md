# Magazine Component Refactor

## Zadanie

Vytvoriť novú triedu `UMagazineChildActorComponent` zdedenú od `UChildActorComponent`, ktorá bude:
- Obsahovať `FirstPersonPrimitiveType` ako EditDefaultsOnly property
- V BeginPlay inicializovať visibility na child actor (ABaseMagazine)

### Kľúčové body:
- `UMagazineChildActorComponent` dedí z `UChildActorComponent`
- Obsahuje len `FirstPersonPrimitiveType` (EditDefaultsOnly)
- `ABaseMagazine` zostáva **BEZ ZMIEN**
- Komponent v BeginPlay nastaví `FirstPersonPrimitiveType` na child actor a zavolá `ApplyVisibilityToMeshes()`

---

## Nová architektúra

### Diagram:
```
BaseWeapon
├── FPSMagazineComponent : UMagazineChildActorComponent
│   ├── FirstPersonPrimitiveType = FirstPerson (EditDefaultsOnly)
│   └── ChildActor → ABaseMagazine (existujúci, bez zmien)
│
├── TPSMagazineComponent : UMagazineChildActorComponent
│   ├── FirstPersonPrimitiveType = WorldSpaceRepresentation (EditDefaultsOnly)
│   └── ChildActor → ABaseMagazine (existujúci, bez zmien)
│
└── CurrentMagazine → TPS ABaseMagazine (ako doteraz, REPLICATED)

UMagazineChildActorComponent : UChildActorComponent
├── FirstPersonPrimitiveType (EditDefaultsOnly)
└── BeginPlay() → Inicializuje visibility na ChildActor

ABaseMagazine (BEZ ZMIEN)
├── CurrentAmmo, MaxCapacity, AmmoType
├── FirstPersonPrimitiveType (nastavené z komponentu)
├── ApplyVisibilityToMeshes()
└── Meshe priamo na actorovi
```

### Rozdelenie zodpovedností:

| Trieda | Zodpovednosť |
|--------|--------------|
| `UMagazineChildActorComponent` | FirstPersonPrimitiveType config, inicializácia visibility |
| `ABaseMagazine` | Ammo data, meshe, ApplyVisibilityToMeshes() - BEZ ZMIEN |
| `ABaseWeapon` | FPS/TPS komponenty, CurrentMagazine reference |

### Výhody:
1. **Minimálne zmeny** - ABaseMagazine zostáva rovnaký
2. **EditDefaultsOnly** - FirstPersonPrimitiveType nastaviteľný v Blueprint
3. **Čisté oddelenie** - Komponent riadi len visibility config
4. **Spätná kompatibilita** - Existujúci kód funguje

---

## Navrhované zmeny

### Krok 1: Vytvoriť UMagazineChildActorComponent

**Súbor:** `Public/Components/MagazineChildActorComponent.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Components/ChildActorComponent.h"
#include "MagazineChildActorComponent.generated.h"

class ABaseMagazine;

/**
 * Magazine Child Actor Component
 *
 * Špecializovaný ChildActorComponent pre magazine s FirstPersonPrimitiveType konfiguráciou.
 *
 * ZODPOVEDNOSTI:
 * - Držať FirstPersonPrimitiveType (EditDefaultsOnly)
 * - V BeginPlay inicializovať visibility na child actor (ABaseMagazine)
 *
 * POUŽITIE v BaseWeapon Blueprint:
 * - FPSMagazineComponent: FirstPersonPrimitiveType = FirstPerson
 * - TPSMagazineComponent: FirstPersonPrimitiveType = WorldSpaceRepresentation
 *
 * ABaseMagazine zostáva BEZ ZMIEN - tento komponent len nastavuje visibility.
 */
UCLASS(ClassGroup=(FPSCore), meta=(BlueprintSpawnableComponent))
class FPSCORE_API UMagazineChildActorComponent : public UChildActorComponent
{
    GENERATED_BODY()

public:
    UMagazineChildActorComponent();

protected:
    virtual void BeginPlay() override;

public:
    // ============================================
    // CONFIGURATION (EditDefaultsOnly)
    // ============================================

    /**
     * First-person primitive type for visibility control
     * Applied to child actor (ABaseMagazine) in BeginPlay
     *
     * - FirstPerson: OnlyOwnerSee = true (FPS magazine)
     * - WorldSpaceRepresentation: OwnerNoSee = true (TPS magazine)
     *
     * Set in Blueprint defaults:
     * - FPSMagazineComponent -> FirstPerson
     * - TPSMagazineComponent -> WorldSpaceRepresentation
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Magazine")
    EFirstPersonPrimitiveType FirstPersonPrimitiveType = EFirstPersonPrimitiveType::None;

    // ============================================
    // VISIBILITY INITIALIZATION
    // ============================================

    /**
     * Initialize visibility on child actor
     * Sets FirstPersonPrimitiveType on ABaseMagazine and calls ApplyVisibilityToMeshes()
     *
     * Called automatically from BeginPlay()
     * Can be called manually after child actor is spawned
     */
    UFUNCTION(BlueprintCallable, Category = "Magazine")
    void InitializeChildActorVisibility();

    // ============================================
    // OWNER PROPAGATION
    // ============================================

    /**
     * Propagate owner to child actor for correct visibility
     * Required for OnlyOwnerSee/OwnerNoSee to work
     *
     * @param NewOwner - New owner actor (character)
     */
    UFUNCTION(BlueprintCallable, Category = "Magazine")
    void PropagateOwner(AActor* NewOwner);
};
```

**Súbor:** `Private/Components/MagazineChildActorComponent.cpp`

```cpp
#include "Components/MagazineChildActorComponent.h"
#include "BaseMagazine.h"

UMagazineChildActorComponent::UMagazineChildActorComponent()
{
    // ChildActorComponent defaults
    PrimaryComponentTick.bCanEverTick = false;
}

void UMagazineChildActorComponent::BeginPlay()
{
    Super::BeginPlay();

    // Initialize visibility on child actor
    InitializeChildActorVisibility();
}

void UMagazineChildActorComponent::InitializeChildActorVisibility()
{
    AActor* ChildActor = GetChildActor();
    if (!ChildActor)
    {
        return;
    }

    // Set FirstPersonPrimitiveType on child actor (ABaseMagazine)
    // Using direct cast - this component is specifically for ABaseMagazine
    // This is internal initialization, not external access pattern
    if (ABaseMagazine* Magazine = Cast<ABaseMagazine>(ChildActor))
    {
        Magazine->FirstPersonPrimitiveType = FirstPersonPrimitiveType;
        Magazine->ApplyVisibilityToMeshes();
    }
}

void UMagazineChildActorComponent::PropagateOwner(AActor* NewOwner)
{
    if (AActor* ChildActor = GetChildActor())
    {
        ChildActor->SetOwner(NewOwner);
    }
}
```

---

### Krok 2: Upraviť BaseWeapon.h

**Zmeny:**

1. Zmeniť typ komponentov z `UChildActorComponent*` na `UMagazineChildActorComponent*`
2. Pridať forward declaration pre `UMagazineChildActorComponent`
3. Odstrániť hardcoded FirstPersonPrimitiveType nastavovanie z InitMagazineComponents()

```cpp
// PRED:
UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Components")
UChildActorComponent* FPSMagazineComponent;

UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Components")
UChildActorComponent* TPSMagazineComponent;

// PO:
class UMagazineChildActorComponent;

/**
 * FPS Magazine Component
 * FirstPersonPrimitiveType = FirstPerson (set in Blueprint defaults)
 */
UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Components")
UMagazineChildActorComponent* FPSMagazineComponent;

/**
 * TPS Magazine Component
 * FirstPersonPrimitiveType = WorldSpaceRepresentation (set in Blueprint defaults)
 */
UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Components")
UMagazineChildActorComponent* TPSMagazineComponent;
```

---

### Krok 3: Upraviť BaseWeapon.cpp

**Constructor:**

```cpp
// PRED:
FPSMagazineComponent = CreateDefaultSubobject<UChildActorComponent>(TEXT("FPSMagazineComponent"));
FPSMagazineComponent->SetupAttachment(FPSMesh, FName("magazine"));

TPSMagazineComponent = CreateDefaultSubobject<UChildActorComponent>(TEXT("TPSMagazineComponent"));
TPSMagazineComponent->SetupAttachment(TPSMesh, FName("magazine"));

// PO:
FPSMagazineComponent = CreateDefaultSubobject<UMagazineChildActorComponent>(TEXT("FPSMagazineComponent"));
FPSMagazineComponent->SetupAttachment(FPSMesh, FName("magazine"));
// FirstPersonPrimitiveType = FirstPerson (set in Blueprint defaults)

TPSMagazineComponent = CreateDefaultSubobject<UMagazineChildActorComponent>(TEXT("TPSMagazineComponent"));
TPSMagazineComponent->SetupAttachment(TPSMesh, FName("magazine"));
// FirstPersonPrimitiveType = WorldSpaceRepresentation (set in Blueprint defaults)
```

**InitMagazineComponents:**

```cpp
// PRED:
void ABaseWeapon::InitMagazineComponents(TSubclassOf<ABaseMagazine> MagazineClass)
{
    if (MagazineClass)
    {
        FPSMagazineComponent->SetChildActorClass(TSubclassOf<AActor>(MagazineClass));
        TPSMagazineComponent->SetChildActorClass(TSubclassOf<AActor>(MagazineClass));

        // FPS Magazine: FirstPerson visibility (only owner sees)
        if (ABaseMagazine* FPSMag = Cast<ABaseMagazine>(FPSMagazineComponent->GetChildActor()))
        {
            FPSMag->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
            FPSMag->ApplyVisibilityToMeshes();
            FPSMag->SetOwner(GetOwner());
        }

        // TPS Magazine: WorldSpaceRepresentation visibility (others see, owner doesn't)
        if (ABaseMagazine* TPSMag = Cast<ABaseMagazine>(TPSMagazineComponent->GetChildActor()))
        {
            TPSMag->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;
            TPSMag->ApplyVisibilityToMeshes();
            TPSMag->SetOwner(GetOwner());
        }
    }
    else
    {
        FPSMagazineComponent->SetChildActorClass(nullptr);
        TPSMagazineComponent->SetChildActorClass(nullptr);
    }
}

// PO:
void ABaseWeapon::InitMagazineComponents(TSubclassOf<ABaseMagazine> MagazineClass)
{
    if (MagazineClass)
    {
        FPSMagazineComponent->SetChildActorClass(TSubclassOf<AActor>(MagazineClass));
        TPSMagazineComponent->SetChildActorClass(TSubclassOf<AActor>(MagazineClass));

        // Visibility je inicializovaná v komponente BeginPlay()
        // FirstPersonPrimitiveType je nastavený v Blueprint defaults

        // Propagate owner for visibility
        FPSMagazineComponent->PropagateOwner(GetOwner());
        TPSMagazineComponent->PropagateOwner(GetOwner());
    }
    else
    {
        FPSMagazineComponent->SetChildActorClass(nullptr);
        TPSMagazineComponent->SetChildActorClass(nullptr);
    }
}
```

**SetOwner:**

```cpp
// PRED:
if (FPSMagazineComponent->GetChildActor())
{
    FPSMagazineComponent->GetChildActor()->SetOwner(NewOwner);
}
if (TPSMagazineComponent->GetChildActor())
{
    TPSMagazineComponent->GetChildActor()->SetOwner(NewOwner);
}

// PO:
if (FPSMagazineComponent)
{
    FPSMagazineComponent->PropagateOwner(NewOwner);
}
if (TPSMagazineComponent)
{
    TPSMagazineComponent->PropagateOwner(NewOwner);
}
```

**OnRep_Owner:**

```cpp
// Rovnaká zmena - použiť PropagateOwner()
if (FPSMagazineComponent)
{
    FPSMagazineComponent->PropagateOwner(NewOwner);
}
if (TPSMagazineComponent)
{
    TPSMagazineComponent->PropagateOwner(NewOwner);
}
```

---

### Krok 4: ABaseMagazine - ŽIADNE ZMENY

`ABaseMagazine` zostáva **úplne bez zmien**. Komponent len nastavuje `FirstPersonPrimitiveType` a volá existujúcu `ApplyVisibilityToMeshes()` metódu.

---

## Súhrn zmien

| Súbor | Akcia |
|-------|-------|
| `Public/Components/MagazineChildActorComponent.h` | **NOVÝ** |
| `Private/Components/MagazineChildActorComponent.cpp` | **NOVÝ** |
| `Public/BaseWeapon.h` | Zmeniť typ komponentov |
| `Private/BaseWeapon.cpp` | Zjednodušiť InitMagazineComponents, SetOwner, OnRep_Owner |
| `Public/BaseMagazine.h` | **BEZ ZMIEN** |
| `Private/BaseMagazine.cpp` | **BEZ ZMIEN** |

---

## Blueprint konfigurácia po refaktore

### V Blueprint weapon (napr. BP_M4A1):

**FPSMagazineComponent:**
- `ChildActorClass` = BP_STANAGMagazine (ABaseMagazine)
- `FirstPersonPrimitiveType` = FirstPerson

**TPSMagazineComponent:**
- `ChildActorClass` = BP_STANAGMagazine (ABaseMagazine)
- `FirstPersonPrimitiveType` = WorldSpaceRepresentation

---

## Výhody riešenia

1. **Minimálne zmeny** - ABaseMagazine zostáva úplne rovnaký
2. **EditDefaultsOnly** - FirstPersonPrimitiveType nastaviteľný v Blueprint defaults
3. **Žiadna duplicitná logika** - Visibility logika je v ABaseMagazine, komponent len konfiguruje
4. **Čisté oddelenie** - Komponent drží config, actor má logiku
5. **Spätná kompatibilita** - Existujúci kód funguje bez zmien
6. **Single Responsibility** - Komponent má jednu úlohu (visibility config)

---

## Inicializačná sekvencia

1. **Constructor (BaseWeapon):**
   - Vytvorí FPS/TPS MagazineChildActorComponent
   - Attachne k FPSMesh/TPSMesh

2. **PostInitializeComponents (BaseWeapon):**
   - Server: SetChildActorClass() → spawne ABaseMagazine
   - Volá PropagateOwner()

3. **BeginPlay (MagazineChildActorComponent):**
   - Zavolá InitializeChildActorVisibility()
   - Nastaví FirstPersonPrimitiveType na ABaseMagazine
   - Zavolá ApplyVisibilityToMeshes()

4. **SetOwner / OnRep_Owner (BaseWeapon):**
   - Propaguje owner cez PropagateOwner()
   - ABaseMagazine::SetOwner() volá ApplyVisibilityToMeshes()

---

## Multiplayer synchronizácia

**Server:**
- PostInitializeComponents: Spawne child actors, nastaví owner
- BeginPlay: Inicializuje visibility

**Client (OnRep_CurrentMagazineClass):**
- Spawne child actors
- BeginPlay: Inicializuje visibility

**Client (OnRep_Owner):**
- Propaguje owner na child actors
- Visibility sa aktualizuje

**Žiadne zmeny v replikácii** - CurrentMagazine a CurrentMagazineClass fungujú ako doteraz.

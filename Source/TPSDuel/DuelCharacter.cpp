#include "DuelCharacter.h"
#include "DuelGameMode.h"
#include "DuelGameState.h"
#include "DuelPlayerController.h"
#include "DuelPlayerState.h"
#include "DuelRules.h"
#include "DuelSettings.h"
#include "DuelTracer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimInstance.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TPSDuel.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

ADuelCharacter::ADuelCharacter()
{
    bReplicates = true;
    NetUpdateFrequency = 60.f;
    MinNetUpdateFrequency = 20.f;
    PrimaryActorTick.bCanEverTick = true;
    GetCapsuleComponent()->InitCapsuleSize(42.f, 88.f);
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    bUseControllerRotationYaw = true;
    GetCharacterMovement()->bOrientRotationToMovement = false;
    GetCharacterMovement()->MaxWalkSpeed = 500.f;
    GetCharacterMovement()->JumpZVelocity = 500.f;
    GetCharacterMovement()->AirControl = 0.3f;
    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(GetRootComponent());
    CameraBoom->SetRelativeLocation(FVector(0, 0, 55));
    CameraBoom->TargetArmLength = 300.f;
    CameraBoom->SocketOffset = FVector(0, 65, 0);
    CameraBoom->ProbeSize = 12.f;
    CameraBoom->bUsePawnControlRotation = true;
    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    FollowCamera->SetupAttachment(CameraBoom);
    FollowCamera->FieldOfView = 90.f;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> Soldier(TEXT("/Game/Mannequin/Character/Mesh/SK_Mannequin.SK_Mannequin"));
    static ConstructorHelpers::FClassFinder<UAnimInstance> Locomotion(TEXT("/Game/Mannequin/Animations/ThirdPerson_AnimBP"));
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> Quantum(TEXT("/Game/ThirdParty/Quantum/SKM_Character.SKM_Character"));
    static ConstructorHelpers::FClassFinder<UAnimInstance> QuantumLocomotion(TEXT("/Game/ThirdParty/Quantum/Animations/Q_ThirdPerson_AnimBP"));
    bQuantumCharacter = Quantum.Succeeded() && QuantumLocomotion.Succeeded();
    GetMesh()->SetSkeletalMesh(Soldier.Object);
    GetMesh()->SetRelativeLocation(FVector(0,0,-88));
    GetMesh()->SetRelativeRotation(FRotator(0,-90,0));
    GetMesh()->SetAnimInstanceClass(Locomotion.Class);
    if (bQuantumCharacter)
    {
        GetMesh()->SetSkeletalMesh(Quantum.Object);
        GetMesh()->SetAnimInstanceClass(QuantumLocomotion.Class);
    }
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
    Body->SetupAttachment(GetRootComponent());
    Body->SetStaticMesh(Cube.Object);
    Body->SetRelativeLocation(FVector(0, 0, -15));
    Body->SetRelativeScale3D(FVector(.45f, .65f, 1.1f));
    Head = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Head"));
    Head->SetupAttachment(GetRootComponent());
    Head->SetStaticMesh(Sphere.Object);
    Head->SetRelativeLocation(FVector(0, 0, 65));
    Head->SetRelativeScale3D(FVector(.45f));
    Rifle = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Rifle"));
    Rifle->SetupAttachment(GetRootComponent());
    Rifle->SetStaticMesh(Cube.Object);
    Rifle->SetRelativeLocation(FVector(65, 25, 40));
    Rifle->SetRelativeScale3D(FVector(.75f, .12f, .15f));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> QuantumRifle(TEXT("/Game/ThirdParty/Quantum/SM_Rifle.SM_Rifle"));
    if (bQuantumCharacter && QuantumRifle.Succeeded())
    {
        Rifle->SetupAttachment(GetMesh(), TEXT("hand_r"));
        Rifle->SetStaticMesh(QuantumRifle.Object);
        Rifle->SetRelativeScale3D(FVector(1.f));
        Rifle->SetRelativeLocation(FVector::ZeroVector);
        Rifle->SetAbsolute(false, true, false);
    }
    ShieldMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShieldMarker"));
    ShieldMarker->SetupAttachment(GetRootComponent());
    ShieldMarker->SetStaticMesh(Sphere.Object);
    ShieldMarker->SetRelativeLocation(FVector(0, 0, 130));
    ShieldMarker->SetRelativeScale3D(FVector(.2f));
    for (UStaticMeshComponent* Part : {Body, Head, Rifle, ShieldMarker})
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    // A simple assembled weapon, keeping server muzzle coordinates unchanged.
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> WeaponMaterial(TEXT("/Game/Materials/M_Metal.M_Metal"));
    auto WeaponPart=[this](const TCHAR* Name,UStaticMesh* Shape,FVector Location,FVector Scale,FRotator Rotation)
    {
        auto* Part=CreateDefaultSubobject<UStaticMeshComponent>(Name);
        Part->SetupAttachment(Rifle); Part->SetStaticMesh(Shape); Part->SetRelativeLocation(Location);
        Part->SetAbsolute(false,false,true); Part->SetRelativeScale3D(Scale); Part->SetRelativeRotation(Rotation);
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision); Part->SetMaterial(0,WeaponMaterial.Object);
    };
    if (!bQuantumCharacter)
    {
        WeaponPart(TEXT("Barrel"),Cylinder.Object,FVector(43,0,0),FVector(.045f,.045f,.35f),FRotator(90,0,0));
        WeaponPart(TEXT("Stock"),Cube.Object,FVector(-55,0,0),FVector(.24f,.11f,.14f),FRotator::ZeroRotator);
        WeaponPart(TEXT("Magazine"),Cube.Object,FVector(-5,0,-85),FVector(.13f,.09f,.2f),FRotator(-12,0,0));
        WeaponPart(TEXT("Sight"),Cube.Object,FVector(10,0,65),FVector(.12f,.06f,.045f),FRotator::ZeroRotator);
    }
}

void ADuelCharacter::BeginPlay()
{
    Super::BeginPlay();
    const UDuelSettings* Rules = GetDefault<UDuelSettings>();
    if (HasAuthority())
    {
        Health = FMath::Max(1.f, Rules->MaxHealth);
        Ammo = FMath::Max(1, Rules->MagazineCapacity);
        ServerAimRotation = GetActorRotation();
        GrantProtection();
    }
    UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_Paint.M_Paint"));
    if (Material)
    {
        BodyMaterial = UMaterialInstanceDynamic::Create(Material, this);
        HeadMaterial = UMaterialInstanceDynamic::Create(Material, this);
        Body->SetMaterial(0, BodyMaterial);
        Head->SetMaterial(0, HeadMaterial);
        if (bQuantumCharacter)
        {
            const int32 PatchSlot = GetMesh()->GetMaterialIndex(TEXT("M_Patches"));
            if (PatchSlot != INDEX_NONE)
            {
                BodyMaterial = UMaterialInstanceDynamic::Create(GetMesh()->GetMaterial(PatchSlot), this);
                GetMesh()->SetMaterial(PatchSlot, BodyMaterial);
            }
        }
        else
        {
            GetMesh()->SetMaterial(0,BodyMaterial);
            GetMesh()->SetMaterial(1,BodyMaterial);
        }
        auto ColorPart = [this, Material](UStaticMeshComponent* Part, FLinearColor Color)
        {
            UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(Material, this);
            Instance->SetVectorParameterValue(TEXT("Color"), Color);
            Part->SetMaterial(0, Instance);
        };
        if (!bQuantumCharacter) ColorPart(Rifle, FLinearColor(.1f, .12f, .16f));
        ColorPart(ShieldMarker, FLinearColor(.1f, 1.f, .3f));
    }
    FireSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/S_Fire.S_Fire"));
}

void ADuelCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ADuelCharacter, Health);
    DOREPLIFETIME(ADuelCharacter, Ammo);
    DOREPLIFETIME(ADuelCharacter, bReloading);
    DOREPLIFETIME(ADuelCharacter, bProtected);
    DOREPLIFETIME(ADuelCharacter, bAiming);
}

bool ADuelCharacter::CanCombat() const
{
    const ADuelGameState* State = GetWorld()->GetGameState<ADuelGameState>();
    return IsAlive() && State && State->Phase == EDuelPhase::Playing;
}
bool ADuelCharacter::CanMove() const
{
    const ADuelGameState* State = GetWorld()->GetGameState<ADuelGameState>();
    return IsAlive() && State && (State->Phase == EDuelPhase::Waiting || State->Phase == EDuelPhase::Playing);
}
void ADuelCharacter::RefreshMovement()
{
    const bool Allowed = CanMove();
    if (Allowed != bMovementAllowed)
    {
        bMovementAllowed = Allowed;
        if (Allowed) GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        else GetCharacterMovement()->DisableMovement();
    }
}
void ADuelCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    RefreshMovement();
    ShieldMarker->SetVisibility(bProtected && IsAlive());
    const ADuelPlayerState* PS = GetPlayerState<ADuelPlayerState>();
    if (PS && PS->Slot != DisplayedSlot)
    {
        DisplayedSlot = PS->Slot;
        const FLinearColor Color = PS->Slot == 0 ? FLinearColor(.08f, .5f, 1.f) : FLinearColor(1.f, .15f, .12f);
        if (BodyMaterial) BodyMaterial->SetVectorParameterValue(TEXT("Color"), Color);
        if (HeadMaterial) HeadMaterial->SetVectorParameterValue(TEXT("Color"), Color);
    }
    Body->SetVisibility(false);
    Head->SetVisibility(false);
    GetMesh()->SetVisibility(IsAlive());
    Rifle->SetVisibility(IsAlive(),true);
    const float Pitch = IsLocallyControlled() ? LocalAim().Pitch : FMath::UnwindDegrees(GetBaseAimRotation().Pitch);
    if (bQuantumCharacter)
        Rifle->SetWorldRotation(FRotator(Pitch, GetActorRotation().Yaw-90.f, 0));
    else
        Rifle->SetRelativeRotation(FRotator(Pitch, 0, 0));
    if (IsLocallyControlled())
    {
        FollowCamera->FieldOfView = FMath::FInterpTo(FollowCamera->FieldOfView, bAiming ? 65.f : 90.f, DeltaSeconds, 12.f);
        CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, bAiming ? 220.f : 300.f, DeltaSeconds, 12.f);
        if (!CanCombat() && bLocalTrigger) EndFire();
        if (bLocalTrigger && GetWorld()->TimeSeconds - LastAimSendTime >= .05f)
        {
            LastAimSendTime = GetWorld()->TimeSeconds;
            ServerAim(LocalAim());
        }
    }
}
void ADuelCharacter::MoveForward(float Value)
{
    if (CanMove() && Controller) AddMovementInput(FRotationMatrix(FRotator(0, Controller->GetControlRotation().Yaw, 0)).GetUnitAxis(EAxis::X), Value);
}
void ADuelCharacter::MoveRight(float Value)
{
    if (CanMove() && Controller) AddMovementInput(FRotationMatrix(FRotator(0, Controller->GetControlRotation().Yaw, 0)).GetUnitAxis(EAxis::Y), Value);
}
FRotator ADuelCharacter::LocalAim() const
{
    const FRotator Rotation = GetControlRotation();
    return FRotator(FMath::Clamp(FMath::UnwindDegrees(Rotation.Pitch), -80.f, 80.f), FMath::UnwindDegrees(Rotation.Yaw), 0);
}
bool ADuelCharacter::AcceptAim(const FRotator& Aim)
{
    float Pitch, Yaw;
    if (!DuelRules::DecodeAim(Aim.Pitch, Aim.Yaw, Pitch, Yaw) || !FMath::IsFinite(Aim.Roll) || FMath::Abs(Aim.Roll) > 360.f) return false;
    ServerAimRotation = FRotator(Pitch, Yaw, 0);
    LastAimReceiveTime = GetWorld()->TimeSeconds;
    return true;
}
void ADuelCharacter::BeginFire()
{
    if (!IsLocallyControlled() || bLocalTrigger || !CanCombat() || bReloading || Ammo <= 0) return;
    bLocalTrigger = true;
    if (!HasAuthority())
    {
        LocalFireFeedback();
        GetWorldTimerManager().SetTimer(FeedbackTimer, this, &ADuelCharacter::LocalFireFeedback, FMath::Max(.05f, GetDefault<UDuelSettings>()->FireInterval), true);
    }
    ServerFireIntent(true, LocalAim());
}
void ADuelCharacter::EndFire()
{
    if (!bLocalTrigger) return;
    bLocalTrigger = false;
    GetWorldTimerManager().ClearTimer(FeedbackTimer);
    ServerFireIntent(false, LocalAim());
}
void ADuelCharacter::ServerFireIntent_Implementation(bool Pressed, FRotator Aim)
{
#if !UE_BUILD_SHIPPING
    if (Pressed && FParse::Param(FCommandLine::Get(), TEXT("DuelShotLog")))
        UE_LOG(LogTPSDuel, Display, TEXT("FIRE_INTENT actor=%s combat=%d trigger=%d reload=%d ammo=%d aim=%s"), *GetName(), CanCombat(), bServerTrigger, bReloading, Ammo, *Aim.ToString());
#endif
    if (!Pressed)
    {
        bServerTrigger = false;
        GetWorldTimerManager().ClearTimer(FireTimer);
        return;
    }
    if (!CanCombat() || bServerTrigger || bReloading || Ammo <= 0 || !AcceptAim(Aim)) return;
    bServerTrigger = true;
    FireOnce();
    if (bServerTrigger)
        GetWorldTimerManager().SetTimer(FireTimer, this, &ADuelCharacter::FireOnce, FMath::Max(.05f, GetDefault<UDuelSettings>()->FireInterval), true);
}
void ADuelCharacter::ServerAim_Implementation(FRotator Aim) { if (CanCombat()) AcceptAim(Aim); }
void ADuelCharacter::SetAiming(bool Aiming)
{
    if (!IsLocallyControlled()) return;
    bAiming = Aiming && IsAlive();
    ServerSetAiming(bAiming);
}
void ADuelCharacter::ServerSetAiming_Implementation(bool Aiming) { bAiming = Aiming && IsAlive(); }

void ADuelCharacter::ComputeShotView(FVector& Origin, FVector& Direction) const
{
    Direction = ServerAimRotation.Vector();
    const FVector Pivot = GetActorLocation() + FVector(0, 0, 55);
    const FVector Desired = Pivot - Direction * (bAiming ? 220.f : 300.f) + FRotationMatrix(ServerAimRotation).GetUnitAxis(EAxis::Y) * 65.f;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(DuelCamera), false, this);
    FHitResult Hit;
    const bool Blocked = GetWorld()->SweepSingleByChannel(Hit, Pivot, Desired, FQuat::Identity, ECC_Camera, FCollisionShape::MakeSphere(12.f), Params);
    Origin = Blocked ? Hit.Location : Desired;
}
void ADuelCharacter::FireOnce()
{
    const UDuelSettings* Rules = GetDefault<UDuelSettings>();
    if (!bServerTrigger || !CanCombat() || bReloading || Ammo <= 0 || GetWorld()->TimeSeconds - LastAimReceiveTime > .5f)
    {
        bServerTrigger = false;
        GetWorldTimerManager().ClearTimer(FireTimer);
        return;
    }
    if (!DuelRules::CanFire(true, true, false, Ammo, GetWorld()->TimeSeconds, LastShotTime, Rules->FireInterval)) return;
    LastShotTime = GetWorld()->TimeSeconds;
    --Ammo;
    ClearProtection();
    FVector CameraOrigin, Direction;
    ComputeShotView(CameraOrigin, Direction);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(DuelShot), false, this);
    FHitResult AimHit;
    FVector AimPoint = CameraOrigin + Direction * Rules->WeaponRange;
    if (GetWorld()->LineTraceSingleByChannel(AimHit, CameraOrigin, AimPoint, ECC_Visibility, Params)) AimPoint = AimHit.ImpactPoint;
    const FVector Muzzle = GetActorLocation() + FVector(0, 0, 40) + Direction * 103.f + FRotationMatrix(ServerAimRotation).GetUnitAxis(EAxis::Y) * 25.f;
    FHitResult Barrier;
    const FVector GunPivot = GetActorLocation() + FVector(0, 0, 40);
    const bool MuzzleBlocked = GetWorld()->SweepSingleByChannel(Barrier, GunPivot, Muzzle, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(5.f), Params);
    FHitResult Hit;
    FVector End = AimPoint;
    if (MuzzleBlocked)
    {
        Hit = Barrier;
        End = Barrier.ImpactPoint;
    }
    else if (GetWorld()->OverlapBlockingTestByChannel(Muzzle, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(5.f), Params)) End = Muzzle;
    else if (FVector::DotProduct((AimPoint - Muzzle).GetSafeNormal(), Direction) <= 0) End = Muzzle;
    // Continue just beyond the camera hit surface: stopping exactly on a
    // capsule boundary can miss because the muzzle approaches at another angle.
    else if (GetWorld()->LineTraceSingleByChannel(Hit, Muzzle,
        AimPoint + (AimPoint - Muzzle).GetSafeNormal() * 10.f, ECC_Visibility, Params))
        End = Hit.ImpactPoint;
    // A character obstructing the barrel is still a valid point-blank hit.
    if (ADuelCharacter* Target = Cast<ADuelCharacter>(Hit.GetActor()))
    {
        const float Applied = UGameplayStatics::ApplyPointDamage(Target, Rules->ShotDamage, Direction, Hit, Controller, this, nullptr);
        if (Applied > 0)
            if (ADuelPlayerController* PC = Cast<ADuelPlayerController>(Controller)) PC->ClientHitConfirmed();
    }
    MulticastShot(Muzzle, End);
#if !UE_BUILD_SHIPPING
    if (FParse::Param(FCommandLine::Get(), TEXT("DuelShotLog")) && Ammo % 10 == 0)
        UE_LOG(LogTPSDuel, Display, TEXT("SHOT ammo=%d camera=%s aim=%s muzzleBlocked=%d hit=%s end=%s"), Ammo, *CameraOrigin.ToString(), *ServerAimRotation.ToString(), MuzzleBlocked, *GetNameSafe(Hit.GetActor()), *End.ToString());
#endif
    ForceNetUpdate();
}
void ADuelCharacter::LocalFireFeedback()
{
    if (!CanCombat() || bReloading || Ammo <= 0 || !bLocalTrigger) return;
    if (FireSound) UGameplayStatics::PlaySound2D(this, FireSound, .4f);
    if (Controller) AddControllerPitchInput(-.15f);
}
void ADuelCharacter::MulticastShot_Implementation(FVector_NetQuantize Start, FVector_NetQuantize End)
{
    if (GetNetMode() == NM_DedicatedServer) return;
    if (IsLocallyControlled() && HasAuthority())
    {
        if (FireSound) UGameplayStatics::PlaySound2D(this, FireSound, .4f);
        AddControllerPitchInput(-.15f);
    }
    if (!IsLocallyControlled() && FireSound) UGameplayStatics::PlaySoundAtLocation(this, FireSound, Start, .4f);
    ADuelTracer* Tracer = GetWorld()->SpawnActor<ADuelTracer>();
    if (Tracer) Tracer->SetBeam(Start, End);
}
void ADuelCharacter::Reload()
{
    if (IsLocallyControlled()) { EndFire(); ServerReload(); }
}
void ADuelCharacter::ServerReload_Implementation()
{
    const UDuelSettings* Rules = GetDefault<UDuelSettings>();
    if (!CanCombat() || bReloading || Ammo >= Rules->MagazineCapacity) return;
    bServerTrigger = false;
    GetWorldTimerManager().ClearTimer(FireTimer);
    bReloading = true;
    GetWorldTimerManager().SetTimer(ReloadTimer, this, &ADuelCharacter::FinishReload, FMath::Max(.1f, Rules->ReloadSeconds), false);
    ForceNetUpdate();
}
void ADuelCharacter::FinishReload()
{
    if (CanCombat() && bReloading) Ammo = GetDefault<UDuelSettings>()->MagazineCapacity;
    bReloading = false;
    ForceNetUpdate();
}
void ADuelCharacter::StopCombat()
{
    if (IsLocallyControlled()) EndFire();
    bServerTrigger = false;
    bLocalTrigger = false;
    bReloading = false;
    bAiming = false;
    GetWorldTimerManager().ClearTimer(FireTimer);
    GetWorldTimerManager().ClearTimer(FeedbackTimer);
    GetWorldTimerManager().ClearTimer(ReloadTimer);
}
void ADuelCharacter::GrantProtection()
{
    if (!HasAuthority()) return;
    bProtected = true;
    GetWorldTimerManager().SetTimer(ProtectionTimer, this, &ADuelCharacter::ClearProtection, FMath::Max(.1f, GetDefault<UDuelSettings>()->ProtectionSeconds), false);
}
void ADuelCharacter::ClearProtection()
{
    bProtected = false;
    GetWorldTimerManager().ClearTimer(ProtectionTimer);
}
float ADuelCharacter::TakeDamage(float Damage, const FDamageEvent& Event, AController* EventInstigator, AActor* Causer)
{
    if (!HasAuthority() || EventInstigator == Controller) return 0.f;
    const float Before = Health;
    Health = DuelRules::DamageResult(Health, Damage, bProtected, CanCombat());
    if (Health == Before) return 0.f;
    if (!IsAlive())
    {
        StopCombat();
        ClearProtection();
        GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        GetCharacterMovement()->DisableMovement();
        if (ADuelGameMode* Mode = GetWorld()->GetAuthGameMode<ADuelGameMode>()) Mode->PlayerKilled(this, EventInstigator);
    }
    ForceNetUpdate();
    return Before - Health;
}
void ADuelCharacter::OnRep_Health()
{
    if (!IsAlive())
    {
        StopCombat();
        GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        GetCharacterMovement()->DisableMovement();
    }
}
void ADuelCharacter::EndPlay(const EEndPlayReason::Type Reason)
{
    GetWorldTimerManager().ClearAllTimersForObject(this);
    Super::EndPlay(Reason);
}

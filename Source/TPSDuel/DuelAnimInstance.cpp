#include "DuelAnimInstance.h"
#include "DuelCharacter.h"
#include "DuelSettings.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"

void UDuelAnimInstance::NativeInitializeAnimation()
{
    Super::NativeInitializeAnimation();
    if (!bDedicatedLocomotion) return;
    FireClip=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/ThirdParty/Quantum/Animations/Combat/ASP_Fire_Rifle_Hip.ASP_Fire_Rifle_Hip"));
    AimFireClip=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/ThirdParty/Quantum/Animations/Combat/ASP_Fire_Rifle_Ironsights.ASP_Fire_Rifle_Ironsights"));
    ReloadClip=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/ThirdParty/Quantum/Animations/Combat/ASP_Reload_Rifle_Hip.ASP_Reload_Rifle_Hip"));
    LastShot=-100.f; ReloadWasActive=false; ReloadMontage=nullptr;
}
void UDuelAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);
    auto* Character=Cast<ADuelCharacter>(TryGetPawnOwner());
    if (!Character) return;
    const FVector Local=Character->GetActorRotation().UnrotateVector(Character->GetVelocity());
    DuelCrouched=Character->bIsCrouched;
    DuelLocomotionRate=bDedicatedLocomotion ? 1.f : (Local.X < -20.f ? -1.f : 1.f);
    if (Local.SizeSquared2D()>100.f)
    {
        const float Target=FMath::RadiansToDegrees(FMath::Atan2(Local.Y,Local.X));
        DuelDirection=FMath::UnwindDegrees(FMath::FixedTurn(DuelDirection,Target,720.f*DeltaSeconds));
    }
    if (!bDedicatedLocomotion) return;
    const float Progress=Character->GetReloadProgress();
    const bool ReloadActive=Progress>=0.f;
    if (ReloadActive && !ReloadWasActive && ReloadClip)
    {
        ReloadMontage=PlaySlotAnimationAsDynamicMontage(ReloadClip,TEXT("DefaultSlot"),.12f,.15f,
            ReloadClip->GetPlayLength()/FMath::Max(.1f,GetDefault<UDuelSettings>()->ReloadSeconds));
        if (ReloadMontage) Montage_SetPosition(ReloadMontage,Progress*ReloadClip->GetPlayLength());
    }
    if (!ReloadActive && ReloadWasActive && ReloadMontage) Montage_Stop(.15f,ReloadMontage);
    ReloadWasActive=ReloadActive;
    const float Shot=Character->GetVisualShotTime();
    if (Shot>LastShot)
    {
        LastShot=Shot;
        UAnimSequence* SelectedFire=Character->IsAiming() && AimFireClip ? AimFireClip : FireClip;
        if (!ReloadActive && SelectedFire) PlaySlotAnimationAsDynamicMontage(SelectedFire,TEXT("DefaultSlot"),.03f,.08f);
    }
}

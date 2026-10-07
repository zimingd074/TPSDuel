#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "DuelWeaponPose.h"
#include "DuelAnimInstance.generated.h"

UCLASS()
class TPSDUEL_API UDuelAnimInstance : public UAnimInstance
{
    GENERATED_BODY()
public:
    virtual void NativeInitializeAnimation() override;
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;
    UPROPERTY(BlueprintReadOnly, Category="Locomotion") float DuelLocomotionRate=1.f;
    UPROPERTY(BlueprintReadOnly, Category="Locomotion") float DuelDirection=0.f;
    UPROPERTY(BlueprintReadOnly, Category="Locomotion") bool DuelCrouched=false;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Locomotion") bool bDedicatedLocomotion=false;
    void CacheWeaponPose(const FDuelWeaponPose& Pose) const { WeaponPoseSnapshot=Pose; }
    const FDuelWeaponPose& GetWeaponPoseSnapshot() const { return WeaponPoseSnapshot; }
private:
    UPROPERTY(Transient) class UAnimSequence* FireClip=nullptr;
    UPROPERTY(Transient) class UAnimSequence* AimFireClip=nullptr;
    UPROPERTY(Transient) class UAnimSequence* ReloadClip=nullptr;
    UPROPERTY(Transient) class UAnimMontage* ReloadMontage=nullptr;
    float LastShot=-100.f;
    bool ReloadWasActive=false;
    mutable FDuelWeaponPose WeaponPoseSnapshot=FDuelWeaponPose::Calculate(0.f,0.f,-1.f,0.f);
};

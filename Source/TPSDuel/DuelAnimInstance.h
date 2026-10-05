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
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;
    UPROPERTY(BlueprintReadOnly, Category="Locomotion") float DuelLocomotionRate=1.f;
    void CacheWeaponPose(const FDuelWeaponPose& Pose) const { WeaponPoseSnapshot=Pose; }
    const FDuelWeaponPose& GetWeaponPoseSnapshot() const { return WeaponPoseSnapshot; }
private:
    mutable FDuelWeaponPose WeaponPoseSnapshot=FDuelWeaponPose::Calculate(0.f,0.f,-1.f,0.f);
};

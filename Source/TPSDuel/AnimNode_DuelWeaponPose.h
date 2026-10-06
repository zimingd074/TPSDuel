#pragma once
#include "CoreMinimal.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "DuelWeaponPose.h"
#include "AnimNode_DuelWeaponPose.generated.h"

USTRUCT(BlueprintInternalUseOnly)
struct TPSDUEL_API FAnimNode_DuelWeaponPose : public FAnimNode_SkeletalControlBase
{
    GENERATED_BODY()
    FAnimNode_DuelWeaponPose();
    virtual bool HasPreUpdate() const override { return true; }
    virtual void PreUpdate(const UAnimInstance* Instance) override;
    virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;
    virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;
    virtual void EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& Transforms) override;
private:
    TArray<FBoneReference> Arms;
    TArray<FBoneReference> Fingers;
    TArray<FBoneReference> Legs;
    FBoneReference Spine;
    FBoneReference Pelvis;
    const class UDuelAnimInstance* SnapshotOwner=nullptr;
    FDuelWeaponPose Weapon=FDuelWeaponPose::Calculate(0.f,0.f,-1.f,0.f);
    float Carry = 0.f;
    float LegYaw = 0.f;
};

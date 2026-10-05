#pragma once
#include "CoreMinimal.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"
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
    float Pitch = 0.f;
    float Aim = 0.f;
    float Reload = -1.f;
    float Kick = 0.f;
};

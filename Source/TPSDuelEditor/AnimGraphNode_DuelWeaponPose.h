#pragma once
#include "CoreMinimal.h"
#include "AnimGraphNode_SkeletalControlBase.h"
#include "AnimNode_DuelWeaponPose.h"
#include "AnimGraphNode_DuelWeaponPose.generated.h"

UCLASS()
class TPSDUELEDITOR_API UAnimGraphNode_DuelWeaponPose : public UAnimGraphNode_SkeletalControlBase
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, Category=Settings) FAnimNode_DuelWeaponPose Node;
    virtual FText GetNodeTitle(ENodeTitleType::Type) const override { return FText::FromString(TEXT("Duel Weapon Grip and Reload")); }
protected:
    virtual FText GetControllerDescription() const override { return FText::FromString(TEXT("Duel Weapon Pose")); }
    virtual const FAnimNode_SkeletalControlBase* GetNode() const override { return &Node; }
};

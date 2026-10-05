#include "AnimNode_DuelWeaponPose.h"
#include "DuelCharacter.h"
#include "DuelWeaponPose.h"
#include "Animation/AnimInstance.h"
#include "TwoBoneIK.h"

FAnimNode_DuelWeaponPose::FAnimNode_DuelWeaponPose()
{
    for (const TCHAR* Name : {TEXT("upperarm_r"),TEXT("lowerarm_r"),TEXT("hand_r"),TEXT("upperarm_l"),TEXT("lowerarm_l"),TEXT("hand_l")})
    {
        FBoneReference Bone; Bone.BoneName=Name; Arms.Add(Bone);
    }
    for (const TCHAR* Side : {TEXT("r"),TEXT("l")})
        for (const TCHAR* Finger : {TEXT("index"),TEXT("middle"),TEXT("ring"),TEXT("pinky"),TEXT("thumb")})
            for (int32 Joint=1; Joint<=3; ++Joint)
            {
                FBoneReference Bone;
                Bone.BoneName=*FString::Printf(TEXT("%s_0%d_%s"),Finger,Joint,Side);
                Fingers.Add(Bone);
            }
}
void FAnimNode_DuelWeaponPose::PreUpdate(const UAnimInstance* Instance)
{
    if (const auto* Character=Cast<ADuelCharacter>(Instance->GetOwningActor()))
    {
        Pitch=Character->GetVisualAimPitch(); Aim=Character->GetVisualAimAlpha();
        Reload=Character->GetReloadProgress(); Kick=Character->GetVisualRecoil();
    }
}
void FAnimNode_DuelWeaponPose::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
    for (auto& Bone : Arms) Bone.Initialize(RequiredBones);
    for (auto& Bone : Fingers) Bone.Initialize(RequiredBones);
}
bool FAnimNode_DuelWeaponPose::IsValidToEvaluate(const USkeleton*, const FBoneContainer& RequiredBones)
{
    for (const auto& Bone : Arms) if (!Bone.IsValidToEvaluate(RequiredBones)) return false;
    return true;
}
void FAnimNode_DuelWeaponPose::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& Transforms)
{
    const auto& Bones=Output.Pose.GetPose().GetBoneContainer();
    const FDuelWeaponPose Weapon=FDuelWeaponPose::Calculate(Pitch,Aim,Reload,Kick);
    for (int32 Side=0; Side<2; ++Side)
    {
        const auto UpperIndex=Arms[Side*3].GetCompactPoseIndex(Bones);
        const auto LowerIndex=Arms[Side*3+1].GetCompactPoseIndex(Bones);
        const auto HandIndex=Arms[Side*3+2].GetCompactPoseIndex(Bones);
        FTransform Upper=Output.Pose.GetComponentSpaceTransform(UpperIndex);
        FTransform Lower=Output.Pose.GetComponentSpaceTransform(LowerIndex);
        FTransform Hand=Output.Pose.GetComponentSpaceTransform(HandIndex);
        const FVector Target=Side==0 ? Weapon.RightHand : Weapon.LeftHand;
        AnimationCore::SolveTwoBoneIK(Upper,Lower,Hand,FVector(Side==0 ? -60.f : 55.f,15,105),Target,false,1.f,1.f);
        // Palm orientation follows the gun; finger bones curl around grip/forend.
        const FQuat Palm=FRotationMatrix::MakeFromXY(FVector(0,1,0), FVector(Side==0 ? -1.f : 1.f,0,0)).ToQuat();
        Hand.SetRotation(Weapon.Gun.GetRotation()*Palm);
        TArray<FBoneTransform> ArmTransforms;
        ArmTransforms.Emplace(UpperIndex,Upper); ArmTransforms.Emplace(LowerIndex,Lower); ArmTransforms.Emplace(HandIndex,Hand);
        Output.Pose.LocalBlendCSBoneTransforms(ArmTransforms,1.f);
        Transforms.Append(ArmTransforms);
    }
    // Parent before child is required by LocalBlendCSBoneTransforms.
    for (auto& Finger : Fingers)
        if (Finger.IsValidToEvaluate(Bones))
        {
            const auto Index=Finger.GetCompactPoseIndex(Bones);
            const auto Parent=Bones.GetParentBoneIndex(Index);
            FTransform Local=Output.Pose.GetLocalSpaceTransform(Index);
            const FString Name=Finger.BoneName.ToString();
            const float Curl=Name.StartsWith(TEXT("thumb")) ? 30.f : Name.Contains(TEXT("index_01_r")) ? 18.f : Name.Contains(TEXT("_01_")) ? 45.f : 65.f;
            Local.SetRotation(Local.GetRotation()*FQuat(FVector::YAxisVector,FMath::DegreesToRadians(Curl)));
            FTransform Curled=Local*Output.Pose.GetComponentSpaceTransform(Parent);
            TArray<FBoneTransform> One; One.Emplace(Index,Curled);
            Output.Pose.LocalBlendCSBoneTransforms(One,1.f); Transforms.Append(One);
        }
    Transforms.Sort(FCompareBoneTransformIndex());
}

#include "AnimNode_DuelWeaponPose.h"
#include "DuelCharacter.h"
#include "DuelAnimInstance.h"
#include "DuelWeaponPose.h"
#include "Animation/AnimInstance.h"
#include "TwoBoneIK.h"
#include "GameFramework/CharacterMovementComponent.h"

FAnimNode_DuelWeaponPose::FAnimNode_DuelWeaponPose()
{
    Spine.BoneName=TEXT("spine_01");
    for (const TCHAR* Name : {TEXT("thigh_r"),TEXT("calf_r"),TEXT("foot_r"),TEXT("thigh_l"),TEXT("calf_l"),TEXT("foot_l")})
    {
        FBoneReference Bone; Bone.BoneName=Name; Legs.Add(Bone);
    }
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
        Weapon=Character->GetWeaponPoseForAnimation();
        SnapshotOwner=Cast<UDuelAnimInstance>(Instance);
        Carry=Weapon.CarryAlpha;
        const FVector Local=Character->GetActorRotation().UnrotateVector(Character->GetVelocity());
        StrideScale=1.f;
        LegYaw=Character->GetCharacterMovement()->IsFalling() || Local.SizeSquared2D()<100.f ? 0.f : FMath::RadiansToDegrees(FMath::Atan2(Local.Y,Local.X));
        if (Local.X < -20.f) LegYaw=FMath::UnwindDegrees(LegYaw-180.f);
        // Dedicated strafe/backward clips already contain the correct foot path.
        if (const auto* DuelInstance=Cast<UDuelAnimInstance>(Instance))
            if (DuelInstance->bDedicatedLocomotion)
            {
                LegYaw=0.f;
                // Blending forward and lateral strides shortens the diagonal
                // foot path. Restore its length without speeding up the cycle.
                if (!Character->GetCharacterMovement()->IsFalling() && Local.SizeSquared2D()>100.f)
                    StrideScale=FMath::Clamp((FMath::Abs(Local.X)+FMath::Abs(Local.Y))/Local.Size2D(),1.f,FMath::Sqrt(2.f));
            }
    }
}
void FAnimNode_DuelWeaponPose::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
    for (auto& Bone : Arms) Bone.Initialize(RequiredBones);
    for (auto& Bone : Fingers) Bone.Initialize(RequiredBones);
    for (auto& Bone : Legs) Bone.Initialize(RequiredBones);
    Spine.Initialize(RequiredBones);
}
bool FAnimNode_DuelWeaponPose::IsValidToEvaluate(const USkeleton*, const FBoneContainer& RequiredBones)
{
    for (const auto& Bone : Arms) if (!Bone.IsValidToEvaluate(RequiredBones)) return false;
    return true;
}
void FAnimNode_DuelWeaponPose::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& Transforms)
{
    const auto& Bones=Output.Pose.GetPose().GetBoneContainer();
    // Anchor to the evaluated shoulders, including crouch lean and recoil.
    // Publish this exact evaluated pose; gun and both hands move together.
    FDuelWeaponPose Evaluated=Weapon;
    if (Arms[0].IsValidToEvaluate(Bones) && Arms[3].IsValidToEvaluate(Bones))
    {
        const FVector Shoulder=(Output.Pose.GetComponentSpaceTransform(Arms[0].GetCompactPoseIndex(Bones)).GetLocation()
            +Output.Pose.GetComponentSpaceTransform(Arms[3].GetCompactPoseIndex(Bones)).GetLocation())*.5f;
        const FVector Offset=Shoulder-FVector(0,0,148);
        Evaluated.Gun.AddToTranslation(Offset); Evaluated.LeftHand+=Offset; Evaluated.RightHand+=Offset; Evaluated.Magazine+=Offset;
    }
    if (SnapshotOwner) SnapshotOwner->CacheWeaponPose(Evaluated);
    // Adapt fallback strides to direction, or restore diagonal stride length
    // on dedicated clips. Keep the authored foot height and knee bend plane.
    if (FMath::Abs(LegYaw)>1.f || StrideScale>1.001f)
        for (int32 Side=0; Side<2; ++Side)
        {
            if (!Legs[Side*3].IsValidToEvaluate(Bones) || !Legs[Side*3+1].IsValidToEvaluate(Bones) || !Legs[Side*3+2].IsValidToEvaluate(Bones)) continue;
            const auto UpperIndex=Legs[Side*3].GetCompactPoseIndex(Bones);
            const auto LowerIndex=Legs[Side*3+1].GetCompactPoseIndex(Bones);
            const auto FootIndex=Legs[Side*3+2].GetCompactPoseIndex(Bones);
            FTransform Reference=FTransform::Identity;
            for (auto Index=FootIndex; Index!=INDEX_NONE; Index=Bones.GetParentBoneIndex(Index)) Reference=Reference*Bones.GetRefPoseTransform(Index);
            FTransform Upper=Output.Pose.GetComponentSpaceTransform(UpperIndex);
            FTransform Lower=Output.Pose.GetComponentSpaceTransform(LowerIndex);
            FTransform Foot=Output.Pose.GetComponentSpaceTransform(FootIndex);
            FVector Delta=Foot.GetLocation()-Reference.GetLocation();
            Delta.X*=StrideScale; Delta.Y*=StrideScale;
            FVector Goal=Reference.GetLocation()+FQuat(FVector::UpVector,FMath::DegreesToRadians(LegYaw)).RotateVector(Delta);
            const FVector Knee=StrideScale>1.001f ? Lower.GetLocation() : Upper.GetLocation()+FVector(Side==0 ? -12.f : 12.f,55.f,0);
            if (FMath::Abs(LegYaw)>1.f) Goal.X=Side==0 ? FMath::Clamp(Goal.X,-54.f,-4.f) : FMath::Clamp(Goal.X,4.f,54.f);
            AnimationCore::SolveTwoBoneIK(Upper,Lower,Foot,Knee,Goal,false,1.f,1.f);
            TArray<FBoneTransform> Adjusted;
            Adjusted.Emplace(UpperIndex,Upper); Adjusted.Emplace(LowerIndex,Lower); Adjusted.Emplace(FootIndex,Foot);
            Output.Pose.LocalBlendCSBoneTransforms(Adjusted,1.f); Transforms.Append(Adjusted);
        }
    if (Spine.IsValidToEvaluate(Bones) && Carry>0.f)
    {
        const auto Index=Spine.GetCompactPoseIndex(Bones);
        FTransform Upright=Output.Pose.GetComponentSpaceTransform(Index);
        Upright.SetRotation(FQuat(FVector::ForwardVector,FMath::DegreesToRadians(3.f*Carry))*Upright.GetRotation());
        TArray<FBoneTransform> Adjusted; Adjusted.Emplace(Index,Upright);
        Output.Pose.LocalBlendCSBoneTransforms(Adjusted,1.f); Transforms.Append(Adjusted);
    }
    for (int32 Side=0; Side<2; ++Side)
    {
        const auto UpperIndex=Arms[Side*3].GetCompactPoseIndex(Bones);
        const auto LowerIndex=Arms[Side*3+1].GetCompactPoseIndex(Bones);
        const auto HandIndex=Arms[Side*3+2].GetCompactPoseIndex(Bones);
        FTransform Upper=Output.Pose.GetComponentSpaceTransform(UpperIndex);
        FTransform Lower=Output.Pose.GetComponentSpaceTransform(LowerIndex);
        FTransform Hand=Output.Pose.GetComponentSpaceTransform(HandIndex);
        const FVector Target=Side==0 ? Evaluated.RightHand : Evaluated.LeftHand;
        // A shoulder-relative, downward pole keeps crouched elbows below the
        // shoulder rather than aiming at the old standing-height pole.
        const FVector Elbow=Upper.GetLocation()+FVector(Side==0 ? -32.f : 32.f,-6.f,-30.f);
        AnimationCore::SolveTwoBoneIK(Upper,Lower,Hand,Elbow,Target,false,1.f,1.f);
        // Palm orientation follows the gun; finger bones curl around grip/forend.
        const FQuat Palm=FRotationMatrix::MakeFromXY(FVector(0,1,0), FVector(Side==0 ? -1.f : 1.f,0,0)).ToQuat();
        Hand.SetRotation(Evaluated.Gun.GetRotation()*Palm);
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
            // ASP already animates curled fingers. Build the final grip from
            // the reference pose to avoid applying a second curl on top.
            Local.SetRotation(Bones.GetRefPoseTransform(Index).GetRotation()*FQuat(FVector::YAxisVector,FMath::DegreesToRadians(Curl)));
            FTransform Curled=Local*Output.Pose.GetComponentSpaceTransform(Parent);
            TArray<FBoneTransform> One; One.Emplace(Index,Curled);
            Output.Pose.LocalBlendCSBoneTransforms(One,1.f); Transforms.Append(One);
        }
    Transforms.Sort(FCompareBoneTransformIndex());
}

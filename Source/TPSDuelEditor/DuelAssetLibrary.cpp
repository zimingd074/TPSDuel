#include "DuelAssetLibrary.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimSequence.h"
#include "Animation/Rig.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "EditorAnimUtils.h"
#include "Modules/ModuleManager.h"
#include "TwoBoneIK.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, TPSDuelEditor)

TArray<FString> UDuelAssetLibrary::DescribeQuantumBones()
{
    TArray<FString> Result;
    auto* Mesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/ThirdParty/Quantum/SKM_Character.SKM_Character"));
    if(!Mesh) return Result;
    const auto& Bones=Mesh->GetRefSkeleton();
    TArray<FTransform> ComponentPose;
    for(int32 Index=0; Index<Bones.GetNum(); ++Index)
    {
        const int32 Parent=Bones.GetParentIndex(Index);
        ComponentPose.Add(Parent==INDEX_NONE ? Bones.GetRefBonePose()[Index] : Bones.GetRefBonePose()[Index]*ComponentPose[Parent]);
        Result.Add(FString::Printf(TEXT("%s:%d:%s"),*Bones.GetBoneName(Index).ToString(),Parent,*ComponentPose[Index].GetLocation().ToString()));
    }
    return Result;
}

bool UDuelAssetLibrary::MakeQuantumHoldPose()
{
    auto* Mesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/ThirdParty/Quantum/SKM_Character.SKM_Character"));
    if(!Mesh || !Mesh->GetSkeleton()) return false;
    const auto& Bones=Mesh->GetRefSkeleton();
    TArray<FTransform> Reference;
    for(int32 Index=0; Index<Bones.GetNum(); ++Index)
    {
        const int32 Parent=Bones.GetParentIndex(Index);
        Reference.Add(Parent==INDEX_NONE ? Bones.GetRefBonePose()[Index] : Bones.GetRefBonePose()[Index]*Reference[Parent]);
    }
    TMap<FName,FTransform> Hold;
    for(int32 Side : {-1,1})
    {
        const FString Suffix=Side<0 ? TEXT("r") : TEXT("l");
        const FName UpperName(*(TEXT("upperarm_")+Suffix));
        const FName LowerName(*(TEXT("lowerarm_")+Suffix));
        const FName HandName(*(TEXT("hand_")+Suffix));
        const int32 UpperIndex=Bones.FindBoneIndex(UpperName);
        const int32 LowerIndex=Bones.FindBoneIndex(LowerName);
        const int32 HandIndex=Bones.FindBoneIndex(HandName);
        if(UpperIndex==INDEX_NONE || LowerIndex==INDEX_NONE || HandIndex==INDEX_NONE) return false;
        FTransform Upper=Reference[UpperIndex],Lower=Reference[LowerIndex],Hand=Reference[HandIndex];
        const FVector Target=Side<0 ? FVector(-24,38,130) : FVector(-18,66,130);
        AnimationCore::SolveTwoBoneIK(Upper,Lower,Hand,FVector(Side*60,15,105),Target,false,1.f,1.f);
        Hold.Add(UpperName,Upper.GetRelativeTransform(Reference[Bones.GetParentIndex(UpperIndex)]));
        Hold.Add(LowerName,Lower.GetRelativeTransform(Upper));
        Hold.Add(HandName,Hand.GetRelativeTransform(Lower));
    }
    // Bake the initial holding pose only into our duplicated locomotion clips.
    // The source Mannequin animations remain available for re-retargeting.
    for(const TCHAR* Name : {TEXT("Idle"),TEXT("Run"),TEXT("Walk"),TEXT("Jump_Start"),TEXT("Jump_Loop"),TEXT("Jump_End")})
    {
        const FString Path=FString::Printf(TEXT("/Game/ThirdParty/Quantum/Animations/Q_ThirdPerson%s.Q_ThirdPerson%s"),Name,Name);
        auto* Sequence=LoadObject<UAnimSequence>(nullptr,*Path);
        if(!Sequence || Sequence->GetSkeleton()!=Mesh->GetSkeleton()) return false;
        Sequence->Modify();
        for(const auto& Entry : Hold)
        {
            int32 TrackIndex=Sequence->GetAnimationTrackNames().Find(Entry.Key);
            if(TrackIndex==INDEX_NONE) TrackIndex=Sequence->AddNewRawTrack(Entry.Key);
            auto& Track=Sequence->GetRawAnimationTrack(TrackIndex);
            Track.PosKeys={Entry.Value.GetLocation()};
            Track.RotKeys={Entry.Value.GetRotation()};
            Track.ScaleKeys={Entry.Value.GetScale3D()};
        }
        Sequence->MarkRawDataAsModified();
        Sequence->PostEditChange();
        Sequence->MarkPackageDirty();
    }
    return true;
}

TArray<FString> UDuelAssetLibrary::RetargetQuantumLocomotion()
{
    auto* OldMesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Mannequin/Character/Mesh/SK_Mannequin.SK_Mannequin"));
    auto* NewMesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/ThirdParty/Quantum/SKM_Character.SKM_Character"));
    auto* Blueprint=LoadObject<UAnimBlueprint>(nullptr,TEXT("/Game/Mannequin/Animations/ThirdPerson_AnimBP.ThirdPerson_AnimBP"));
    auto* Rig=LoadObject<URig>(nullptr,TEXT("/Engine/EngineMeshes/Humanoid.Humanoid"));
    TArray<FString> Result;
    if(!OldMesh || !NewMesh || !Blueprint || !Rig) return Result;
    USkeleton* OldSkeleton=OldMesh->GetSkeleton();
    USkeleton* NewSkeleton=NewMesh->GetSkeleton();
    if(!OldSkeleton || !NewSkeleton) return Result;
    OldSkeleton->SetRigConfig(Rig);
    NewSkeleton->SetRigConfig(Rig);
    OldSkeleton->SetPreviewMesh(OldMesh);
    NewSkeleton->SetPreviewMesh(NewMesh);
    for(const FNode& Node : Rig->GetNodes())
    {
        const FName Name=Node.Name;
        if(OldMesh->GetRefSkeleton().FindBoneIndex(Name)!=INDEX_NONE)
            OldSkeleton->SetRigBoneMapping(Name,Name);
        // UE5 has five spine joints: map the UE4 chest to the actual upper chest.
        FName Target=Name;
        if(Name==TEXT("spine_02")) Target=TEXT("spine_03");
        if(Name==TEXT("spine_03")) Target=TEXT("spine_05");
        if(NewMesh->GetRefSkeleton().FindBoneIndex(Target)!=INDEX_NONE)
            NewSkeleton->SetRigBoneMapping(Name,Target);
    }
    NewSkeleton->SetBoneTranslationRetargetingMode(0,EBoneTranslationRetargetingMode::Skeleton,true);
    NewSkeleton->SetBoneTranslationRetargetingMode(0,EBoneTranslationRetargetingMode::Animation);
    const int32 Pelvis=NewSkeleton->GetReferenceSkeleton().FindBoneIndex(TEXT("pelvis"));
    if(Pelvis!=INDEX_NONE) NewSkeleton->SetBoneTranslationRetargetingMode(Pelvis,EBoneTranslationRetargetingMode::AnimationScaled);
    EditorAnimUtils::FNameDuplicationRule Names;
    Names.Prefix=TEXT("Q_");
    Names.FolderPath=TEXT("/Game/ThirdParty/Quantum/Animations");
    TArray<TWeakObjectPtr<UObject>> Sources;
    Sources.Add(Blueprint);
    EditorAnimUtils::FAnimationRetargetContext Context(Sources,true,true,Names);
    // Use the same retarget context without UI notifications/browser selection.
    Context.DuplicateAssetsToRetarget(NewSkeleton->GetOutermost(),&Names);
    Context.RetargetAnimations(OldSkeleton,NewSkeleton);
    for(UObject* Asset : Context.GetAllDuplicates())
        if(Asset) Result.Add(Asset->GetPathName());
    return Result;
}

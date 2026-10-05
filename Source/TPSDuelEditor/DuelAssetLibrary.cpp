#include "DuelAssetLibrary.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimSequence.h"
#include "Animation/Rig.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "EditorAnimUtils.h"
#include "Modules/ModuleManager.h"
#include "TwoBoneIK.h"
#include "AnimGraphNode_DuelWeaponPose.h"
#include "AnimGraphNode_LocalToComponentSpace.h"
#include "AnimGraphNode_ComponentToLocalSpace.h"
#include "AnimGraphNode_Root.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Engine/StaticMesh.h"
#include "RawMesh.h"
#include "AssetRegistryModule.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, TPSDuelEditor)

bool UDuelAssetLibrary::SplitQuantumRifle()
{
    auto* Source=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/ThirdParty/Quantum/SM_Rifle.SM_Rifle"));
    if (!Source) return false;
    FRawMesh Raw; Source->GetSourceModel(0).LoadRawMesh(Raw);
    FRawMesh Parts[2];
    for (auto& Part : Parts) Part.VertexPositions=Raw.VertexPositions;
    for (int32 Face=0; Face<Raw.WedgeIndices.Num()/3; ++Face)
    {
        FVector Center=FVector::ZeroVector;
        for (int32 Corner=0; Corner<3; ++Corner) Center+=Raw.VertexPositions[Raw.WedgeIndices[Face*3+Corner]]/3.f;
        // The sample's curved magazine is the isolated region below the receiver.
        // Preserve the original asset and every triangle, UV and tangent.
        const int32 PartIndex=Center.Y>10.f && Center.Y<25.f && Center.Z<7.2f ? 1 : 0;
        auto& Part=Parts[PartIndex];
        Part.FaceMaterialIndices.Add(Raw.FaceMaterialIndices[Face]);
        Part.FaceSmoothingMasks.Add(Raw.FaceSmoothingMasks.IsValidIndex(Face) ? Raw.FaceSmoothingMasks[Face] : 0);
        for (int32 Corner=0; Corner<3; ++Corner)
        {
            const int32 Wedge=Face*3+Corner;
            Part.WedgeIndices.Add(Raw.WedgeIndices[Wedge]);
            if (Raw.WedgeTangentX.IsValidIndex(Wedge)) Part.WedgeTangentX.Add(Raw.WedgeTangentX[Wedge]);
            if (Raw.WedgeTangentY.IsValidIndex(Wedge)) Part.WedgeTangentY.Add(Raw.WedgeTangentY[Wedge]);
            if (Raw.WedgeTangentZ.IsValidIndex(Wedge)) Part.WedgeTangentZ.Add(Raw.WedgeTangentZ[Wedge]);
            if (Raw.WedgeColors.IsValidIndex(Wedge)) Part.WedgeColors.Add(Raw.WedgeColors[Wedge]);
            for (int32 UV=0; UV<MAX_MESH_TEXTURE_COORDS; ++UV)
                if (Raw.WedgeTexCoords[UV].IsValidIndex(Wedge)) Part.WedgeTexCoords[UV].Add(Raw.WedgeTexCoords[UV][Wedge]);
        }
    }
    if (Parts[1].WedgeIndices.Num()<100 || Parts[1].WedgeIndices.Num()>Raw.WedgeIndices.Num()/2) return false;
    for (int32 Index=0; Index<2; ++Index)
    {
        const FString Name=Index==0 ? TEXT("SM_RifleBody") : TEXT("SM_RifleMagazine");
        const FString Path=TEXT("/Game/ThirdParty/Quantum/")+Name;
        auto* Package=CreatePackage(*Path);
        auto* Mesh=FindObject<UStaticMesh>(Package,*Name);
        if (!Mesh)
        {
            Mesh=NewObject<UStaticMesh>(Package,*Name,RF_Public|RF_Standalone);
            Mesh->InitResources(); Mesh->SetLightingGuid(); Mesh->AddSourceModel();
            FAssetRegistryModule::AssetCreated(Mesh);
        }
        Mesh->GetStaticMaterials()=Source->GetStaticMaterials();
        auto& Model=Mesh->GetSourceModel(0);
        Model.BuildSettings.bRecomputeNormals=false; Model.BuildSettings.bRecomputeTangents=false;
        Model.BuildSettings.bGenerateLightmapUVs=false; Model.SaveRawMesh(Parts[Index]);
        Mesh->Build(false); Mesh->PostEditChange(); Mesh->MarkPackageDirty();
    }
    return true;
}

TArray<FString> UDuelAssetLibrary::DescribeRifleGeometry()
{
    TArray<FString> Result;
    auto* Mesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/ThirdParty/Quantum/SM_Rifle.SM_Rifle"));
    if (!Mesh) return Result;
    FRawMesh Raw; Mesh->GetSourceModel(0).LoadRawMesh(Raw);
    for (int32 Y=-30; Y<60; Y+=3)
    {
        FBox Bounds(ForceInit);
        for (const FVector& Vertex : Raw.VertexPositions)
            if (Vertex.Y>=Y && Vertex.Y<Y+3) Bounds+=Vertex;
        if (Bounds.IsValid) Result.Add(FString::Printf(TEXT("Y=%d..%d X=%f..%f Z=%f..%f"),Y,Y+3,Bounds.Min.X,Bounds.Max.X,Bounds.Min.Z,Bounds.Max.Z));
    }
    return Result;
}

bool UDuelAssetLibrary::InstallQuantumWeaponPose()
{
    auto* Blueprint=LoadObject<UAnimBlueprint>(nullptr,TEXT("/Game/ThirdParty/Quantum/Animations/Q_ThirdPerson_AnimBP.Q_ThirdPerson_AnimBP"));
    if (!Blueprint) return false;
    for (UEdGraph* Graph : Blueprint->FunctionGraphs)
    {
        UAnimGraphNode_Root* Root=nullptr;
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (Cast<UAnimGraphNode_DuelWeaponPose>(Node))
            {
                FKismetEditorUtilities::CompileBlueprint(Blueprint);
                Blueprint->MarkPackageDirty();
                return Blueprint->Status!=BS_Error;
            }
            if (auto* Candidate=Cast<UAnimGraphNode_Root>(Node)) Root=Candidate;
        }
        if (!Root) continue;
        UEdGraphPin* Input=Root->FindPin(TEXT("Result"));
        if (!Input || Input->LinkedTo.Num()!=1) return false;
        UEdGraphPin* Source=Input->LinkedTo[0];
        auto AddNode=[Graph](UEdGraphNode* Node, int32 X)
        {
            Graph->AddNode(Node,false,false); Node->CreateNewGuid(); Node->PostPlacedNewNode();
            Node->AllocateDefaultPins(); Node->NodePosX=X; Node->NodePosY=200;
        };
        Blueprint->Modify(); Graph->Modify();
        auto* ToComponent=NewObject<UAnimGraphNode_LocalToComponentSpace>(Graph);
        auto* Weapon=NewObject<UAnimGraphNode_DuelWeaponPose>(Graph);
        auto* ToLocal=NewObject<UAnimGraphNode_ComponentToLocalSpace>(Graph);
        AddNode(ToComponent,Root->NodePosX-600); AddNode(Weapon,Root->NodePosX-400); AddNode(ToLocal,Root->NodePosX-200);
        Input->BreakAllPinLinks();
        Source->MakeLinkTo(ToComponent->FindPinChecked(TEXT("LocalPose")));
        ToComponent->FindPinChecked(TEXT("ComponentPose"))->MakeLinkTo(Weapon->FindPinChecked(TEXT("ComponentPose")));
        Weapon->FindPinChecked(TEXT("Pose"))->MakeLinkTo(ToLocal->FindPinChecked(TEXT("ComponentPose")));
        ToLocal->FindPinChecked(TEXT("Pose"))->MakeLinkTo(Input);
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        Blueprint->MarkPackageDirty();
        return Blueprint->Status!=BS_Error;
    }
    return false;
}

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

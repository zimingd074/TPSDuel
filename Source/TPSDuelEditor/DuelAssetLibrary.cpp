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
#include "K2Node_DynamicCast.h"
#include "AnimGraphNode_StateMachine.h"
#include "AnimGraphNode_BlendSpacePlayer.h"
#include "AnimGraphNode_BlendListByBool.h"
#include "K2Node_VariableGet.h"
#include "DuelAnimInstance.h"
#include "Animation/BlendSpace.h"
#include "AnimGraphNode_Slot.h"
#include "AnimGraphNode_LayeredBoneBlend.h"
#include "AnimGraphNode_SaveCachedPose.h"
#include "AnimGraphNode_UseCachedPose.h"
#include "UObject/UnrealType.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, TPSDuelEditor)

bool UDuelAssetLibrary::ConfigureQuantumLocomotion()
{
    auto* Blueprint=LoadObject<UAnimBlueprint>(nullptr,TEXT("/Game/ThirdParty/Quantum/Animations/Q_ThirdPerson_AnimBP.Q_ThirdPerson_AnimBP"));
    if (!Blueprint) return false;
    Blueprint->Modify(); Blueprint->ParentClass=UDuelAnimInstance::StaticClass();
    // Older local assets may contain an experimental template/combat switch.
    // Restore the single rifle locomotion path before compiling the blueprint.
    TArray<UEdGraph*> PreviousGraphs; Blueprint->GetAllGraphs(PreviousGraphs);
    for (auto* Graph : PreviousGraphs)
        for (auto* Node : TArray<UEdGraphNode*>(Graph->Nodes))
            if (auto* Select=Cast<UAnimGraphNode_BlendListByBool>(Node))
            {
                auto* RiflePin=Select->FindPin(TEXT("BlendPose_0"));
                auto* Output=Select->FindPin(TEXT("Pose"));
                if (!RiflePin || RiflePin->LinkedTo.Num()!=1 || !Output) return false;
                auto* Rifle=Cast<UAnimGraphNode_BlendSpacePlayer>(RiflePin->LinkedTo[0]->GetOwningNode());
                if (!Rifle) return false;
                for (auto* Target : TArray<UEdGraphPin*>(Output->LinkedTo))
                {
                    Target->BreakAllPinLinks(); Rifle->FindPinChecked(TEXT("Pose"))->MakeLinkTo(Target);
                }
                FBlueprintEditorUtils::RemoveNode(Blueprint,Select,true);
                for (auto* Candidate : TArray<UEdGraphNode*>(Graph->Nodes))
                {
                    auto* Getter=Cast<UK2Node_VariableGet>(Candidate);
                    if ((Cast<UAnimGraphNode_BlendSpacePlayer>(Candidate) && Candidate!=Rifle) ||
                        (Getter && Getter->VariableReference.GetMemberName()==TEXT("DuelCombatLocomotion")))
                        FBlueprintEditorUtils::RemoveNode(Blueprint,Candidate,true);
                }
            }
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    auto* Combat=LoadObject<UBlendSpace>(nullptr,TEXT("/Game/ThirdParty/Quantum/Animations/Combat/BS_Combat.BS_Combat"));
    TArray<UEdGraph*> Graphs; Blueprint->GetAllGraphs(Graphs);
    bool Configured=false;
    for (auto* Graph : Graphs)
        for (auto* Node : TArray<UEdGraphNode*>(Graph->Nodes))
            if (auto* Player=Cast<UAnimGraphNode_BlendSpacePlayer>(Node))
            {
                if (Combat) Player->Node.BlendSpace=Combat;
                for (auto& Property : Player->ShowPinForProperties)
                    if (Property.PropertyName==TEXT("PlayRate") || Property.PropertyName==TEXT("X") || Property.PropertyName==TEXT("Y")) Property.bShowPin=true;
                Player->ReconstructNode();
                auto* Rate=Player->FindPin(TEXT("PlayRate"));
                if (!Rate) return false;
                UK2Node_VariableGet* Getter=nullptr;
                for (auto* Candidate : Graph->Nodes)
                    if (auto* Variable=Cast<UK2Node_VariableGet>(Candidate))
                        if (Variable->VariableReference.GetMemberName()==TEXT("DuelLocomotionRate")) Getter=Variable;
                if (!Getter)
                {
                    Getter=NewObject<UK2Node_VariableGet>(Graph);
                    Getter->VariableReference.SetSelfMember(TEXT("DuelLocomotionRate"));
                    Graph->AddNode(Getter,false,false); Getter->CreateNewGuid(); Getter->PostPlacedNewNode(); Getter->AllocateDefaultPins();
                    Getter->NodePosX=Player->NodePosX-250; Getter->NodePosY=Player->NodePosY+160;
                }
                Rate->BreakAllPinLinks(); Getter->FindPinChecked(TEXT("DuelLocomotionRate"))->MakeLinkTo(Rate); Configured=true;
                if (Combat)
                {
                    auto* X=Player->FindPinChecked(TEXT("X"));
                    auto* Y=Player->FindPinChecked(TEXT("Y"));
                    Y->BreakAllPinLinks(); X->BreakAllPinLinks();
                    for (const TCHAR* Member : {TEXT("Speed"),TEXT("DuelDirection")})
                    {
                        UK2Node_VariableGet* Variable=nullptr;
                        for (auto* Candidate : Graph->Nodes)
                            if (auto* Existing=Cast<UK2Node_VariableGet>(Candidate))
                                if (Existing->VariableReference.GetMemberName()==Member) Variable=Existing;
                        if (!Variable)
                        {
                            Variable=NewObject<UK2Node_VariableGet>(Graph);
                            Variable->VariableReference.SetSelfMember(Member);
                            Graph->AddNode(Variable,false,false); Variable->CreateNewGuid(); Variable->PostPlacedNewNode(); Variable->AllocateDefaultPins();
                            Variable->NodePosX=Player->NodePosX-250;
                        }
                        Variable->FindPinChecked(Member)->MakeLinkTo(FString(Member)==TEXT("Speed") ? Y : X);
                    }
                }
            }
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint); Blueprint->MarkPackageDirty();
    if (auto* Defaults=Cast<UDuelAnimInstance>(Blueprint->GeneratedClass->GetDefaultObject())) Defaults->bDedicatedLocomotion=Combat!=nullptr;
    return Configured && Blueprint->Status!=BS_Error;
}

TArray<FString> UDuelAssetLibrary::DescribeLocomotion()
{
    TArray<FString> Result;
    auto* Blueprint=LoadObject<UAnimBlueprint>(nullptr,TEXT("/Game/ThirdParty/Quantum/Animations/Q_ThirdPerson_AnimBP.Q_ThirdPerson_AnimBP"));
    if (!Blueprint) return Result;
    TArray<UEdGraph*> Graphs;
    Blueprint->GetAllGraphs(Graphs);
    for (auto* Graph : Graphs)
        for (auto* Node : Graph->Nodes)
        {
            FString Detail=Graph->GetName()+TEXT(": ")+Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
            if (auto* CastNode=Cast<UK2Node_DynamicCast>(Node)) Detail+=TEXT(" target=")+GetPathNameSafe(CastNode->TargetType);
            for (auto* Pin : Node->Pins)
            {
                if (Pin->Direction==EGPD_Input) Detail+=FString::Printf(TEXT(" [%s=%s links=%d]"),*Pin->PinName.ToString(),*Pin->DefaultValue,Pin->LinkedTo.Num());
                for (auto* Link : Pin->LinkedTo) Detail+=FString::Printf(TEXT(" {%s <- %s.%s}"),*Pin->PinName.ToString(),*Link->GetOwningNode()->GetName(),*Link->PinName.ToString());
            }
            Result.Add(Detail);
        }
    for (const TCHAR* Name : {TEXT("Idle"),TEXT("Walk"),TEXT("Run")})
    {
        const FString Path=FString::Printf(TEXT("/Game/ThirdParty/Quantum/Animations/Q_ThirdPerson%s.Q_ThirdPerson%s"),Name,Name);
        auto* Sequence=LoadObject<UAnimSequence>(nullptr,*Path);
        if (!Sequence) continue;
        for (const TCHAR* Bone : {TEXT("thigh_l"),TEXT("calf_l"),TEXT("foot_l"),TEXT("thigh_r")})
        {
            const int32 Index=Sequence->GetAnimationTrackNames().Find(FName(Bone));
            if (Index==INDEX_NONE) { Result.Add(Path+TEXT(" missing ")+Bone); continue; }
            const auto& Track=Sequence->GetRawAnimationTrack(Index);
            float Maximum=0;
            for (const auto& Rotation : Track.RotKeys) Maximum=FMath::Max(Maximum,Track.RotKeys[0].AngularDistance(Rotation));
            Result.Add(FString::Printf(TEXT("%s %s keys=%d swing=%.2fdeg"),Name,Bone,Track.RotKeys.Num(),FMath::RadiansToDegrees(Maximum)));
        }
    }
    return Result;
}

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
        UAnimGraphNode_DuelWeaponPose* ExistingWeapon=nullptr;
        UAnimGraphNode_StateMachine* Locomotion=nullptr;
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (auto* Candidate=Cast<UAnimGraphNode_DuelWeaponPose>(Node)) ExistingWeapon=Candidate;
            if (auto* Candidate=Cast<UAnimGraphNode_StateMachine>(Node)) Locomotion=Candidate;
            if (auto* Candidate=Cast<UAnimGraphNode_Root>(Node)) Root=Candidate;
        }
        if (!Root) continue;
        // Rebuild the complete pose chain, including assets saved by older tools.
        // An existing grip node alone does not prove the locomotion is connected.
        if (!Locomotion) return false;
        UEdGraphPin* Input=Root->FindPin(TEXT("Result"));
        if (!Input || Input->LinkedTo.Num()!=1) return false;
        UEdGraphPin* Source=Locomotion->FindPinChecked(TEXT("Pose"));
        auto AddNode=[Graph](UEdGraphNode* Node, int32 X)
        {
            Graph->AddNode(Node,false,false); Node->CreateNewGuid(); Node->PostPlacedNewNode();
            Node->AllocateDefaultPins(); Node->NodePosX=X; Node->NodePosY=200;
        };
        Blueprint->Modify(); Graph->Modify();
        TArray<UEdGraphNode*> OldControls;
        for (auto* Node : Graph->Nodes)
            if (Cast<UAnimGraphNode_LocalToComponentSpace>(Node) || Cast<UAnimGraphNode_ComponentToLocalSpace>(Node) || Cast<UAnimGraphNode_Slot>(Node) || Cast<UAnimGraphNode_LayeredBoneBlend>(Node) || Cast<UAnimGraphNode_SaveCachedPose>(Node) || Cast<UAnimGraphNode_UseCachedPose>(Node) || Node==ExistingWeapon) OldControls.Add(Node);
        for (auto* Node : OldControls) FBlueprintEditorUtils::RemoveNode(Blueprint,Node,true);
        auto* ToComponent=NewObject<UAnimGraphNode_LocalToComponentSpace>(Graph);
        auto* Weapon=NewObject<UAnimGraphNode_DuelWeaponPose>(Graph);
        auto* ToLocal=NewObject<UAnimGraphNode_ComponentToLocalSpace>(Graph);
        auto* Slot=NewObject<UAnimGraphNode_Slot>(Graph);
        Slot->Node.SlotName=TEXT("DefaultSlot");
        auto* Layer=NewObject<UAnimGraphNode_LayeredBoneBlend>(Graph);
        Layer->Node.BlendWeights.Empty(); Layer->Node.BlendPoses.Empty(); Layer->Node.LayerSetup.Empty();
        Layer->Node.BlendWeights.Add(1.f); Layer->Node.BlendPoses.AddDefaulted(); Layer->Node.LayerSetup.AddDefaulted();
        Layer->Node.bMeshSpaceRotationBlend=true;
        FBranchFilter Filter; Filter.BoneName=TEXT("spine_01"); Filter.BlendDepth=3;
        Layer->Node.LayerSetup[0].BranchFilters.Add(Filter);
        auto* Cache=NewObject<UAnimGraphNode_SaveCachedPose>(Graph);
        Cache->CacheName=TEXT("DuelLocomotion");
        auto* Base=NewObject<UAnimGraphNode_UseCachedPose>(Graph);
        auto* ActionSource=NewObject<UAnimGraphNode_UseCachedPose>(Graph);
        Base->SaveCachedPoseNode=Cache; ActionSource->SaveCachedPoseNode=Cache;
        AddNode(Cache,Root->NodePosX-1600); AddNode(Base,Root->NodePosX-1400); AddNode(ActionSource,Root->NodePosX-1200);
        AddNode(Slot,Root->NodePosX-1000); AddNode(Layer,Root->NodePosX-800);
        AddNode(ToComponent,Root->NodePosX-600); AddNode(Weapon,Root->NodePosX-400); AddNode(ToLocal,Root->NodePosX-200);
        Input->BreakAllPinLinks();
        Source->BreakAllPinLinks();
        Source->MakeLinkTo(Cache->FindPinChecked(TEXT("Pose")));
        ActionSource->FindPinChecked(TEXT("Pose"))->MakeLinkTo(Slot->FindPinChecked(TEXT("Source")));
        Base->FindPinChecked(TEXT("Pose"))->MakeLinkTo(Layer->FindPinChecked(TEXT("BasePose")));
        Slot->FindPinChecked(TEXT("Pose"))->MakeLinkTo(Layer->FindPinChecked(TEXT("BlendPoses_0")));
        Layer->FindPinChecked(TEXT("Pose"))->MakeLinkTo(ToComponent->FindPinChecked(TEXT("LocalPose")));
        ToComponent->FindPinChecked(TEXT("ComponentPose"))->MakeLinkTo(Weapon->FindPinChecked(TEXT("ComponentPose")));
        Weapon->FindPinChecked(TEXT("Pose"))->MakeLinkTo(ToLocal->FindPinChecked(TEXT("ComponentPose")));
        ToLocal->FindPinChecked(TEXT("Pose"))->MakeLinkTo(Input);
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        if (auto* Defaults=Cast<UDuelAnimInstance>(Blueprint->GeneratedClass->GetDefaultObject()))
            Defaults->bDedicatedLocomotion=LoadObject<UBlendSpace>(nullptr,TEXT("/Game/ThirdParty/Quantum/Animations/Combat/BS_Combat.BS_Combat"))!=nullptr;
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

TArray<FString> UDuelAssetLibrary::RetargetCombatAnimations()
{
    TArray<FString> Result;
    auto* OldMesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/AnimStarterPack/UE4_Mannequin/Mesh/SK_Mannequin.SK_Mannequin"));
    auto* NewMesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/ThirdParty/Quantum/SKM_Character.SKM_Character"));
    auto* Rig=LoadObject<URig>(nullptr,TEXT("/Engine/EngineMeshes/Humanoid.Humanoid"));
    if (!OldMesh || !NewMesh || !Rig) return Result;
    auto* OldSkeleton=OldMesh->GetSkeleton(); auto* NewSkeleton=NewMesh->GetSkeleton();
    OldSkeleton->SetRigConfig(Rig); NewSkeleton->SetRigConfig(Rig);
    OldSkeleton->SetPreviewMesh(OldMesh); NewSkeleton->SetPreviewMesh(NewMesh);
    for (const auto& Node : Rig->GetNodes())
    {
        if (OldMesh->GetRefSkeleton().FindBoneIndex(Node.Name)!=INDEX_NONE) OldSkeleton->SetRigBoneMapping(Node.Name,Node.Name);
        FName Target=Node.Name;
        if (Target==TEXT("spine_02")) Target=TEXT("spine_03");
        if (Node.Name==TEXT("spine_03")) Target=TEXT("spine_05");
        if (NewMesh->GetRefSkeleton().FindBoneIndex(Target)!=INDEX_NONE) NewSkeleton->SetRigBoneMapping(Node.Name,Target);
    }
    NewSkeleton->SetBoneTranslationRetargetingMode(0,EBoneTranslationRetargetingMode::Skeleton,true);
    NewSkeleton->SetBoneTranslationRetargetingMode(0,EBoneTranslationRetargetingMode::Animation);
    NewSkeleton->SetBoneTranslationRetargetingMode(NewMesh->GetRefSkeleton().FindBoneIndex(TEXT("pelvis")),EBoneTranslationRetargetingMode::AnimationScaled);
    const TCHAR* Names[]={TEXT("Idle_Rifle_Hip"),TEXT("Idle_Rifle_Ironsights"),TEXT("Jog_Fwd_Rifle"),TEXT("Jog_Bwd_Rifle"),TEXT("Jog_Lt_Rifle"),TEXT("Jog_Rt_Rifle"),TEXT("Walk_Fwd_Rifle_Ironsights"),TEXT("Walk_Bwd_Rifle_Ironsights"),TEXT("Walk_Lt_Rifle_Ironsights"),TEXT("Walk_Rt_Rifle_Ironsights"),TEXT("Sprint_Fwd_Rifle"),TEXT("Fire_Rifle_Hip"),TEXT("Fire_Rifle_Ironsights"),TEXT("Reload_Rifle_Hip"),TEXT("Reload_Rifle_Ironsights"),TEXT("Jump_From_Jog")};
    TArray<TWeakObjectPtr<UObject>> Sources;
    for (const TCHAR* Name : Names)
    {
        const FString Target=FString::Printf(TEXT("/Game/ThirdParty/Quantum/Animations/Combat/ASP_%s.ASP_%s"),Name,Name);
        if (auto* Existing=LoadObject<UAnimSequence>(nullptr,*Target, nullptr,LOAD_NoWarn)) Result.Add(Target);
        else
        {
            const FString Source=FString::Printf(TEXT("/Game/AnimStarterPack/%s.%s"),Name,Name);
            auto* Sequence=LoadObject<UAnimSequence>(nullptr,*Source);
            if (!Sequence) return TArray<FString>();
            Sources.Add(Sequence);
        }
    }
    if (Sources.Num())
    {
        EditorAnimUtils::FNameDuplicationRule Rule; Rule.Prefix=TEXT("ASP_"); Rule.FolderPath=TEXT("/Game/ThirdParty/Quantum/Animations/Combat");
        EditorAnimUtils::FAnimationRetargetContext Context(Sources,true,true,Rule);
        Context.DuplicateAssetsToRetarget(NewSkeleton->GetOutermost(),&Rule); Context.RetargetAnimations(OldSkeleton,NewSkeleton);
        for (auto* Asset : Context.GetAllDuplicates()) if (Asset) Result.Add(Asset->GetPathName());
    }
    const FString BSName=TEXT("BS_Combat");
    auto* Package=CreatePackage(TEXT("/Game/ThirdParty/Quantum/Animations/Combat/BS_Combat"));
    auto* Blend=FindObject<UBlendSpace>(Package,*BSName);
    if (!Blend) { Blend=NewObject<UBlendSpace>(Package,*BSName,RF_Public|RF_Standalone); FAssetRegistryModule::AssetCreated(Blend); }
    Blend->SetSkeleton(NewSkeleton);
    auto* Parameters=FindFProperty<FStructProperty>(UBlendSpaceBase::StaticClass(),TEXT("BlendParameters"));
    if (!Parameters) return TArray<FString>();
    auto* Direction=Parameters->ContainerPtrToValuePtr<FBlendParameter>(Blend,0);
    Direction->DisplayName=TEXT("Direction"); Direction->Min=-180.f; Direction->Max=180.f; Direction->GridNum=4;
    auto* Speed=Parameters->ContainerPtrToValuePtr<FBlendParameter>(Blend,1);
    Speed->DisplayName=TEXT("Speed"); Speed->Min=0.f; Speed->Max=375.f; Speed->GridNum=2;
    while (Blend->GetNumberOfBlendSamples()) Blend->DeleteSample(Blend->GetNumberOfBlendSamples()-1);
    TArray<int32> Mapping; TArray<FEditorElement> Grid;
    const TCHAR* Walk[]={TEXT("Walk_Bwd_Rifle_Ironsights"),TEXT("Walk_Lt_Rifle_Ironsights"),TEXT("Walk_Fwd_Rifle_Ironsights"),TEXT("Walk_Rt_Rifle_Ironsights"),TEXT("Walk_Bwd_Rifle_Ironsights")};
    const TCHAR* Jog[]={TEXT("Jog_Bwd_Rifle"),TEXT("Jog_Lt_Rifle"),TEXT("Jog_Fwd_Rifle"),TEXT("Jog_Rt_Rifle"),TEXT("Jog_Bwd_Rifle")};
    for (int32 X=0; X<5; ++X) for (int32 Y=0; Y<3; ++Y)
    {
        const TCHAR* Name=Y==0 ? TEXT("Idle_Rifle_Hip") : (Y==1 ? Walk[X] : Jog[X]);
        auto* Sequence=LoadObject<UAnimSequence>(nullptr,*FString::Printf(TEXT("/Game/ThirdParty/Quantum/Animations/Combat/ASP_%s.ASP_%s"),Name,Name));
        if (!Sequence || !Blend->AddSample(Sequence,FVector(-180+90*X,187.5f*Y,0))) return TArray<FString>();
        Mapping.Add(X*3+Y); FEditorElement Element; Element.Indices[0]=X*3+Y; Element.Weights[0]=1.f; Grid.Add(Element);
    }
    Blend->TargetWeightInterpolationSpeedPerSec=8.f;
    Blend->ValidateSampleData(); Blend->FillupGridElements(Mapping,Grid); Blend->MarkPackageDirty();
    Result.Add(Blend->GetPathName());
    return Result;
}

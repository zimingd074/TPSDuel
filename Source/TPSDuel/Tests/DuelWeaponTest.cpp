#include "DuelPlayerController.h"
#include "DuelCharacter.h"
#include "TPSDuel.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformMisc.h"
#include "UnrealClient.h"
#include "Misc/App.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"

bool ADuelPlayerController::TickWeaponTest()
{
#if !UE_BUILD_SHIPPING
    if (!FParse::Param(FCommandLine::Get(),TEXT("DuelWeaponTest"))) return false;
    auto* Self=Cast<ADuelCharacter>(GetPawn());
    if (!Self) return true;
    const double Now=GetWorld()->TimeSeconds;
    if (SmokeStart==0)
    {
        SmokeStart=InputStageStart=Now;
        FApp::SetFixedDeltaTime(1.0/60.0); FApp::SetUseFixedTimeStep(true);
        Self->SetActorLocationAndRotation(FVector(-700,-50,90),FRotator::ZeroRotator,false,nullptr,ETeleportType::TeleportPhysics);
        Self->GetMesh()->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
        SetControlRotation(FRotator::ZeroRotator);
    }
    auto Report=[this,Now](bool Passed,const FString& Detail)
    {
        const FString Result=(Passed ? TEXT("PASS ") : TEXT("FAIL "))+Detail;
        FFileHelper::SaveStringToFile(Result,*FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("WeaponTest.txt")));
        UE_LOG(LogTPSDuel,Display,TEXT("WEAPON_TEST %s"),*Result);
        ClearLocalInput(); if (InputTestCeiling.IsValid()) InputTestCeiling->Destroy();
        FApp::SetUseFixedTimeStep(false);
        SmokeExitAt=Now+1;
    };
    if (SmokeExitAt>0) { if(Now>=SmokeExitAt) FPlatformMisc::RequestExit(false); return true; }
    if (Now-SmokeStart>25) { Report(false,FString::Printf(TEXT("stage=%d timeout"),InputTestStage)); return true; }
    auto Next=[this,Now]() { ++InputTestStage; InputStageStart=Now; };
    auto Sample=[&](const TCHAR* Name,bool CheckHeight)
    {
        auto* Mesh=Self->GetMesh(); const auto Pose=Self->GetVisualWeaponPose();
        auto Bone=[Mesh](const TCHAR* BoneName) { return Mesh->GetBoneLocation(BoneName,EBoneSpaces::ComponentSpace); };
        const float LeftError=FVector::Dist(Bone(TEXT("hand_l")),Pose.LeftHand);
        const float RightError=FVector::Dist(Bone(TEXT("hand_r")),Pose.RightHand);
        const float RightElbow=Bone(TEXT("lowerarm_r")).Z-Bone(TEXT("upperarm_r")).Z;
        const float LeftElbow=Bone(TEXT("lowerarm_l")).Z-Bone(TEXT("upperarm_l")).Z;
        const float RightHand=Bone(TEXT("hand_r")).Z-Bone(TEXT("upperarm_r")).Z;
        UE_LOG(LogTPSDuel,Display,TEXT("WEAPON_POSE name=%s crouch=%d leftError=%.2f rightError=%.2f elbowR=%.2f elbowL=%.2f handR=%.2f obstruction=%.3f ammo=%d"),Name,Self->bIsCrouched,LeftError,RightError,RightElbow,LeftElbow,RightHand,Self->GetVisualObstructionAlpha(),Self->GetAmmo());
        FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Screenshots"),FString(Name)+TEXT(".png")),true,false);
        if (LeftError>1.f || RightError>1.f || (CheckHeight && (RightElbow>3.f || LeftElbow>3.f || RightHand>5.f)))
        { Report(false,FString(Name)+TEXT(" grip or shoulder-height check")); return false; }
        return true;
    };
    const double Elapsed=Now-InputStageStart;
    // Sample every animation frame during ADS bursts, including recoil peaks.
    // The chamber/stock top is 18 cm above the model origin. A 15 cm envelope
    // includes the head radius, weapon half-width and a small separation margin.
    if ((InputTestStage>=12 && InputTestStage<=17 && Elapsed>.05) || InputTestStage==20)
    {
        const auto Pose=Self->GetVisualWeaponPose();
        const FVector Head=Self->GetMesh()->GetBoneLocation(TEXT("head"),EBoneSpaces::ComponentSpace)+FVector(0,0,6);
        const float Clearance=FMath::PointDistToSegment(Head,Pose.Gun.TransformPosition(FVector(0,-27,18)),Pose.Gun.TransformPosition(FVector(0,15,18)));
        WeaponHeadMinimum=FMath::Min(WeaponHeadMinimum,Clearance); ++WeaponClearanceSamples;
        if (Clearance<15.f)
        { Report(false,FString::Printf(TEXT("ADS head overlap stage=%d clearance=%.2f"),InputTestStage,Clearance)); return true; }
        const float GripError=FMath::Max(FVector::Dist(Self->GetMesh()->GetBoneLocation(TEXT("hand_l"),EBoneSpaces::ComponentSpace),Pose.LeftHand),FVector::Dist(Self->GetMesh()->GetBoneLocation(TEXT("hand_r"),EBoneSpaces::ComponentSpace),Pose.RightHand));
        if (GripError>1.f)
        { Report(false,FString::Printf(TEXT("ADS hand reach stage=%d error=%.2f"),InputTestStage,GripError)); return true; }
    }
    switch (InputTestStage)
    {
    case 0:
        if(Elapsed<1) break;
        FirePressed(); Next(); break;
    case 1:
        if(Elapsed<.3) break;
        if(!Self->IsFiring() || Self->GetAmmo()>=30) { Report(false,TEXT("standing fire failed")); break; }
        if(Sample(TEXT("Weapon-StandHip"),true)) { AimPressed(); Next(); } break;
    case 2:
        if(Elapsed<.3) break;
        if(Sample(TEXT("Weapon-StandAim"),true)) { CrouchPressed(); Next(); } break;
    case 3:
        if(Elapsed<.5) break;
        if(!Self->bIsCrouched) { Report(false,TEXT("crouch failed")); break; }
        if(Sample(TEXT("Weapon-CrouchAim"),true)) { AimReleased(); Next(); } break;
    case 4:
        if(Elapsed<.3) break;
        if(Sample(TEXT("Weapon-CrouchHip"),true)) { FireReleased(); AimPressed(); SetControlRotation(FRotator(60,0,0)); Next(); } break;
    case 5:
        if(Elapsed<.3) break;
        if(Sample(TEXT("Weapon-CrouchUp"),false)) { SetControlRotation(FRotator(-60,0,0)); Next(); } break;
    case 6:
        if(Elapsed<.3) break;
        if(Sample(TEXT("Weapon-CrouchDown"),false))
        {
            SetControlRotation(FRotator::ZeroRotator);
            auto* Wall=GetWorld()->SpawnActor<AActor>();
            auto* Box=NewObject<UBoxComponent>(Wall); Wall->SetRootComponent(Box);
            Box->SetBoxExtent(FVector(4,120,100)); Box->SetCollisionProfileName(TEXT("BlockAll")); Box->RegisterComponent();
            Wall->SetActorLocation(Self->GetActorLocation()+FVector(65,0,50)); InputTestCeiling=Wall;
            FirePressed(); Next();
        }
        break;
    case 7:
        if(Elapsed<.4) break;
        if(Self->GetVisualObstructionAlpha()<.3f) { Report(false,TEXT("cover did not fold rifle")); break; }
        if(Sample(TEXT("Weapon-CrouchCover"),false))
        {
            const auto Pose=Self->GetVisualWeaponPose();
            const FVector Tip=Self->GetMesh()->GetComponentTransform().TransformPosition(Pose.Gun.TransformPosition(FVector(0,57,14)));
            if(Tip.X>InputTestCeiling->GetActorLocation().X-7.f) { Report(false,TEXT("barrel remained inside cover")); break; }
            UE_LOG(LogTPSDuel,Display,TEXT("WEAPON_COVER tipX=%.2f wallNearX=%.2f"),Tip.X,InputTestCeiling->GetActorLocation().X-4.f);
            FireReleased(); InputTestCeiling->Destroy(); Next();
        }
        break;
    case 8:
        if(Elapsed<.8) break;
        if(Self->GetVisualObstructionAlpha()>.02f) { Report(false,TEXT("rifle did not recover after leaving cover")); break; }
        ReloadPressed(); Next(); break;
    case 9:
        if(Elapsed<.6) break;
        if(!Self->IsReloading()) { Report(false,TEXT("crouch reload failed")); break; }
        if(Sample(TEXT("Weapon-CrouchReload"),false)) Next(); break;
    case 10:
        if(Self->IsReloading()) break;
        if(Self->GetAmmo()!=30) { Report(false,TEXT("reload did not refill")); break; }
        FireReleased(); CrouchReleased(); AimPressed(); SetControlRotation(FRotator::ZeroRotator); Next(); break;
    case 11:
        if(Elapsed<.4) break;
        FirePressed(); Next(); break;
    case 12: case 13: case 14: case 15: case 16: case 17:
    {
        if(Elapsed<(InputTestStage==17 ? .5 : .25)) break;
        const auto* Montage=Self->GetMesh()->GetAnimInstance()->GetCurrentActiveMontage();
        const auto* Clip=Montage && Montage->SlotAnimTracks.Num() && Montage->SlotAnimTracks[0].AnimTrack.AnimSegments.Num() ? Montage->SlotAnimTracks[0].AnimTrack.AnimSegments[0].AnimReference : nullptr;
        if (!Self->IsFiring() || Self->GetAmmo()<=0 || !Clip || !Clip->GetName().Contains(TEXT("Fire_Rifle_Ironsights")))
        { Report(false,TEXT("ADS burst did not use ironsights fire clip")); break; }
        const TCHAR* Names[]={TEXT("ADS-Stand"),TEXT("ADS-Up30"),TEXT("ADS-Down30"),TEXT("ADS-Up60"),TEXT("ADS-Down60"),TEXT("ADS-Crouch")};
        UE_LOG(LogTPSDuel,Display,TEXT("ADS_CLEARANCE stage=%d minimum=%.2f samples=%d clip=%s"),InputTestStage,WeaponHeadMinimum,WeaponClearanceSamples,*Clip->GetName());
        if (!Sample(Names[InputTestStage-12],InputTestStage==12 || InputTestStage==17)) break;
        const float Pitches[]={30,-30,60,-60,0};
        if (InputTestStage<17) SetControlRotation(FRotator(Pitches[InputTestStage-12],0,0));
        if (InputTestStage==16) CrouchPressed();
        if (InputTestStage==17)
        {
            FireReleased(); AimReleased(); CrouchReleased(); Next();
        }
        else Next();
        break;
    }
    case 18:
        if (Elapsed<.3) break;
        ReloadPressed(); Next(); break;
    case 19:
        if (Self->IsReloading() || Self->GetAmmo()!=30) break;
        AimPressed(); FirePressed(); Next(); break;
    case 20:
        if (Elapsed<.4) break;
        if (!Sample(TEXT("ADS-Immediate"),true)) break;
        UE_LOG(LogTPSDuel,Display,TEXT("ADS_CLEARANCE immediate=1 minimum=%.2f samples=%d"),WeaponHeadMinimum,WeaponClearanceSamples);
        if (WeaponClearanceSamples<70) { Report(false,TEXT("insufficient ADS recoil samples")); break; }
        Report(true,FString::Printf(TEXT("stand/crouch fire; grips; cover; reload; ADS and immediate aim/fire head clearance %.2fcm across %d frames"),WeaponHeadMinimum,WeaponClearanceSamples)); break;
    }
    return true;
#else
    return false;
#endif
}

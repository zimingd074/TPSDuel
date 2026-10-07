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

bool ADuelPlayerController::TickWeaponTest()
{
#if !UE_BUILD_SHIPPING
    if (!FParse::Param(FCommandLine::Get(),TEXT("DuelWeaponTest"))) return false;
    auto* Self=Cast<ADuelCharacter>(GetPawn());
    if (!Self) return true;
    const double Now=FPlatformTime::Seconds();
    if (SmokeStart==0)
    {
        SmokeStart=InputStageStart=Now;
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
        Report(true,TEXT("stand/crouch fire; elbows below shoulders; shared grips; +/-60 aim; cover fold/recover; crouch reload")); break;
    }
    return true;
#else
    return false;
#endif
}

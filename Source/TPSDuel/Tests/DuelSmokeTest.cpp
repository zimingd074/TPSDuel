#include "DuelPlayerController.h"
#include "DuelCharacter.h"
#include "DuelGameInstance.h"
#include "DuelGameState.h"
#include "DuelPlayerState.h"
#include "TPSDuel.h"
#include "Engine/World.h"
#include "UnrealClient.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DuelWeaponPose.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimInstance.h"
#include "UObject/UnrealType.h"
#include "DuelAnimInstance.h"

void ADuelPlayerController::TickSmokeTest()
{
#if !UE_BUILD_SHIPPING
    if (TickInputTest()) return;
    // Exercise real movement and sample the resulting pose, not a teleported pose.
    if (FParse::Param(FCommandLine::Get(),TEXT("DuelMotionTest")))
    {
        auto* Runner=Cast<ADuelCharacter>(GetPawn());
        if (!Runner) return;
        const double Now=FPlatformTime::Seconds();
        if (SmokeStart==0)
        {
            SmokeStart=Now;
            Runner->SetActorLocationAndRotation(FVector(-1350,0,90),FRotator::ZeroRotator,false,nullptr,ETeleportType::TeleportPhysics);
            Runner->GetMesh()->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
            SetControlRotation(FRotator::ZeroRotator);
            auto* Camera=GetWorld()->SpawnActor<ACameraActor>();
            Camera->GetCameraComponent()->bConstrainAspectRatio=false;
            SetViewTarget(Camera);
        }
        const float Time=Now-SmokeStart;
        auto* Camera=Cast<ACameraActor>(GetViewTarget());
        if (Camera)
        {
            const FVector Focus=Runner->GetActorLocation()+FVector(0,0,10);
            const FVector Location=Focus+FVector(70,-300,55);
            Camera->SetActorLocationAndRotation(Location,(Focus-Location).Rotation());
        }
        // Forward / stop / backward / right / left / jump. Each stage uses input.
        if (Time>2.f && Time<3.2f) Runner->MoveForward(1.f);
        if (Time>3.8f && Time<4.8f) Runner->MoveForward(-1.f);
        if (Time>5.4f && Time<6.4f) Runner->MoveRight(1.f);
        if (Time>7.f && Time<8.f) Runner->MoveRight(-1.f);
        if (Time>8.6f && Time<9.f) Runner->Jump();
        else Runner->StopJumping();
        Runner->SetAiming(Time>10.f && Time<11.2f);
        if ((Time>10.f && Time<11.2f) || (Time>12.f && Time<14.f)) Runner->MoveForward(1.f);
        if (Time>13.3f && !MotionFireRequested)
        {
            InputKey(EKeys::LeftMouseButton,IE_Pressed,1.f,false); MotionFireRequested=true;
        }
        if (Time>14.f && Time<14.2f) InputKey(EKeys::LeftMouseButton,IE_Released,0.f,false);
        if (Time>14.4f && !MotionReloadRequested)
        {
            InputKey(EKeys::R,IE_Pressed,1.f,false); InputKey(EKeys::R,IE_Released,0.f,false); MotionReloadRequested=true;
        }
        const float Speed=Runner->GetVelocity().Size2D();
        MotionMaximumSpeed=FMath::Max(MotionMaximumSpeed,Speed);
        MotionJumpSeen |= Runner->GetCharacterMovement()->IsFalling() && Time>8.6f;
        const FVector Foot=Runner->GetMesh()->GetBoneLocation(TEXT("foot_l"),EBoneSpaces::ComponentSpace);
        if (Time>2.4f && Time<3.2f)
        {
            if (!MotionLastFoot.IsZero()) MotionFootTravel+=FVector::Dist(MotionLastFoot,Foot);
            MotionLastFoot=Foot;
        }
        const float SampleTimes[]={2.45f,2.52f,2.59f,2.66f,2.73f,2.80f,2.87f,2.94f,3.65f,4.4f,6.f,7.6f,9.f,10.8f,12.8f,13.8f,15.2f,16.8f};
        if (MotionSample<UE_ARRAY_COUNT(SampleTimes) && Time>=SampleTimes[MotionSample])
        {
            float AnimSpeed=-1.f;
            if (auto* Instance=Runner->GetMesh()->GetAnimInstance())
                if (auto* Property=FindFProperty<FFloatProperty>(Instance->GetClass(),TEXT("Speed"))) AnimSpeed=Property->GetPropertyValue_InContainer(Instance);
            const auto Pose=Runner->GetVisualWeaponPose();
            const float GripError=FVector::Dist(Runner->GetMesh()->GetBoneLocation(TEXT("hand_l"),EBoneSpaces::ComponentSpace),Pose.LeftHand);
            const auto* Instance=Cast<UDuelAnimInstance>(Runner->GetMesh()->GetAnimInstance());
            const FVector GunDirection=Pose.Gun.GetRotation().GetAxisY();
            UE_LOG(LogTPSDuel,Display,TEXT("MOTION_SAMPLE frame=%d speed=%.2f animSpeed=%.2f foot=%s gripError=%.2f carry=%.3f rate=%.1f gunDirection=%s ammo=%d reload=%.2f"),MotionSample,Speed,AnimSpeed,*Foot.ToString(),GripError,Runner->GetVisualCarryAlpha(),Instance ? Instance->DuelLocomotionRate : 0.f,*GunDirection.ToString(),Runner->GetAmmo(),Runner->GetReloadProgress());
            FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Screenshots"),FString::Printf(TEXT("Motion-%02d.png"),MotionSample)),true,false);
            ++MotionSample;
        }
        if (Time>18.f && SmokeExitAt==0)
        {
            const bool Passed=MotionFootTravel>50.f && MotionMaximumSpeed>350.f && MotionJumpSeen && MotionSample==UE_ARRAY_COUNT(SampleTimes) && MotionFireRequested && MotionReloadRequested && Runner->GetAmmo()==30 && !Runner->IsReloading();
            const FString Result=FString::Printf(TEXT("%s footTravel=%.2fcm maximumSpeed=%.2f jump=%d frames=%d"),Passed ? TEXT("PASS") : TEXT("FAIL"),MotionFootTravel,MotionMaximumSpeed,MotionJumpSeen,MotionSample);
            FFileHelper::SaveStringToFile(Result,*FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("MotionTest.txt")));
            UE_LOG(LogTPSDuel,Display,TEXT("MOTION_TEST %s"),*Result);
            SmokeExitAt=Now+1;
        }
        if (SmokeExitAt>0 && Now>=SmokeExitAt) FPlatformMisc::RequestExit(false);
        return;
    }
    // Explicit screenshot mode for visual QA, inert during ordinary play.
    FString Preview;
    if(FParse::Value(FCommandLine::Get(),TEXT("DuelPreview="),Preview))
    {
        const double PreviewNow=FPlatformTime::Seconds();
        if(SmokeStart==0)
        {
            SmokeStart=PreviewNow;
            if (GetPawn()) GetPawn()->SetActorLocationAndRotation(FVector(-700,-50,90),FRotator::ZeroRotator,false,nullptr,ETeleportType::TeleportPhysics);
        }
        if (auto* PreviewCharacter=Cast<ADuelCharacter>(GetPawn()))
        {
            if (FParse::Param(FCommandLine::Get(),TEXT("DuelPreviewAim"))) PreviewCharacter->SetAiming(true);
            float Pitch=0.f;
            const bool HasPitch=FParse::Value(FCommandLine::Get(),TEXT("DuelPreviewPitch="),Pitch);
            if (HasPitch || FParse::Param(FCommandLine::Get(),TEXT("DuelPreviewSide")))
            {
                SetControlRotation(FRotator(Pitch,0,0));
                PreviewCharacter->SetActorRotation(FRotator::ZeroRotator);
            }
        }
        if (!SmokePreviewCamera && GetPawn() && FParse::Param(FCommandLine::Get(),TEXT("DuelPreviewSide")))
        {
            auto* Camera=GetWorld()->SpawnActor<ACameraActor>();
            Camera->GetCameraComponent()->bConstrainAspectRatio=false;
            const FVector Focus=GetPawn()->GetActorLocation()+FVector(0,0,35);
            const FVector Location=Focus+GetPawn()->GetActorForwardVector()*170.f+GetPawn()->GetActorRightVector()*220.f+FVector(0,0,40);
            Camera->SetActorLocationAndRotation(Location,(Focus-Location).Rotation());
            SetViewTarget(Camera); SmokePreviewCamera=true;
        }
        if(SmokeExitAt>0)
        {
            if(PreviewNow>=SmokeExitAt) FPlatformMisc::RequestExit(false);
        }
        else if(PreviewNow-SmokeStart>8)
        {
            if (auto* PreviewCharacter=Cast<ADuelCharacter>(GetPawn()))
            {
                const auto Pose=PreviewCharacter->GetVisualWeaponPose();
                const FVector Left=PreviewCharacter->GetMesh()->GetBoneLocation(TEXT("hand_l"),EBoneSpaces::ComponentSpace);
                const FVector Right=PreviewCharacter->GetMesh()->GetBoneLocation(TEXT("hand_r"),EBoneSpaces::ComponentSpace);
                UE_LOG(LogTPSDuel,Display,TEXT("VISUAL_GRIP leftError=%.2f rightError=%.2f reload=%.2f"),FVector::Dist(Left,Pose.LeftHand),FVector::Dist(Right,Pose.RightHand),PreviewCharacter->GetReloadProgress());
            }
            FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Screenshots"),Preview+TEXT(".png")),true,false);
            SmokeExitAt=PreviewNow+2;
        }
        return;
    }
    FString TestRole;
    if (!FParse::Value(FCommandLine::Get(), TEXT("DuelSmoke="), TestRole) || (TestRole != TEXT("Host") && TestRole != TEXT("Client"))) return;
    const double Now = FPlatformTime::Seconds();
    if (SmokeStart == 0) SmokeStart = Now;
    if (SmokeExitAt > 0)
    {
        if (Now >= SmokeExitAt) FPlatformMisc::RequestExit(false);
        return;
    }
    auto Report = [this, &TestRole, Now](bool Passed, const FString& Detail)
    {
        FString Run;
        FParse::Value(FCommandLine::Get(), TEXT("DuelSmokeRun="), Run);
        if (!Run.IsNumeric() || Run.Len() > 24) Run = TEXT("default");
        const FString Folder = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Smoke"), Run);
        IFileManager::Get().MakeDirectory(*Folder, true);
        const FString Result = (Passed ? TEXT("PASS ") : TEXT("FAIL ")) + Detail;
        FFileHelper::SaveStringToFile(Result, *FPaths::Combine(Folder, TestRole + TEXT(".txt")));
        UE_LOG(LogTPSDuel, Display, TEXT("NET_SMOKE %s %s"), *TestRole, *Result);
        SmokeExitAt = Now + (TestRole == TEXT("Host") ? 4.0 : 2.0);
        FireReleased();
    };
    if (Now - SmokeStart > 100)
    {
        Report(false, TEXT("Timeout waiting for real two-process match completion"));
        return;
    }
    const ADuelGameState* State = GetWorld()->GetGameState<ADuelGameState>();
    if (!State)
    {
        if (IsFrontEnd()) if (const UDuelGameInstance* GI = GetGameInstance<UDuelGameInstance>())
            if (!GI->ConnectionStatus.IsEmpty()) Report(false, GI->ConnectionStatus);
        return;
    }
    for (TActorIterator<ADuelCharacter> It(GetWorld()); It; ++It)
        if (It->IsReloading())
        {
            const float Progress=It->GetReloadProgress();
            if (Progress>=0.f && Progress<=1.f && !SmokeReloadSeen)
            {
                SmokeReloadSeen=true;
                UE_LOG(LogTPSDuel,Display,TEXT("RELOAD_SYNC role=%s progress=%.3f"),*TestRole,Progress);
            }
        }
    int32 Blue = 0, Red = 0;
    for (const APlayerState* MatchPlayer : State->PlayerArray)
        if (const ADuelPlayerState* PS = Cast<ADuelPlayerState>(MatchPlayer))
        { if (PS->Slot == 0) Blue = PS->Kills; else if (PS->Slot == 1) Red = PS->Kills; }
    if (State->Phase == EDuelPhase::Finished && SmokeFinishSeen == 0) SmokeFinishSeen = Now;
    if (SmokeFinishSeen > 0)
    {
        FireReleased();
        if (Now - SmokeFinishSeen >= 1)
            Report(State->Phase == EDuelPhase::Finished && State->WinnerSlot == 0 && Blue == 3 && Red == 1 && SmokeReloadSeen,
                FString::Printf(TEXT("BLUE=%d RED=%d WINNER=%d"), Blue, Red, State->WinnerSlot));
        return;
    }
    if (State->Phase != EDuelPhase::Playing) return;
    // Explicit development-only test setup. Normal games never reposition players.
    if (HasAuthority())
    {
        for (TActorIterator<ADuelCharacter> It(GetWorld()); It; ++It)
        {
            const ADuelPlayerState* PS = It->GetPlayerState<ADuelPlayerState>();
            if (!PS || PS->Slot < 0 || PS->Slot > 1 || !It->IsAlive() || SmokePawns[PS->Slot].Get() == *It) continue;
            SmokePawns[PS->Slot] = *It;
            const float TestDistance = FParse::Param(FCommandLine::Get(), TEXT("DuelSmokeClose")) ? 60.f : 600.f;
            It->SetActorLocationAndRotation(FVector(PS->Slot == 0 ? -TestDistance : TestDistance, 0, 100), FRotator(0, PS->Slot == 0 ? 0 : 180, 0), false, nullptr, ETeleportType::TeleportPhysics);
            It->ForceNetUpdate();
        }
    }
    ADuelCharacter* Self = Cast<ADuelCharacter>(GetPawn());
    const ADuelPlayerState* MyState = GetPlayerState<ADuelPlayerState>();
    static double LastDiagnostic = 0;
    const bool LogDiagnostic = Now - LastDiagnostic > 5;
    if (LogDiagnostic)
    {
        LastDiagnostic = Now;
        UE_LOG(LogTPSDuel, Display, TEXT("SMOKE_STATE role=%s pawn=%s slot=%d alive=%d local=%d ammo=%d health=%.0f blue=%d red=%d players=%d combat=%d"), *TestRole, *GetNameSafe(Self), MyState ? MyState->Slot : -1, Self && Self->IsAlive(), Self && Self->IsLocallyControlled(), Self ? Self->GetAmmo() : -1, Self ? Self->GetHealth() : -1.f, Blue, Red, State->PlayerArray.Num(), Self && Self->CanCombat());
    }
    const int32 Shooter = Blue == 0 || Red >= 1 ? 0 : 1;
    if (!Self || !MyState || !Self->IsAlive() || MyState->Slot != Shooter) { FireReleased(); return; }
    if (!SmokeReloadRequested && MyState->Slot==0 && Self->GetAmmo()<30 && Blue==0 && Red==0)
    {
        FireReleased(); ReloadPressed(); SmokeReloadRequested=true;
        return;
    }
    if (Self->IsReloading()) { FireReleased(); return; }
    ADuelCharacter* Target = nullptr;
    for (TActorIterator<ADuelCharacter> It(GetWorld()); It; ++It)
    {
        const ADuelPlayerState* OpponentState = It->GetPlayerState<ADuelPlayerState>();
        if (*It != Self && It->IsAlive() && OpponentState && OpponentState->Slot != MyState->Slot) Target = *It;
    }
    if (LogDiagnostic) UE_LOG(LogTPSDuel, Display, TEXT("SMOKE_TARGET target=%s protected=%d menu=%d"), *GetNameSafe(Target), Target && Target->IsProtected(), IsMenuVisible());
    if (!Target || Target->IsProtected()) { FireReleased(); return; }
    FVector Eye;
    FRotator View;
    GetPlayerViewPoint(Eye, View);
    SetControlRotation((Target->GetActorLocation() + FVector(0, 0, -15) - Eye).Rotation());
    if (Self->GetAmmo() <= 0) ReloadPressed();
    else FirePressed();
#endif
}

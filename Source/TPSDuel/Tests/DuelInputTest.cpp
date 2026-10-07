#include "DuelPlayerController.h"
#include "DuelCharacter.h"
#include "DuelGameState.h"
#include "TPSDuel.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWindow.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/App.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "DuelAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "UnrealClient.h"

bool ADuelPlayerController::TickInputTest()
{
#if !UE_BUILD_SHIPPING
    if (!FParse::Param(FCommandLine::Get(),TEXT("DuelInputTest"))) return false;
    auto* Self=Cast<ADuelCharacter>(GetPawn());
    auto* State=GetWorld()->GetGameState<ADuelGameState>();
    if (!Self || !State) return true;
    const double Now=FPlatformTime::Seconds();
    if (SmokeStart==0) { SmokeStart=Now; InputStageStart=Now; }
    auto Report=[this,Now](bool Passed,const FString& Detail)
    {
        const FString Result=(Passed ? TEXT("PASS ") : TEXT("FAIL "))+Detail;
        FFileHelper::SaveStringToFile(Result,*FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("InputTest.txt")));
        UE_LOG(LogTPSDuel,Display,TEXT("INPUT_TEST %s"),*Result);
        SmokeExitAt=Now+1; ClearLocalInput();
        if (InputTestCeiling.IsValid()) InputTestCeiling->Destroy();
        FApp::SetUseFixedTimeStep(false);
    };
    if (SmokeExitAt>0) { if (Now>=SmokeExitAt) FPlatformMisc::RequestExit(false); return true; }
    if (Now-SmokeStart>35) { Report(false,FString::Printf(TEXT("stage=%d timed out"),InputTestStage)); return true; }
    auto Next=[this,Now]() { ++InputTestStage; InputStageStart=Now; };
    auto Key=[this](FKey Value) { InputKey(Value,IE_Pressed,1.f,false); InputKey(Value,IE_Released,0.f,false); };
    auto Mouse=[this](bool Pressed)
    {
        auto* Viewport=GetWorld()->GetGameViewport();
        if (!Viewport || !FSlateApplication::IsInitialized()) return false;
        const auto Widget=Viewport->GetGameViewportWidget();
        const auto Window=Viewport->GetWindow();
        if (!Widget.IsValid() || !Window.IsValid()) return false;
        const FGeometry& Geometry=Widget->GetCachedGeometry();
        const FVector2D Size=Geometry.GetLocalSize();
        const FVector2D Position=Geometry.LocalToAbsolute(FVector2D(Size.X*(IsMenuVisible() ? .1f : .5f),Size.Y*.5f));
        TSet<FKey> Buttons; if (Pressed) Buttons.Add(EKeys::LeftMouseButton);
        const FPointerEvent Event(0,Position,Position,Buttons,EKeys::LeftMouseButton,0.f,FModifierKeysState());
        if (Pressed) FSlateApplication::Get().ProcessMouseButtonDownEvent(Window->GetNativeWindow(),Event);
        else FSlateApplication::Get().ProcessMouseButtonUpEvent(Event);
        return true;
    };
    const double Elapsed=Now-InputStageStart;
    switch (InputTestStage)
    {
    case 0:
        if (Elapsed<2) break;
        if (State->Phase!=EDuelPhase::Waiting || !Self->CanUseWeapon()) { Report(false,TEXT("waiting practice disabled")); break; }
        InputTestAmmo=Self->GetAmmo();
        // Release pointer capture to reproduce the first click after focus/capture.
        FSlateApplication::Get().ReleaseAllPointerCapture();
        if (!Mouse(true)) { Report(false,TEXT("no Slate game viewport")); break; }
        Next(); break;
    case 1:
        if (Elapsed<.3) break;
        Mouse(false);
        if (Self->GetAmmo()>=InputTestAmmo) { Report(false,TEXT("first left mouse click did not fire")); break; }
        UE_LOG(LogTPSDuel,Display,TEXT("INPUT_CHECK firstClick fired ammo=%d"),Self->GetAmmo());
        Next(); break;
    case 2:
        if (Elapsed<.2) break;
        InputTestAmmo=Self->GetAmmo(); Next(); break;
    case 3:
        if (Elapsed<.3) break;
        if (Self->GetAmmo()!=InputTestAmmo || Self->IsFiring()) { Report(false,TEXT("mouse release did not stop firing")); break; }
        Key(EKeys::Escape); Next(); break;
    case 4:
        if (Elapsed<.2) break;
        if (!IsMenuVisible()) { Report(false,TEXT("ESC binding did not open menu")); break; }
        Mouse(true); Next(); break;
    case 5:
        if (Elapsed<.2) break;
        Mouse(false);
        if (Self->GetAmmo()!=InputTestAmmo) { Report(false,TEXT("menu click fired weapon")); break; }
        Key(EKeys::Escape); Next(); break;
    case 6:
        if (Elapsed<.2) break;
        if (IsMenuVisible()) { Report(false,TEXT("ESC binding did not resume game")); break; }
        Mouse(true); Next(); break;
    case 7:
        if (Elapsed<.25) break;
        Mouse(false);
        if (Self->GetAmmo()>=InputTestAmmo) { Report(false,TEXT("first click after menu failed")); break; }
        UE_LOG(LogTPSDuel,Display,TEXT("INPUT_CHECK release/menu/resume passed"));
        Next(); break;
    case 8:
        if (Elapsed<.2) break;
        Key(EKeys::R); Next(); break;
    case 9:
        InputReloadSeen |= Self->IsReloading();
        if (Elapsed<2.3) break;
        if (!InputReloadSeen || Self->IsReloading() || Self->GetAmmo()!=30) { Report(false,TEXT("R binding failed to reload in waiting area")); break; }
        // Verify Waiting does not allow damage even after spawn protection expires.
        Self->TakeDamage(25.f,FDamageEvent(),nullptr,nullptr);
        if (Self->GetHealth()!=100.f) { Report(false,TEXT("waiting practice allowed damage")); break; }
        InputTestAmmo=Self->GetAmmo(); State->Phase=EDuelPhase::Countdown;
        InputKey(EKeys::LeftMouseButton,IE_Pressed,1.f,false); Next(); break;
    case 10:
        if (Elapsed<.3) break;
        InputKey(EKeys::LeftMouseButton,IE_Released,0.f,false);
        if (Self->GetAmmo()!=InputTestAmmo) { Report(false,TEXT("countdown allowed shooting")); break; }
        State->Phase=EDuelPhase::Waiting;
        Next(); break;
    case 11:
    case 13:
        FApp::SetFixedDeltaTime(InputTestStage==11 ? 1.0/30.0 : 1.0/60.0);
        FApp::SetUseFixedTimeStep(true);
        InputTestAmmo=Self->GetAmmo(); InputCadenceStartedAt=GetWorld()->TimeSeconds; InputCadenceFrames=0;
        InputKey(EKeys::LeftMouseButton,IE_Pressed,1.f,false);
        Next(); break;
    case 12:
    case 14:
    {
        if (InputTestStage==14)
        {
            const double Steps[]={1.0/120.0,1.0/30.0,1.0/60.0};
            FApp::SetFixedDeltaTime(Steps[InputCadenceFrames++%3]);
        }
        const float Duration=GetWorld()->TimeSeconds-InputCadenceStartedAt;
        if (Duration<1.f) break;
        InputKey(EKeys::LeftMouseButton,IE_Released,0.f,false);
        const int32 Shots=InputTestAmmo-Self->GetAmmo();
        UE_LOG(LogTPSDuel,Display,TEXT("CADENCE_CHECK mode=%s shots=%d elapsed=%.3f"),InputTestStage==12 ? TEXT("30Hz") : TEXT("variable"),Shots,Duration);
        if (Shots<12 || Shots>15) { Report(false,TEXT("automatic fire dropped timer shots or bypassed cooldown")); break; }
        if (InputTestStage==14) FApp::SetUseFixedTimeStep(false);
        Next();
        break;
    }
    case 15:
        Self->GetCharacterMovement()->StopMovementImmediately();
        Self->SetActorLocation(FVector(-1350,0,90),false,nullptr,ETeleportType::TeleportPhysics);
        InputKey(EKeys::LeftControl,IE_Pressed,1.f,false);
        InputKey(EKeys::LeftShift,IE_Pressed,1.f,false);
        InputKey(EKeys::W,IE_Pressed,1.f,false);
        Next(); break;
    case 16:
    {
        if (Elapsed<.6) break;
        const auto* Anim=Cast<UDuelAnimInstance>(Self->GetMesh()->GetAnimInstance());
        if (!Self->bIsCrouched || !Self->IsSlowWalking() || !Anim || !Anim->DuelCrouched ||
            FMath::Abs(Self->GetVelocity().Size2D()-80.f)>5.f || FMath::Abs(Self->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()-58.f)>1.f)
        { Report(false,TEXT("Ctrl+Shift crouch walk or reduced capsule failed")); break; }
        const float Grip=FVector::Dist(Self->GetMesh()->GetBoneLocation(TEXT("hand_l"),EBoneSpaces::ComponentSpace),Self->GetVisualWeaponPose().LeftHand);
        if (Grip>1.f) { Report(false,TEXT("crouched hand lost rifle grip")); break; }
        UE_LOG(LogTPSDuel,Display,TEXT("STANCE_CHECK crouchQuiet speed=%.2f capsule=%.1f grip=%.2f"),Self->GetVelocity().Size2D(),Self->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight(),Grip);
        FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Screenshots/Stance-CrouchWalk.png")),true,false);
        InputKey(EKeys::LeftShift,IE_Released,0.f,false); Next(); break;
    }
    case 17:
        if (Elapsed<.4) break;
        if (!Self->bIsCrouched || Self->IsSlowWalking() || FMath::Abs(Self->GetVelocity().Size2D()-120.f)>5.f) { Report(false,TEXT("Shift release did not restore crouch speed")); break; }
        UE_LOG(LogTPSDuel,Display,TEXT("STANCE_CHECK crouch speed=%.2f"),Self->GetVelocity().Size2D());
        InputKey(EKeys::LeftControl,IE_Released,0.f,false); InputKey(EKeys::LeftShift,IE_Pressed,1.f,false); Next(); break;
    case 18:
        if (Elapsed<.4) break;
        if (Self->bIsCrouched || !Self->IsSlowWalking() || FMath::Abs(Self->GetVelocity().Size2D()-150.f)>5.f) { Report(false,TEXT("standing quiet walk failed")); break; }
        UE_LOG(LogTPSDuel,Display,TEXT("STANCE_CHECK quiet speed=%.2f"),Self->GetVelocity().Size2D());
        InputKey(EKeys::LeftShift,IE_Released,0.f,false); Next(); break;
    case 19:
        if (Elapsed<.4) break;
        if (FMath::Abs(Self->GetVelocity().Size2D()-375.f)>5.f) { Report(false,TEXT("Shift release did not restore running")); break; }
        UE_LOG(LogTPSDuel,Display,TEXT("STANCE_CHECK run speed=%.2f"),Self->GetVelocity().Size2D());
        InputKey(EKeys::W,IE_Released,0.f,false);
        InputKey(EKeys::LeftControl,IE_Pressed,1.f,false); InputKey(EKeys::LeftShift,IE_Pressed,1.f,false); Key(EKeys::Escape); Next(); break;
    case 20:
        if (Elapsed<.3) break;
        if (!IsMenuVisible() || Self->IsSlowWalking() || Self->bIsCrouched) { Report(false,TEXT("menu did not clear stance input")); break; }
        InputKey(EKeys::LeftControl,IE_Released,0.f,false); InputKey(EKeys::LeftShift,IE_Released,0.f,false); Key(EKeys::Escape);
        Self->GetCharacterMovement()->StopMovementImmediately();
        Self->SetActorLocation(FVector(-1350,0,90),false,nullptr,ETeleportType::TeleportPhysics);
        InputKey(EKeys::RightControl,IE_Pressed,1.f,false); Next(); break;
    case 21:
    {
        if (Elapsed<.3) break;
        if (!Self->bIsCrouched) { Report(false,TEXT("right Ctrl mapping failed")); break; }
        auto* Ceiling=GetWorld()->SpawnActor<AActor>();
        auto* Box=NewObject<UBoxComponent>(Ceiling);
        Ceiling->SetRootComponent(Box); Box->SetBoxExtent(FVector(150,150,8));
        Box->SetCollisionProfileName(TEXT("BlockAll")); Box->RegisterComponent();
        Ceiling->SetActorLocation(Self->GetActorLocation()+FVector(0,0,77));
        InputTestCeiling=Ceiling;
        InputKey(EKeys::RightControl,IE_Released,0.f,false); Next(); break;
    }
    case 22:
        if (Elapsed<.3) break;
        if (!Self->bIsCrouched) { Report(false,TEXT("stood through low ceiling")); break; }
        InputTestCeiling->Destroy(); Next(); break;
    case 23:
        if (Elapsed<.3) break;
        if (Self->bIsCrouched || FMath::Abs(Self->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()-88.f)>1.f) { Report(false,TEXT("did not stand after ceiling cleared")); break; }
        UE_LOG(LogTPSDuel,Display,TEXT("STANCE_CHECK menuClear=1 ceilingBlocked=1 ceilingRelease=1"));
        Report(true,TEXT("Slate mouse/menu/reload/cadence; Ctrl/Shift speeds 80/120/150/375; crouch grip/capsule; menu clear; low-ceiling stand protection"));
        break;
    }
    return true;
#else
    return false;
#endif
}

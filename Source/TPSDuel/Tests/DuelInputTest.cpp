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
        SmokeExitAt=Now+1; FireReleased();
    };
    if (SmokeExitAt>0) { if (Now>=SmokeExitAt) FPlatformMisc::RequestExit(false); return true; }
    if (Now-SmokeStart>20) { Report(false,FString::Printf(TEXT("stage=%d timed out"),InputTestStage)); return true; }
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
        Report(true,TEXT("Slate firstClick/release/menu/resume; R reload; waiting no damage; countdown blocked")); break;
    }
    return true;
#else
    return false;
#endif
}

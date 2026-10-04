#include "DuelPlayerController.h"
#include "DuelCharacter.h"
#include "DuelGameInstance.h"
#include "DuelGameState.h"
#include "DuelPlayerState.h"
#include "TPSDuel.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

void ADuelPlayerController::TickSmokeTest()
{
#if !UE_BUILD_SHIPPING
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
    int32 Blue = 0, Red = 0;
    for (const APlayerState* MatchPlayer : State->PlayerArray)
        if (const ADuelPlayerState* PS = Cast<ADuelPlayerState>(MatchPlayer))
        { if (PS->Slot == 0) Blue = PS->Kills; else if (PS->Slot == 1) Red = PS->Kills; }
    if (State->Phase == EDuelPhase::Finished && SmokeFinishSeen == 0) SmokeFinishSeen = Now;
    if (SmokeFinishSeen > 0)
    {
        FireReleased();
        if (Now - SmokeFinishSeen >= 1)
            Report(State->Phase == EDuelPhase::Finished && State->WinnerSlot == 0 && Blue == 3 && Red == 1,
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

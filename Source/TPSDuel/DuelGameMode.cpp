#include "DuelGameMode.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "DuelArena.h"
#include "DuelCharacter.h"
#include "DuelGameState.h"
#include "DuelPlayerController.h"
#include "DuelPlayerState.h"
#include "DuelSettings.h"
#include "TPSDuel.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

ADuelGameMode::ADuelGameMode()
{
    DefaultPawnClass = ADuelCharacter::StaticClass();
    PlayerControllerClass = ADuelPlayerController::StaticClass();
    PlayerStateClass = ADuelPlayerState::StaticClass();
    GameStateClass = ADuelGameState::StaticClass();
}
ADuelMenuGameMode::ADuelMenuGameMode()
{
    DefaultPawnClass = nullptr;
    PlayerControllerClass = ADuelPlayerController::StaticClass();
    PlayerStateClass = ADuelPlayerState::StaticClass();
}
void ADuelGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
    Super::InitGame(MapName, Options, ErrorMessage);
    GetWorld()->SpawnActor<ADuelArena>();
    for (int32 Slot = 0; Slot < 2; ++Slot)
    {
        const FVector Position = Slot == 0 ? FVector(-1400, -700, 100) : FVector(1400, 700, 100);
        const FRotator Rotation(0, Slot == 0 ? 27.f : 207.f, 0);
        Starts.Add(GetWorld()->SpawnActor<APlayerStart>(Position, Rotation));
    }
    UE_LOG(LogTPSDuel, Log, TEXT("Arena initialized. Protocol=1 MaxPlayers=2"));
}
int32 ADuelGameMode::FindFreeSlot() const
{
    for (int32 Slot = 0; Slot < 2; ++Slot)
    {
        bool Used = false;
        for (const ADuelPlayerController* Player : Players)
            if (IsValid(Player))
                if (const ADuelPlayerState* PS = Player->GetPlayerState<ADuelPlayerState>()) Used |= PS->Slot == Slot;
        if (!Used) return Slot;
    }
    return -1;
}
void ADuelGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
    Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
    if (!ErrorMessage.IsEmpty()) return;
    if (FindFreeSlot() < 0) ErrorMessage = TEXT("Server full (2 players).");
    else if (UGameplayStatics::ParseOption(Options, TEXT("duel")) != TEXT("1") && GetWorld()->WorldType != EWorldType::PIE)
        ErrorMessage = TEXT("Incompatible prototype protocol. Join using the menu or ?duel=1.");
}
void ADuelGameMode::PostLogin(APlayerController* NewPlayer)
{
    ADuelPlayerController* PC = Cast<ADuelPlayerController>(NewPlayer);
    ADuelPlayerState* PS = NewPlayer->GetPlayerState<ADuelPlayerState>();
    const int32 Slot = FindFreeSlot();
    // Re-check after login to close simultaneous admission races.
    if (!PC || !PS || Slot < 0)
    {
        Super::PostLogin(NewPlayer);
        NewPlayer->ClientReturnToMainMenuWithTextReason(FText::FromString(TEXT("Server full (2 players).")));
        NewPlayer->Destroy();
        return;
    }
    PS->Slot = Slot;
    PS->SetPlayerName(Slot == 0 ? TEXT("BLUE") : TEXT("RED"));
    Players.Add(PC);
    Super::PostLogin(NewPlayer);
    UE_LOG(LogTPSDuel, Log, TEXT("Player joined slot=%d"), Slot);
    if (ADuelGameState* State = GetGameState<ADuelGameState>())
    {
        if (State->Phase == EDuelPhase::Aborted) State->Phase = EDuelPhase::Waiting;
        State->Status = TEXT("Waiting for both players to load");
        State->ForceNetUpdate();
    }
}
void ADuelGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
    if (Players.Contains(Cast<ADuelPlayerController>(NewPlayer))) Super::HandleStartingNewPlayer_Implementation(NewPlayer);
}
AActor* ADuelGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
    const ADuelPlayerState* PS = Player ? Player->GetPlayerState<ADuelPlayerState>() : nullptr;
    if (PS && Starts.IsValidIndex(PS->Slot)) return Starts[PS->Slot];
    return Super::ChoosePlayerStart_Implementation(Player);
}
void ADuelGameMode::PlayerLoaded(ADuelPlayerController* Player)
{
    if (!Players.Contains(Player)) return;
    if (ADuelPlayerState* PS = Player->GetPlayerState<ADuelPlayerState>()) PS->bLoaded = true;
    TryStart();
}
void ADuelGameMode::TryStart()
{
    ADuelGameState* State = GetGameState<ADuelGameState>();
    if (!State || Players.Num() != 2 || State->Phase != EDuelPhase::Waiting) return;
    for (const ADuelPlayerController* Player : Players)
    {
        const ADuelPlayerState* PS = Player->GetPlayerState<ADuelPlayerState>();
        if (!PS || !PS->bLoaded || !Player->GetPawn()) return;
    }
    StopRound();
    for (ADuelPlayerController* Player : Players)
    {
        ADuelPlayerState* PS = Player->GetPlayerState<ADuelPlayerState>();
        PS->Kills = 0;
        PS->bReady = false;
        PS->ForceNetUpdate();
        if (APawn* Old = Player->GetPawn()) { Player->UnPossess(); Old->Destroy(); }
        RestartPlayer(Player);
    }
    State->WinnerSlot = -1;
    State->Phase = EDuelPhase::Countdown;
    const float Duration = FMath::Max(.1f, GetDefault<UDuelSettings>()->CountdownSeconds);
    State->PhaseEndTime = GetWorld()->TimeSeconds + Duration;
    State->Status = TEXT("Get ready");
    State->ForceNetUpdate();
    GetWorldTimerManager().SetTimer(CountdownTimer, this, &ADuelGameMode::StartPlaying, Duration, false);
}
void ADuelGameMode::StartPlaying()
{
    if (ADuelGameState* State = GetGameState<ADuelGameState>())
    {
        if (Players.Num() != 2 || State->Phase != EDuelPhase::Countdown) return;
        State->Phase = EDuelPhase::Playing;
        State->Status = TEXT("First to 3 kills");
        State->ForceNetUpdate();
        UE_LOG(LogTPSDuel, Log, TEXT("Round started"));
    }
}
void ADuelGameMode::PlayerKilled(ADuelCharacter* Victim, AController* Killer)
{
    ADuelGameState* State = GetGameState<ADuelGameState>();
    if (!State || State->Phase != EDuelPhase::Playing || !Victim) return;
    ADuelPlayerController* DeadPlayer = Cast<ADuelPlayerController>(Victim->GetController());
    ADuelPlayerState* KillerPS = Killer ? Killer->GetPlayerState<ADuelPlayerState>() : nullptr;
    if (KillerPS && Killer != Victim->GetController())
    {
        ++KillerPS->Kills;
        KillerPS->ForceNetUpdate();
        UE_LOG(LogTPSDuel, Log, TEXT("Kill slot=%d score=%d"), KillerPS->Slot, KillerPS->Kills);
        if (KillerPS->Kills >= GetDefault<UDuelSettings>()->WinningScore)
        {
            State->WinnerSlot = KillerPS->Slot;
            State->Phase = EDuelPhase::Finished;
            State->Status = TEXT("Both players press READY for rematch");
            State->ForceNetUpdate();
            StopRound();
            return;
        }
    }
    if (DeadPlayer && Players.Contains(DeadPlayer) && !RespawnTimers.Contains(DeadPlayer))
    {
        FTimerHandle Handle;
        const TWeakObjectPtr<ADuelPlayerController> WeakPlayer(DeadPlayer);
        FTimerDelegate Callback = FTimerDelegate::CreateUObject(this, &ADuelGameMode::Respawn, WeakPlayer);
        GetWorldTimerManager().SetTimer(Handle, Callback, FMath::Max(.1f, GetDefault<UDuelSettings>()->RespawnSeconds), false);
        RespawnTimers.Add(WeakPlayer, Handle);
    }
}
void ADuelGameMode::Respawn(TWeakObjectPtr<ADuelPlayerController> Player)
{
    RespawnTimers.Remove(Player);
    const ADuelGameState* State = GetGameState<ADuelGameState>();
    if (!Player.IsValid() || !State || State->Phase != EDuelPhase::Playing || !Players.Contains(Player.Get())) return;
    if (APawn* Old = Player->GetPawn()) { Player->UnPossess(); Old->Destroy(); }
    RestartPlayer(Player.Get());
}
void ADuelGameMode::StopRound()
{
    GetWorldTimerManager().ClearTimer(CountdownTimer);
    for (auto& Entry : RespawnTimers) GetWorldTimerManager().ClearTimer(Entry.Value);
    RespawnTimers.Empty();
    for (ADuelPlayerController* Player : Players)
        if (IsValid(Player))
            if (ADuelCharacter* Character = Cast<ADuelCharacter>(Player->GetPawn())) Character->StopCombat();
}
void ADuelGameMode::PlayerReady(ADuelPlayerController* Player)
{
    ADuelGameState* State = GetGameState<ADuelGameState>();
    if (!State || State->Phase != EDuelPhase::Finished || !Players.Contains(Player)) return;
    if (ADuelPlayerState* PS = Player->GetPlayerState<ADuelPlayerState>()) { PS->bReady = true; PS->ForceNetUpdate(); }
    if (Players.Num() != 2) return;
    for (const ADuelPlayerController* PC : Players)
        if (!PC->GetPlayerState<ADuelPlayerState>() || !PC->GetPlayerState<ADuelPlayerState>()->bReady) return;
    State->Phase = EDuelPhase::Waiting;
    TryStart();
}
void ADuelGameMode::Logout(AController* Exiting)
{
    if (ADuelPlayerController* PC = Cast<ADuelPlayerController>(Exiting))
    {
        if (Players.Remove(PC) > 0)
        {
            StopRound();
            if (ADuelGameState* State = GetGameState<ADuelGameState>())
            {
                State->Phase = EDuelPhase::Aborted;
                State->WinnerSlot = -1;
                State->Status = TEXT("Opponent disconnected. Leave and host again.");
                State->ForceNetUpdate();
            }
            UE_LOG(LogTPSDuel, Log, TEXT("Player disconnected; round aborted"));
        }
    }
    Super::Logout(Exiting);
}
void ADuelGameMode::EndPlay(const EEndPlayReason::Type Reason)
{
    StopRound();
    Super::EndPlay(Reason);
}
void ADuelMenuGameMode::BeginPlay()
{
    Super::BeginPlay();
    GetWorld()->SpawnActor<ADuelArena>();
    const FVector Eye(-1380,-930,330);
    ACameraActor* Camera=GetWorld()->SpawnActor<ACameraActor>(Eye,(FVector(250,100,190)-Eye).Rotation());
    if(Camera)
    {
        Camera->GetCameraComponent()->SetFieldOfView(85);
        if(APlayerController* PC=GetWorld()->GetFirstPlayerController()) PC->SetViewTarget(Camera);
    }
}

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DuelGameMode.generated.h"
class ADuelCharacter;
class ADuelPlayerController;
class APlayerStart;

UCLASS()
class TPSDUEL_API ADuelGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    ADuelGameMode();
    virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
    virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
    virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
    virtual void Logout(AController* Exiting) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    void PlayerLoaded(ADuelPlayerController* Player);
    void PlayerReady(ADuelPlayerController* Player);
    void PlayerKilled(ADuelCharacter* Victim, AController* Killer);
private:
    void TryStart();
    void StartPlaying();
    void Respawn(TWeakObjectPtr<ADuelPlayerController> Player);
    void StopRound();
    int32 FindFreeSlot() const;
    UPROPERTY() TArray<ADuelPlayerController*> Players;
    UPROPERTY() TArray<APlayerStart*> Starts;
    FTimerHandle CountdownTimer;
    TMap<TWeakObjectPtr<ADuelPlayerController>, FTimerHandle> RespawnTimers;
};

UCLASS()
class TPSDUEL_API ADuelMenuGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    ADuelMenuGameMode();
};

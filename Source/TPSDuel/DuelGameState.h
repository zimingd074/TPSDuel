#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "DuelGameState.generated.h"

UENUM()
enum class EDuelPhase : uint8 { Waiting, Countdown, Playing, Finished, Aborted };

UCLASS()
class TPSDUEL_API ADuelGameState : public AGameStateBase
{
    GENERATED_BODY()
public:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    UPROPERTY(Replicated) EDuelPhase Phase = EDuelPhase::Waiting;
    UPROPERTY(Replicated) int32 WinnerSlot = -1;
    UPROPERTY(Replicated) float PhaseEndTime = 0.f;
    UPROPERTY(Replicated) FString Status;
    int32 SecondsLeft() const;
};

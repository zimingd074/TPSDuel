#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "DuelPlayerState.generated.h"

UCLASS()
class TPSDUEL_API ADuelPlayerState : public APlayerState
{
    GENERATED_BODY()
public:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    UPROPERTY(Replicated) int32 Slot = -1;
    UPROPERTY(Replicated) int32 Kills = 0;
    UPROPERTY(Replicated) bool bReady = false;
    UPROPERTY(Replicated) bool bLoaded = false;
};

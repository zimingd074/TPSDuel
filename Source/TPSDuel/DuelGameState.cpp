#include "DuelGameState.h"
#include "Net/UnrealNetwork.h"
void ADuelGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ADuelGameState, Phase);
    DOREPLIFETIME(ADuelGameState, WinnerSlot);
    DOREPLIFETIME(ADuelGameState, PhaseEndTime);
    DOREPLIFETIME(ADuelGameState, Status);
}
int32 ADuelGameState::SecondsLeft() const
{
    return FMath::Max(0, FMath::CeilToInt(PhaseEndTime - GetServerWorldTimeSeconds()));
}

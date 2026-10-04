#include "DuelPlayerState.h"
#include "Net/UnrealNetwork.h"
void ADuelPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ADuelPlayerState, Slot);
    DOREPLIFETIME(ADuelPlayerState, Kills);
    DOREPLIFETIME(ADuelPlayerState, bReady);
    DOREPLIFETIME(ADuelPlayerState, bLoaded);
}

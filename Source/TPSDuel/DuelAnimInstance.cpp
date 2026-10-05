#include "DuelAnimInstance.h"
#include "GameFramework/Pawn.h"

void UDuelAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);
    if (const APawn* Pawn=TryGetPawnOwner())
    {
        // Reverse the foot cycle when moving backward; jumping keeps its own
        // sequence players and therefore is not reversed by this value.
        const FVector Local=Pawn->GetActorRotation().UnrotateVector(Pawn->GetVelocity());
        DuelLocomotionRate=Local.X < -20.f ? -1.f : 1.f;
    }
}

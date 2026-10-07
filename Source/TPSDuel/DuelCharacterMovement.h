#pragma once
#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "DuelCharacterMovement.generated.h"

UCLASS()
class TPSDUEL_API UDuelCharacterMovement : public UCharacterMovementComponent
{
    GENERATED_BODY()
public:
    bool bWantsSlowWalk=false;
    bool bWantsCombatFacing=false;
    virtual float GetMaxSpeed() const override;
    virtual void PhysicsRotation(float DeltaTime) override;
    virtual FRotator ComputeOrientToMovementRotation(const FRotator& CurrentRotation,float DeltaTime,FRotator& DeltaRotation) const override;
    virtual void UpdateFromCompressedFlags(uint8 Flags) override;
    virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;
};

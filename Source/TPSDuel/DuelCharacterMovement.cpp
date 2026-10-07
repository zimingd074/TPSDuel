#include "DuelCharacterMovement.h"
#include "DuelCharacter.h"

namespace
{
class FSavedMove_Duel : public FSavedMove_Character
{
public:
    using Super=FSavedMove_Character;
    bool bSavedSlowWalk=false;
    virtual void Clear() override { Super::Clear(); bSavedSlowWalk=false; }
    virtual uint8 GetCompressedFlags() const override
    {
        return Super::GetCompressedFlags() | (bSavedSlowWalk ? FLAG_Custom_0 : 0);
    }
    virtual bool CanCombineWith(const FSavedMovePtr& Move,ACharacter* Character,float MaxDelta) const override
    {
        if (bSavedSlowWalk!=static_cast<const FSavedMove_Duel*>(Move.Get())->bSavedSlowWalk) return false;
        return Super::CanCombineWith(Move,Character,MaxDelta);
    }
    virtual void SetMoveFor(ACharacter* Character,float Delta,FVector const& NewAcceleration,FNetworkPredictionData_Client_Character& Data) override
    {
        Super::SetMoveFor(Character,Delta,NewAcceleration,Data);
        bSavedSlowWalk=CastChecked<UDuelCharacterMovement>(Character->GetCharacterMovement())->bWantsSlowWalk;
    }
    virtual void PrepMoveFor(ACharacter* Character) override
    {
        Super::PrepMoveFor(Character);
        CastChecked<UDuelCharacterMovement>(Character->GetCharacterMovement())->bWantsSlowWalk=bSavedSlowWalk;
    }
};
class FPredictionData_Duel : public FNetworkPredictionData_Client_Character
{
public:
    explicit FPredictionData_Duel(const UCharacterMovementComponent& Movement) : FNetworkPredictionData_Client_Character(Movement) {}
    virtual FSavedMovePtr AllocateNewMove() override { return FSavedMovePtr(new FSavedMove_Duel()); }
};
}
float UDuelCharacterMovement::GetMaxSpeed() const
{
    const float Speed=Super::GetMaxSpeed();
    return bWantsSlowWalk && IsMovingOnGround() ? FMath::Min(Speed,IsCrouching() ? 80.f : 150.f) : Speed;
}
void UDuelCharacterMovement::UpdateFromCompressedFlags(uint8 Flags)
{
    Super::UpdateFromCompressedFlags(Flags);
    bWantsSlowWalk=(Flags & FSavedMove_Character::FLAG_Custom_0)!=0;
    if (auto* Character=Cast<ADuelCharacter>(CharacterOwner)) Character->SyncSlowWalking(bWantsSlowWalk);
}
FNetworkPredictionData_Client* UDuelCharacterMovement::GetPredictionData_Client() const
{
    if (!ClientPredictionData) const_cast<UDuelCharacterMovement*>(this)->ClientPredictionData=new FPredictionData_Duel(*this);
    return ClientPredictionData;
}

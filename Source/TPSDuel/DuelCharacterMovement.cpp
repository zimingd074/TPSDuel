#include "DuelCharacterMovement.h"
#include "DuelCharacter.h"

namespace
{
class FSavedMove_Duel : public FSavedMove_Character
{
public:
    using Super=FSavedMove_Character;
    bool bSavedSlowWalk=false;
    bool bSavedCombatFacing=false;
    virtual void Clear() override { Super::Clear(); bSavedSlowWalk=false; bSavedCombatFacing=false; }
    virtual uint8 GetCompressedFlags() const override
    {
        return Super::GetCompressedFlags() | (bSavedSlowWalk ? FLAG_Custom_0 : 0) | (bSavedCombatFacing ? FLAG_Custom_1 : 0);
    }
    virtual bool CanCombineWith(const FSavedMovePtr& Move,ACharacter* Character,float MaxDelta) const override
    {
        const auto* Other=static_cast<const FSavedMove_Duel*>(Move.Get());
        if (bSavedSlowWalk!=Other->bSavedSlowWalk || bSavedCombatFacing!=Other->bSavedCombatFacing) return false;
        return Super::CanCombineWith(Move,Character,MaxDelta);
    }
    virtual void SetMoveFor(ACharacter* Character,float Delta,FVector const& NewAcceleration,FNetworkPredictionData_Client_Character& Data) override
    {
        Super::SetMoveFor(Character,Delta,NewAcceleration,Data);
        bSavedSlowWalk=CastChecked<UDuelCharacterMovement>(Character->GetCharacterMovement())->bWantsSlowWalk;
        bSavedCombatFacing=CastChecked<UDuelCharacterMovement>(Character->GetCharacterMovement())->bWantsCombatFacing;
    }
    virtual void PrepMoveFor(ACharacter* Character) override
    {
        Super::PrepMoveFor(Character);
        CastChecked<UDuelCharacterMovement>(Character->GetCharacterMovement())->bWantsSlowWalk=bSavedSlowWalk;
        CastChecked<UDuelCharacterMovement>(Character->GetCharacterMovement())->bWantsCombatFacing=bSavedCombatFacing;
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
FRotator UDuelCharacterMovement::ComputeOrientToMovementRotation(const FRotator& CurrentRotation,float DeltaTime,FRotator& DeltaRotation) const
{
    if (!CharacterOwner) return CurrentRotation;
    const FRotator View(0,CharacterOwner->GetControlRotation().Yaw,0);
    // Forward/sideways travel follows input; backward travel faces the camera
    // forward and uses the existing backward or backward-diagonal rifle clips.
    if (Acceleration.SizeSquared2D()<KINDA_SMALL_NUMBER || FVector::DotProduct(Acceleration,View.Vector())<-.01f)
        return View;
    return FRotator(0,Acceleration.Rotation().Yaw,0);
}
void UDuelCharacterMovement::PhysicsRotation(float DeltaTime)
{
    if (bWantsCombatFacing && HasValidData() && CharacterOwner->GetController())
    {
        // Align before the shot/aim pose so a moving body cannot point the gun
        // away from the crosshair. Camera rotation remains independent.
        MoveUpdatedComponent(FVector::ZeroVector,FRotator(0,CharacterOwner->GetControlRotation().Yaw,0),true);
        return;
    }
    Super::PhysicsRotation(DeltaTime);
}
void UDuelCharacterMovement::UpdateFromCompressedFlags(uint8 Flags)
{
    Super::UpdateFromCompressedFlags(Flags);
    bWantsSlowWalk=(Flags & FSavedMove_Character::FLAG_Custom_0)!=0;
    bWantsCombatFacing=(Flags & FSavedMove_Character::FLAG_Custom_1)!=0;
    if (auto* Character=Cast<ADuelCharacter>(CharacterOwner)) Character->SyncSlowWalking(bWantsSlowWalk);
}
FNetworkPredictionData_Client* UDuelCharacterMovement::GetPredictionData_Client() const
{
    if (!ClientPredictionData) const_cast<UDuelCharacterMovement*>(this)->ClientPredictionData=new FPredictionData_Duel(*this);
    return ClientPredictionData;
}

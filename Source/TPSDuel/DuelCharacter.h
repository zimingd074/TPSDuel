#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "DuelWeaponPose.h"
#include "DuelCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class USoundBase;

UCLASS()
class TPSDUEL_API ADuelCharacter : public ACharacter
{
    GENERATED_BODY()
public:
    ADuelCharacter(const FObjectInitializer& ObjectInitializer=FObjectInitializer::Get());
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual float TakeDamage(float Damage, const FDamageEvent& Event, AController* Instigator, AActor* Causer) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;

    void MoveForward(float Value);
    void MoveRight(float Value);
    void SetCrouching(bool Pressed);
    void SetSlowWalking(bool Pressed);
    bool IsSlowWalking() const;
    void SyncSlowWalking(bool Pressed);
    void BeginFire();
    void EndFire();
    void SetAiming(bool Aiming);
    void Reload();
    void StopCombat();
    void GrantProtection();
    bool IsAlive() const { return Health > 0.f; }
    bool CanCombat() const;
    bool CanUseWeapon() const;
    bool CanMove() const;
    float GetHealth() const { return Health; }
    int32 GetAmmo() const { return Ammo; }
    bool IsReloading() const { return bReloading; }
    bool IsProtected() const { return bProtected; }
    bool IsAiming() const { return bAiming; }
    float GetVisualAimPitch() const;
    float GetVisualAimAlpha() const { return VisualAimAlpha; }
    float GetVisualCarryAlpha() const { return VisualWeaponPose.CarryAlpha; }
    const FDuelWeaponPose& GetVisualWeaponPose() const { return VisualWeaponPose; }
    const FDuelWeaponPose& GetWeaponPoseForAnimation() const { return DesiredWeaponPose; }
    bool IsFiring() const { return IsLocallyControlled() ? bLocalTrigger : bServerTrigger; }
    float GetReloadProgress() const;
    float GetVisualRecoil() const;
    float GetVisualShotTime() const { return VisualShotTime; }

protected:
    UPROPERTY(VisibleAnywhere) USpringArmComponent* CameraBoom;
    UPROPERTY(VisibleAnywhere) UCameraComponent* FollowCamera;
    UPROPERTY(VisibleAnywhere) UStaticMeshComponent* Body;
    UPROPERTY(VisibleAnywhere) UStaticMeshComponent* Head;
    UPROPERTY(VisibleAnywhere) UStaticMeshComponent* Rifle;
    UPROPERTY(VisibleAnywhere) UStaticMeshComponent* Magazine;
    UPROPERTY(VisibleAnywhere) UStaticMeshComponent* ShieldMarker;
    UPROPERTY() UMaterialInstanceDynamic* BodyMaterial;
    UPROPERTY() UMaterialInstanceDynamic* HeadMaterial;
    UPROPERTY() USoundBase* FireSound;

    UPROPERTY(ReplicatedUsing=OnRep_Health) float Health = 100.f;
    UPROPERTY(Replicated) int32 Ammo = 30;
    UPROPERTY(Replicated) bool bReloading = false;
    UPROPERTY(Replicated) float ReloadStartedAt = -1.f;
    UPROPERTY(Replicated) bool bProtected = false;
    UPROPERTY(Replicated) bool bAiming = false;
    UPROPERTY(Replicated) bool bSlowWalking = false;
    UPROPERTY(Replicated) bool bServerTrigger = false;

    UFUNCTION() void OnRep_Health();
    UFUNCTION(Server, Reliable) void ServerFireIntent(bool Pressed, FRotator Aim);
    UFUNCTION(Server, Unreliable) void ServerAim(FRotator Aim);
    UFUNCTION(Server, Reliable) void ServerSetAiming(bool Aiming);
    UFUNCTION(Server, Reliable) void ServerReload();
    UFUNCTION(NetMulticast, Unreliable) void MulticastShot(FVector_NetQuantize Start, FVector_NetQuantize End);

private:
    void FireOnce();
    void FinishReload();
    void ClearProtection();
    void LocalFireFeedback();
    void RefreshMovement();
    void UpdateWeaponAfterAnimation();
    void ComputeShotView(FVector& Origin, FVector& Direction) const;
    FRotator LocalAim() const;
    bool AcceptAim(const FRotator& Aim);
    FRotator ServerAimRotation;
    bool bLocalTrigger = false;
    bool bMovementAllowed = true;
    float LastShotTime = -100.f;
    float NextShotTime = -100.f;
    float NextFeedbackTime = -100.f;
    float LastAimSendTime = -100.f;
    float LastAimReceiveTime = -100.f;
    int32 DisplayedSlot = -2;
    bool bQuantumCharacter = false;
    float VisualAimAlpha = 0.f;
    float VisualCarryAlpha = 0.f;
    FDuelWeaponPose VisualWeaponPose;
    FDuelWeaponPose DesiredWeaponPose;
    FDelegateHandle WeaponPoseFinalizedHandle;
    float VisualShotTime = -100.f;
    FTimerHandle FireTimer;
    FTimerHandle FeedbackTimer;
    FTimerHandle ReloadTimer;
    FTimerHandle ProtectionTimer;
};

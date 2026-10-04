#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "DuelPlayerController.generated.h"
class SDuelOverlay;
class ADuelCharacter;

UCLASS()
class TPSDUEL_API ADuelPlayerController : public APlayerController
{
    GENERATED_BODY()
public:
    virtual void BeginPlay() override;
    virtual void PlayerTick(float DeltaTime) override;
    virtual void SetupInputComponent() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void ClientReturnToMainMenuWithTextReason_Implementation(const FText& Reason) override;
    bool IsFrontEnd() const;
    bool IsMenuVisible() const { return bMenuVisible || IsFrontEnd(); }
    bool WantsTouch() const;
    void Host();
    void Join(const FString& Address);
    void Leave();
    void Ready();
    void ToggleMenu();
    void TouchMove(FVector2D Value) { TouchMovement = Value; }
    void TouchLook(FVector2D Delta);
    void FirePressed();
    void FireReleased();
    void AimPressed();
    void AimReleased();
    void ReloadPressed();
    void JumpPressed();
    void JumpReleased();
    UFUNCTION(Client, Unreliable) void ClientHitConfirmed();
    float GetHitTime() const { return HitTime; }
private:
    void TryCreateUI();
    void ApplyInputMode();
    void ClearLocalInput();
    void MoveForward(float Value);
    void MoveRight(float Value);
    void Turn(float Value);
    void LookUp(float Value);
    void TickSmokeTest();
    UFUNCTION(Server, Reliable) void ServerLoaded();
    UFUNCTION(Server, Reliable) void ServerReady();
    TSharedPtr<SDuelOverlay> Overlay;
    FVector2D TouchMovement = FVector2D::ZeroVector;
    bool bMenuVisible = false;
    bool bResultsVisible = false;
    bool bReportedLoaded = false;
    float HitTime = -100.f;
    double SmokeStart = 0;
    double SmokeFinishSeen = 0;
    double SmokeExitAt = 0;
    TWeakObjectPtr<ADuelCharacter> SmokePawns[2];
};

#include "DuelPlayerController.h"
#include "DuelCharacter.h"
#include "DuelGameInstance.h"
#include "DuelGameMode.h"
#include "DuelGameState.h"
#include "DuelPlayerState.h"
#include "DuelRules.h"
#include "DuelUI.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

void ADuelPlayerController::BeginPlay()
{
    Super::BeginPlay();
    if (IsLocalController())
    {
        if (PlayerCameraManager) { PlayerCameraManager->ViewPitchMin = -80.f; PlayerCameraManager->ViewPitchMax = 80.f; }
        if (UDuelGameInstance* GI = GetGameInstance<UDuelGameInstance>())
        {
            if (IsFrontEnd()) GI->MenuLoaded();
            else GI->ConnectionStatus.Empty();
        }
        TryCreateUI();
    }
}
bool ADuelPlayerController::IsFrontEnd() const { return GetWorld() && GetWorld()->GetMapName().Contains(TEXT("L_Menu")); }
bool ADuelPlayerController::WantsTouch() const
{
#if PLATFORM_ANDROID || PLATFORM_IOS
    return true;
#else
    return FParse::Param(FCommandLine::Get(), TEXT("DuelTouch"));
#endif
}
void ADuelPlayerController::TryCreateUI()
{
    if (Overlay.IsValid() || !IsLocalController()) return;
    ULocalPlayer* LP = GetLocalPlayer();
    if (!LP || !LP->ViewportClient) return;
    SAssignNew(Overlay, SDuelOverlay).Owner(this);
    LP->ViewportClient->AddViewportWidgetForPlayer(LP, Overlay.ToSharedRef(), 10);
    ApplyInputMode();
}
void ADuelPlayerController::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);
    if (!IsLocalController()) return;
    TryCreateUI();
    const ADuelGameState* MatchState = GetWorld()->GetGameState<ADuelGameState>();
    const bool ShowResults = !IsFrontEnd() && MatchState && MatchState->Phase == EDuelPhase::Finished;
    if (ShowResults != bResultsVisible)
    {
        bResultsVisible = ShowResults;
        ClearLocalInput();
        ApplyInputMode();
    }
    if (!IsFrontEnd() && !bReportedLoaded && GetPawn() && GetPlayerState<ADuelPlayerState>())
    {
        bReportedLoaded = true;
        ServerLoaded();
    }
    if (!IsMenuVisible())
    {
        MoveForward(TouchMovement.Y);
        MoveRight(TouchMovement.X);
    }
    TickSmokeTest();
}
void ADuelPlayerController::ServerLoaded_Implementation()
{
    if (ADuelGameMode* Mode = GetWorld()->GetAuthGameMode<ADuelGameMode>()) Mode->PlayerLoaded(this);
}
void ADuelPlayerController::ServerReady_Implementation()
{
    if (ADuelGameMode* Mode = GetWorld()->GetAuthGameMode<ADuelGameMode>()) Mode->PlayerReady(this);
}
void ADuelPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    InputComponent->BindAxis(TEXT("MoveForward"), this, &ADuelPlayerController::MoveForward);
    InputComponent->BindAxis(TEXT("MoveRight"), this, &ADuelPlayerController::MoveRight);
    InputComponent->BindAxis(TEXT("Turn"), this, &ADuelPlayerController::Turn);
    InputComponent->BindAxis(TEXT("LookUp"), this, &ADuelPlayerController::LookUp);
    InputComponent->BindAction(TEXT("Fire"), IE_Pressed, this, &ADuelPlayerController::FirePressed);
    InputComponent->BindAction(TEXT("Fire"), IE_Released, this, &ADuelPlayerController::FireReleased);
    InputComponent->BindAction(TEXT("Aim"), IE_Pressed, this, &ADuelPlayerController::AimPressed);
    InputComponent->BindAction(TEXT("Aim"), IE_Released, this, &ADuelPlayerController::AimReleased);
    InputComponent->BindAction(TEXT("Reload"), IE_Pressed, this, &ADuelPlayerController::ReloadPressed);
    InputComponent->BindAction(TEXT("Jump"), IE_Pressed, this, &ADuelPlayerController::JumpPressed);
    InputComponent->BindAction(TEXT("Jump"), IE_Released, this, &ADuelPlayerController::JumpReleased);
    InputComponent->BindAction(TEXT("Menu"), IE_Pressed, this, &ADuelPlayerController::ToggleMenu);
}
void ADuelPlayerController::MoveForward(float Value)
{
    if (!IsMenuVisible()) if (ADuelCharacter* C = Cast<ADuelCharacter>(GetPawn())) C->MoveForward(Value);
}
void ADuelPlayerController::MoveRight(float Value)
{
    if (!IsMenuVisible()) if (ADuelCharacter* C = Cast<ADuelCharacter>(GetPawn())) C->MoveRight(Value);
}
void ADuelPlayerController::Turn(float Value) { if (!IsMenuVisible()) AddYawInput(Value); }
void ADuelPlayerController::LookUp(float Value) { if (!IsMenuVisible()) AddPitchInput(Value); }
void ADuelPlayerController::TouchLook(FVector2D Delta) { Turn(Delta.X * .07f); LookUp(Delta.Y * .07f); }
void ADuelPlayerController::FirePressed() { if (!IsMenuVisible()) if (ADuelCharacter* C = Cast<ADuelCharacter>(GetPawn())) C->BeginFire(); }
void ADuelPlayerController::FireReleased() { if (ADuelCharacter* C = Cast<ADuelCharacter>(GetPawn())) C->EndFire(); }
void ADuelPlayerController::AimPressed() { if (!IsMenuVisible()) if (ADuelCharacter* C = Cast<ADuelCharacter>(GetPawn())) C->SetAiming(true); }
void ADuelPlayerController::AimReleased() { if (ADuelCharacter* C = Cast<ADuelCharacter>(GetPawn())) C->SetAiming(false); }
void ADuelPlayerController::ReloadPressed() { if (!IsMenuVisible()) if (ADuelCharacter* C = Cast<ADuelCharacter>(GetPawn())) C->Reload(); }
void ADuelPlayerController::JumpPressed() { if (!IsMenuVisible()) if (ADuelCharacter* C = Cast<ADuelCharacter>(GetPawn())) if (C->CanMove()) C->Jump(); }
void ADuelPlayerController::JumpReleased() { if (ADuelCharacter* C = Cast<ADuelCharacter>(GetPawn())) C->StopJumping(); }
void ADuelPlayerController::ClearLocalInput()
{
    TouchMovement = FVector2D::ZeroVector;
    FireReleased();
    AimReleased();
    JumpReleased();
}
void ADuelPlayerController::ApplyInputMode()
{
    bShowMouseCursor = IsMenuVisible() || bResultsVisible;
    if (IsMenuVisible() || bResultsVisible)
    {
        FInputModeGameAndUI Mode;
        Mode.SetHideCursorDuringCapture(false);
        SetInputMode(Mode);
    }
    else if (WantsTouch()) SetInputMode(FInputModeGameAndUI());
    else
    {
        FInputModeGameOnly Mode;
        Mode.SetConsumeCaptureMouseDown(false);
        SetInputMode(Mode);
    }
}
void ADuelPlayerController::ToggleMenu()
{
    if (IsFrontEnd()) return;
    bMenuVisible = !bMenuVisible;
    ClearLocalInput();
    ApplyInputMode();
}
void ADuelPlayerController::Host()
{
    if (!IsFrontEnd()) return;
    if (UDuelGameInstance* GI = GetGameInstance<UDuelGameInstance>()) GI->ConnectionStatus = TEXT("Creating match...");
    UGameplayStatics::OpenLevel(this, FName(TEXT("/Game/Maps/L_Arena")), true, TEXT("listen?duel=1"));
}
void ADuelPlayerController::Join(const FString& Address)
{
    if (!IsFrontEnd()) return;
    UDuelGameInstance* GI = GetGameInstance<UDuelGameInstance>();
    FString Trimmed = Address.TrimStartAndEnd();
    std::string Endpoint;
    if (!DuelRules::ParseEndpoint(TCHAR_TO_UTF8(*Trimmed), Endpoint))
    {
        if (GI) GI->ConnectionStatus = TEXT("Enter a valid IPv4:port, e.g. 192.168.1.100:7777");
        return;
    }
    const FString Canonical(UTF8_TO_TCHAR(Endpoint.c_str()));
    if (GI) { GI->LastEndpoint = Canonical; GI->ConnectionStatus = TEXT("Connecting to ") + Canonical; }
    ClientTravel(Canonical + TEXT("?duel=1"), TRAVEL_Absolute);
}
void ADuelPlayerController::Leave()
{
    ClearLocalInput();
    if (UDuelGameInstance* GI = GetGameInstance<UDuelGameInstance>()) GI->ReturnToMenu(TEXT("Left match"));
}
void ADuelPlayerController::Ready() { ServerReady(); }
void ADuelPlayerController::ClientHitConfirmed_Implementation() { HitTime = GetWorld()->TimeSeconds; }
void ADuelPlayerController::ClientReturnToMainMenuWithTextReason_Implementation(const FText& Reason)
{
    ClearLocalInput();
    if (UDuelGameInstance* GI = GetGameInstance<UDuelGameInstance>()) GI->ReturnToMenu(Reason.ToString());
    else Super::ClientReturnToMainMenuWithTextReason_Implementation(Reason);
}
void ADuelPlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
    if (Overlay.IsValid())
    {
        if (ULocalPlayer* LP = GetLocalPlayer())
            if (LP->ViewportClient) LP->ViewportClient->RemoveViewportWidgetForPlayer(LP, Overlay.ToSharedRef());
        Overlay.Reset();
    }
    Super::EndPlay(Reason);
}

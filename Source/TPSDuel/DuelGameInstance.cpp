#include "DuelGameInstance.h"
#include "TPSDuel.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
void UDuelGameInstance::Init()
{
    Super::Init();
    if (GEngine)
    {
        GEngine->OnNetworkFailure().AddUObject(this, &UDuelGameInstance::NetworkFailure);
        GEngine->OnTravelFailure().AddUObject(this, &UDuelGameInstance::TravelFailure);
    }
}
void UDuelGameInstance::Shutdown()
{
    if (GEngine)
    {
        GEngine->OnNetworkFailure().RemoveAll(this);
        GEngine->OnTravelFailure().RemoveAll(this);
    }
    Super::Shutdown();
}
void UDuelGameInstance::NetworkFailure(UWorld* World, UNetDriver* Driver, ENetworkFailure::Type Type, const FString& Message)
{
    if (!World || World->GetGameInstance() != this) return;
    ReturnToMenu(TEXT("Connection failed/disconnected: ") + Message);
}
void UDuelGameInstance::TravelFailure(UWorld* World, ETravelFailure::Type Type, const FString& Message)
{
    if (!World || World->GetGameInstance() != this) return;
    ConnectionStatus = TEXT("Map loading failed: ") + Message;
    UE_LOG(LogTPSDuel, Error, TEXT("%s"), *ConnectionStatus);
    if (!World->GetMapName().Contains(TEXT("L_Menu"))) ReturnToMenu(ConnectionStatus);
}
void UDuelGameInstance::ReturnToMenu(const FString& Message)
{
    ConnectionStatus = Message;
    UE_LOG(LogTPSDuel, Log, TEXT("Return to menu: %s"), *Message);
    if (bReturning || !GetWorld()) return;
    bReturning = true;
    GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
    {
        UGameplayStatics::OpenLevel(this, FName(TEXT("/Game/Maps/L_Menu")), true);
    }));
}

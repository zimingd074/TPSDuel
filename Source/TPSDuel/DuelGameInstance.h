#pragma once
#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Engine/EngineBaseTypes.h"
#include "DuelGameInstance.generated.h"
class UNetDriver;
UCLASS()
class TPSDUEL_API UDuelGameInstance : public UGameInstance
{
    GENERATED_BODY()
public:
    virtual void Init() override;
    virtual void Shutdown() override;
    void ReturnToMenu(const FString& Message);
    void MenuLoaded() { bReturning = false; }
    FString ConnectionStatus;
    FString LastEndpoint = TEXT("192.168.1.100:7777");
private:
    void NetworkFailure(UWorld* World, UNetDriver* Driver, ENetworkFailure::Type Type, const FString& Message);
    void TravelFailure(UWorld* World, ETravelFailure::Type Type, const FString& Message);
    bool bReturning = false;
};

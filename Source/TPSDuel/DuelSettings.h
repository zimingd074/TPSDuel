#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "DuelSettings.generated.h"

UCLASS(Config=Game, DefaultConfig)
class TPSDUEL_API UDuelSettings : public UObject
{
    GENERATED_BODY()
public:
    UPROPERTY(Config) float MaxHealth = 100.f;
    UPROPERTY(Config) float ShotDamage = 25.f;
    UPROPERTY(Config) float FireInterval = 0.075f;
    UPROPERTY(Config) float WeaponRange = 5000.f;
    UPROPERTY(Config) int32 MagazineCapacity = 30;
    UPROPERTY(Config) float ReloadSeconds = 2.f;
    UPROPERTY(Config) float RespawnSeconds = 3.f;
    UPROPERTY(Config) float ProtectionSeconds = 2.f;
    UPROPERTY(Config) float CountdownSeconds = 3.f;
    UPROPERTY(Config) int32 WinningScore = 3;
};

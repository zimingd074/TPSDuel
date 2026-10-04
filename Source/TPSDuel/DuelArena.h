#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DuelArena.generated.h"
class UStaticMeshComponent;
UCLASS()
class TPSDUEL_API ADuelArena : public AActor
{
    GENERATED_BODY()
public:
    ADuelArena();
    virtual void BeginPlay() override;
private:
    UPROPERTY() TArray<UStaticMeshComponent*> Blocks;
    TArray<FLinearColor> Tints;
    TArray<float> Tiles;
};

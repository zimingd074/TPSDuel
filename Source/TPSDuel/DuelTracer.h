#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DuelTracer.generated.h"
class UStaticMeshComponent;
UCLASS()
class TPSDUEL_API ADuelTracer : public AActor
{
    GENERATED_BODY()
public:
    ADuelTracer();
    void SetBeam(const FVector& Start, const FVector& End);
private:
    UPROPERTY() UStaticMeshComponent* Beam;
};

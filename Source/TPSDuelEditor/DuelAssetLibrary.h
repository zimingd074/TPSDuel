#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DuelAssetLibrary.generated.h"

UCLASS()
class TPSDUELEDITOR_API UDuelAssetLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="TPSDuel|Assets")
    static TArray<FString> RetargetQuantumLocomotion();

    UFUNCTION(BlueprintCallable, Category="TPSDuel|Assets")
    static TArray<FString> DescribeQuantumBones();

    UFUNCTION(BlueprintCallable, Category="TPSDuel|Assets")
    static bool MakeQuantumHoldPose();

    UFUNCTION(BlueprintCallable, Category="TPSDuel|Assets")
    static bool InstallQuantumWeaponPose();

    UFUNCTION(BlueprintCallable, Category="TPSDuel|Assets")
    static TArray<FString> DescribeRifleGeometry();

    UFUNCTION(BlueprintCallable, Category="TPSDuel|Assets")
    static bool SplitQuantumRifle();
};

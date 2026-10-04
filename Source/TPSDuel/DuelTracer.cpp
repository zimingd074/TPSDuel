#include "DuelTracer.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
ADuelTracer::ADuelTracer()
{
    Beam = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Beam"));
    SetRootComponent(Beam);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    Beam->SetStaticMesh(Cylinder.Object);
    Beam->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    InitialLifeSpan = .07f;
}
void ADuelTracer::SetBeam(const FVector& Start, const FVector& End)
{
    SetActorLocation((Start + End) * .5f);
    SetActorRotation(FRotationMatrix::MakeFromZ(End - Start).Rotator());
    SetActorScale3D(FVector(.018f, .018f, FMath::Max(1.f, FVector::Distance(Start, End)) / 100.f));
    UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_DuelColor.M_DuelColor"));
    if (Material)
    {
        UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(Material, this);
        Instance->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f, .8f, .2f));
        Beam->SetMaterial(0, Instance);
    }
}

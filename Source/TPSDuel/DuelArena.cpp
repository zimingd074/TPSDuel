#include "DuelArena.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
ADuelArena::ADuelArena()
{
    bReplicates = true;
    bAlwaysRelevant = true;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    auto Block = [this](const TCHAR* Name, FVector Location, FVector Size)
    {
        UStaticMeshComponent* Mesh = CreateDefaultSubobject<UStaticMeshComponent>(Name);
        Mesh->SetupAttachment(GetRootComponent());
        Mesh->SetStaticMesh(Cube.Object);
        Mesh->SetRelativeLocation(Location);
        Mesh->SetRelativeScale3D(Size / 100.f);
        Mesh->SetCollisionProfileName(TEXT("BlockAll"));
        Blocks.Add(Mesh);
    };
    Block(TEXT("Floor"), FVector(0, 0, -20), FVector(3600, 2400, 40));
    Block(TEXT("North"), FVector(0, 1200, 180), FVector(3700, 60, 360));
    Block(TEXT("South"), FVector(0, -1200, 180), FVector(3700, 60, 360));
    Block(TEXT("West"), FVector(-1800, 0, 180), FVector(60, 2400, 360));
    Block(TEXT("East"), FVector(1800, 0, 180), FVector(60, 2400, 360));
    Block(TEXT("CenterA"), FVector(-250, -250, 100), FVector(350, 100, 200));
    Block(TEXT("CenterB"), FVector(250, 250, 100), FVector(350, 100, 200));
    Block(TEXT("CoverA"), FVector(-950, 300, 65), FVector(100, 450, 130));
    Block(TEXT("CoverB"), FVector(950, -300, 65), FVector(100, 450, 130));
    Block(TEXT("LowA"), FVector(-600, -700, 50), FVector(250, 150, 100));
    Block(TEXT("LowB"), FVector(600, 700, 50), FVector(250, 150, 100));
}
void ADuelArena::BeginPlay()
{
    Super::BeginPlay();
    UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_DuelColor.M_DuelColor"));
    if (!Material) return;
    for (int32 Index = 0; Index < Blocks.Num(); ++Index)
    {
        UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(Material, this);
        Instance->SetVectorParameterValue(TEXT("Color"), Index == 0 ? FLinearColor(.13f, .17f, .22f) :
            (Index < 5 ? FLinearColor(.24f, .3f, .37f) : FLinearColor(.12f, .35f, .38f)));
        Blocks[Index]->SetMaterial(0, Instance);
    }
}

#include "DuelArena.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ADuelArena::ADuelArena()
{
    bReplicates = true;
    bAlwaysRelevant = true;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Concrete(TEXT("/Game/Materials/M_Concrete.M_Concrete"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Brick(TEXT("/Game/Materials/M_Brick.M_Brick"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Metal(TEXT("/Game/Materials/M_Metal.M_Metal"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Wood(TEXT("/Game/Materials/M_Wood.M_Wood"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Paint(TEXT("/Game/Materials/M_Paint.M_Paint"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Sky(TEXT("/Game/Materials/M_Sky.M_Sky"));
    auto Block = [this](FString Name, FVector Location, FVector Size, UMaterialInterface* Material,
        FLinearColor Tint = FLinearColor::White, bool Solid = true, float Tiling = 1.f)
    {
        auto* Mesh = CreateDefaultSubobject<UStaticMeshComponent>(*Name);
        Mesh->SetupAttachment(GetRootComponent()); Mesh->SetStaticMesh(Cube.Object);
        Mesh->SetRelativeLocation(Location); Mesh->SetRelativeScale3D(Size / 100.f);
        Mesh->SetCollisionProfileName(Solid ? TEXT("BlockAll") : TEXT("NoCollision"));
        Mesh->SetMaterial(0, Material); Mesh->SetCullDistance(6500.f);
        Blocks.Add(Mesh); Tints.Add(Tint); Tiles.Add(Tiling);
        return Mesh;
    };
    const FLinearColor Steel(.18f,.22f,.25f), Yellow(.95f,.58f,.08f);
    Block(TEXT("Floor"),FVector(0,0,-20),FVector(3600,2400,40),Concrete.Object,FLinearColor(.62f,.65f,.64f),true,12);
    Block(TEXT("North"),FVector(0,1200,360),FVector(3660,60,720),Brick.Object,FLinearColor(.75f,.67f,.58f),true,8);
    Block(TEXT("South"),FVector(0,-1200,360),FVector(3660,60,720),Brick.Object,FLinearColor(.75f,.67f,.58f),true,8);
    Block(TEXT("West"),FVector(-1800,0,360),FVector(60,2400,720),Concrete.Object,FLinearColor(.6f,.64f,.65f),true,6);
    Block(TEXT("East"),FVector(1800,0,360),FVector(60,2400,720),Concrete.Object,FLinearColor(.6f,.64f,.65f),true,6);
    // Paired cover footprints remain symmetric; the central test lane stays open.
    Block(TEXT("CenterA"),FVector(-250,-250,100),FVector(350,100,200),Concrete.Object,FLinearColor(.62f,.64f,.61f),true,2);
    Block(TEXT("CenterB"),FVector(250,250,100),FVector(350,100,200),Concrete.Object,FLinearColor(.62f,.64f,.61f),true,2);
    Block(TEXT("CoverA"),FVector(-950,300,65),FVector(100,450,130),Metal.Object,Steel,true,2);
    Block(TEXT("CoverB"),FVector(950,-300,65),FVector(100,450,130),Metal.Object,Steel,true,2);
    Block(TEXT("LowA"),FVector(-600,-700,50),FVector(250,150,100),Wood.Object,FLinearColor(.72f,.54f,.34f),true,2);
    Block(TEXT("LowB"),FVector(600,700,50),FVector(250,150,100),Wood.Object,FLinearColor(.72f,.54f,.34f),true,2);
    for(int32 Side : {-1,1})
    {
        const FString S=Side<0 ? TEXT("A") : TEXT("B");
        const FLinearColor Team=Side<0 ? FLinearColor(.08f,.35f,.56f) : FLinearColor(.6f,.16f,.09f);
        const FVector Container(Side*1000.f,Side*850.f,140.f);
        Block(TEXT("Container")+S,Container,FVector(600,260,280),Metal.Object,Team);
        for(int32 Rib=0; Rib<13; ++Rib)
            Block(FString::Printf(TEXT("Rib%s%d"),*S,Rib),Container+FVector(-270+Rib*45,-Side*133,0),FVector(8,8,260),Paint.Object,Team*.65f,false);
        for(int32 End : {-1,1})
            Block(FString::Printf(TEXT("ContainerPost%s%d"),*S,End),Container+FVector(End*285,-Side*135,0),FVector(18,12,280),Metal.Object,Steel,false);
        Block(TEXT("ContainerRail")+S,Container+FVector(0,-Side*135,135),FVector(600,14,12),Metal.Object,Steel,false);
        Block(TEXT("CargoCrate")+S,FVector(Side*450,Side*900,90),FVector(180,160,180),Wood.Object,FLinearColor(.8f,.63f,.41f),true,2);
        for(int32 Rail : {-1,1})
            Block(FString::Printf(TEXT("CrateBrace%s%d"),*S,Rail),FVector(Side*450+Rail*65,Side*817,90),FVector(15,6,180),Metal.Object,Steel,false);
        Block(TEXT("Shutter")+S,FVector(Side*1766,Side*650,200),FVector(8,560,400),Metal.Object,FLinearColor(.32f,.36f,.38f),false,3);
        for(int32 Slat=0; Slat<10; ++Slat)
            Block(FString::Printf(TEXT("ShutterSlat%s%d"),*S,Slat),FVector(Side*1759,Side*650,Slat*38+20),FVector(6,560,4),Paint.Object,Steel,false);
        Block(TEXT("SpawnStripe")+S,FVector(Side*1400,-Side*700,1),FVector(480,8,2),Paint.Object,Team,false);
        Block(TEXT("SpawnStripeEnd")+S,FVector(Side*1610,-Side*700,1),FVector(8,420,2),Paint.Object,Team,false);
    }
    for(int32 Beam=0; Beam<5; ++Beam)
    {
        const float X=-1500.f+Beam*750.f;
        for(int32 Side : {-1,1})
        {
            Block(FString::Printf(TEXT("Column%d_%d"),Beam,Side),FVector(X,Side*1150,360),FVector(35,45,720),Metal.Object,Steel);
            Block(FString::Printf(TEXT("ColumnFoot%d_%d"),Beam,Side),FVector(X,Side*1150,60),FVector(48,55,120),Paint.Object,Yellow,false);
        }
        Block(FString::Printf(TEXT("Rafter%d"),Beam),FVector(X,0,700),FVector(35,2400,45),Metal.Object,Steel);
        Block(FString::Printf(TEXT("Fixture%d"),Beam),FVector(X,0,660),FVector(18,300,12),Paint.Object,FLinearColor(.88f,.9f,.84f),false);
    }
    // Daylight through roof strips: one shadowed sun, without a point-light array.
    for(int32 Side : {-1,1})
        Block(FString::Printf(TEXT("Roof%d"),Side),FVector(0,Side*875,745),FVector(3600,650,30),Metal.Object,Steel);
    for(int32 Side : {-1,1})
        Block(FString::Printf(TEXT("LaneLine%d"),Side),FVector(0,Side*100,1),FVector(3300,5,2),Paint.Object,Yellow,false);
    auto Label=[this](const TCHAR* Name,const TCHAR* Text,FVector Location,FRotator Rotation,FColor Color,float Size)
    {
        auto* Sign=CreateDefaultSubobject<UTextRenderComponent>(Name);
        Sign->SetupAttachment(GetRootComponent()); Sign->SetRelativeLocation(Location); Sign->SetRelativeRotation(Rotation);
        Sign->SetText(FText::FromString(Text)); Sign->SetHorizontalAlignment(EHTA_Center);
        Sign->SetWorldSize(Size); Sign->SetTextRenderColor(Color); Sign->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    };
    Label(TEXT("DepotSign"),TEXT("DEPOT 07"),FVector(0,1165,430),FRotator(0,-90,0),FColor(213,207,185),100);
    Label(TEXT("BlueSign"),TEXT("01 / BLUE"),FVector(-1760,-650,455),FRotator::ZeroRotator,FColor(93,182,235),65);
    Label(TEXT("RedSign"),TEXT("02 / RED"),FVector(1760,650,455),FRotator(0,180,0),FColor(240,129,90),65);
    auto* Dome=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SkyDome"));
    Dome->SetupAttachment(GetRootComponent()); Dome->SetStaticMesh(Sphere.Object); Dome->SetMaterial(0,Sky.Object);
    Dome->SetRelativeScale3D(FVector(200)); Dome->SetCollisionEnabled(ECollisionEnabled::NoCollision); Dome->SetCastShadow(false);
    auto* Sun=CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
    Sun->SetMobility(EComponentMobility::Movable);
    Sun->SetupAttachment(GetRootComponent()); Sun->SetRelativeRotation(FRotator(-48,-35,0));
    Sun->SetIntensity(3.f); Sun->SetLightColor(FLinearColor(1.f,.91f,.77f));
    Sun->DynamicShadowDistanceMovableLight=4500.f; Sun->DynamicShadowCascades=2;
    auto* Ambient=CreateDefaultSubobject<USkyLightComponent>(TEXT("Ambient"));
    Ambient->SetMobility(EComponentMobility::Movable);
    Ambient->SetupAttachment(GetRootComponent()); Ambient->SetIntensity(1.1f);
    Ambient->SkyDistanceThreshold=4000.f; Ambient->bLowerHemisphereIsBlack=false;
}
void ADuelArena::BeginPlay()
{
    Super::BeginPlay();
    for(int32 Index=0; Index<Blocks.Num(); ++Index)
    {
        UMaterialInterface* Base=Blocks[Index]->GetMaterial(0);
        if(!Base) continue;
        auto* Instance=UMaterialInstanceDynamic::Create(Base,this);
        Instance->SetVectorParameterValue(TEXT("Color"),Tints[Index]);
        Instance->SetScalarParameterValue(TEXT("Tiling"),Tiles[Index]);
        Blocks[Index]->SetMaterial(0,Instance);
    }
}

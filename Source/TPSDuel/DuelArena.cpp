#include "DuelArena.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/PostProcessComponent.h"
#include "Engine/TextureCube.h"
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
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Concrete(TEXT("/Game/Materials/M_Concrete.M_Concrete"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> WornFloor(TEXT("/Game/Materials/M_WornFloor.M_WornFloor"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Brick(TEXT("/Game/Materials/M_Brick.M_Brick"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Metal(TEXT("/Game/Materials/M_Metal.M_Metal"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Wood(TEXT("/Game/Materials/M_Wood.M_Wood"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Paint(TEXT("/Game/Materials/M_Paint.M_Paint"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Sky(TEXT("/Game/Materials/M_Sky.M_Sky"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> IndustrialSky(TEXT("/Game/FreeAssets/PolyHaven/Realism/Materials/M_IndustrialSky.M_IndustrialSky"));
    static ConstructorHelpers::FObjectFinder<UTextureCube> SkyCube(TEXT("/Game/FreeAssets/PolyHaven/Realism/Textures/T_IndustrialSky_hdri.T_IndustrialSky_hdri"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Rust(TEXT("/Game/FreeAssets/PolyHaven/Realism/Materials/M_RustySteel.M_RustySteel"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Glow(TEXT("/Game/FreeAssets/PolyHaven/Realism/Materials/M_FixtureGlow.M_FixtureGlow"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Oil(TEXT("/Game/FreeAssets/PolyHaven/Realism/Materials/M_OilWear.M_OilWear"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Dust(TEXT("/Game/FreeAssets/PolyHaven/Realism/Materials/M_DustWear.M_DustWear"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Crack(TEXT("/Game/FreeAssets/PolyHaven/Realism/Materials/M_CrackWear.M_CrackWear"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Barrel(TEXT("/Game/FreeAssets/PolyHaven/Barrel/SM_Barrel.SM_Barrel"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> MilitaryCrate(TEXT("/Game/FreeAssets/PolyHaven/MilitaryCrate/SM_MilitaryCrate.SM_MilitaryCrate"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Plaster(TEXT("/Game/FreeAssets/PolyHaven/Warehouse/Materials/M_SandPlaster.M_SandPlaster"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> FactoryWall(TEXT("/Game/FreeAssets/PolyHaven/Warehouse/Factory/SM_Factory_wall_standard_standard_01.SM_Factory_wall_standard_standard_01"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> FactoryWindowWall(TEXT("/Game/FreeAssets/PolyHaven/Warehouse/Factory/SM_Factory_wall_window_tall_large_01.SM_Factory_wall_window_tall_large_01"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> FactoryWindow(TEXT("/Game/FreeAssets/PolyHaven/Warehouse/Factory/SM_Factory_window_tall_large_01.SM_Factory_window_tall_large_01"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> FactoryGarage(TEXT("/Game/FreeAssets/PolyHaven/Warehouse/Factory/SM_Factory_door_garage_door_01.SM_Factory_door_garage_door_01"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> RoadBarrier(TEXT("/Game/FreeAssets/PolyHaven/Warehouse/RoadBarrier/SM_RoadBarrier.SM_RoadBarrier"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> StorageCart(TEXT("/Game/FreeAssets/PolyHaven/Warehouse/StorageCart/SM_StorageCart.SM_StorageCart"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Shelf(TEXT("/Game/Meshes/SM_StorageShelves_group01.SM_StorageShelves_group01"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> ElectricalPanel(TEXT("/Game/Meshes/SM_ElectricalPanel_01.SM_ElectricalPanel_01"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Switchbox(TEXT("/Game/Meshes/SM_ElectricalSupply_Switchboard01.SM_ElectricalSupply_Switchboard01"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Pipe(TEXT("/Game/Meshes/SM_Pipe_round_long.SM_Pipe_round_long"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Elbow(TEXT("/Game/Meshes/SM_Pipe_round_corner1.SM_Pipe_round_corner1"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Lamp(TEXT("/Game/Meshes/SM_Lamp01.SM_Lamp01"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Vent(TEXT("/Game/Meshes/SM_AirVentilation01.SM_AirVentilation01"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> RoofFrame(TEXT("/Game/Meshes/SM_RoofFrame01.SM_RoofFrame01"));
    auto FactoryProp=[this](FString Name,UStaticMesh* Asset,FVector Center,FVector Size,FRotator Rotation,bool Solid=false)
    {
        if (!Asset) return;
        const auto Bounds=Asset->GetBounds();
        const FVector Dimensions=Bounds.BoxExtent*2.f;
        const FVector Scale(Size.X/FMath::Max(1.f,Dimensions.X),Size.Y/FMath::Max(1.f,Dimensions.Y),Size.Z/FMath::Max(1.f,Dimensions.Z));
        auto* Mesh=CreateDefaultSubobject<UStaticMeshComponent>(*Name);
        Mesh->SetupAttachment(GetRootComponent()); Mesh->SetStaticMesh(Asset);
        Mesh->SetRelativeScale3D(Scale); Mesh->SetRelativeRotation(Rotation);
        Mesh->SetRelativeLocation(Center-Rotation.RotateVector(Bounds.Origin*Scale));
        Mesh->SetCollisionProfileName(Solid ? TEXT("BlockAll") : TEXT("NoCollision")); Mesh->SetCullDistance(5000.f);
    };
    auto Prop = [this](FString Name, UStaticMesh* Asset, FVector Location, float Scale, float Yaw)
    {
        auto* Mesh = CreateDefaultSubobject<UStaticMeshComponent>(*Name);
        Mesh->SetupAttachment(GetRootComponent()); Mesh->SetStaticMesh(Asset);
        Mesh->SetRelativeLocation(Location); Mesh->SetRelativeScale3D(FVector(Scale));
        Mesh->SetRelativeRotation(FRotator(0,Yaw,0));
        Mesh->SetCollisionProfileName(TEXT("BlockAll")); Mesh->SetCullDistance(5000.f);
    };
    // Collection FBX pivots include the author's layout offsets. Recenter each
    // module by bounds while retaining its UVs, geometry and original materials.
    auto Module=[this](FString Name,UStaticMesh* Asset,FVector Center,FRotator Rotation,bool Solid=false)
    {
        if (!Asset) return;
        auto* Mesh=CreateDefaultSubobject<UStaticMeshComponent>(*Name);
        Mesh->SetupAttachment(GetRootComponent()); Mesh->SetStaticMesh(Asset);
        Mesh->SetRelativeRotation(Rotation);
        Mesh->SetRelativeLocation(Center-Rotation.RotateVector(Asset->GetBounds().Origin));
        Mesh->SetCollisionProfileName(Solid ? TEXT("BlockAll") : TEXT("NoCollision"));
        Mesh->SetCullDistance(6500.f);
    };
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
    Block(TEXT("Floor"),FVector(0,0,-20),FVector(3600,2400,40),WornFloor.Object,FLinearColor::White,true);
    Block(TEXT("North"),FVector(0,1200,360),FVector(3660,60,720),Plaster.Object,FLinearColor::White,true,6);
    Block(TEXT("South"),FVector(0,-1200,360),FVector(3660,60,720),Plaster.Object,FLinearColor::White,true,6);
    Block(TEXT("West"),FVector(-1800,0,360),FVector(60,2400,720),Plaster.Object,FLinearColor::White,true,6);
    Block(TEXT("East"),FVector(1800,0,360),FVector(60,2400,720),Plaster.Object,FLinearColor::White,true,6);
    for (int32 Side : {-1,1})
        for (int32 Panel=0; Panel<12; ++Panel)
        {
            const float X=-1650.f+Panel*300.f;
            const FRotator Facing(0,Side<0 ? 180.f : 0.f,0);
            Module(FString::Printf(TEXT("FactoryLower%d_%d"),Side,Panel),FactoryWall.Object,FVector(X,Side*1160,150),Facing);
            Module(FString::Printf(TEXT("FactoryUpper%d_%d"),Side,Panel),FactoryWindowWall.Object,FVector(X,Side*1160,450),Facing);
            Module(FString::Printf(TEXT("FactoryWindow%d_%d"),Side,Panel),FactoryWindow.Object,FVector(X,Side*1145,499),Facing);
        }
    // Paired cover footprints remain symmetric; the central test lane stays open.
    for (int32 Side : {-1,1})
        for (int32 Piece=0; Piece<2; ++Piece)
            Module(FString::Printf(TEXT("Barrier%d_%d"),Side,Piece),RoadBarrier.Object,FVector(Side*250.f+(Piece-.5f)*153.f,Side*250.f,42),FRotator(0,Side<0 ? 0.f : 180.f,0),true);
    Block(TEXT("CoverA"),FVector(-950,300,65),FVector(100,450,130),Metal.Object,Steel,true,2);
    Block(TEXT("CoverB"),FVector(950,-300,65),FVector(100,450,130),Metal.Object,Steel,true,2);
    Block(TEXT("LowA"),FVector(-600,-700,50),FVector(250,150,100),Wood.Object,FLinearColor(.72f,.54f,.34f),true,2);
    Block(TEXT("LowB"),FVector(600,700,50),FVector(250,150,100),Wood.Object,FLinearColor(.72f,.54f,.34f),true,2);
    for(int32 Side : {-1,1})
    {
        const FString S=Side<0 ? TEXT("A") : TEXT("B");
        const FLinearColor Team=Side<0 ? FLinearColor(.08f,.35f,.56f) : FLinearColor(.6f,.16f,.09f);
        const FLinearColor ContainerColor=Side<0 ? FLinearColor(.12f,.15f,.12f) : FLinearColor(.22f,.16f,.1f);
        const FVector Container(Side*1000.f,Side*850.f,140.f);
        Block(TEXT("Container")+S,Container,FVector(600,260,280),Rust.Object,FLinearColor(.72f,.78f,.70f),true,3);
        for(int32 Rib=0; Rib<13; ++Rib)
            Block(FString::Printf(TEXT("Rib%s%d"),*S,Rib),Container+FVector(-270+Rib*45,-Side*133,0),FVector(8,8,260),Paint.Object,ContainerColor*.65f,false);
        for(int32 End : {-1,1})
            Block(FString::Printf(TEXT("ContainerPost%s%d"),*S,End),Container+FVector(End*285,-Side*135,0),FVector(18,12,280),Metal.Object,Steel,false);
        Block(TEXT("ContainerRail")+S,Container+FVector(0,-Side*135,135),FVector(600,14,12),Metal.Object,Steel,false);
        // Real scanned crates retain a paired layout and use their own box collisions.
        for(int32 Row : {-1,1})
            for(int32 Layer=0; Layer<3; ++Layer)
                Prop(FString::Printf(TEXT("MilitaryCrate%s_%d_%d"),*S,Row,Layer),MilitaryCrate.Object,
                    FVector(Side*450,Side*900+Row*35,Layer*59.5f),1.28f,Side<0 ? 0.f : 180.f);
        for(int32 Drum=0; Drum<3; ++Drum)
            Prop(FString::Printf(TEXT("Barrel%s_%d"),*S,Drum),Barrel.Object,
                FVector(Side*(1400+Drum*80),Side*1000,0),1.f,Side*20.f+Drum*35.f);
        Module(TEXT("FactoryGarage")+S,FactoryGarage.Object,FVector(Side*1750,Side*650,160),FRotator(0,Side<0 ? 90.f : -90.f,0));
        Module(TEXT("StorageCart")+S,StorageCart.Object,FVector(Side*900,-Side*900,69),FRotator(0,Side<0 ? 0.f : 180.f,0),true);
        // Mirrored stairs and loading platforms add readable height variation.
        for (int32 Step=0; Step<10; ++Step)
        {
            const float Height=(Step+1)*18.f;
            Block(FString::Printf(TEXT("Step%s%d"),*S,Step),FVector(Side*(1120+Step*35),Side*450,Height*.5f),FVector(35,220,Height),Plaster.Object,FLinearColor::White,true,1);
        }
        Block(TEXT("LoadingPlatform")+S,FVector(Side*1545,Side*450,90),FVector(220,350,180),Plaster.Object,FLinearColor::White,true,2);
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
        Block(FString::Printf(TEXT("Fixture%d"),Beam),FVector(X,0,660),FVector(24,300,14),Metal.Object,Steel,false);
        Block(FString::Printf(TEXT("FixtureBulb%d"),Beam),FVector(X,0,651),FVector(16,270,3),Glow.Object,FLinearColor::White,false);
        // I-beam flanges and alternating diagonal roof braces give the rafters
        // a structural silhouette while staying above the playable space.
        for(int32 Flange : {-1,1})
            Block(FString::Printf(TEXT("RafterFlange%d_%d"),Beam,Flange),FVector(X,0,700+Flange*24),FVector(70,2400,6),Metal.Object,Steel,false);
        for(int32 Side : {-1,1})
        {
            auto* Brace=Block(FString::Printf(TEXT("RoofBrace%d_%d"),Beam,Side),FVector(X,Side*520,625),FVector(18,1070,16),Rust.Object,FLinearColor(.8f,.8f,.72f),false,3);
            Brace->SetRelativeRotation(FRotator(0,0,Side*8.f));
        }
    }
    // Licensed Factory meshes retain their original materials. Services and
    // shelves stay near the perimeter; the central duel lane remains clear.
    for(int32 Side : {-1,1})
    {
        const FRotator Facing(0,Side<0 ? 0.f : 180.f,0);
        for(int32 Bay=0;Bay<2;++Bay)
            FactoryProp(FString::Printf(TEXT("FactoryShelf%d_%d"),Side,Bay),Shelf.Object,FVector(Side*(350+Bay*250),-Side*1040,165),FVector(180,100,330),Facing,true);
        FactoryProp(FString::Printf(TEXT("FactoryPanel%d"),Side),ElectricalPanel.Object,FVector(Side*1200,-Side*1110,235),FVector(100,45,150),Facing,true);
        FactoryProp(FString::Printf(TEXT("FactorySwitchbox%d"),Side),Switchbox.Object,FVector(Side*1550,-Side*1120,180),FVector(120,35,170),FRotator(0,Side<0 ? 180.f : 0.f,0),true);
        FactoryProp(FString::Printf(TEXT("FactoryVent%d"),Side),Vent.Object,FVector(Side*1200,-Side*1110,475),FVector(120,25,120),Facing);
        for(int32 Section=0;Section<3;++Section)
            FactoryProp(FString::Printf(TEXT("FactoryPipe%d_%d"),Side,Section),Pipe.Object,FVector(-1000+Section*700,-Side*1100,550),FVector(32,700,32),FRotator(0,90,0));
        FactoryProp(FString::Printf(TEXT("FactoryElbow%d"),Side),Elbow.Object,FVector(Side*1420,-Side*1100,515),FVector(32,100,100),FRotator(0,Side*90,0));
        for(int32 Fixture=0;Fixture<3;++Fixture)
            FactoryProp(FString::Printf(TEXT("FactoryLamp%d_%d"),Side,Fixture),Lamp.Object,FVector(-1200+Fixture*1200,-Side*1080,610),FVector(35,35,40),FRotator::ZeroRotator);
        FactoryProp(FString::Printf(TEXT("FactoryRoofFrame%d"),Side),RoofFrame.Object,FVector(Side*800,0,660),FVector(1200,2200,180),FRotator::ZeroRotator);
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
    // Small layered meshes work consistently with the ES3.1 renderer. They do
    // not participate in collision or the authoritative shooting trace.
    auto Wear=[this](FString Name,UMaterialInterface* Material,FVector Position,FVector Scale,FRotator Rotation)
    {
        auto* Mesh=CreateDefaultSubobject<UStaticMeshComponent>(*Name);
        Mesh->SetupAttachment(GetRootComponent()); Mesh->SetStaticMesh(Plane.Object); Mesh->SetMaterial(0,Material);
        Mesh->SetRelativeLocation(Position); Mesh->SetRelativeScale3D(Scale); Mesh->SetRelativeRotation(Rotation);
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); Mesh->SetCastShadow(false); Mesh->SetCullDistance(3500.f);
    };
    for(int32 Side : {-1,1})
    {
        Wear(FString::Printf(TEXT("OilWear%d"),Side),Oil.Object,FVector(Side*1080,Side*660,.8f),FVector(2.7f,1.6f,1),FRotator(0,Side*22,0));
        Wear(FString::Printf(TEXT("CartOil%d"),Side),Oil.Object,FVector(Side*900,-Side*850,.8f),FVector(1.8f,1.2f,1),FRotator(0,Side*45,0));
        for(int32 Patch=0;Patch<4;++Patch)
        {
            Wear(FString::Printf(TEXT("WallDust%d_%d"),Side,Patch),Dust.Object,FVector(-1250+Patch*800,Side*1128,38),FVector(5,1.2f,1),FRotator(0,0,90));
            Wear(FString::Printf(TEXT("FloorDust%d_%d"),Side,Patch),Dust.Object,FVector(-1250+Patch*800,Side*1050,.9f),FVector(5,2,1),FRotator::ZeroRotator);
        }
        for(int32 Segment=0;Segment<4;++Segment)
            Wear(FString::Printf(TEXT("FloorCrack%d_%d"),Side,Segment),Crack.Object,FVector(Side*(580+Segment*25),Side*(120+Segment*45),1.1f),FVector(.08f,.8f,1),FRotator(0,Side*(20+Segment*14),0));
        for(int32 Stripe=0;Stripe<12;++Stripe)
        {
            auto* Mark=Block(FString::Printf(TEXT("LoadingWarning%d_%d"),Side,Stripe),FVector(Side*1545,Side*450-165+Stripe*30,181),FVector(18,24,1),Paint.Object,Stripe%2 ? FLinearColor(.035f,.035f,.025f) : Yellow,false);
            Mark->SetCastShadow(false);
        }
        Label(Side<0 ? TEXT("StoreSignA") : TEXT("StoreSignB"),TEXT("STORAGE / KEEP CLEAR"),FVector(Side*900,-Side*1125,365),FRotator(0,Side<0 ? -90.f : 90.f,0),FColor(224,206,155),34);
    }
    auto* Dome=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SkyDome"));
    Dome->SetupAttachment(GetRootComponent()); Dome->SetStaticMesh(Sphere.Object); Dome->SetMaterial(0,IndustrialSky.Succeeded() ? IndustrialSky.Object : Sky.Object);
    Dome->SetRelativeScale3D(FVector(200)); Dome->SetCollisionEnabled(ECollisionEnabled::NoCollision); Dome->SetCastShadow(false);
    auto* Sun=CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
    Sun->SetMobility(EComponentMobility::Movable);
    Sun->SetupAttachment(GetRootComponent()); Sun->SetRelativeRotation(FRotator(-38,-90,0));
    Sun->SetIntensity(2.8f); Sun->SetLightColor(FLinearColor(1.f,.88f,.73f));
    Sun->DynamicShadowDistanceMovableLight=4500.f; Sun->DynamicShadowCascades=2;
    auto* Ambient=CreateDefaultSubobject<USkyLightComponent>(TEXT("Ambient"));
    Ambient->SetMobility(EComponentMobility::Movable);
    Ambient->SetupAttachment(GetRootComponent()); Ambient->SetIntensity(1.1f);
    if(SkyCube.Succeeded()) { Ambient->SourceType=SLS_SpecifiedCubemap; Ambient->SetCubemap(SkyCube.Object); }
    Ambient->SkyDistanceThreshold=4000.f; Ambient->bLowerHemisphereIsBlack=false;
    auto* Grade=CreateDefaultSubobject<UPostProcessComponent>(TEXT("WarehouseGrade"));
    Grade->SetupAttachment(GetRootComponent()); Grade->bUnbound=true;
    Grade->Settings.bOverride_AutoExposureMethod=true; Grade->Settings.AutoExposureMethod=AEM_Manual;
    Grade->Settings.bOverride_AutoExposureApplyPhysicalCameraExposure=true; Grade->Settings.AutoExposureApplyPhysicalCameraExposure=false;
    Grade->Settings.bOverride_AutoExposureBias=true; Grade->Settings.AutoExposureBias=0.4f;
    Grade->Settings.bOverride_ScreenSpaceReflectionIntensity=true; Grade->Settings.ScreenSpaceReflectionIntensity=0.f;
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

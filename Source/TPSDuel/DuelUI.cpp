#include "DuelUI.h"
#include "DuelCharacter.h"
#include "DuelGameInstance.h"
#include "DuelGameState.h"
#include "DuelPlayerController.h"
#include "DuelPlayerState.h"
#include "DuelSettings.h"
#include "Engine/World.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Brushes/SlateColorBrush.h"
#include "Misc/ConfigCacheIni.h"


namespace
{
const FButtonStyle& DuelButtonStyle(bool Accent)
{
    static const FButtonStyle Dark=FButtonStyle()
        .SetNormal(FSlateColorBrush(FLinearColor(.09f,.12f,.14f,.9f)))
        .SetHovered(FSlateColorBrush(FLinearColor(.16f,.21f,.24f)))
        .SetPressed(FSlateColorBrush(FLinearColor(.04f,.07f,.09f)));
    static const FButtonStyle Gold=FButtonStyle()
        .SetNormal(FSlateColorBrush(FLinearColor(.48f,.28f,.06f,.95f)))
        .SetHovered(FSlateColorBrush(FLinearColor(.68f,.4f,.09f)))
        .SetPressed(FSlateColorBrush(FLinearColor(.3f,.17f,.03f)));
    return Accent ? Gold : Dark;
}
const FProgressBarStyle& DuelHealthStyle()
{
    static const FProgressBarStyle Style=FProgressBarStyle()
        .SetBackgroundImage(FSlateColorBrush(FLinearColor(.12f,.15f,.17f)))
        .SetFillImage(FSlateColorBrush(FLinearColor::White));
    return Style;
}
void Circle(FSlateWindowElementList& Draw,int32 Layer,const FGeometry& Geometry,FVector2D Center,float Radius,FLinearColor Color,float Thickness)
{
    TArray<FVector2D> Points;
    for(int32 I=0; I<=40; ++I)
    {
        const float A=2*PI*I/40;
        Points.Add(Center+FVector2D(FMath::Cos(A),FMath::Sin(A))*Radius);
    }
    FSlateDrawElement::MakeLines(Draw,Layer,Geometry.ToPaintGeometry(),Points,ESlateDrawEffect::None,Color,true,Thickness);
}
}
class SDuelCrosshair : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SDuelCrosshair){} SLATE_ARGUMENT(ADuelPlayerController*,Owner) SLATE_END_ARGS()
    void Construct(const FArguments& Args){ Owner=Args._Owner; }
    virtual FVector2D ComputeDesiredSize(float) const override {return FVector2D(70,70);}
    virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Draw,int32 Layer,const FWidgetStyle&,bool) const override
    {
        const auto* C=Owner.IsValid() ? Cast<ADuelCharacter>(Owner->GetPawn()) : nullptr;
        if(!C || !C->IsAlive()) return Layer;
        const FVector2D Center=G.GetLocalSize()*.5f;
        const float Gap=C->IsAiming() ? 4.f : 8.f+FMath::Clamp(C->GetVelocity().Size2D()/90.f,0.f,6.f);
        auto Line=[&](FVector2D A,FVector2D B,FLinearColor Color,float Width)
        {
            TArray<FVector2D> P; P.Add(Center+A); P.Add(Center+B);
            FSlateDrawElement::MakeLines(Draw,Layer,G.ToPaintGeometry(),P,ESlateDrawEffect::None,Color,true,Width);
        };
        for(int32 Sign : {-1,1})
        {
            Line(FVector2D(Sign*Gap,0),FVector2D(Sign*(Gap+8),0),FLinearColor::White,2);
            Line(FVector2D(0,Sign*Gap),FVector2D(0,Sign*(Gap+8)),FLinearColor::White,2);
        }
        if(Owner->GetWorld()->TimeSeconds-Owner->GetHitTime()<.15f)
            for(int32 X : {-1,1}) for(int32 Y : {-1,1})
                Line(FVector2D(X*4,Y*4),FVector2D(X*10,Y*10),FLinearColor(1,.68f,.25f),2);
        return Layer;
    }
private: TWeakObjectPtr<ADuelPlayerController> Owner;
};


// A separate touch surface captures only its own finger, allowing simultaneous
// movement, camera rotation and action buttons.
class SDuelTouchPad : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SDuelTouchPad) {} SLATE_ARGUMENT(ADuelPlayerController*, Owner) SLATE_ARGUMENT(bool, Movement) SLATE_END_ARGS()
    void Construct(const FArguments& Args) { Owner = Args._Owner; bMovement = Args._Movement; }
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(200, 200); }
    virtual FReply OnTouchStarted(const FGeometry& Geometry, const FPointerEvent& Event) override
    {
        if (!Owner.IsValid() || Owner->IsMenuVisible() || Finger != INDEX_NONE) return FReply::Unhandled();
        Finger = Event.GetPointerIndex();
        Origin = Previous = Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
        Current = Origin;
        return FReply::Handled().CaptureMouse(SharedThis(this));
    }
    virtual FReply OnTouchMoved(const FGeometry& Geometry, const FPointerEvent& Event) override
    {
        if (Event.GetPointerIndex() != Finger || !Owner.IsValid()) return FReply::Unhandled();
        Current = Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
        if (Owner->IsMenuVisible()) { Reset(); return FReply::Handled().ReleaseMouseCapture(); }
        if (bMovement)
        {
            const FVector2D Delta = (Current - Origin) / 65.f;
            FVector2D Move(Delta.X, -Delta.Y);
            if (Move.Size() > 1.f) Move.Normalize();
            if (Move.Size() < .12f) Move = FVector2D::ZeroVector;
            Owner->TouchMove(Move);
        }
        else Owner->TouchLook(Current - Previous);
        Previous = Current;
        return FReply::Handled();
    }
    virtual FReply OnTouchEnded(const FGeometry&, const FPointerEvent& Event) override
    {
        if (Event.GetPointerIndex() != Finger) return FReply::Unhandled();
        Reset();
        return FReply::Handled().ReleaseMouseCapture();
    }
    virtual void OnMouseCaptureLost(const FCaptureLostEvent&) override { Reset(); }
    virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&, FSlateWindowElementList& Draw,
        int32 Layer, const FWidgetStyle&, bool) const override
    {
        if (bMovement)
        {
            const FVector2D Center=Finger==INDEX_NONE ? FVector2D(130,Geometry.GetLocalSize().Y-180) : Origin;
            FVector2D Offset=Finger==INDEX_NONE ? FVector2D::ZeroVector : Current-Origin;
            if(Offset.Size()>65.f) Offset=Offset.GetSafeNormal()*65.f;
            Circle(Draw,Layer,Geometry,Center,65,FLinearColor(1,1,1,.28f),2);
            Circle(Draw,Layer+1,Geometry,Center+Offset,23,FLinearColor(1,1,1,.62f),3);
            return Layer+1;
        }
        return Layer;
    }
private:
    void Reset() { Finger = INDEX_NONE; if (bMovement && Owner.IsValid()) Owner->TouchMove(FVector2D::ZeroVector); }
    TWeakObjectPtr<ADuelPlayerController> Owner;
    int32 Finger = INDEX_NONE;
    bool bMovement = false;
    FVector2D Origin, Previous, Current;
};

void SDuelOverlay::Construct(const FArguments& Args)
{
    Owner=Args._Owner;
    FString InitialAddress=TEXT("192.168.1.100:7777"), Version;
    if(Owner.IsValid()) if(const auto* GI=Owner->GetGameInstance<UDuelGameInstance>()) InitialAddress=GI->LastEndpoint;
    GConfig->GetString(TEXT("/Script/EngineSettings.GeneralProjectSettings"),TEXT("ProjectVersion"),Version,GGameIni);
    const FLinearColor Ink(.025f,.035f,.045f,.88f), Muted(.62f,.68f,.7f), Gold(.95f,.65f,.24f);
    auto Text=[](const TCHAR* Label,int32 Size,FLinearColor Color=FLinearColor::White)
    {
        return SNew(STextBlock).Text(FText::FromString(Label)).Font(FCoreStyle::GetDefaultFontStyle("Bold",Size)).ColorAndOpacity(Color);
    };
    auto Button=[Text](const TCHAR* Label,FOnClicked Click,bool Accent=false)->TSharedRef<SWidget>
    {
        return SNew(SButton).ButtonStyle(&DuelButtonStyle(Accent)).ContentPadding(FMargin(18,14))
            .ForegroundColor(FLinearColor::White).HAlign(HAlign_Center).OnClicked(Click)[Text(Label,16)];
    };
    auto Action=[this,Text](const TCHAR* Label,void(ADuelPlayerController::*Press)(),void(ADuelPlayerController::*Release)(),bool Fire=false)->TSharedRef<SWidget>
    {
        return SNew(SBox).WidthOverride(Fire ? 108 : 82).HeightOverride(Fire ? 92 : 72)
        [SNew(SButton).ButtonStyle(&DuelButtonStyle(Fire)).HAlign(HAlign_Center).VAlign(VAlign_Center)
            .OnPressed_Lambda([this,Press](){ if(Owner.IsValid()) (Owner.Get()->*Press)(); })
            .OnReleased_Lambda([this,Release](){ if(Owner.IsValid() && Release) (Owner.Get()->*Release)(); })
            [Text(Label,Fire ? 21 : 13)]];
    };
    ChildSlot
    [SNew(SScaleBox).Stretch(EStretch::ScaleToFit)
        [SNew(SBox).WidthOverride(1280).HeightOverride(720)
            [SNew(SOverlay)
                + SOverlay::Slot()
                [SNew(SHorizontalBox).Visibility(this,&SDuelOverlay::TouchVisibility)
                    + SHorizontalBox::Slot().FillWidth(.4f)[SNew(SDuelTouchPad).Owner(Owner.Get()).Movement(true)]
                    + SHorizontalBox::Slot().FillWidth(.6f)[SNew(SDuelTouchPad).Owner(Owner.Get()).Movement(false)]]
                + SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(FMargin(28,24))
                [SNew(SVerticalBox).Visibility(this,&SDuelOverlay::GameVisibility)
                    + SVerticalBox::Slot().AutoHeight()[Text(TEXT("DEPOT 07"),20)]
                    + SVerticalBox::Slot().AutoHeight().Padding(FMargin(0,5))[Text(TEXT("WAREHOUSE / 1V1"),11,Muted)]]
                + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(FMargin(0,22))
                [SNew(SVerticalBox).Visibility_Lambda([this](){return Owner.IsValid() && !Owner->IsFrontEnd() ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed;})
                    + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
                    [SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().AutoWidth()
                        [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.035f,.2f,.32f,.9f)).Padding(FMargin(20,6))
                            [SNew(STextBlock).Text_Lambda([this](){return TeamScore(0);}).Font(FCoreStyle::GetDefaultFontStyle("Bold",30)).ColorAndOpacity(FLinearColor(.35f,.75f,1))]]
                        + SHorizontalBox::Slot().AutoWidth()
                        [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Ink).Padding(FMargin(20,12))[Text(TEXT("FIRST TO 3"),12,Gold)]]
                        + SHorizontalBox::Slot().AutoWidth()
                        [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.32f,.08f,.04f,.9f)).Padding(FMargin(20,6))
                            [SNew(STextBlock).Text_Lambda([this](){return TeamScore(1);}).Font(FCoreStyle::GetDefaultFontStyle("Bold",30)).ColorAndOpacity(FLinearColor(1,.5f,.35f))]]]
                    + SVerticalBox::Slot().AutoHeight().Padding(FMargin(0,9))
                    [SNew(SBox).WidthOverride(560)[SNew(STextBlock).Text(this,&SDuelOverlay::PhaseText).Font(FCoreStyle::GetDefaultFontStyle("Regular",14))
                        .ColorAndOpacity(FLinearColor::White).Justification(ETextJustify::Center).AutoWrapText(true)]]
                    + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
                    [SNew(SButton).ButtonStyle(&DuelButtonStyle(true)).Visibility(this,&SDuelOverlay::ReadyVisibility).ContentPadding(FMargin(24,12))
                        .OnClicked_Lambda([this](){ if(Owner.IsValid()) Owner->Ready(); return FReply::Handled(); })
                        [SNew(STextBlock).Text_Lambda([this](){const auto* PS=Owner.IsValid() ? Owner->GetPlayerState<ADuelPlayerState>() : nullptr;
                            return FText::FromString(PS && PS->bReady ? TEXT("READY / WAITING") : TEXT("PLAY AGAIN"));})
                            .Font(FCoreStyle::GetDefaultFontStyle("Bold",16)).ColorAndOpacity(FLinearColor::White)]]]
                + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
                [SNew(SDuelCrosshair).Owner(Owner.Get()).Visibility(this,&SDuelOverlay::GameVisibility)]
                + SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(FMargin(28,24))
                [SNew(SBox).Visibility_Lambda([this](){return Owner.IsValid() && !Owner->IsFrontEnd() ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed;})
                    [Button(TEXT("MENU  /  ESC"),FOnClicked::CreateLambda([this](){if(Owner.IsValid()) Owner->ToggleMenu(); return FReply::Handled();}))]]
                + SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(FMargin(28,24))
                [SNew(SBox).WidthOverride(240).Visibility(this,&SDuelOverlay::GameVisibility)
                    [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Ink).Padding(FMargin(16,12))
                        [SNew(SVerticalBox)
                            + SVerticalBox::Slot().AutoHeight()
                            [SNew(STextBlock).Text(this,&SDuelOverlay::HealthText).Font(FCoreStyle::GetDefaultFontStyle("Bold",19)).ColorAndOpacity(FLinearColor::White)]
                            + SVerticalBox::Slot().AutoHeight().Padding(FMargin(0,8,0,0))
                            [SNew(SBox).HeightOverride(5)[SNew(SProgressBar).Style(&DuelHealthStyle()).Percent_Lambda([this](){return HealthFraction();})
                                .FillColorAndOpacity_Lambda([this](){return HealthFraction()<.3f ? FLinearColor(1,.2f,.12f) : FLinearColor(.35f,.85f,.62f);})]]]]]
                + SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(FMargin(28,24))
                [SNew(SBox).WidthOverride(215).Visibility(this,&SDuelOverlay::GameVisibility)
                    [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Ink).Padding(FMargin(16,10))
                        [SNew(SVerticalBox)
                            + SVerticalBox::Slot().AutoHeight()[Text(TEXT("AR-01 / AUTO"),11,Muted)]
                            + SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(this,&SDuelOverlay::AmmoText).Font(FCoreStyle::GetDefaultFontStyle("Bold",30)).ColorAndOpacity(FLinearColor::White)]]]]
                + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(FMargin(0,28))
                [SNew(STextBlock).Visibility(this,&SDuelOverlay::GameVisibility).Text_Lambda([this](){return FText::FromString(Owner.IsValid() && Owner->WantsTouch() ?
                    TEXT("LEFT: MOVE   /   RIGHT: LOOK") : TEXT("WASD  MOVE    RMB  AIM    R  RELOAD    SPACE  JUMP"));})
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular",11)).ColorAndOpacity(Muted)]
                + SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(FMargin(28,126))
                [SNew(SHorizontalBox).Visibility(this,&SDuelOverlay::TouchVisibility)
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(5)[Action(TEXT("JUMP"),&ADuelPlayerController::JumpPressed,&ADuelPlayerController::JumpReleased)]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(5)[Action(TEXT("RELOAD"),&ADuelPlayerController::ReloadPressed,nullptr)]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(5)[Action(TEXT("AIM"),&ADuelPlayerController::AimPressed,&ADuelPlayerController::AimReleased)]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(5)[Action(TEXT("FIRE"),&ADuelPlayerController::FirePressed,&ADuelPlayerController::FireReleased,true)]]
                + SOverlay::Slot()
                [SNew(SBorder).Visibility(this,&SDuelOverlay::FrontVisibility).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                    .BorderBackgroundColor(FLinearColor(.015f,.025f,.03f,.7f)).Padding(FMargin(80,48))
                    [SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight()[Text(TEXT("TPS DUEL   /   FIELD OPERATIONS"),13,Gold)]
                        + SVerticalBox::Slot().FillHeight(1).VAlign(VAlign_Center)
                        [SNew(SHorizontalBox)
                            + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).Padding(FMargin(0,0,52,0))
                            [SNew(SVerticalBox)
                                + SVerticalBox::Slot().AutoHeight()[Text(TEXT("ONE ARENA."),54)]
                                + SVerticalBox::Slot().AutoHeight()[Text(TEXT("TWO RIVALS."),54,Gold)]
                                + SVerticalBox::Slot().AutoHeight().Padding(FMargin(0,24,0,5))[Text(TEXT("DEPOT 07 / INDUSTRIAL WAREHOUSE"),15)]
                                + SVerticalBox::Slot().AutoHeight()[Text(TEXT("1V1   |   FIRST TO 3   |   PC + ANDROID"),12,Muted)]]
                            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                            [SNew(SBox).WidthOverride(370)
                                [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Ink).Padding(28)
                                    [SNew(SVerticalBox)
                                        + SVerticalBox::Slot().AutoHeight()[Text(TEXT("LOCAL MATCH"),23)]
                                        + SVerticalBox::Slot().AutoHeight().Padding(FMargin(0,8,0,20))[Text(TEXT("CONNECT BOTH DEVICES TO THE SAME WI-FI"),10,Muted)]
                                        + SVerticalBox::Slot().AutoHeight()[Button(TEXT("HOST MATCH"),FOnClicked::CreateLambda([this](){if(Owner.IsValid()) Owner->Host(); return FReply::Handled();}),true)]
                                        + SVerticalBox::Slot().AutoHeight().Padding(FMargin(0,24,0,8))[Text(TEXT("SERVER ADDRESS / IPV4 : PORT"),10,Muted)]
                                        + SVerticalBox::Slot().AutoHeight()
                                        [SAssignNew(Address,SEditableTextBox).Text(FText::FromString(InitialAddress)).Font(FCoreStyle::GetDefaultFontStyle("Regular",18))
                                            .HintText(FText::FromString(TEXT("192.168.1.100:7777")))]
                                        + SVerticalBox::Slot().AutoHeight().Padding(FMargin(0,12,0,0))[Button(TEXT("JOIN MATCH"),FOnClicked::CreateLambda([this](){if(Owner.IsValid()) Owner->Join(Address->GetText().ToString()); return FReply::Handled();}))]
                                        + SVerticalBox::Slot().AutoHeight().Padding(FMargin(0,15,0,0))[SNew(STextBlock).Text(this,&SDuelOverlay::ConnectionText).AutoWrapText(true)
                                            .Font(FCoreStyle::GetDefaultFontStyle("Regular",12)).ColorAndOpacity(Gold)]]]]]
                        + SVerticalBox::Slot().AutoHeight()
                        [SNew(SHorizontalBox)
                            + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[Text(*(TEXT("BUILD ")+Version+TEXT("   /   LAN TRAINING")),11,Muted)]
                            + SHorizontalBox::Slot().AutoWidth()[Button(TEXT("QUIT"),FOnClicked::CreateLambda([this](){if(Owner.IsValid()) UKismetSystemLibrary::QuitGame(Owner.Get(),Owner.Get(),EQuitPreference::Quit,false); return FReply::Handled();}))]]]]
                + SOverlay::Slot()
                [SNew(SBorder).Visibility(this,&SDuelOverlay::MenuVisibility).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                    .BorderBackgroundColor(FLinearColor(0,0,0,.55f)).HAlign(HAlign_Center).VAlign(VAlign_Center)
                    [SNew(SBox).WidthOverride(360)[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Ink).Padding(28)
                        [SNew(SVerticalBox)
                            + SVerticalBox::Slot().AutoHeight().Padding(FMargin(0,0,0,20))[Text(TEXT("MATCH MENU"),26)]
                            + SVerticalBox::Slot().AutoHeight()[Button(TEXT("RESUME"),FOnClicked::CreateLambda([this](){if(Owner.IsValid()) Owner->ToggleMenu(); return FReply::Handled();}),true)]
                            + SVerticalBox::Slot().AutoHeight().Padding(FMargin(0,12))[Button(TEXT("LEAVE MATCH"),FOnClicked::CreateLambda([this](){if(Owner.IsValid()) Owner->Leave(); return FReply::Handled();}))]
                            + SVerticalBox::Slot().AutoHeight()[Text(TEXT("THE ONLINE MATCH CONTINUES"),11,Muted)]]]]]
            ]
        ]
    ];
}
EVisibility SDuelOverlay::FrontVisibility() const { return Owner.IsValid() && Owner->IsFrontEnd() ? EVisibility::Visible : EVisibility::Collapsed; }
EVisibility SDuelOverlay::GameVisibility() const { return Owner.IsValid() && !Owner->IsFrontEnd() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; }
EVisibility SDuelOverlay::MenuVisibility() const { return Owner.IsValid() && !Owner->IsFrontEnd() && Owner->IsMenuVisible() ? EVisibility::Visible : EVisibility::Collapsed; }
EVisibility SDuelOverlay::TouchVisibility() const { return Owner.IsValid() && Owner->WantsTouch() && !Owner->IsMenuVisible() ? EVisibility::Visible : EVisibility::Collapsed; }
EVisibility SDuelOverlay::ReadyVisibility() const
{
    const ADuelGameState* State = Owner.IsValid() ? Owner->GetWorld()->GetGameState<ADuelGameState>() : nullptr;
    return State && State->Phase == EDuelPhase::Finished ? EVisibility::Visible : EVisibility::Collapsed;
}
FText SDuelOverlay::TeamScore(int32 Slot) const
{
    int32 Kills=0;
    const auto* State=Owner.IsValid() ? Owner->GetWorld()->GetGameState<ADuelGameState>() : nullptr;
    if(State) for(const auto* Player : State->PlayerArray)
        if(const auto* PS=Cast<ADuelPlayerState>(Player)) if(PS->Slot==Slot) Kills=PS->Kills;
    return FText::FromString(FString::Printf(TEXT("%s  %02d"),Slot==0 ? TEXT("BLUE") : TEXT("RED"),Kills));
}
float SDuelOverlay::HealthFraction() const
{
    const auto* C=Owner.IsValid() ? Cast<ADuelCharacter>(Owner->GetPawn()) : nullptr;
    return C ? FMath::Clamp(C->GetHealth()/FMath::Max(1.f,GetDefault<UDuelSettings>()->MaxHealth),0.f,1.f) : 0.f;
}
FText SDuelOverlay::HealthText() const
{
    const auto* C=Owner.IsValid() ? Cast<ADuelCharacter>(Owner->GetPawn()) : nullptr;
    if(!C) return FText::FromString(TEXT("DEPLOYING..."));
    if(!C->IsAlive()) return FText::FromString(TEXT("DOWN / RESPAWNING"));
    return FText::FromString(FString::Printf(TEXT("%03d  /  %s"),FMath::CeilToInt(C->GetHealth()),C->IsProtected() ? TEXT("SHIELD") : TEXT("HEALTH")));
}
FText SDuelOverlay::AmmoText() const
{
    const auto* C=Owner.IsValid() ? Cast<ADuelCharacter>(Owner->GetPawn()) : nullptr;
    if(C && C->IsReloading()) return FText::FromString(TEXT("RELOADING"));
    return FText::FromString(FString::Printf(TEXT("%02d / %d"),C ? C->GetAmmo() : 0,GetDefault<UDuelSettings>()->MagazineCapacity));
}
FText SDuelOverlay::PhaseText() const
{
    const ADuelGameState* State = Owner.IsValid() ? Owner->GetWorld()->GetGameState<ADuelGameState>() : nullptr;
    if (!State) return FText::FromString(TEXT("Loading match..."));
    if (State->Phase == EDuelPhase::Countdown) return FText::FromString(FString::Printf(TEXT("STARTING IN %d"), State->SecondsLeft()));
    if (State->Phase == EDuelPhase::Finished)
        return FText::FromString(FString::Printf(TEXT("%s WINS / %s"), State->WinnerSlot == 0 ? TEXT("BLUE") : TEXT("RED"), *State->Status));
    return FText::FromString(State->Status);
}
FText SDuelOverlay::ConnectionText() const
{
    const UDuelGameInstance* GI = Owner.IsValid() ? Owner->GetGameInstance<UDuelGameInstance>() : nullptr;
    return FText::FromString(GI ? GI->ConnectionStatus : FString());
}

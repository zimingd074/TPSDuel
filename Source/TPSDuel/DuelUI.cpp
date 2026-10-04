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
            const FVector2D Delta = (Current - Origin) / 80.f;
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
        if (bMovement && Finger != INDEX_NONE)
        {
            FVector2D Offset = Current - Origin;
            if (Offset.Size() > 80.f) Offset = Offset.GetSafeNormal() * 80.f;
            const FVector2D Stick = Origin + Offset;
            FSlateDrawElement::MakeBox(Draw, Layer, Geometry.ToPaintGeometry(Origin - FVector2D(80, 80), FVector2D(160, 160)),
                FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, FLinearColor(.1f, .4f, .6f, .22f));
            FSlateDrawElement::MakeBox(Draw, Layer + 1, Geometry.ToPaintGeometry(Stick - FVector2D(20, 20), FVector2D(40, 40)),
                FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, FLinearColor(.2f, .8f, 1.f, .7f));
            return Layer + 1;
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
    Owner = Args._Owner;
    FString InitialAddress = TEXT("192.168.1.100:7777");
    if (Owner.IsValid()) if (const UDuelGameInstance* GI = Owner->GetGameInstance<UDuelGameInstance>()) InitialAddress = GI->LastEndpoint;
    const auto Font = FCoreStyle::GetDefaultFontStyle("Regular", 18);
    const auto TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 34);
    auto ActionButton = [this, Font](const TCHAR* Label, void (ADuelPlayerController::*Pressed)(), void (ADuelPlayerController::*Released)()) -> TSharedRef<SWidget>
    {
        return SNew(SBox).WidthOverride(100).HeightOverride(64)
        [ SNew(SButton).OnPressed_Lambda([this, Pressed]() { if (Owner.IsValid()) (Owner.Get()->*Pressed)(); })
            .OnReleased_Lambda([this, Released]() { if (Owner.IsValid() && Released) (Owner.Get()->*Released)(); })
            [ SNew(STextBlock).Text(FText::FromString(Label)).Font(Font).Justification(ETextJustify::Center) ] ];
    };
    ChildSlot
    [ SNew(SOverlay)
        + SOverlay::Slot()
        [ SNew(SHorizontalBox).Visibility(this, &SDuelOverlay::TouchVisibility)
            + SHorizontalBox::Slot().FillWidth(.4f) [ SNew(SDuelTouchPad).Owner(Owner.Get()).Movement(true) ]
            + SHorizontalBox::Slot().FillWidth(.6f) [ SNew(SDuelTouchPad).Owner(Owner.Get()).Movement(false) ] ]
        + SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(24)
        [ SNew(SVerticalBox).Visibility(this, &SDuelOverlay::GameVisibility)
            + SVerticalBox::Slot().AutoHeight() [ SNew(STextBlock).Text(this, &SDuelOverlay::ScoreText).Font(TitleFont) ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 12) [ SNew(STextBlock).Text(this, &SDuelOverlay::HealthText).Font(Font) ]
            + SVerticalBox::Slot().AutoHeight() [ SNew(STextBlock).Text_Lambda([this]() { return FText::FromString(Owner.IsValid() && Owner->WantsTouch() ? TEXT("Drag left: Move  |  Drag right: Look  |  Hold FIRE / AIM") : TEXT("WASD  Move  |  RMB  Aim  |  R  Reload  |  Esc  Menu")); }).Font(FCoreStyle::GetDefaultFontStyle("Regular", 12)) ] ]
        + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0, 28)
        [ SNew(SVerticalBox).Visibility_Lambda([this]() { return Owner.IsValid() && !Owner->IsFrontEnd() ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed; })
            + SVerticalBox::Slot().AutoHeight() [ SNew(STextBlock).Text(this, &SDuelOverlay::PhaseText).Font(Font).Justification(ETextJustify::Center) ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 14)
            [ SNew(SButton).Visibility(this, &SDuelOverlay::ReadyVisibility)
                .OnClicked_Lambda([this]() { if (Owner.IsValid()) Owner->Ready(); return FReply::Handled(); })
                [ SNew(STextBlock).Text_Lambda([this]() { const ADuelPlayerState* PS = Owner.IsValid() ? Owner->GetPlayerState<ADuelPlayerState>() : nullptr;
                    return FText::FromString(PS && PS->bReady ? TEXT("READY - waiting for opponent") : TEXT("READY FOR REMATCH")); }).Font(Font) ] ] ]
        + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
        [ SNew(STextBlock).Text_Lambda([this]() { return FText::FromString(Owner.IsValid() && Owner->GetWorld()->TimeSeconds - Owner->GetHitTime() < .15f ? TEXT("X") : TEXT("+")); })
            .Font(TitleFont).Visibility(this, &SDuelOverlay::GameVisibility) ]
        + SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(24)
        [ SNew(SButton).Visibility_Lambda([this]() { return Owner.IsValid() && !Owner->IsFrontEnd() ? EVisibility::Visible : EVisibility::Collapsed; })
            .OnClicked_Lambda([this]() { if (Owner.IsValid()) Owner->ToggleMenu(); return FReply::Handled(); })
            [ SNew(STextBlock).Text(FText::FromString(TEXT("MENU"))).Font(Font) ] ]
        + SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(24)
        [ SNew(SHorizontalBox).Visibility(this, &SDuelOverlay::TouchVisibility)
            + SHorizontalBox::Slot().AutoWidth().Padding(4) [ ActionButton(TEXT("JUMP"), &ADuelPlayerController::JumpPressed, &ADuelPlayerController::JumpReleased) ]
            + SHorizontalBox::Slot().AutoWidth().Padding(4) [ ActionButton(TEXT("RELOAD"), &ADuelPlayerController::ReloadPressed, nullptr) ]
            + SHorizontalBox::Slot().AutoWidth().Padding(4) [ ActionButton(TEXT("AIM"), &ADuelPlayerController::AimPressed, &ADuelPlayerController::AimReleased) ]
            + SHorizontalBox::Slot().AutoWidth().Padding(4) [ ActionButton(TEXT("FIRE"), &ADuelPlayerController::FirePressed, &ADuelPlayerController::FireReleased) ] ]
        + SOverlay::Slot()
        [ SNew(SBorder).Visibility(this, &SDuelOverlay::FrontVisibility).BorderBackgroundColor(FLinearColor(.015f, .03f, .06f, 1.f)).HAlign(HAlign_Center).VAlign(VAlign_Center)
            [ SNew(SBox).WidthOverride(460)
                [ SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 14) [ SNew(STextBlock).Text(FText::FromString(TEXT("TPS DUEL"))).Font(TitleFont) ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 8) [ SNew(STextBlock).Text(FText::FromString(TEXT("1v1 LAN / First to 3 kills"))).Font(Font) ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 12)
                    [ SNew(SButton).OnClicked_Lambda([this]() { if (Owner.IsValid()) Owner->Host(); return FReply::Handled(); })
                        [ SNew(STextBlock).Text(FText::FromString(TEXT("HOST MATCH"))).Font(Font) ] ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 8)
                    [ SAssignNew(Address, SEditableTextBox).Text(FText::FromString(InitialAddress)).Font(Font).HintText(FText::FromString(TEXT("PC IPv4:port"))) ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 8)
                    [ SNew(SButton).OnClicked_Lambda([this]() { if (Owner.IsValid()) Owner->Join(Address->GetText().ToString()); return FReply::Handled(); })
                        [ SNew(STextBlock).Text(FText::FromString(TEXT("JOIN MATCH"))).Font(Font) ] ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 12)
                    [ SNew(STextBlock).Text(this, &SDuelOverlay::ConnectionText).Font(Font).AutoWrapText(true) ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 8)
                    [ SNew(SButton).OnClicked_Lambda([this]() { if (Owner.IsValid()) UKismetSystemLibrary::QuitGame(Owner.Get(), Owner.Get(), EQuitPreference::Quit, false); return FReply::Handled(); })
                        [ SNew(STextBlock).Text(FText::FromString(TEXT("QUIT"))).Font(Font) ] ] ] ] ]
        + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
        [ SNew(SBorder).Visibility(this, &SDuelOverlay::MenuVisibility).Padding(24)
            [ SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(8)
                [ SNew(SButton).OnClicked_Lambda([this]() { if (Owner.IsValid()) Owner->ToggleMenu(); return FReply::Handled(); })
                    [ SNew(STextBlock).Text(FText::FromString(TEXT("RESUME"))).Font(Font) ] ]
                + SVerticalBox::Slot().AutoHeight().Padding(8)
                [ SNew(SButton).OnClicked_Lambda([this]() { if (Owner.IsValid()) Owner->Leave(); return FReply::Handled(); })
                    [ SNew(STextBlock).Text(FText::FromString(TEXT("LEAVE MATCH"))).Font(Font) ] ] ] ]
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
FText SDuelOverlay::ScoreText() const
{
    int32 Blue = 0, Red = 0;
    const ADuelGameState* State = Owner.IsValid() ? Owner->GetWorld()->GetGameState<ADuelGameState>() : nullptr;
    if (State) for (const APlayerState* Player : State->PlayerArray)
        if (const ADuelPlayerState* PS = Cast<ADuelPlayerState>(Player))
        { if (PS->Slot == 0) Blue = PS->Kills; else if (PS->Slot == 1) Red = PS->Kills; }
    return FText::FromString(FString::Printf(TEXT("BLUE  %d  :  %d  RED"), Blue, Red));
}
FText SDuelOverlay::HealthText() const
{
    const ADuelCharacter* Character = Owner.IsValid() ? Cast<ADuelCharacter>(Owner->GetPawn()) : nullptr;
    const ADuelPlayerState* PS = Owner.IsValid() ? Owner->GetPlayerState<ADuelPlayerState>() : nullptr;
    if (!Character) return FText::FromString(TEXT("Loading player..."));
    if (!Character->IsAlive()) return FText::FromString(TEXT("DOWN / waiting for respawn"));
    return FText::FromString(FString::Printf(TEXT("YOU: %s   HP %d   AMMO %d/%d  %s%s"), PS && PS->Slot == 0 ? TEXT("BLUE") : TEXT("RED"),
        FMath::CeilToInt(Character->GetHealth()), Character->GetAmmo(), GetDefault<UDuelSettings>()->MagazineCapacity,
        Character->IsReloading() ? TEXT("RELOADING ") : TEXT(""), Character->IsProtected() ? TEXT("SHIELD") : TEXT("")));
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

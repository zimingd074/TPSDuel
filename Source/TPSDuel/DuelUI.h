#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
class ADuelPlayerController;
class SEditableTextBox;

class SDuelOverlay : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SDuelOverlay) {} SLATE_ARGUMENT(ADuelPlayerController*, Owner) SLATE_END_ARGS()
    void Construct(const FArguments& Args);
private:
    FText ScoreText() const;
    FText HealthText() const;
    FText PhaseText() const;
    FText ConnectionText() const;
    EVisibility FrontVisibility() const;
    EVisibility GameVisibility() const;
    EVisibility MenuVisibility() const;
    EVisibility TouchVisibility() const;
    EVisibility ReadyVisibility() const;
    TWeakObjectPtr<ADuelPlayerController> Owner;
    TSharedPtr<SEditableTextBox> Address;
};

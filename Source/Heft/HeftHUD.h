#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "HeftHUD.generated.h"

UCLASS()
class AHeftHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	void Toggle() { bHudVisible = !bHudVisible; }

protected:
	float FrameMs = 16.f;
	bool bHudVisible = false;
};

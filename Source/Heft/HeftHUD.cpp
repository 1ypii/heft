#include "HeftHUD.h"
#include "HeftCharacter.h"
#include "HeftGameMode.h"
#include "HeftGrabComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/App.h"

void AHeftHUD::DrawHUD()
{
	Super::DrawHUD();

	const AHeftCharacter* Player = Cast<AHeftCharacter>(GetOwningPawn());
	const AHeftGameMode* GameMode = GetWorld()->GetAuthGameMode<AHeftGameMode>();
	if (!bHudVisible || !Canvas || !Player || !GameMode)
		return;

	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	const float CX = W * 0.5f;
	const float CY = H * 0.5f;
	const float Pad = 32.f;
	const float Line = 26.f;
	const float Scale = 1.3f;
	const FLinearColor White(1.f, 1.f, 1.f, 0.92f);
	const FLinearColor Dim(1.f, 1.f, 1.f, 0.45f);
	UFont* Font = GEngine->GetMediumFont();

	auto Text = [&](const FString& Str, float X, float Y, float Align, const FLinearColor& Color)
	{
		float TW = 0.f;
		float TH = 0.f;
		GetTextSize(Str, TW, TH, Font, Scale);
		DrawText(Str, Color, X - TW * Align, Y, Font, Scale);
	};

	DrawRect(White, CX - 2.f, CY - 2.f, 4.f, 4.f);

	float Y = Pad;
	for (int32 i = 0; i < GameMode->GetScenarioCount(); i++)
	{
		Text(FString::Printf(TEXT("%d  %s"), i + 1, GameMode->GetScenarioName(i)), Pad, Y, 0.f, i == GameMode->Current ? White : Dim);
		Y += Line;
	}

	const float BarX = W - Pad - 160.f;
	Y = Pad;
	for (int32 i = 0; i < GameMode->Params.Num(); i++)
	{
		const FHeftParam& Param = GameMode->Params[i];
		const FLinearColor& Color = i == GameMode->Selected ? White : Dim;
		const float Fill = (Param.Value - Param.Min) / (Param.Max - Param.Min);

		Text(FString::Printf(TEXT("%s  %.2f"), Param.Name, Param.Value), BarX - 16.f, Y, 1.f, Color);
		DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.15f), BarX, Y + 10.f, 160.f, 6.f);
		DrawRect(Color, BarX, Y + 10.f, 160.f * Fill, 6.f);
		Y += Line;
	}
	Text(TEXT("arrows tweak   backspace reset"), W - Pad, Y + 4.f, 1.f, Dim);
	Text(FString::Printf(TEXT("%d particles"), GameMode->GetParticleCount()), W - Pad, Y + 4.f + Line, 1.f, Dim);
	Text(FString::Printf(TEXT("solver %.1f ms   surface %.1f ms"), GameMode->GetSolverMs(), GameMode->GetSurfaceMs()), W - Pad, Y + 4.f + Line * 2.f, 1.f, Dim);

	FrameMs = FMath::Lerp(FrameMs, static_cast<float>(FApp::GetDeltaTime()) * 1000.f, 0.08f);
	Text(FString::Printf(TEXT("%d fps"), FMath::RoundToInt(1000.f / FMath::Max(FrameMs, 0.1f))), W - Pad, H - Pad - Line, 1.f, Dim);

	Text(GameMode->GetHint(), CX, Pad, 0.5f, White);
	Text(TEXT("r reset   q shoot   g blast   e grab   click throw   h hide"), CX, H - Pad - Line, 0.5f, Dim);

	const UHeftGrabComponent* Grab = Player->GetGrab();
	if (!Grab->IsHolding() && Grab->FindParticle() != INDEX_NONE)
		Text(TEXT("e grab"), CX, CY + 36.f, 0.5f, White);
}

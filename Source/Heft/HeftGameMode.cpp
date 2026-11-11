#include "HeftGameMode.h"
#include "HeftBody.h"
#include "HeftCharacter.h"
#include "HeftHUD.h"
#include "HeftSim.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

enum
{
	GravityParam,
	BounceParam,
	FrictionParam,
	TimeParam
};

AHeftGameMode::AHeftGameMode()
{
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FloorFind(TEXT("/Game/Materials/MI_Floor.MI_Floor"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> WallFind(TEXT("/Game/Materials/MI_Wall.MI_Wall"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GlowFind(TEXT("/Game/Materials/M_Glow.M_Glow"));
	FloorMat = FloorFind.Object;
	WallMat = WallFind.Object;
	GlowMat = GlowFind.Object;

	PrimaryActorTick.bCanEverTick = true;

	DefaultPawnClass = AHeftCharacter::StaticClass();
	HUDClass = AHeftHUD::StaticClass();

	Params.Add({ TEXT("gravity"), 1.f, 0.f, 3.f, 0.1f, 1.f });
	Params.Add({ TEXT("bounce"), 0.3f, 0.f, 1.f, 0.05f, 0.3f });
	Params.Add({ TEXT("friction"), 0.7f, 0.f, 1.5f, 0.05f, 0.7f });
	Params.Add({ TEXT("time"), 1.f, 0.05f, 2.f, 0.05f, 1.f });
}

void AHeftGameMode::BeginPlay()
{
	Super::BeginPlay();

	AHeftBody* Floor = GetWorld()->SpawnActor<AHeftBody>(FVector(0.f, 0.f, -50.f), FRotator::ZeroRotator);
	Floor->Setup(FVector(16000.f, 16000.f, 100.f), FloorMat);

	ApplyParams();

	int32 First = 0;
	FParse::Value(FCommandLine::Get(), TEXT("scenario="), First);
	LoadScenario(GetScenarios().IsValidIndex(First) ? First : 0);
}

void AHeftGameMode::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	Spheres.Reset();
	if (const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		const FVector3f At(Player->GetActorLocation());
		Spheres.Add({ At + FVector3f(0.f, 0.f, 45.f), 38.f });
		Spheres.Add({ At - FVector3f(0.f, 0.f, 45.f), 38.f });
	}
}

void AHeftGameMode::Clear()
{
	GetWorldTimerManager().ClearTimer(PourTimer);
	PourLeft = 0;

	if (Sim)
		Sim->Destroy();
	Sim = nullptr;

	for (AHeftBody* Wall : Walls)
	{
		if (Wall)
			Wall->Destroy();
	}
	Walls.Reset();
}

void AHeftGameMode::LoadScenario(int32 Index)
{
	const TArray<FScenario>& All = GetScenarios();
	if (!All.IsValidIndex(Index))
		return;

	Clear();
	Current = Index;
	Sim = GetWorld()->SpawnActor<AHeftSim>(FVector::ZeroVector, FRotator::ZeroRotator);
	(this->*All[Index].Build)();
}

void AHeftGameMode::Reload()
{
	LoadScenario(Current);
}

void AHeftGameMode::Trigger()
{
	if (Sim && Sim->World.Num() < MaxParticles)
		(this->*GetScenarios()[Current].Trigger)();
}

int32 AHeftGameMode::GetScenarioCount() const
{
	return GetScenarios().Num();
}

const TCHAR* AHeftGameMode::GetScenarioName(int32 Index) const
{
	return GetScenarios()[Index].Name;
}

const TCHAR* AHeftGameMode::GetHint() const
{
	return GetScenarios()[Current].Hint;
}

int32 AHeftGameMode::GetParticleCount() const
{
	return Sim ? Sim->World.Num() : 0;
}

float AHeftGameMode::GetSolverMs() const
{
	return Sim ? Sim->World.StepMs : 0.f;
}

float AHeftGameMode::GetSurfaceMs() const
{
	return Sim ? Sim->Surface.BuildMs : 0.f;
}

void AHeftGameMode::SelectParam(int32 Dir)
{
	Selected = (Selected + Dir + Params.Num()) % Params.Num();
}

void AHeftGameMode::AdjustParam(int32 Dir)
{
	FHeftParam& Param = Params[Selected];
	Param.Value = FMath::Clamp(FMath::RoundToFloat((Param.Value + Dir * Param.Step) / Param.Step) * Param.Step, Param.Min, Param.Max);
	ApplyParams();
}

void AHeftGameMode::ResetParams()
{
	for (FHeftParam& Param : Params)
		Param.Value = Param.Base;

	ApplyParams();
}

void AHeftGameMode::ApplyParams()
{
	UWorld* World = GetWorld();

	AWorldSettings* Settings = World->GetWorldSettings();
	Settings->bGlobalGravitySet = true;
	Settings->GlobalGravityZ = -980.f * Params[GravityParam].Value;

	const float Speed = Params[TimeParam].Value;
	UGameplayStatics::SetGlobalTimeDilation(World, Speed);
	if (APawn* Player = UGameplayStatics::GetPlayerPawn(World, 0))
		Player->CustomTimeDilation = 1.f / Speed;
}

void AHeftGameMode::AddWall(const FVector& Center, const FVector& Size, bool bGlass)
{
	if (AHeftBody* Wall = GetWorld()->SpawnActor<AHeftBody>(Center, FRotator::ZeroRotator))
	{
		Wall->Setup(Size, WallMat);
		Wall->SetActorHiddenInGame(bGlass);
		Walls.Add(Wall);
	}

	Sim->AddWall(FVector3f(Center), FVector3f(Size));
}

void AHeftGameMode::AddTank(const FVector2D& Inner, float Height)
{
	const float Thick = 40.f;
	const float X = Inner.X * 0.5f + Thick * 0.5f;
	const float Y = Inner.Y * 0.5f + Thick * 0.5f;
	const float Z = Height * 0.5f;

	AddWall(FVector(-X, 0.f, Z), FVector(Thick, Inner.Y + Thick * 2.f, Height), true);
	AddWall(FVector(X, 0.f, Z), FVector(Thick, Inner.Y + Thick * 2.f, Height), false);
	AddWall(FVector(0.f, -Y, Z), FVector(Inner.X, Thick, Height), true);
	AddWall(FVector(0.f, Y, Z), FVector(Inner.X, Thick, Height), false);
}

void AHeftGameMode::Shoot(const FVector& From, const FVector& Dir)
{
	if (Sim && Sim->World.Num() < MaxParticles)
		Sim->AddBall(FVector3f(From + Dir * 90.f), 3, 0.1f, FVector3f(Dir) * 1900.f);
}

void AHeftGameMode::Explode(const FVector& Pos)
{
	if (Sim)
		Sim->World.Impulse(FVector3f(Pos), BlastRadius, BlastSpeed);

	if (AHeftBody* Burst = GetWorld()->SpawnActor<AHeftBody>(Pos, FRotator::ZeroRotator))
		Burst->Burst(BlastRadius * 0.5f, GlowMat);
}

void AHeftGameMode::PourTick()
{
	if (!Sim || PourLeft <= 0)
	{
		GetWorldTimerManager().ClearTimer(PourTimer);
		return;
	}

	const int32 Body = Sim->World.NewGroup();
	for (int32 X = -1; X <= 1; X++)
	{
		for (int32 Y = -1; Y <= 1; Y++)
		{
			const int32 At = Sim->World.AddParticle(FVector3f(X * FHeftWorld::FluidSpacing, Y * FHeftWorld::FluidSpacing, 560.f), 1.f, EHeftKind::Fluid, Body);
			Sim->World.Vel[At] = FVector3f(0.f, 0.f, -400.f);
			PourLeft--;
		}
	}
}

const TArray<AHeftGameMode::FScenario>& AHeftGameMode::GetScenarios()
{
	static const TArray<FScenario> All =
	{
		{ TEXT("dam break"), TEXT("f drops crates in the water"), &AHeftGameMode::BuildDam, &AHeftGameMode::DropCrates },
		{ TEXT("water on cloth"), TEXT("f pours water   shoot the cloth to tear it"), &AHeftGameMode::BuildClothPour, &AHeftGameMode::StartPour },
		{ TEXT("jelly pile"), TEXT("f drops another cube"), &AHeftGameMode::BuildJelly, &AHeftGameMode::DropJelly },
		{ TEXT("crate wall"), TEXT("f fires a heavy ball at it"), &AHeftGameMode::BuildCrates, &AHeftGameMode::SmashCrates },
	};
	return All;
}

void AHeftGameMode::BuildDam()
{
	AddTank(FVector2D(520.f, 312.f), 200.f);
	Sim->AddWater(FVector3f(-252.f, -149.f, 7.f), FIntVector(20, 23, 14));
}

void AHeftGameMode::DropCrates()
{
	for (int32 i = 0; i < 4; i++)
		Sim->AddCrate(FVector3f(FMath::FRandRange(-180.f, 180.f), FMath::FRandRange(-90.f, 90.f), 340.f + i * 80.f), 4, 2.5f);
}

void AHeftGameMode::BuildClothPour()
{
	Sim->AddCloth(FVector3f(-234.f, -234.f, 300.f), 40, 40);
}

void AHeftGameMode::StartPour()
{
	if (PourLeft > 0)
		return;

	PourLeft = 3600;
	GetWorldTimerManager().SetTimer(PourTimer, this, &AHeftGameMode::PourTick, 0.03f, true);
}

void AHeftGameMode::BuildJelly()
{
	for (int32 i = 0; i < 3; i++)
		Sim->AddJelly(FVector3f(i * 14.f, i * 10.f, 120.f + i * 190.f), 7);
}

void AHeftGameMode::DropJelly()
{
	Sim->AddJelly(FVector3f(FMath::FRandRange(-60.f, 60.f), FMath::FRandRange(-60.f, 60.f), 720.f), 7);
}

void AHeftGameMode::BuildCrates()
{
	for (int32 Level = 0; Level < 6; Level++)
	{
		for (int32 K = -1; K <= 1; K++)
			Sim->AddCrate(FVector3f(0.f, K * 49.f, 24.f + Level * 49.f), 4, 1.f);
	}
}

void AHeftGameMode::SmashCrates()
{
	Sim->AddBall(FVector3f(-800.f, FMath::FRandRange(-40.f, 40.f), 130.f), 7, 0.05f, FVector3f(1800.f, 0.f, 80.f));
}

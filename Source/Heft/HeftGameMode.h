#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "HeftWorld.h"
#include "HeftGameMode.generated.h"

class AHeftBody;
class AHeftSim;
class UMaterialInterface;

struct FHeftParam
{
	const TCHAR* Name;
	float Value;
	float Min;
	float Max;
	float Step;
	float Base;
};

UCLASS()
class AHeftGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AHeftGameMode();

	virtual void Tick(float DeltaTime) override;

	UFUNCTION(Exec)
	void LoadScenario(int32 Index);

	UFUNCTION(Exec)
	void Trigger();

	void Reload();
	void Shoot(const FVector& From, const FVector& Dir);
	void Explode(const FVector& Pos);

	void SelectParam(int32 Dir);
	void AdjustParam(int32 Dir);
	void ResetParams();

	int32 GetScenarioCount() const;
	const TCHAR* GetScenarioName(int32 Index) const;
	const TCHAR* GetHint() const;
	int32 GetParticleCount() const;
	float GetSolverMs() const;
	float GetSurfaceMs() const;
	float GetBounce() const { return Params[1].Value; }
	float GetFriction() const { return Params[2].Value; }
	AHeftSim* GetSim() const { return Sim; }

	TArray<FHeftParam> Params;
	TArray<FHeftSphere> Spheres;
	int32 Selected = 0;
	int32 Current = 0;

protected:
	virtual void BeginPlay() override;

	struct FScenario
	{
		const TCHAR* Name;
		const TCHAR* Hint;
		void (AHeftGameMode::*Build)();
		void (AHeftGameMode::*Trigger)();
	};

	static const TArray<FScenario>& GetScenarios();

	void ApplyParams();
	void Clear();

	void AddWall(const FVector& Center, const FVector& Size, bool bGlass);
	void AddTank(const FVector2D& Inner, float Height);
	void PourTick();

	void BuildDam();
	void BuildClothPour();
	void BuildJelly();
	void BuildCrates();

	void DropCrates();
	void StartPour();
	void DropJelly();
	void SmashCrates();

	UPROPERTY(EditAnywhere, Category = "Heft")
	float BlastRadius = 600.f;

	UPROPERTY(EditAnywhere, Category = "Heft")
	float BlastSpeed = 1800.f;

	UPROPERTY(EditAnywhere, Category = "Heft")
	int32 MaxParticles = 14000;

	UPROPERTY()
	TArray<TObjectPtr<AHeftBody>> Walls;

	UPROPERTY()
	TObjectPtr<AHeftSim> Sim;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> FloorMat;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> WallMat;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> GlowMat;

	FTimerHandle PourTimer;
	int32 PourLeft = 0;
};

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HeftGrabComponent.generated.h"

class AHeftSim;

UCLASS()
class UHeftGrabComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHeftGrabComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void Toggle();
	void Throw();
	void Release();
	void Reel(float Amount);

	int32 FindParticle() const;
	bool IsHolding() const { return Particle != INDEX_NONE; }

protected:
	AHeftSim* GetSim() const;

	UPROPERTY(EditAnywhere, Category = "Heft")
	float Reach = 380.f;

	UPROPERTY(EditAnywhere, Category = "Heft")
	float MinHold = 130.f;

	UPROPERTY(EditAnywhere, Category = "Heft")
	float MaxHold = 320.f;

	UPROPERTY(EditAnywhere, Category = "Heft")
	float ThrowSpeed = 1600.f;

	TWeakObjectPtr<AHeftSim> HeldSim;
	int32 Particle = INDEX_NONE;
	float HeldInvMass = 1.f;
	float HoldDist = 0.f;
};

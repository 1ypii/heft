#include "HeftGrabComponent.h"
#include "HeftGameMode.h"
#include "HeftSim.h"
#include "Engine/World.h"

UHeftGrabComponent::UHeftGrabComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

AHeftSim* UHeftGrabComponent::GetSim() const
{
	const AHeftGameMode* GameMode = GetWorld()->GetAuthGameMode<AHeftGameMode>();
	return GameMode ? GameMode->GetSim() : nullptr;
}

int32 UHeftGrabComponent::FindParticle() const
{
	const AHeftSim* Sim = GetSim();
	if (!Sim)
		return INDEX_NONE;

	FVector Eye;
	FRotator View;
	GetOwner()->GetActorEyesViewPoint(Eye, View);
	return Sim->World.Pick(FVector3f(Eye), FVector3f(View.Vector()), Reach);
}

void UHeftGrabComponent::Toggle()
{
	if (Particle != INDEX_NONE)
	{
		Release();
		return;
	}

	AHeftSim* Sim = GetSim();
	const int32 Found = FindParticle();
	if (!Sim || Found == INDEX_NONE)
		return;

	FVector Eye;
	FRotator View;
	GetOwner()->GetActorEyesViewPoint(Eye, View);

	HeldSim = Sim;
	Particle = Found;
	HeldInvMass = Sim->World.InvMass[Found];
	Sim->World.InvMass[Found] = 0.f;
	HoldDist = FMath::Clamp(FVector::Dist(Eye, FVector(Sim->World.Pos[Found])), MinHold, MaxHold);
	SetComponentTickEnabled(true);
}

void UHeftGrabComponent::Release()
{
	AHeftSim* Sim = HeldSim.Get();
	if (Sim && Sim->World.InvMass.IsValidIndex(Particle))
		Sim->World.InvMass[Particle] = HeldInvMass;

	HeldSim = nullptr;
	Particle = INDEX_NONE;
	SetComponentTickEnabled(false);
}

void UHeftGrabComponent::Throw()
{
	AHeftSim* Sim = HeldSim.Get();
	const int32 Index = Particle;
	if (Index == INDEX_NONE)
		return;

	FVector Eye;
	FRotator View;
	GetOwner()->GetActorEyesViewPoint(Eye, View);

	Release();
	if (Sim && Sim->World.Vel.IsValidIndex(Index))
		Sim->World.Vel[Index] = FVector3f(View.Vector()) * ThrowSpeed;
}

void UHeftGrabComponent::Reel(float Amount)
{
	if (Particle != INDEX_NONE)
		HoldDist = FMath::Clamp(HoldDist + Amount * 30.f, MinHold, MaxHold);
}

void UHeftGrabComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AHeftSim* Sim = HeldSim.Get();
	if (!Sim || !Sim->World.Pos.IsValidIndex(Particle))
	{
		Release();
		return;
	}

	FVector Eye;
	FRotator View;
	GetOwner()->GetActorEyesViewPoint(Eye, View);

	const FVector Target = Eye + View.Vector() * HoldDist;
	const FVector At = FMath::VInterpTo(FVector(Sim->World.Pos[Particle]), Target, DeltaTime, 18.f);
	Sim->World.Pos[Particle] = FVector3f(At);
	Sim->World.Old[Particle] = FVector3f(At);
}

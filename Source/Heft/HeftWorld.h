#pragma once

#include "CoreMinimal.h"

enum class EHeftKind : uint8
{
	Soft,
	Fluid,
	Rigid
};

struct FHeftLink
{
	int32 A;
	int32 B;
	float Rest;
	float Stiffness;
	float Tear;
	bool bBroken;
};

struct FHeftRigid
{
	int32 First;
	int32 Count;
	FVector3f Com;
	FQuat4f Rot;
	FVector3f Size;
	bool bRound;
};

struct FHeftBox
{
	FVector3f Center;
	FQuat4f Rot;
	FVector3f Extent;
};

struct FHeftSphere
{
	FVector3f Center;
	float Radius;
};

class FHeftWorld
{
public:
	FHeftWorld();

	int32 NewGroup() { return GroupCount++; }
	int32 AddParticle(const FVector3f& At, float Inv, EHeftKind Type, int32 Body);
	int32 AddLink(int32 A, int32 B, float Stiffness, float Tear);
	int32 AddRigid(const FVector3f& Center, const FIntVector& Cells, float Inv, bool bRound);

	void Step(float DeltaTime);
	void Impulse(const FVector3f& Origin, float Range, float Speed);
	int32 Pick(const FVector3f& Origin, const FVector3f& Dir, float Range) const;
	int32 Num() const { return Pos.Num(); }

	static constexpr float Radius = 6.f;
	static constexpr float Spacing = 12.f;
	static constexpr float FluidSpacing = 13.f;

	TArray<FVector3f> Pos;
	TArray<FVector3f> Old;
	TArray<FVector3f> Vel;
	TArray<FVector3f> Rest;
	TArray<float> InvMass;
	TArray<int32> Group;
	TArray<EHeftKind> Kind;
	TArray<uint8> Crowd;

	TArray<FHeftLink> Links;
	TArray<FHeftRigid> Rigids;
	TArray<FHeftBox> Boxes;
	TArray<FHeftSphere> Spheres;

	FVector3f Gravity = FVector3f(0.f, 0.f, -980.f);
	float Friction = 0.7f;
	float Bounce = 0.3f;
	float StepMs = 0.f;
	int32 Passes = 3;
	int32 FluidCount = 0;
	bool bTorn = false;

private:
	void BuildGrid();
	void FindNeighbors();
	void SolveFluid();
	void SolveContacts();
	void SolveStatics();
	void SolveLinks();
	void SolveRigids();
	void Smooth();

	uint32 Hash(const FIntVector& Cell) const;

	TArray<FVector3f> Delta;
	TArray<FVector3f> Push;
	TArray<int32> Hits;
	TArray<float> Lambda;

	TArray<FIntVector> CellOf;
	TArray<int32> CellStart;
	TArray<int32> CellFill;
	TArray<int32> Sorted;
	TArray<int32> Neigh;
	TArray<int32> NeighCount;

	float RestDensity = 1.f;
	int32 GroupCount = 0;
};

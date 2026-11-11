#include "HeftWorld.h"
#include "Async/ParallelFor.h"
#include "HAL/PlatformTime.h"

static constexpr float Kernel = 26.f;
static constexpr int32 MaxNeigh = 64;
static constexpr int32 TableSize = 1 << 17;
static constexpr int32 LinkSweeps = 4;
static constexpr float Softness = 0.3f;
static constexpr float SelfScale = 0.7f;
static constexpr float SpeedCap = 2000.f;
static constexpr float Viscosity = 0.05f;
static constexpr float FluidGrip = 0.05f;
static constexpr float MaxLift = 4.f;
static constexpr float PickRange = 26.f;

static float Poly(float Q)
{
	const float K = 1.f - Q * Q;
	return 1.5666814f * K * K * K;
}

static float Spiky(float Q)
{
	const float K = 1.f - Q;
	return -14.323945f * K * K;
}

static FQuat4f ExtractRotation(const FVector3f& A0, const FVector3f& A1, const FVector3f& A2, FQuat4f Q)
{
	for (int32 i = 0; i < 10; i++)
	{
		const FVector3f R0 = Q.GetAxisX();
		const FVector3f R1 = Q.GetAxisY();
		const FVector3f R2 = Q.GetAxisZ();

		const FVector3f Twist = FVector3f::CrossProduct(R0, A0) + FVector3f::CrossProduct(R1, A1) + FVector3f::CrossProduct(R2, A2);
		const float Scale = FMath::Abs(FVector3f::DotProduct(R0, A0) + FVector3f::DotProduct(R1, A1) + FVector3f::DotProduct(R2, A2)) + 1.e-9f;
		const FVector3f Omega = Twist / Scale;
		const float Angle = Omega.Size();
		if (Angle < 1.e-6f)
			break;

		Q = FQuat4f(Omega / Angle, Angle) * Q;
		Q.Normalize();
	}
	return Q;
}

static void Slide(FVector3f& At, const FVector3f& From, const FVector3f& Normal, float Depth, float Grip)
{
	const FVector3f Moved = At - From;
	const FVector3f Tang = Moved - Normal * FVector3f::DotProduct(Moved, Normal);
	const float Len = Tang.Size();
	if (Len < 1.e-6f)
		return;

	const float Limit = Grip * Depth;
	At -= Tang * (Len <= Limit ? 1.f : Limit / Len);
}

FHeftWorld::FHeftWorld()
{
	const float StepSize = FluidSpacing / Kernel;
	float Sum = 0.f;
	for (int32 X = -2; X <= 2; X++)
	{
		for (int32 Y = -2; Y <= 2; Y++)
		{
			for (int32 Z = -2; Z <= 2; Z++)
			{
				const float Q = FVector3f(X, Y, Z).Size() * StepSize;
				if (Q < 1.f)
					Sum += Poly(Q);
			}
		}
	}
	RestDensity = Sum;
}

int32 FHeftWorld::AddParticle(const FVector3f& At, float Inv, EHeftKind Type, int32 Body)
{
	Pos.Add(At);
	Old.Add(At);
	Vel.Add(FVector3f::ZeroVector);
	Rest.Add(FVector3f::ZeroVector);
	InvMass.Add(Inv);
	Group.Add(Body);
	if (Type == EHeftKind::Fluid)
		FluidCount++;
	return Kind.Add(Type);
}

int32 FHeftWorld::AddLink(int32 A, int32 B, float Stiffness, float Tear)
{
	return Links.Add({ A, B, FVector3f::Dist(Pos[A], Pos[B]), Stiffness, Tear, false });
}

int32 FHeftWorld::AddRigid(const FVector3f& Center, const FIntVector& Cells, float Inv, bool bRound)
{
	const int32 Body = NewGroup();
	const int32 First = Pos.Num();
	const FVector3f Half = FVector3f(Cells.X - 1, Cells.Y - 1, Cells.Z - 1) * Spacing * 0.5f;
	const float Bound = Half.X + 1.f;

	for (int32 X = 0; X < Cells.X; X++)
	{
		for (int32 Y = 0; Y < Cells.Y; Y++)
		{
			for (int32 Z = 0; Z < Cells.Z; Z++)
			{
				const FVector3f Local = FVector3f(X, Y, Z) * Spacing - Half;
				if (bRound && Local.SizeSquared() > Bound * Bound)
					continue;

				const int32 i = AddParticle(Center + Local, Inv, EHeftKind::Rigid, Body);
				Rest[i] = Local;
			}
		}
	}

	FVector3f Mean = FVector3f::ZeroVector;
	for (int32 i = First; i < Pos.Num(); i++)
		Mean += Rest[i];
	Mean /= Pos.Num() - First;
	for (int32 i = First; i < Pos.Num(); i++)
		Rest[i] -= Mean;

	return Rigids.Add({ First, Pos.Num() - First, Center + Mean, FQuat4f::Identity, FVector3f(Cells.X, Cells.Y, Cells.Z) * Spacing, bRound });
}

uint32 FHeftWorld::Hash(const FIntVector& Cell) const
{
	const uint32 Mixed = static_cast<uint32>(Cell.X) * 73856093u ^ static_cast<uint32>(Cell.Y) * 19349663u ^ static_cast<uint32>(Cell.Z) * 83492791u;
	return Mixed & (TableSize - 1);
}

void FHeftWorld::BuildGrid()
{
	const int32 Count = Pos.Num();
	CellOf.SetNumUninitialized(Count);
	Sorted.SetNumUninitialized(Count);
	CellStart.Init(0, TableSize + 1);

	for (int32 i = 0; i < Count; i++)
	{
		const FVector3f Scaled = Pos[i] / Kernel;
		CellOf[i] = FIntVector(FMath::FloorToInt32(Scaled.X), FMath::FloorToInt32(Scaled.Y), FMath::FloorToInt32(Scaled.Z));
		CellStart[Hash(CellOf[i]) + 1]++;
	}
	for (int32 i = 0; i < TableSize; i++)
		CellStart[i + 1] += CellStart[i];

	CellFill = CellStart;
	for (int32 i = 0; i < Count; i++)
		Sorted[CellFill[Hash(CellOf[i])]++] = i;
}

void FHeftWorld::FindNeighbors()
{
	const int32 Count = Pos.Num();
	Neigh.SetNumUninitialized(Count * MaxNeigh);
	NeighCount.SetNumUninitialized(Count);

	ParallelFor(Count, [this](int32 i)
	{
		int32 Found = 0;
		const FVector3f At = Pos[i];
		for (int32 N = 0; N < 27; N++)
		{
			const FIntVector Cell = CellOf[i] + FIntVector(N / 9 - 1, N / 3 % 3 - 1, N % 3 - 1);
			const uint32 Slot = Hash(Cell);
			for (int32 S = CellStart[Slot]; S < CellStart[Slot + 1]; S++)
			{
				const int32 j = Sorted[S];
				if (j == i || CellOf[j] != Cell)
					continue;
				if (FVector3f::DistSquared(At, Pos[j]) >= Kernel * Kernel)
					continue;
				if (Found < MaxNeigh)
					Neigh[i * MaxNeigh + Found++] = j;
			}
		}
		NeighCount[i] = Found;
	});
}

void FHeftWorld::SolveFluid()
{
	const int32 Count = Pos.Num();

	ParallelFor(Count, [this](int32 i)
	{
		Lambda[i] = 0.f;
		Crowd[i] = 0;
		if (Kind[i] != EHeftKind::Fluid)
			return;

		int32 WetCount = 0;
		float Density = Poly(0.f);
		float GradSq = 0.f;
		FVector3f GradSum = FVector3f::ZeroVector;
		for (int32 N = 0; N < NeighCount[i]; N++)
		{
			const int32 j = Neigh[i * MaxNeigh + N];
			const FVector3f Away = (Pos[i] - Pos[j]) / Kernel;
			const float Q = Away.Size();
			if (Q >= 1.f || Q < 1.e-5f)
				continue;

			if (Kind[j] == EHeftKind::Fluid)
				WetCount++;

			Density += Poly(Q);
			const FVector3f Grad = Away * (Spiky(Q) / (Q * RestDensity));
			GradSq += Grad.SizeSquared();
			GradSum += Grad;
		}

		Crowd[i] = static_cast<uint8>(FMath::Min(WetCount, 255));

		const float Squeeze = Density / RestDensity - 1.f;
		if (Squeeze > 0.f)
			Lambda[i] = -Squeeze / (GradSq + GradSum.SizeSquared() + Softness);
	});
}

void FHeftWorld::SolveContacts()
{
	ParallelFor(Pos.Num(), [this](int32 i)
	{
		Delta[i] = FVector3f::ZeroVector;
		Push[i] = FVector3f::ZeroVector;
		Hits[i] = 0;
		if (InvMass[i] <= 0.f)
			return;

		const bool bWet = Kind[i] == EHeftKind::Fluid;
		const bool bRigid = Kind[i] == EHeftKind::Rigid;
		const float Mine = bWet ? Lambda[i] : 0.f;
		FVector3f Squeeze = FVector3f::ZeroVector;

		for (int32 N = 0; N < NeighCount[i]; N++)
		{
			const int32 j = Neigh[i * MaxNeigh + N];
			const bool bOtherWet = Kind[j] == EHeftKind::Fluid;
			const FVector3f Away = Pos[i] - Pos[j];
			const float DistSq = Away.SizeSquared();

			if (bWet || bOtherWet)
			{
				const float Weight = Mine + (bOtherWet ? Lambda[j] : 0.f);
				if (Weight != 0.f && DistSq < Kernel * Kernel && DistSq > 1.e-6f)
				{
					const float Dist = FMath::Sqrt(DistSq);
					Squeeze += Away * (Weight * Spiky(Dist / Kernel) / (Dist * RestDensity));
				}
				if (bWet && bOtherWet)
					continue;
			}

			const bool bSame = Group[i] == Group[j];
			if (bSame && bRigid)
				continue;

			const float Limit = bSame ? Spacing * SelfScale : Spacing;
			if (DistSq >= Limit * Limit)
				continue;

			const float Dist = FMath::Sqrt(DistSq);
			const FVector3f Normal = Dist > 1.e-4f ? Away / Dist : FVector3f::UpVector;
			const float Depth = Limit - Dist;
			const float Share = InvMass[i] / (InvMass[i] + InvMass[j]);

			FVector3f Fix = Normal * (Depth * Share);
			if (!bWet && !bOtherWet)
			{
				const FVector3f Moved = (Pos[i] - Old[i]) - (Pos[j] - Old[j]);
				const FVector3f Tang = Moved - Normal * FVector3f::DotProduct(Moved, Normal);
				const float Len = Tang.Size();
				if (Len > 1.e-6f)
				{
					const float Cap = Friction * Depth;
					Fix -= Tang * ((Len <= Cap ? 1.f : Cap / Len) * Share);
				}
			}

			Push[i] += Fix;
			Hits[i]++;
		}

		Delta[i] = Squeeze * (Kernel * (bWet ? 1.f : FMath::Min(InvMass[i], MaxLift)));
	});
}

void FHeftWorld::SolveStatics()
{
	ParallelFor(Pos.Num(), [this](int32 i)
	{
		if (InvMass[i] <= 0.f)
			return;

		const float Grip = Kind[i] == EHeftKind::Fluid ? Friction * FluidGrip : Friction;
		FVector3f At = Pos[i];

		for (const FHeftSphere& Ball : Spheres)
		{
			const FVector3f Away = At - Ball.Center;
			const float Gap = Ball.Radius + Radius;
			const float DistSq = Away.SizeSquared();
			if (DistSq >= Gap * Gap || DistSq < 1.e-6f)
				continue;

			At = Ball.Center + Away * (Gap / FMath::Sqrt(DistSq));
		}

		for (const FHeftBox& Box : Boxes)
		{
			FVector3f Local = Box.Rot.UnrotateVector(At - Box.Center);
			const FVector3f Edge = Box.Extent + FVector3f(Radius);
			const FVector3f Inside = Edge - Local.GetAbs();
			if (Inside.X <= 0.f || Inside.Y <= 0.f || Inside.Z <= 0.f)
				continue;

			int32 Axis = 0;
			if (Inside.Y < Inside[Axis])
				Axis = 1;
			if (Inside.Z < Inside[Axis])
				Axis = 2;

			FVector3f Normal = FVector3f::ZeroVector;
			Normal[Axis] = Local[Axis] >= 0.f ? 1.f : -1.f;
			Local[Axis] = Normal[Axis] * Edge[Axis];

			At = Box.Center + Box.Rot.RotateVector(Local);
			Slide(At, Old[i], Box.Rot.RotateVector(Normal), Inside[Axis], Grip);
		}

		if (At.Z < Radius)
		{
			const float Depth = Radius - At.Z;
			At.Z = Radius;
			Slide(At, Old[i], FVector3f::UpVector, Depth, Grip);
		}

		Pos[i] = At;
	});
}

void FHeftWorld::SolveLinks()
{
	for (int32 Sweep = 0; Sweep < LinkSweeps; Sweep++)
	{
		for (FHeftLink& Link : Links)
		{
			if (Link.bBroken)
				continue;

			const float Weight = InvMass[Link.A] + InvMass[Link.B];
			if (Weight <= 0.f)
				continue;

			const FVector3f Span = Pos[Link.B] - Pos[Link.A];
			const float Len = Span.Size();
			if (Len < 1.e-4f)
				continue;

			if (Link.Tear > 0.f && Len > Link.Rest * Link.Tear)
			{
				Link.bBroken = true;
				bTorn = true;
				continue;
			}

			const FVector3f Fix = Span * ((Len - Link.Rest) / Len * Link.Stiffness / Weight);
			Pos[Link.A] += Fix * InvMass[Link.A];
			Pos[Link.B] -= Fix * InvMass[Link.B];
		}
	}
}

void FHeftWorld::SolveRigids()
{
	ParallelFor(Rigids.Num(), [this](int32 R)
	{
		FHeftRigid& Body = Rigids[R];
		const int32 End = Body.First + Body.Count;

		FVector3f Com = FVector3f::ZeroVector;
		for (int32 i = Body.First; i < End; i++)
			Com += Pos[i];
		Com /= Body.Count;

		FVector3f A0 = FVector3f::ZeroVector;
		FVector3f A1 = FVector3f::ZeroVector;
		FVector3f A2 = FVector3f::ZeroVector;
		for (int32 i = Body.First; i < End; i++)
		{
			const FVector3f Offset = Pos[i] - Com;
			A0 += Offset * Rest[i].X;
			A1 += Offset * Rest[i].Y;
			A2 += Offset * Rest[i].Z;
		}

		Body.Com = Com;
		Body.Rot = ExtractRotation(A0, A1, A2, Body.Rot);
		for (int32 i = Body.First; i < End; i++)
			Pos[i] = Com + Body.Rot.RotateVector(Rest[i]);
	});
}

void FHeftWorld::Smooth()
{
	ParallelFor(Pos.Num(), [this](int32 i)
	{
		Delta[i] = FVector3f::ZeroVector;
		if (Kind[i] != EHeftKind::Fluid)
			return;

		FVector3f Sum = FVector3f::ZeroVector;
		for (int32 N = 0; N < NeighCount[i]; N++)
		{
			const int32 j = Neigh[i * MaxNeigh + N];
			if (Kind[j] != EHeftKind::Fluid)
				continue;

			const float Q = FVector3f::Dist(Pos[i], Pos[j]) / Kernel;
			if (Q < 1.f)
				Sum += (Vel[j] - Vel[i]) * Poly(Q);
		}
		Delta[i] = Sum * (Viscosity / RestDensity);
	});

	ParallelFor(Pos.Num(), [this](int32 i)
	{
		Vel[i] += Delta[i];
	});
}

void FHeftWorld::Step(float DeltaTime)
{
	const double Began = FPlatformTime::Seconds();
	const int32 Count = Pos.Num();
	if (Count == 0)
		return;

	Delta.SetNumUninitialized(Count);
	Push.SetNumUninitialized(Count);
	Hits.SetNumUninitialized(Count);
	Lambda.SetNumUninitialized(Count);
	Crowd.SetNumZeroed(Count);

	ParallelFor(Count, [this, DeltaTime](int32 i)
	{
		Old[i] = Pos[i];
		if (InvMass[i] <= 0.f)
		{
			Vel[i] = FVector3f::ZeroVector;
			return;
		}

		Vel[i] = (Vel[i] + Gravity * DeltaTime).GetClampedToMaxSize(SpeedCap);
		Pos[i] += Vel[i] * DeltaTime;
	});

	BuildGrid();
	FindNeighbors();

	for (int32 Pass = 0; Pass < Passes; Pass++)
	{
		SolveFluid();
		SolveContacts();

		ParallelFor(Count, [this](int32 i)
		{
			if (InvMass[i] <= 0.f)
				return;

			Pos[i] += Delta[i];
			if (Hits[i] > 0)
				Pos[i] += Push[i] / static_cast<float>(Hits[i]);
		});

		SolveStatics();
		SolveLinks();
		SolveRigids();
	}

	const float Rebound = Bounce;
	ParallelFor(Count, [this, DeltaTime, Rebound](int32 i)
	{
		if (InvMass[i] <= 0.f)
			return;

		const float FallSpeed = Vel[i].Z;
		Vel[i] = (Pos[i] - Old[i]) / DeltaTime * 0.9995f;
		if (Pos[i].Z <= Radius + 0.01f && FallSpeed < -60.f && Kind[i] != EHeftKind::Fluid)
			Vel[i].Z = FMath::Max(Vel[i].Z, -FallSpeed * Rebound);
	});

	Smooth();

	StepMs = static_cast<float>((FPlatformTime::Seconds() - Began) * 1000.0);
}

void FHeftWorld::Impulse(const FVector3f& Origin, float Range, float Speed)
{
	for (int32 i = 0; i < Pos.Num(); i++)
	{
		if (InvMass[i] <= 0.f)
			continue;

		const FVector3f Away = Pos[i] - Origin;
		const float Dist = Away.Size();
		if (Dist < Range && Dist > 1.e-3f)
			Vel[i] += Away / Dist * (Speed * (1.f - Dist / Range));
	}
}

int32 FHeftWorld::Pick(const FVector3f& Origin, const FVector3f& Dir, float Range) const
{
	int32 Best = INDEX_NONE;
	float BestAlong = Range;
	for (int32 i = 0; i < Pos.Num(); i++)
	{
		if (Kind[i] == EHeftKind::Fluid)
			continue;

		const FVector3f To = Pos[i] - Origin;
		const float Along = FVector3f::DotProduct(To, Dir);
		if (Along < 0.f || Along > BestAlong)
			continue;
		if (To.SizeSquared() - Along * Along > PickRange * PickRange)
			continue;

		Best = i;
		BestAlong = Along;
	}
	return Best;
}

#include "HeftSurface.h"
#include "HeftWorld.h"
#include "Async/ParallelFor.h"
#include "HAL/PlatformTime.h"

static constexpr float GridCell = 10.f;
static constexpr float BlobRadius = 20.f;
static constexpr float BlobCells = BlobRadius / GridCell;
static constexpr float Waterline = 0.6f;
static constexpr int32 SlabCells = 6;
static const FIntVector GridCap(160, 160, 96);
static constexpr int32 MinCrowd = 6;

static const int32 CubeEdges[12][2] =
{
	{ 0, 1 }, { 2, 3 }, { 4, 5 }, { 6, 7 },
	{ 0, 2 }, { 1, 3 }, { 4, 6 }, { 5, 7 },
	{ 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 }
};

void FHeftSurface::Splat(const FVector3f& At)
{
	const FVector3f Grid = (At - Origin) / GridCell;
	const int32 X0 = FMath::Max(0, FMath::CeilToInt32(Grid.X - BlobCells));
	const int32 Y0 = FMath::Max(0, FMath::CeilToInt32(Grid.Y - BlobCells));
	const int32 Z0 = FMath::Max(0, FMath::CeilToInt32(Grid.Z - BlobCells));
	const int32 X1 = FMath::Min(Size.X - 1, FMath::FloorToInt32(Grid.X + BlobCells));
	const int32 Y1 = FMath::Min(Size.Y - 1, FMath::FloorToInt32(Grid.Y + BlobCells));
	const int32 Z1 = FMath::Min(Size.Z - 1, FMath::FloorToInt32(Grid.Z + BlobCells));

	for (int32 Z = Z0; Z <= Z1; Z++)
	{
		for (int32 Y = Y0; Y <= Y1; Y++)
		{
			for (int32 X = X0; X <= X1; X++)
			{
				const float Q = FVector3f::DistSquared(Grid, FVector3f(X, Y, Z)) / (BlobCells * BlobCells);
				if (Q >= 1.f)
					continue;

				const float K = 1.f - Q;
				Field[Corner(X, Y, Z)] += K * K * K;
			}
		}
	}
}

void FHeftSurface::PlaceVerts(int32 Z)
{
	TArray<FVector3f>& OutPos = SlicePos[Z];
	TArray<FVector3f>& OutNormal = SliceNormal[Z];
	OutPos.Reset();
	OutNormal.Reset();

	for (int32 Y = 0; Y < Size.Y - 1; Y++)
	{
		for (int32 X = 0; X < Size.X - 1; X++)
		{
			float V[8];
			int32 Inside = 0;
			for (int32 N = 0; N < 8; N++)
			{
				V[N] = Field[Corner(X + (N & 1), Y + (N >> 1 & 1), Z + (N >> 2 & 1))];
				if (V[N] >= Waterline)
					Inside++;
			}

			const int32 CellIndex = Cell(X, Y, Z);
			if (Inside == 0 || Inside == 8)
			{
				CellVert[CellIndex] = INDEX_NONE;
				continue;
			}

			FVector3f Sum = FVector3f::ZeroVector;
			int32 Crossings = 0;
			for (const int32* Edge : CubeEdges)
			{
				const float A = V[Edge[0]];
				const float B = V[Edge[1]];
				if ((A >= Waterline) == (B >= Waterline))
					continue;

				const float T = (Waterline - A) / (B - A);
				const FVector3f From(Edge[0] & 1, Edge[0] >> 1 & 1, Edge[0] >> 2 & 1);
				const FVector3f To(Edge[1] & 1, Edge[1] >> 1 & 1, Edge[1] >> 2 & 1);
				Sum += FMath::Lerp(From, To, T);
				Crossings++;
			}

			const FVector3f Slope(
				V[1] + V[3] + V[5] + V[7] - V[0] - V[2] - V[4] - V[6],
				V[2] + V[3] + V[6] + V[7] - V[0] - V[1] - V[4] - V[5],
				V[4] + V[5] + V[6] + V[7] - V[0] - V[1] - V[2] - V[3]);

			CellVert[CellIndex] = OutPos.Num();
			OutPos.Add(Origin + (FVector3f(X, Y, Z) + Sum / Crossings) * GridCell);
			OutNormal.Add((-Slope).GetSafeNormal(UE_SMALL_NUMBER, FVector3f::UpVector));
		}
	}
}

void FHeftSurface::Emit(int32 Layer, const FIntVector Cells[4])
{
	FVector3f Pos[4];
	FVector3f Normal[4];
	for (int32 N = 0; N < 4; N++)
	{
		const FIntVector& C = Cells[N];
		const int32 Local = CellVert[Cell(C.X, C.Y, C.Z)];
		if (Local == INDEX_NONE)
			return;

		Pos[N] = SlicePos[C.Z][Local];
		Normal[N] = SliceNormal[C.Z][Local];
	}

	static const int32 Order[6] = { 0, 1, 2, 0, 2, 3 };
	static const int32 Flipped[6] = { 0, 2, 1, 0, 3, 2 };
	const FVector3f Facing = FVector3f::CrossProduct(Pos[2] - Pos[0], Pos[1] - Pos[0]);
	const bool bFlip = FVector3f::DotProduct(Facing, Normal[0] + Normal[1] + Normal[2]) < 0.f;
	const int32* Winding = bFlip ? Flipped : Order;
	for (int32 N = 0; N < 6; N++)
	{
		LayerPos[Layer].Add(Pos[Winding[N]]);
		LayerNormal[Layer].Add(Normal[Winding[N]]);
	}
}

void FHeftSurface::StitchQuads(int32 Z)
{
	LayerPos[Z].Reset();
	LayerNormal[Z].Reset();

	const bool bMidZ = Z >= 1 && Z <= Size.Z - 2;
	for (int32 Y = 0; Y < Size.Y; Y++)
	{
		const bool bMidY = Y >= 1 && Y <= Size.Y - 2;
		for (int32 X = 0; X < Size.X; X++)
		{
			const bool bMidX = X >= 1 && X <= Size.X - 2;
			const bool bWetHere = Field[Corner(X, Y, Z)] >= Waterline;

			if (X <= Size.X - 2 && bMidY && bMidZ && bWetHere != (Field[Corner(X + 1, Y, Z)] >= Waterline))
			{
				const FIntVector Cells[4] = { { X, Y - 1, Z - 1 }, { X, Y, Z - 1 }, { X, Y, Z }, { X, Y - 1, Z } };
				Emit(Z, Cells);
			}
			if (Y <= Size.Y - 2 && bMidX && bMidZ && bWetHere != (Field[Corner(X, Y + 1, Z)] >= Waterline))
			{
				const FIntVector Cells[4] = { { X - 1, Y, Z - 1 }, { X, Y, Z - 1 }, { X, Y, Z }, { X - 1, Y, Z } };
				Emit(Z, Cells);
			}
			if (Z <= Size.Z - 2 && bMidX && bMidY && bWetHere != (Field[Corner(X, Y, Z + 1)] >= Waterline))
			{
				const FIntVector Cells[4] = { { X - 1, Y - 1, Z }, { X, Y - 1, Z }, { X, Y, Z }, { X - 1, Y, Z } };
				Emit(Z, Cells);
			}
		}
	}
}

void FHeftSurface::Build(const FHeftWorld& World)
{
	const double Began = FPlatformTime::Seconds();

	Points.Reset();
	Normals.Reset();
	Loose.Reset();
	Wet.Reset();

	FBox3f Bounds(ForceInit);
	for (int32 i = 0; i < World.Num(); i++)
	{
		if (World.Kind[i] != EHeftKind::Fluid)
			continue;

		if (World.Crowd.IsValidIndex(i) && World.Crowd[i] >= MinCrowd)
			Bounds += World.Pos[i];
	}
	if (!Bounds.IsValid)
	{
		for (int32 i = 0; i < World.Num(); i++)
		{
			if (World.Kind[i] == EHeftKind::Fluid)
				Loose.Add(i);
		}
		BuildMs = 0.f;
		return;
	}

	const float Margin = BlobRadius + GridCell * 2.f;
	Origin = Bounds.Min - FVector3f(Margin);
	const FVector3f Span = (Bounds.Max - Bounds.Min + FVector3f(Margin * 2.f)) / GridCell;
	Size.X = FMath::Min(FMath::CeilToInt32(Span.X) + 1, GridCap.X);
	Size.Y = FMath::Min(FMath::CeilToInt32(Span.Y) + 1, GridCap.Y);
	Size.Z = FMath::Min(FMath::CeilToInt32(Span.Z) + 1, GridCap.Z);

	const FVector3f Usable = FVector3f(Size.X - 1, Size.Y - 1, Size.Z - 1) * GridCell - FVector3f(Margin * 2.f);
	const int32 Slabs = Size.Z / SlabCells + 1;
	SlabStart.Init(0, Slabs + 1);

	for (int32 i = 0; i < World.Num(); i++)
	{
		if (World.Kind[i] != EHeftKind::Fluid)
			continue;

		const FVector3f Local = World.Pos[i] - Bounds.Min;
		if (Local.GetMin() < 0.f || Local.X > Usable.X || Local.Y > Usable.Y || Local.Z > Usable.Z)
		{
			Loose.Add(i);
			continue;
		}

		Wet.Add(World.Pos[i]);
		SlabStart[FMath::FloorToInt32((World.Pos[i].Z - Origin.Z) / GridCell) / SlabCells + 1]++;
	}

	for (int32 S = 0; S < Slabs; S++)
		SlabStart[S + 1] += SlabStart[S];

	SlabFill = SlabStart;
	SlabOrder.SetNumUninitialized(Wet.Num());
	for (int32 i = 0; i < Wet.Num(); i++)
		SlabOrder[SlabFill[FMath::FloorToInt32((Wet[i].Z - Origin.Z) / GridCell) / SlabCells]++] = i;

	Field.Reset();
	Field.SetNumZeroed(Size.X * Size.Y * Size.Z);
	CellVert.SetNumUninitialized((Size.X - 1) * (Size.Y - 1) * (Size.Z - 1));

	for (int32 Phase = 0; Phase < 2; Phase++)
	{
		ParallelFor((Slabs - Phase + 1) / 2, [this, Phase](int32 Half)
		{
			const int32 S = Half * 2 + Phase;
			for (int32 N = SlabStart[S]; N < SlabStart[S + 1]; N++)
				Splat(Wet[SlabOrder[N]]);
		});
	}

	SlicePos.SetNum(Size.Z);
	SliceNormal.SetNum(Size.Z);
	LayerPos.SetNum(Size.Z);
	LayerNormal.SetNum(Size.Z);

	ParallelFor(Size.Z - 1, [this](int32 Z) { PlaceVerts(Z); });
	ParallelFor(Size.Z, [this](int32 Z) { StitchQuads(Z); });

	for (int32 Z = 0; Z < Size.Z; Z++)
	{
		Points.Append(LayerPos[Z]);
		Normals.Append(LayerNormal[Z]);
	}

	BuildMs = static_cast<float>((FPlatformTime::Seconds() - Began) * 1000.0);
}

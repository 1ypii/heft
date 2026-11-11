#pragma once

#include "CoreMinimal.h"

class FHeftWorld;

class FHeftSurface
{
public:
	void Build(const FHeftWorld& World);

	TArray<FVector3f> Points;
	TArray<FVector3f> Normals;
	TArray<int32> Loose;
	float BuildMs = 0.f;

private:
	int32 Corner(int32 X, int32 Y, int32 Z) const { return (Z * Size.Y + Y) * Size.X + X; }
	int32 Cell(int32 X, int32 Y, int32 Z) const { return (Z * (Size.Y - 1) + Y) * (Size.X - 1) + X; }

	void Splat(const FVector3f& At);
	void PlaceVerts(int32 Z);
	void StitchQuads(int32 Z);
	void Emit(int32 Layer, const FIntVector Cells[4]);

	FIntVector Size = FIntVector::ZeroValue;
	FVector3f Origin = FVector3f::ZeroVector;

	TArray<float> Field;
	TArray<int32> CellVert;
	TArray<int32> SlabStart;
	TArray<int32> SlabFill;
	TArray<int32> SlabOrder;
	TArray<FVector3f> Wet;

	TArray<TArray<FVector3f>> SlicePos;
	TArray<TArray<FVector3f>> SliceNormal;
	TArray<TArray<FVector3f>> LayerPos;
	TArray<TArray<FVector3f>> LayerNormal;
};

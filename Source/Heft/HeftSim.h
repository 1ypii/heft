#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HeftWorld.h"
#include "HeftSurface.h"
#include "HeftSim.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UProceduralMeshComponent;

UCLASS()
class AHeftSim : public AActor
{
	GENERATED_BODY()

public:
	AHeftSim();

	virtual void Tick(float DeltaTime) override;

	void AddCloth(const FVector3f& Corner, int32 Cols, int32 Rows);
	void AddJelly(const FVector3f& Center, int32 Count);
	void AddWater(const FVector3f& Corner, const FIntVector& Cells);
	void AddCrate(const FVector3f& Center, int32 Cells, float InvMass);
	void AddBall(const FVector3f& Center, int32 Cells, float InvMass, const FVector3f& Velocity);
	void AddWall(const FVector3f& Center, const FVector3f& Size);
	void Unpin();

	FHeftWorld World;
	FHeftSurface Surface;

protected:
	enum class ESkin : uint8
	{
		Cloth,
		Jelly
	};

	struct FSkin
	{
		ESkin Type = ESkin::Cloth;
		int32 First = 0;
		int32 Count = 0;
		int32 Cols = 0;
		int32 Rows = 0;
		TArray<int32> Across;
		TArray<int32> Down;
		TArray<int32> Map;
		TArray<int32> Tris;
	};

	void BuildTris(FSkin& Skin);
	void Upload(int32 Index, bool bRebuild);
	void DrawInstances();
	void DrawSea();

	UPROPERTY(EditAnywhere, Category = "Heft")
	int32 HeavyFluidCount = 2500;

	UPROPERTY(EditAnywhere, Category = "Heft")
	float MaxFrameTime = 0.02f;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UProceduralMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UProceduralMeshComponent> Sea;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> Water;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> Crates;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> Balls;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> ClothMat;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> JellyMat;

	TArray<FSkin> Skins;
	TArray<FVector> Verts;
	TArray<FVector> Normals;
	TArray<FTransform> WaterDraw;
	TArray<FTransform> CrateDraw;
	TArray<FTransform> BallDraw;

	TArray<FVector> SeaVerts;
	TArray<FVector> SeaNormals;
	int32 SeaSize = 0;
};

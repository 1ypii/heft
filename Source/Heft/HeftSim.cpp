#include "HeftSim.h"
#include "HeftGameMode.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

static void Feed(UInstancedStaticMeshComponent* Target, const TArray<FTransform>& Draw)
{
	if (Target->GetInstanceCount() != Draw.Num())
	{
		Target->ClearInstances();
		Target->AddInstances(Draw, false, true);
		return;
	}

	if (Draw.Num() > 0)
		Target->BatchUpdateInstancesTransforms(0, Draw, true, true, true);
}

AHeftSim::AHeftSim()
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFind(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFind(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> WaterFind(TEXT("/Game/Materials/MI_Water.MI_Water"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> CrateFind(TEXT("/Game/Materials/MI_Crate.MI_Crate"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BallFind(TEXT("/Game/Materials/MI_Metal.MI_Metal"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ClothFind(TEXT("/Game/Materials/MI_Cloth.MI_Cloth"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> JellyFind(TEXT("/Game/Materials/MI_Jelly.MI_Jelly"));
	ClothMat = ClothFind.Object;
	JellyMat = JellyFind.Object;

	PrimaryActorTick.bCanEverTick = true;

	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Mesh"));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RootComponent = Mesh;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SeaFind(TEXT("/Game/Materials/M_Sea.M_Sea"));
	Sea = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Sea"));
	Sea->SetupAttachment(Mesh);
	Sea->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Sea->SetMaterial(0, SeaFind.Object);

	Water = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Water"));
	Water->SetupAttachment(Mesh);
	Water->SetStaticMesh(SphereFind.Object);
	Water->SetMaterial(0, WaterFind.Object);
	Water->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Water->SetCastShadow(false);

	Crates = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Crates"));
	Crates->SetupAttachment(Mesh);
	Crates->SetStaticMesh(CubeFind.Object);
	Crates->SetMaterial(0, CrateFind.Object);
	Crates->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Balls = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Balls"));
	Balls->SetupAttachment(Mesh);
	Balls->SetStaticMesh(SphereFind.Object);
	Balls->SetMaterial(0, BallFind.Object);
	Balls->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AHeftSim::AddCloth(const FVector3f& Corner, int32 Cols, int32 Rows)
{
	FSkin& Skin = Skins.AddDefaulted_GetRef();
	Skin.Type = ESkin::Cloth;
	Skin.First = World.Num();
	Skin.Count = Cols * Rows;
	Skin.Cols = Cols;
	Skin.Rows = Rows;
	Skin.Across.Init(INDEX_NONE, Skin.Count);
	Skin.Down.Init(INDEX_NONE, Skin.Count);

	const int32 Body = World.NewGroup();
	for (int32 R = 0; R < Rows; R++)
	{
		for (int32 C = 0; C < Cols; C++)
		{
			const bool bPinned = (R == 0 || R == Rows - 1) && (C == 0 || C == Cols - 1);
			World.AddParticle(Corner + FVector3f(R, C, 0.f) * FHeftWorld::Spacing, bPinned ? 0.f : 1.f, EHeftKind::Soft, Body);
		}
	}

	for (int32 R = 0; R < Rows; R++)
	{
		for (int32 C = 0; C < Cols; C++)
		{
			const int32 Local = R * Cols + C;
			const int32 At = Skin.First + Local;
			if (C + 1 < Cols)
				Skin.Across[Local] = World.AddLink(At, At + 1, 1.f, 2.2f);
			if (R + 1 < Rows)
				Skin.Down[Local] = World.AddLink(At, At + Cols, 1.f, 2.2f);
			if (C + 1 < Cols && R + 1 < Rows)
			{
				World.AddLink(At, At + Cols + 1, 0.6f, 2.6f);
				World.AddLink(At + 1, At + Cols, 0.6f, 2.6f);
			}
			if (C + 2 < Cols)
				World.AddLink(At, At + 2, 0.3f, 3.f);
			if (R + 2 < Rows)
				World.AddLink(At, At + Cols * 2, 0.3f, 3.f);
		}
	}

	for (int32 i = 0; i < Skin.Count; i++)
		Skin.Map.Add(Skin.First + i);

	const int32 Index = Skins.Num() - 1;
	Mesh->SetMaterial(Index, ClothMat);
	BuildTris(Skin);
	Upload(Index, true);
}

void AHeftSim::AddJelly(const FVector3f& Center, int32 Count)
{
	FSkin& Skin = Skins.AddDefaulted_GetRef();
	Skin.Type = ESkin::Jelly;
	Skin.First = World.Num();
	Skin.Count = Count * Count * Count;

	const int32 Body = World.NewGroup();
	const int32 First = Skin.First;
	const FVector3f Corner = Center - FVector3f((Count - 1) * FHeftWorld::Spacing * 0.5f);
	auto At = [Count, First](int32 X, int32 Y, int32 Z) { return First + (X * Count + Y) * Count + Z; };

	for (int32 X = 0; X < Count; X++)
	{
		for (int32 Y = 0; Y < Count; Y++)
		{
			for (int32 Z = 0; Z < Count; Z++)
				World.AddParticle(Corner + FVector3f(X, Y, Z) * FHeftWorld::Spacing, 1.f, EHeftKind::Soft, Body);
		}
	}

	for (int32 X = 0; X < Count; X++)
	{
		for (int32 Y = 0; Y < Count; Y++)
		{
			for (int32 Z = 0; Z < Count; Z++)
			{
				for (int32 N = 14; N < 27; N++)
				{
					const int32 NX = X + N / 9 - 1;
					const int32 NY = Y + N / 3 % 3 - 1;
					const int32 NZ = Z + N % 3 - 1;
					if (NX < 0 || NY < 0 || NZ < 0 || NX >= Count || NY >= Count || NZ >= Count)
						continue;

					World.AddLink(At(X, Y, Z), At(NX, NY, NZ), 0.05f, 0.f);
				}
			}
		}
	}

	const int32 Last = Count - 1;
	for (int32 Face = 0; Face < 6; Face++)
	{
		const int32 Start = Skin.Map.Num();
		const int32 Fixed = Face % 2 == 0 ? 0 : Last;
		for (int32 U = 0; U < Count; U++)
		{
			for (int32 V = 0; V < Count; V++)
			{
				if (Face < 2)
					Skin.Map.Add(At(Fixed, U, V));
				else if (Face < 4)
					Skin.Map.Add(At(U, Fixed, V));
				else
					Skin.Map.Add(At(U, V, Fixed));
			}
		}

		for (int32 U = 0; U < Last; U++)
		{
			for (int32 V = 0; V < Last; V++)
			{
				const int32 i = Start + U * Count + V;
				Skin.Tris.Append({ i, i + 1, i + Count, i + 1, i + Count + 1, i + Count });
			}
		}
	}

	const int32 Index = Skins.Num() - 1;
	Mesh->SetMaterial(Index, JellyMat);
	Upload(Index, true);
}

void AHeftSim::AddWater(const FVector3f& Corner, const FIntVector& Cells)
{
	const int32 Body = World.NewGroup();
	for (int32 X = 0; X < Cells.X; X++)
	{
		for (int32 Y = 0; Y < Cells.Y; Y++)
		{
			for (int32 Z = 0; Z < Cells.Z; Z++)
			{
				const FVector3f Jitter(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), 0.f);
				World.AddParticle(Corner + FVector3f(X, Y, Z) * FHeftWorld::FluidSpacing + Jitter, 1.f, EHeftKind::Fluid, Body);
			}
		}
	}
}

void AHeftSim::AddCrate(const FVector3f& Center, int32 Cells, float InvMass)
{
	World.AddRigid(Center, FIntVector(Cells), InvMass, false);
}

void AHeftSim::AddBall(const FVector3f& Center, int32 Cells, float InvMass, const FVector3f& Velocity)
{
	const FHeftRigid& Body = World.Rigids[World.AddRigid(Center, FIntVector(Cells), InvMass, true)];
	for (int32 i = Body.First; i < Body.First + Body.Count; i++)
		World.Vel[i] = Velocity;
}

void AHeftSim::AddWall(const FVector3f& Center, const FVector3f& Size)
{
	World.Boxes.Add({ Center, FQuat4f::Identity, Size * 0.5f });
}

void AHeftSim::Unpin()
{
	for (float& Inv : World.InvMass)
	{
		if (Inv <= 0.f)
			Inv = 1.f;
	}
}

void AHeftSim::BuildTris(FSkin& Skin)
{
	Skin.Tris.Reset();

	for (int32 R = 0; R + 1 < Skin.Rows; R++)
	{
		for (int32 C = 0; C + 1 < Skin.Cols; C++)
		{
			const int32 i = R * Skin.Cols + C;
			const bool bWhole = !World.Links[Skin.Across[i]].bBroken && !World.Links[Skin.Across[i + Skin.Cols]].bBroken
				&& !World.Links[Skin.Down[i]].bBroken && !World.Links[Skin.Down[i + 1]].bBroken;
			if (bWhole)
				Skin.Tris.Append({ i, i + 1, i + Skin.Cols, i + 1, i + Skin.Cols + 1, i + Skin.Cols });
		}
	}
}

void AHeftSim::Upload(int32 Index, bool bRebuild)
{
	const FSkin& Skin = Skins[Index];
	const TArray<FVector3f>& Points = World.Pos;

	Verts.SetNumUninitialized(Skin.Map.Num());
	Normals.Init(FVector::ZeroVector, Skin.Map.Num());
	for (int32 i = 0; i < Skin.Map.Num(); i++)
		Verts[i] = FVector(Points[Skin.Map[i]]);

	for (int32 i = 0; i + 2 < Skin.Tris.Num(); i += 3)
	{
		const FVector Face = FVector::CrossProduct(Verts[Skin.Tris[i + 2]] - Verts[Skin.Tris[i]], Verts[Skin.Tris[i + 1]] - Verts[Skin.Tris[i]]);
		Normals[Skin.Tris[i]] += Face;
		Normals[Skin.Tris[i + 1]] += Face;
		Normals[Skin.Tris[i + 2]] += Face;
	}
	for (FVector& Normal : Normals)
		Normal = Normal.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);

	if (bRebuild)
		Mesh->CreateMeshSection_LinearColor(Index, Verts, Skin.Tris, Normals, TArray<FVector2D>(), TArray<FLinearColor>(), TArray<FProcMeshTangent>(), false);
	else
		Mesh->UpdateMeshSection_LinearColor(Index, Verts, Normals, TArray<FVector2D>(), TArray<FLinearColor>(), TArray<FProcMeshTangent>());
}

void AHeftSim::DrawInstances()
{
	const FVector Drop(FHeftWorld::FluidSpacing * 1.15f / 100.f);

	WaterDraw.Reset();
	for (int32 i : Surface.Loose)
	{
		const FVector At(World.Pos[i]);
		const FVector Flight(World.Vel[i]);
		const float Speed = Flight.Size();

		if (At.Z < FHeftWorld::Radius + 3.f && Speed < 60.f)
		{
			WaterDraw.Emplace(FQuat::Identity, FVector(At.X, At.Y, 2.f), FVector(Drop.X * 1.7f, Drop.Y * 1.7f, Drop.Z * 0.3f));
			continue;
		}

		const float Stretch = FMath::Clamp(1.f + Speed / 350.f, 1.f, 3.2f);
		const float Thin = Drop.X / FMath::Sqrt(Stretch);
		const FQuat Facing = Speed > 1.f ? FRotationMatrix::MakeFromX(Flight).ToQuat() : FQuat::Identity;
		WaterDraw.Emplace(Facing, At, FVector(Drop.X * Stretch, Thin, Thin));
	}

	CrateDraw.Reset();
	BallDraw.Reset();
	for (const FHeftRigid& Body : World.Rigids)
	{
		const FTransform At(FQuat(Body.Rot), FVector(Body.Com), FVector(Body.Size) / 100.f);
		if (Body.bRound)
			BallDraw.Add(At);
		else
			CrateDraw.Add(At);
	}

	Feed(Water, WaterDraw);
	Feed(Crates, CrateDraw);
	Feed(Balls, BallDraw);
}

void AHeftSim::DrawSea()
{
	Surface.Build(World);

	const int32 Used = Surface.Points.Num();
	if (Used == 0 && SeaSize == 0)
		return;

	const bool bResize = Used > SeaSize || Used < SeaSize / 4;
	if (bResize)
		SeaSize = FMath::Max(30000, Used + Used / 2);

	SeaVerts.SetNumUninitialized(SeaSize);
	SeaNormals.SetNumUninitialized(SeaSize);
	for (int32 i = 0; i < SeaSize; i++)
	{
		SeaVerts[i] = i < Used ? FVector(Surface.Points[i]) : FVector::ZeroVector;
		SeaNormals[i] = i < Used ? FVector(Surface.Normals[i]) : FVector::UpVector;
	}

	if (!bResize)
	{
		Sea->UpdateMeshSection_LinearColor(0, SeaVerts, SeaNormals, TArray<FVector2D>(), TArray<FLinearColor>(), TArray<FProcMeshTangent>());
		return;
	}

	TArray<int32> Order;
	Order.SetNumUninitialized(SeaSize);
	for (int32 i = 0; i < SeaSize; i++)
		Order[i] = i;
	Sea->CreateMeshSection_LinearColor(0, SeaVerts, Order, SeaNormals, TArray<FVector2D>(), TArray<FLinearColor>(), TArray<FProcMeshTangent>(), false);
}

void AHeftSim::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	const AHeftGameMode* GameMode = GetWorld()->GetAuthGameMode<AHeftGameMode>();
	if (!GameMode)
		return;

	World.Gravity = FVector3f(0.f, 0.f, GetWorld()->GetGravityZ());
	World.Friction = GameMode->GetFriction();
	World.Bounce = GameMode->GetBounce();
	World.Spheres = GameMode->Spheres;

	if (DeltaTime > 0.f)
	{
		const int32 Steps = World.FluidCount > HeavyFluidCount ? 1 : 2;
		const float StepTime = FMath::Min(DeltaTime, MaxFrameTime) / Steps;
		World.Passes = Steps == 1 ? 4 : 3;
		for (int32 i = 0; i < Steps; i++)
			World.Step(StepTime);
	}

	const bool bTorn = World.bTorn;
	World.bTorn = false;
	for (int32 i = 0; i < Skins.Num(); i++)
	{
		const bool bRebuild = bTorn && Skins[i].Type != ESkin::Jelly;
		if (bRebuild)
			BuildTris(Skins[i]);
		Upload(i, bRebuild);
	}

	DrawSea();
	DrawInstances();
}

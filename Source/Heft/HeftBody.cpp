#include "HeftBody.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AHeftBody::AHeftBody()
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	Cube = CubeMesh.Object;
	Sphere = SphereMesh.Object;

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetGenerateOverlapEvents(false);
	RootComponent = Mesh;
}

void AHeftBody::Setup(const FVector& Size, UMaterialInterface* Mat)
{
	Mesh->SetStaticMesh(Cube);
	Mesh->SetMaterial(0, Mat);
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));
	SetActorScale3D(Size / 100.f);
}

void AHeftBody::Burst(float Radius, UMaterialInterface* Mat)
{
	Mesh->SetStaticMesh(Sphere);
	Mesh->SetMaterial(0, Mat);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);

	Life = 0.2f;
	EndScale = Radius / 50.f;
	SetActorScale3D(FVector(EndScale * 0.15f));
	SetActorTickEnabled(true);
	SetLifeSpan(Life);
}

void AHeftBody::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	Age += DeltaTime;
	const float Alpha = FMath::Clamp(Age / Life, 0.f, 1.f);
	const float Eased = 1.f - FMath::Pow(1.f - Alpha, 3.f);
	SetActorScale3D(FVector(FMath::Lerp(EndScale * 0.15f, EndScale, Eased)));
}

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HeftBody.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInterface;

UCLASS()
class AHeftBody : public AActor
{
	GENERATED_BODY()

public:
	AHeftBody();

	virtual void Tick(float DeltaTime) override;

	void Setup(const FVector& Size, UMaterialInterface* Mat);
	void Burst(float Radius, UMaterialInterface* Mat);

protected:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> Cube;

	UPROPERTY()
	TObjectPtr<UStaticMesh> Sphere;

	float Age = 0.f;
	float Life = 0.f;
	float EndScale = 1.f;
};

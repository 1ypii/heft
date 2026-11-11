#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "HeftCharacter.generated.h"

class UCameraComponent;
class UHeftGrabComponent;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

UCLASS()
class AHeftCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AHeftCharacter();

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	UHeftGrabComponent* GetGrab() const { return GrabComponent; }

protected:
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void Reel(const FInputActionValue& Value);
	void Grab();
	void Throw();
	void Shoot();
	void Explode();
	void Trigger();
	void Reload();
	void PickScenario(int32 Index);
	void SelectParam(const FInputActionValue& Value);
	void AdjustParam(const FInputActionValue& Value);
	void ResetParams();
	void ToggleHud();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UHeftGrabComponent> GrabComponent;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputMappingContext> MappingContext;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> ReelAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> GrabAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> ThrowAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> ShootAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> ExplodeAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> TriggerAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> ReloadAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> ResetParamsAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> ToggleHudAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> SelectParamAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> AdjustParamAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TArray<TObjectPtr<UInputAction>> ScenarioActions;
};

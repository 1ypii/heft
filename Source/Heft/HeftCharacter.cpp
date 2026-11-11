#include "HeftCharacter.h"
#include "HeftGameMode.h"
#include "HeftGrabComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HeftHUD.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "UObject/ConstructorHelpers.h"

static UInputAction* FindAction(const TCHAR* Name)
{
	const FString Path = FString::Printf(TEXT("/Game/Input/%s.%s"), Name, Name);
	return ConstructorHelpers::FObjectFinder<UInputAction>(*Path).Object;
}

AHeftCharacter::AHeftCharacter()
{
	static ConstructorHelpers::FObjectFinder<UInputMappingContext> MappingFind(TEXT("/Game/Input/IMC_Default.IMC_Default"));
	MappingContext = MappingFind.Object;

	MoveAction = FindAction(TEXT("IA_Move"));
	LookAction = FindAction(TEXT("IA_Look"));
	ReelAction = FindAction(TEXT("IA_Reel"));
	JumpAction = FindAction(TEXT("IA_Jump"));
	GrabAction = FindAction(TEXT("IA_Grab"));
	ThrowAction = FindAction(TEXT("IA_Throw"));
	ShootAction = FindAction(TEXT("IA_Shoot"));
	ExplodeAction = FindAction(TEXT("IA_Explode"));
	TriggerAction = FindAction(TEXT("IA_Trigger"));
	ReloadAction = FindAction(TEXT("IA_Reload"));
	ResetParamsAction = FindAction(TEXT("IA_ResetParams"));
	ToggleHudAction = FindAction(TEXT("IA_ToggleHud"));
	SelectParamAction = FindAction(TEXT("IA_SelectParam"));
	AdjustParamAction = FindAction(TEXT("IA_AdjustParam"));
	ScenarioActions.Add(FindAction(TEXT("IA_Scenario1")));
	ScenarioActions.Add(FindAction(TEXT("IA_Scenario2")));
	ScenarioActions.Add(FindAction(TEXT("IA_Scenario3")));
	ScenarioActions.Add(FindAction(TEXT("IA_Scenario4")));

	GetCapsuleComponent()->InitCapsuleSize(36.f, 90.f);
	BaseEyeHeight = 66.f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(GetCapsuleComponent());
	Camera->SetRelativeLocation(FVector(0.f, 0.f, 66.f));
	Camera->bUsePawnControlRotation = true;

	GrabComponent = CreateDefaultSubobject<UHeftGrabComponent>(TEXT("GrabComponent"));

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->MaxWalkSpeed = 620.f;
	Movement->MaxAcceleration = 3200.f;
	Movement->BrakingDecelerationWalking = 2400.f;
	Movement->JumpZVelocity = 560.f;
	Movement->AirControl = 0.4f;
}

void AHeftCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	const APlayerController* PC = Cast<APlayerController>(GetController());
	UEnhancedInputLocalPlayerSubsystem* Subsystem = PC ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()) : nullptr;
	if (!Input || !Subsystem)
		return;

	Input->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AHeftCharacter::Move);
	Input->BindAction(LookAction, ETriggerEvent::Triggered, this, &AHeftCharacter::Look);
	Input->BindAction(ReelAction, ETriggerEvent::Triggered, this, &AHeftCharacter::Reel);
	Input->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
	Input->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

	Input->BindAction(GrabAction, ETriggerEvent::Started, this, &AHeftCharacter::Grab);
	Input->BindAction(ThrowAction, ETriggerEvent::Started, this, &AHeftCharacter::Throw);
	Input->BindAction(ShootAction, ETriggerEvent::Started, this, &AHeftCharacter::Shoot);
	Input->BindAction(ExplodeAction, ETriggerEvent::Started, this, &AHeftCharacter::Explode);
	Input->BindAction(TriggerAction, ETriggerEvent::Started, this, &AHeftCharacter::Trigger);
	Input->BindAction(ReloadAction, ETriggerEvent::Started, this, &AHeftCharacter::Reload);
	Input->BindAction(ResetParamsAction, ETriggerEvent::Started, this, &AHeftCharacter::ResetParams);
	Input->BindAction(ToggleHudAction, ETriggerEvent::Started, this, &AHeftCharacter::ToggleHud);
	Input->BindAction(SelectParamAction, ETriggerEvent::Started, this, &AHeftCharacter::SelectParam);
	Input->BindAction(AdjustParamAction, ETriggerEvent::Started, this, &AHeftCharacter::AdjustParam);

	for (int32 i = 0; i < ScenarioActions.Num(); i++)
		Input->BindAction(ScenarioActions[i], ETriggerEvent::Started, this, &AHeftCharacter::PickScenario, i);

	Subsystem->AddMappingContext(MappingContext, 0);
}

void AHeftCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	AddMovementInput(GetActorForwardVector(), Axis.Y);
	AddMovementInput(GetActorRightVector(), Axis.X);
}

void AHeftCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(-Axis.Y);
}

void AHeftCharacter::Reel(const FInputActionValue& Value)
{
	GrabComponent->Reel(Value.Get<float>());
}

void AHeftCharacter::Grab()
{
	GrabComponent->Toggle();
}

void AHeftCharacter::Throw()
{
	GrabComponent->Throw();
}

void AHeftCharacter::Shoot()
{
	AHeftGameMode* GameMode = GetWorld()->GetAuthGameMode<AHeftGameMode>();
	if (!GameMode)
		return;

	FVector Eye;
	FRotator View;
	GetActorEyesViewPoint(Eye, View);
	GameMode->Shoot(Eye, View.Vector());
}

void AHeftCharacter::Explode()
{
	AHeftGameMode* GameMode = GetWorld()->GetAuthGameMode<AHeftGameMode>();
	if (!GameMode)
		return;

	FVector Eye;
	FRotator View;
	GetActorEyesViewPoint(Eye, View);

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HeftExplode), false, this);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Eye, Eye + View.Vector() * 6000.f, ECC_Visibility, Params))
		GameMode->Explode(Hit.ImpactPoint + Hit.ImpactNormal * 20.f);
}

void AHeftCharacter::Trigger()
{
	if (AHeftGameMode* GameMode = GetWorld()->GetAuthGameMode<AHeftGameMode>())
		GameMode->Trigger();
}

void AHeftCharacter::Reload()
{
	GrabComponent->Release();
	if (AHeftGameMode* GameMode = GetWorld()->GetAuthGameMode<AHeftGameMode>())
		GameMode->Reload();
}

void AHeftCharacter::PickScenario(int32 Index)
{
	GrabComponent->Release();
	if (AHeftGameMode* GameMode = GetWorld()->GetAuthGameMode<AHeftGameMode>())
		GameMode->LoadScenario(Index);
}

void AHeftCharacter::SelectParam(const FInputActionValue& Value)
{
	if (AHeftGameMode* GameMode = GetWorld()->GetAuthGameMode<AHeftGameMode>())
		GameMode->SelectParam(FMath::RoundToInt32(Value.Get<float>()));
}

void AHeftCharacter::AdjustParam(const FInputActionValue& Value)
{
	if (AHeftGameMode* GameMode = GetWorld()->GetAuthGameMode<AHeftGameMode>())
		GameMode->AdjustParam(FMath::RoundToInt32(Value.Get<float>()));
}

void AHeftCharacter::ToggleHud()
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (AHeftHUD* Hud = PC ? PC->GetHUD<AHeftHUD>() : nullptr)
		Hud->Toggle();
}

void AHeftCharacter::ResetParams()
{
	if (AHeftGameMode* GameMode = GetWorld()->GetAuthGameMode<AHeftGameMode>())
		GameMode->ResetParams();
}

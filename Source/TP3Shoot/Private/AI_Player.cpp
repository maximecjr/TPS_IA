// Fill out your copyright notice in the Description page of Project Settings.


#include "AI_Player.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Components/TimelineComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include <TP3Shoot/TP3ShootCharacter.h>
#include "Components/WidgetComponent.h"
#include "Blueprint/UserWidget.h"
#include "HealthBarWidget.h"

//////////////////////////////////////////////////////////////////////////
// ATP3ShootCharacter

AAI_Player::AAI_Player()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// set our turn rate for input
	TurnRateGamepad = 50.f;

	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f); // ...at this rotation rate

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Create SK_Gun
	SK_Gun = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Gun"));
	SK_Gun->SetupAttachment(GetMesh());
	// Set parent socket
	SK_Gun->AttachToComponent(GetMesh(), FAttachmentTransformRules::KeepRelativeTransform, TEXT("GripPoint"));

	Team = 1.0f;
	Life = 100.0f;
	FColor TeamColor = FColor::Red;

	HealthBarComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBar"));
	HealthBarComponent->SetupAttachment(RootComponent);
	HealthBarComponent->SetWidgetSpace(EWidgetSpace::Screen);
	HealthBarComponent->SetDrawSize(FVector2D(150, 50)); // Taille de la barre
	HealthBarComponent->SetRelativeLocation(FVector(0, 0, 120)); // Position au-dessus de l'IA
	HealthBarComponent->SetVisibility(true);

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
}

void AAI_Player::BeginPlay()
{
	Super::BeginPlay();

	if (HealthBarComponent)
	{
		// Assurez-vous que le widget est initialisé
		HealthBarComponent->InitWidget();

		// Vérifiez que le widget est bien assigné
		if (!HealthBarComponent->GetWidget())
		{
			UE_LOG(LogTemp, Warning, TEXT("HealthBarComponent does not have a valid widget!"));
		}
	}
}


void AAI_Player::UpdateHealthBar()
{
	// Vérifiez que le composant existe et contient un widget
	if (!HealthBarComponent) return;

	UUserWidget* Widget = HealthBarComponent->GetWidget();

	// Cast pour vérifier si c'est le bon type
	if (UHealthBarWidget* HealthWidget = Cast<UHealthBarWidget>(Widget))
	{
		// Met à jour la barre de vie
		HealthWidget->HealthPercent = Life / 100.0f;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to cast widget to UHealthBarWidget"));
	}
}


//////////////////////////////////////////////////////////////////////////
// Input

void AAI_Player::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// Set up gameplay key bindings
	check(PlayerInputComponent);
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

	PlayerInputComponent->BindAxis("Move Forward / Backward", this, &AAI_Player::MoveForward);
	PlayerInputComponent->BindAxis("Move Right / Left", this, &AAI_Player::MoveRight);

	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	PlayerInputComponent->BindAxis("Turn Right / Left Mouse", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("Turn Right / Left Gamepad", this, &AAI_Player::TurnAtRate);
	PlayerInputComponent->BindAxis("Look Up / Down Mouse", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("Look Up / Down Gamepad", this, &AAI_Player::LookUpAtRate);

	// handle touch devices
	PlayerInputComponent->BindTouch(IE_Pressed, this, &AAI_Player::TouchStarted);
	PlayerInputComponent->BindTouch(IE_Released, this, &AAI_Player::TouchStopped);

	// Aiming 
	PlayerInputComponent->BindAction("Aiming", IE_Pressed, this, &AAI_Player::Aim);
	PlayerInputComponent->BindAction("Aiming", IE_Released, this, &AAI_Player::StopAiming);

	// Fire
	PlayerInputComponent->BindAction("Fire", IE_Pressed, this, &AAI_Player::Fire);

	// Boost Speed
	PlayerInputComponent->BindAction("BoostSpeed", IE_Pressed, this, &AAI_Player::BoostSpeed);
	PlayerInputComponent->BindAction("BoostSpeed", IE_Released, this, &AAI_Player::RemoveSpeedBoost);
}

void AAI_Player::TouchStarted(ETouchIndex::Type FingerIndex, FVector Location)
{
	Jump();
}

void AAI_Player::TouchStopped(ETouchIndex::Type FingerIndex, FVector Location)
{
	StopJumping();
}

void AAI_Player::Aim()
{
	IsAiming = true;
}

void AAI_Player::StopAiming()
{
	IsAiming = false;
}

void AAI_Player::Fire()
{
	FVector Start, LineTraceEnd, ForwardVector;

	if (IsAiming)
	{
		// Start location is from the camera
		Start = FollowCamera->GetComponentLocation();
		ForwardVector = FollowCamera->GetForwardVector();
	}
	else
	{
		// Start location is from the gun muzzle
		Start = SK_Gun->GetSocketLocation("MuzzleFlash");
		ForwardVector = FollowCamera->GetForwardVector();
	}

	// Calculate end point of the line trace
	LineTraceEnd = Start + (ForwardVector * 10000);

	// Perform line trace to detect any hit
	FHitResult HitResult;
	FCollisionQueryParams TraceParams(FName(TEXT("ProjectileTrace")), true, this);
	TraceParams.bReturnPhysicalMaterial = false;
	TraceParams.AddIgnoredActor(this); // Ignore self

	bool bHit = GetWorld()->LineTraceSingleByChannel(
		HitResult,
		Start,
		LineTraceEnd,
		ECC_Visibility,
		TraceParams
	);
	if (Team == 1.0f)
	{
		// Draw debug line
		DrawDebugLine(
			GetWorld(),
			Start,
			LineTraceEnd,
			FColor::Blue,
			false,
			3.0f,
			5,
			3.0f
		);
	}
	else
	{
		// Draw debug line
		DrawDebugLine(
			GetWorld(),
			Start,
			LineTraceEnd,
			FColor::Red,
			false,
			3.0f,
			5,
			3.0f
		);
	}
	


	// Check if we hit something
	if (bHit)
	{

		AActor* HitActor = HitResult.GetActor();

		// Check if the hit actor is an AI_Player
		if (AAI_Player* HitAIPlayer = Cast<AAI_Player>(HitActor))
		{
			// Check if they are on a different team
			if (HitAIPlayer->Team != this->Team)
			{
				HitAIPlayer->DecreaseHealth(5.0f);
			}
		}
		// Check if the hit actor is a TP3ShootCharacter (player character)
		else if (ATP3ShootCharacter* HitPlayer = Cast<ATP3ShootCharacter>(HitActor))
		{
			// Check if they are on a different team
			if (HitPlayer->Team != this->Team)
			{
				HitPlayer->DecreaseHealth(5.0f);
			}
		}

		// Optionally, spawn impact particles at the hit location
		FireParticle(Start, HitResult.Location);
	}
}



void AAI_Player::DecreaseHealth(float Amount)
{
	Life -= Amount;
	if (Life <= 0)
	{
		Life = 0;
		// Logique de mort (exemple : téléportation, réinitialisation)
		SetActorLocation(FVector(1300, 1200, 90));
		Life = 100.0f;
	}

	UpdateHealthBar(); // Actualise la barre de vie
}


void AAI_Player::BoostSpeed()
{
	// Set Max walking speed to 800
	GetCharacterMovement()->MaxWalkSpeed = 800.f;

	GetWorld()->GetTimerManager().SetTimer(BoostSpeedTimer, [&]()
		{
			// Set Max walking speed to 500
			GetCharacterMovement()->MaxWalkSpeed = 500.f;

			// Clear existing timer boost speed
			GetWorldTimerManager().ClearTimer(BoostSpeedTimer);

		}, 4, false);
}

void AAI_Player::RemoveSpeedBoost()
{
	// Set Max walking speed to 500
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
}


void AAI_Player::FireParticle(FVector Start, FVector Impact)
{
	if (!ParticleStart || !ParticleImpact) return;

	FTransform ParticleT;

	ParticleT.SetLocation(Start);

	ParticleT.SetScale3D(FVector(0.25, 0.25, 0.25));

	UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ParticleStart, ParticleT, true);

	// Spawn particle at impact point
	ParticleT.SetLocation(Impact);

	UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ParticleImpact, ParticleT, true);

}

void AAI_Player::TurnAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerYawInput(Rate * TurnRateGamepad * GetWorld()->GetDeltaSeconds());
}

void AAI_Player::LookUpAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * TurnRateGamepad * GetWorld()->GetDeltaSeconds());
}

void AAI_Player::MoveForward(float Value)
{
	if ((Controller != nullptr) && (Value != 0.0f))
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		AddMovementInput(Direction, Value);
	}
}

void AAI_Player::MoveRight(float Value)
{
	if ((Controller != nullptr) && (Value != 0.0f))
	{
		// find out which way is right
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get right vector 
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		// add movement in that direction
		AddMovementInput(Direction, Value);
	}
}

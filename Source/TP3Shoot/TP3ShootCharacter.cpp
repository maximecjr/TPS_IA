// Copyright Epic Games, Inc. All Rights Reserved.

#include "TP3ShootCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Components/TimelineComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "AI_Player.h"


//////////////////////////////////////////////////////////////////////////
// ATP3ShootCharacter

ATP3ShootCharacter::ATP3ShootCharacter()
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
	FColor color = FColor::Blue;
	Life = 20.0f;

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
}

//////////////////////////////////////////////////////////////////////////
// Input

void ATP3ShootCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// Set up gameplay key bindings
	check(PlayerInputComponent);
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

	PlayerInputComponent->BindAxis("Move Forward / Backward", this, &ATP3ShootCharacter::MoveForward);
	PlayerInputComponent->BindAxis("Move Right / Left", this, &ATP3ShootCharacter::MoveRight);

	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	PlayerInputComponent->BindAxis("Turn Right / Left Mouse", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("Turn Right / Left Gamepad", this, &ATP3ShootCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("Look Up / Down Mouse", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("Look Up / Down Gamepad", this, &ATP3ShootCharacter::LookUpAtRate);

	// handle touch devices
	PlayerInputComponent->BindTouch(IE_Pressed, this, &ATP3ShootCharacter::TouchStarted);
	PlayerInputComponent->BindTouch(IE_Released, this, &ATP3ShootCharacter::TouchStopped);

	// Aiming 
	PlayerInputComponent->BindAction("Aiming", IE_Pressed, this, &ATP3ShootCharacter::Aim);
	PlayerInputComponent->BindAction("Aiming", IE_Released, this, &ATP3ShootCharacter::StopAiming);

	// Fire
	PlayerInputComponent->BindAction("Fire", IE_Pressed, this, &ATP3ShootCharacter::Fire);

	// Boost Speed
	PlayerInputComponent->BindAction("BoostSpeed", IE_Pressed, this, &ATP3ShootCharacter::BoostSpeed);
	PlayerInputComponent->BindAction("BoostSpeed", IE_Released, this, &ATP3ShootCharacter::RemoveSpeedBoost);
}

void ATP3ShootCharacter::TouchStarted(ETouchIndex::Type FingerIndex, FVector Location)
{
	Jump();
}

void ATP3ShootCharacter::TouchStopped(ETouchIndex::Type FingerIndex, FVector Location)
{
	StopJumping();
}

void ATP3ShootCharacter::Aim()
{
	IsAiming = true;
}

void ATP3ShootCharacter::StopAiming()
{
	IsAiming = false;
}

void ATP3ShootCharacter::Fire()
{
	FVector Start, LineTraceEnd, ForwardVector;
	FHitResult HitResult;

	// Choisissez le point de départ et de fin en fonction de l'état d'aim (comme dans votre code actuel)
	if (IsAiming)
	{
		Start = FollowCamera->GetComponentLocation();
		ForwardVector = FollowCamera->GetForwardVector();
	}
	else
	{
		Start = SK_Gun->GetSocketLocation("MuzzleFlash");
		ForwardVector = FollowCamera->GetForwardVector();
	}
	LineTraceEnd = Start + (ForwardVector * 10000);

	// Effectuez un Line Trace pour détecter un hit
	bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, Start, LineTraceEnd, ECC_Pawn);


	if (bHit)
	{
		// debug screen showing which object was hit

		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("Hit Object: %s"), *HitResult.GetActor()->GetName()));
		// Vérifiez si l'objet touché est un AI_Player
		if (HitResult.GetActor()->IsA(AAI_Player::StaticClass()))
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("IA ciblee")));
			// Cast pour accéder à la santé de l'AI_Player
			AAI_Player* AIPlayer = Cast<AAI_Player>(HitResult.GetActor());
			if (AIPlayer)
			{
				// debug screen AI team
				GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("AI Team: %f"), AIPlayer->Team));
				// debug screen player team
				GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("Player Team: %f"), Team));
				// Vérifie si l'AI_Player est de la même équipe que le joueur
				if (AIPlayer->Team == Team)
				{
					// debug screen message
					GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Friendly Fire"));
					return;
				}
				// Réduisez la vie de l'AI_Player
				AIPlayer->DecreaseHealth(5.0f);
				// debug screen displaying ai health
				GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("AI Health: %f"), AIPlayer->Life));
			}
		}

		// Dessinez la ligne de débogage pour la ligne de tir
		if (Team == 2.0f)
		{
			DrawDebugLine(GetWorld(), Start, HitResult.ImpactPoint, FColor::Red, false, 3.0f, 5, 3.0f);
		}
		else
		{
			DrawDebugLine(GetWorld(), Start, HitResult.ImpactPoint, FColor::Blue, false, 3.0f, 5, 3.0f);
		}
	}
}

void ATP3ShootCharacter::DecreaseHealth(float Amount)
{
	Life -= Amount;
	if (Life <= 0)
	{
		// Logique de mort de l'AI_Player
		// teleport it to 1300 1200 90
		SetActorLocation(FVector(1300, 1200, 90));
		// reset life
		Life = 20.0f;
	}
}



void ATP3ShootCharacter::BoostSpeed()
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

void ATP3ShootCharacter::RemoveSpeedBoost()
{
	// Set Max walking speed to 500
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
}


void ATP3ShootCharacter::FireParticle(FVector Start, FVector Impact)
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

void ATP3ShootCharacter::TurnAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerYawInput(Rate * TurnRateGamepad * GetWorld()->GetDeltaSeconds());
}

void ATP3ShootCharacter::LookUpAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * TurnRateGamepad * GetWorld()->GetDeltaSeconds());
}

void ATP3ShootCharacter::MoveForward(float Value)
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

void ATP3ShootCharacter::MoveRight(float Value)
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

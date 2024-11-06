// Copyright Epic Games, Inc. All Rights Reserved.

#include "TPS_IAGameMode.h"
#include "TPS_IACharacter.h"
#include "UObject/ConstructorHelpers.h"

ATPS_IAGameMode::ATPS_IAGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}

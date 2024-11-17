// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HealthBarWidget.generated.h"

UCLASS()
class TP3SHOOT_API UHealthBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Variable pour représenter la vie
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float HealthPercent;
};

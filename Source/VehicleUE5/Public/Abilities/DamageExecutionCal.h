// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"
#include "DamageExecutionCal.generated.h"

/**
 * 
 */
UCLASS()
class VEHICLEUE5_API UDamageExecutionCal : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

	UDamageExecutionCal();

	virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, OUT FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;
	
};

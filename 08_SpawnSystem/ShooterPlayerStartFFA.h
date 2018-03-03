// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "ShooterPlayerStart.h"
#include "ShooterPlayerStartFFA.generated.h"

UCLASS()
class SHOOTERGAME_API AShooterPlayerStartFFA : public AShooterPlayerStart
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category = "SpawnControl")
	bool bUseAsInitialSpawnPoint;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	// A UBillboardComponent to hold Icon sprite
	UBillboardComponent* SpriteComponentFFA;
#endif // WITH_EDITORONLY_DATA
};

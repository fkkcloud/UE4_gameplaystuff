// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "ShooterPlayerStart.h"
#include "ShooterPlayerStartTDM.generated.h"

UENUM()
enum class ETDMState : uint8
{
	ASide,
	BSide,
	Neutral,
};

UCLASS()
class SHOOTERGAME_API AShooterPlayerStartTDM : public AShooterPlayerStart
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category = "SpawnControl")
	ETDMState TDMState;

	UPROPERTY(EditAnywhere, Category = "SpawnControl")
	bool bUseAsInitialSpawnPoint;

	virtual void UpdateScore() override;

	virtual bool IsRegenable() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	// A UBillboardComponent to hold Icon sprite
	UBillboardComponent* SpriteComponentASide;

	UPROPERTY()
	// A UBillboardComponent to hold Icon sprite
	UBillboardComponent* SpriteComponentBSide;

	UPROPERTY()
	// A UBillboardComponent to hold Icon sprite
	UBillboardComponent* SpriteComponentNeuturalSide;

	UPROPERTY()
	// A UBillboardComponent to hold Icon sprite
	UBillboardComponent* SpriteComponentTDM;
#endif // WITH_EDITORONLY_DATA

protected:
	int32 GetSpawnPointTeamNum();
};

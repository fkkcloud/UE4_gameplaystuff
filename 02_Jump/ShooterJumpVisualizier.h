// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Components/SceneComponent.h"
#include "ShooterJumpVisualizier.generated.h"

/**
 * 
 */
UCLASS()
class SHOOTERGAME_API UShooterJumpVisualizier : public USceneComponent
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Setup")
	UStaticMeshComponent* SourceComponent;

	UPROPERTY(VisibleAnywhere, Category = "Setup")
	UStaticMeshComponent* DestinationVisualizer;

public:
	UPROPERTY()
	TArray<UStaticMeshComponent*> VizMeshes;

	void InitializeParms();
	void Draw();
	void SetHiddenInGameAll(bool Value);
	void AdjustAlpha(float NewAlpha);

	void SetDestinationVisualizerLoc(const FVector & Location);

	void SetDestinationVisualizerScale(const FVector & Scale3D);
	void SetIndicationVisualizerScale(const FVector & Scale3D);

	void SetDestinationVisualizerRotator(const FRotator & Rotate3D);
	void SetIndicationVisualizerRotater(const FRotator & Rotate3D);

	void SetHiddenInGameIndicators(bool Value);
	void SetHiddenInGameDestination(bool Value);

private:
	int Count;
	
	float JumpTime;
	float GravityZ;

	FVector JumpPadLocation;
	FVector JumpVelocity;
	FVector JumpPadTarget;
	
	class AShooterJumpMovement* JumpPad;
};

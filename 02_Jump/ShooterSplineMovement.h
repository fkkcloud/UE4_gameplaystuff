/*
ShooterSplineMovement

Description
	Abstract Class
	Class that will allow you to move ACharacter along with a spline.
	This class is purposed to derive other classes that would have
	more specific instruction on spline shape and movement.

	To Do
	Handle ReceiveOverlap of actors wants to jump 
	--> make PendingSplineMovementActor pool and launch they by each tick
*/

#pragma once

#include "GameFramework/Actor.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "ShooterSplineMovement.generated.h"

USTRUCT()
struct FSplineCharacters
{
	GENERATED_USTRUCT_BODY()

	AShooterCharacter* Character;

	FVector Velocity;

	EMovementMode CharacterMovementMode;

	float CurrentPlayedTime;
};


UCLASS()
class SHOOTERGAME_API AShooterSplineMovement : public AActor
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	USceneComponent* SceneComp;

	UPROPERTY()
	USplineComponent* SplineComp;

	UPROPERTY(EditAnywhere, Category = "SplineMovement")
	bool bUseCustomSpline;

	UPROPERTY(EditAnywhere, Category = "SplineMovement")
	AActor* CustomSpline;

	UPROPERTY(EditAnywhere, Category = "SplineMovement")
	AActor* DestinationActor;

	UPROPERTY(EditAnywhere, Category = "SplineMovement")
	float Time;

	UPROPERTY(EditAnywhere, Category = "SplineMovement")
		float VelocityCoefficient;

	UFUNCTION(BlueprintCallable, Category = "SplineMovement")
	virtual void MoveCharacterAlongSpline(AShooterCharacter* InCharacter);

	UFUNCTION(BlueprintCallable, Category = "SplineMovement")
	bool IsMoveAvaillable();

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	virtual void Tick(float DeltaSeconds) override;

	virtual void PostInitProperties() override;

protected:
	bool MoveAvailability;

	int32 SplinePointCount;

	float DistAtSpline;
	float SplineLength;
	float ResetTime;
	float MinimumDistThreshold;

	TArray<FSplineCharacters> SplineCharacters;

	FVector Velocity;

	TArray<FVector> SplinePoints;

	void InitSpline();

	// Artificially generated spline
	virtual void SetSpline();

	// Custom generated spline
	virtual void SetCustomSpline();

	/*
	UFUNCTION(Reliable, Server, WithValidation)
	virtual void ServerSplineMovement(AShooterCharacter* InCharacter);

	UFUNCTION(Reliable, netmulticast, WithValidation)
	virtual void MulticastSplineMovement(AShooterCharacter* InCharacter);
	*/

	virtual void MovementImplementation(float DeltaSeconds);
	
	virtual void FinishSplineMovement(AShooterCharacter* InCharacter, EMovementMode CharacterMovementMode);

	void InitSplinePoints(int32 PointCount);
	void ResetMoveAvaillable();

	virtual void CalculateOffset(const AShooterCharacter* InCharacter, FVector & InVector);
	
	void ResetMovementToDefault(AShooterCharacter* InCharacter, EMovementMode CharacterMovementMode);

	FVector StartLocation;
	FVector EndLocation;

private:
	FTimerHandle ToggleMovementModeHandle;
	FTimerHandle ToggleAvailabilityHandle;
};

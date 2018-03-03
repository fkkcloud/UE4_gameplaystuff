/*
ShooterJumpMovement

Description
	AShooterJumpMovment class derived from AShooterSplineMovement,
	specific spline shape and movement to fit Jump behavior along with the spline.
	Spline movement is non-linear, inverse hyperbolic tangent function to fake
	physics behavior - gravity free fall.
*/

#pragma once

#include "ShooterSplineMovement.h"
#include "ShooterJumpMovement.generated.h"

UENUM()
enum class EJumpType : uint8
{
	Spline,
	Launch,
	EJumpType_MAX,
};

UENUM()
enum class EJumpState : uint8
{
	Destination1,
	Destination2,
	EJumpState_MAX,
};

UCLASS()
class SHOOTERGAME_API AShooterJumpMovement : public AShooterSplineMovement
{
	GENERATED_UCLASS_BODY()

	/** choice between "Launch" or "Spline". */
	UPROPERTY(EditAnywhere, Category = "JumpType")
	EJumpType JumpType;

	UPROPERTY(EditAnywhere, Category = "SplineMovement")
	float JumpImpulse;
	
	UPROPERTY(EditAnywhere, Category = "SplineMovement")
	float NearJumpPadSlope;

	UPROPERTY(EditAnywhere, Category = "SplineMovement")
	float NearArrivalSlope;

	// How high it will jump, also will take longer time as the value gets greater
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LaunchMovement", meta = (ClampMin = "0.1"))
	float JumpHeight;

	// Actor that will be used for destination for jump
	UPROPERTY(EditAnywhere, Category = "LaunchMovement")
	AActor* LaunchDestinationActor;

	// How high it will jump, also will take longer time as the value gets greater
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LaunchMovement", meta = (ClampMin = "0.1"))
		float JumpHeight2;

	// Actor that will be used for destination for jump
	UPROPERTY(EditAnywhere, Category = "LaunchMovement")
		AActor* LaunchDestinationActor2;

	// Factor for secondary bounce in XY
	UPROPERTY(EditAnywhere, Category = "LaunchMovement")
	float SecondaryBounceXYFactor;

	//  Factor for secondary bounce in Z
	UPROPERTY(EditAnywhere, Category = "LaunchMovement")
	float SecondaryBounceZFactor;

	UPROPERTY(EditAnywhere, Category = "LaunchMovement")
		float SlowDownPoint;

	UPROPERTY(EditAnywhere, Category = "LaunchMovement")
		float TimeDilation;

	UPROPERTY(EditAnywhere, Category = "LaunchMovement")
		float JumpDisabledTime;

	UPROPERTY()
	class UShooterJumpVisualizier* JumpVisualizer;

	UPROPERTY()
	class UShooterJumpIndicator* JumpIndicatorComp;

	// Minimum distance from character to jump-pad to see the path
	UPROPERTY(EditAnywhere, Category = "Launch Visualization Setup")
	float MinimumVisibleDist;

	// Visualizer meshes - path
	UPROPERTY(EditAnywhere, Category = "Launch Visualization Setup")
	UStaticMesh* JumpVisualizerPathMesh;

	// Visualizer meshes - target
	UPROPERTY(EditAnywhere, Category = "Launch Visualization Setup")
	UStaticMesh* JumpVisualizerTargetMesh;

	// How many path indicator mesh will iterate/render through the jump path
	UPROPERTY(EditAnywhere, Category = "Launch Visualization Setup", meta = (ClampMin = "2"))
	int32 VisualizationStepSize;

	// How many path indicator mesh will iterate/render through the jump path
	UPROPERTY(EditAnywhere, Category = "Launch Visualization Setup", meta = (ClampMin = "2"))
		int32 VisualizationStepSize2;

	// Visualizer Offset - target location
	UPROPERTY(EditAnywhere, Category = "Launch Visualization Setup")
	FVector DestinationVisualizerLoc;

	// Visualizer Offset - target Scale
	UPROPERTY(EditAnywhere, Category = "Launch Visualization Setup")
	FVector DestinationVisualizerScale;

	// Visualizer Offset - path Scale
	UPROPERTY(EditAnywhere, Category = "Launch Visualization Setup")
	FVector IndicationVisualizerScale;

	// Visualizer Offset - target rotation
	UPROPERTY(EditAnywhere, Category = "Launch Visualization Setup")
	FRotator DestinationVisualizerRotator;

	// Visualizer Offset - path rotation
	UPROPERTY(EditAnywhere, Category = "Launch Visualization Setup")
	FRotator IndicationVisualizerRotator;

	virtual void MoveCharacterAlongSpline(AShooterCharacter* InCharacter) override;

	UFUNCTION(BlueprintNativeEvent, Category = "LaunchMovement")
		void PlaySoundAndAnim();

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
#endif

public:
	virtual void Tick(float DeltaSeconds) override;

	virtual void PostInitializeComponents() override;

	FVector CalculateJumpVelocity(AActor* JumpActor, AActor* LaunchDestination);

	float CalculateJumpPathAlpha();

protected:

	UPROPERTY()
	TArray<AShooterCharacter*> PendingJumpCharacters;

	AShooterCharacter* TargetCharcter;

	bool CanLaunch(AShooterCharacter* InCharacter);

	virtual void SetSpline() override;

	virtual void SetCustomSpline() override;
	
	virtual void MovementImplementation(float DeltaSeconds) override;

	virtual void FinishSplineMovement(AShooterCharacter* InCharacter, EMovementMode CharacterMovementMode) override;

	private:
		float timeSinceLastLaunch;

		EJumpState jumpState;

		float TransitionStartTime;

		void RotateJumppad(float DeltaSeconds);

		FRotator NewRotation;

	public:
		UFUNCTION(BlueprintCallable, reliable, netmulticast, Category = "LaunchMovement")
		void MulticastTransitionJumppad(EJumpState newState);

		UPROPERTY(EditAnywhere, Category = "LaunchMovement")
			float TransitionTime;

		EJumpState GetJumpstate();


};

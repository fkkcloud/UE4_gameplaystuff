// Fill out your copyright notice in the Description page of Project Settings.

#include "ShooterGame.h"
#include "ShooterJumpIndicator.h"
#include "ShooterJumpMovement.h"
#include "ShooterJumpVisualizier.h"
#include "ShooterLevelScriptActor.h"
#include "ShooterBot.h"
#include "ShooterCharacterJumpComponent.h"

AShooterJumpMovement::AShooterJumpMovement(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	JumpType = EJumpType::Launch;
	SplinePointCount = 5;

	JumpIndicatorComp = ObjectInitializer.CreateDefaultSubobject<UShooterJumpIndicator>(this, TEXT("JumpPadComp"));
	JumpIndicatorComp->PostPhysicsComponentTick.bCanEverTick = false;
	JumpIndicatorComp->SetupAttachment(RootComponent);
	JumpIndicatorComp->SetVisibility(false, true);

	JumpVisualizer = ObjectInitializer.CreateDefaultSubobject<UShooterJumpVisualizier>(this, TEXT("JumpVizComp"));
	JumpVisualizer->SetupAttachment(RootComponent);
	JumpVisualizer->SetRelativeLocation(FVector::ZeroVector);

	VisualizationStepSize = 10;
	VisualizationStepSize2 = 10;

	DestinationVisualizerLoc = FVector(0.0f, 0.0f, 0.0f);
	
	DestinationVisualizerScale = FVector(1.0f, 1.0f, 1.0f);
	IndicationVisualizerScale = FVector(1.0f, 1.0f, 1.0f);

	DestinationVisualizerRotator = FRotator(0.0f, 0.0f, 0.0f);
	IndicationVisualizerRotator = FRotator(0.0f, 0.0f, 0.0f);

	MinimumVisibleDist = 300.0f;

	SlowDownPoint = .75f;
	TimeDilation = .5f;
	JumpDisabledTime = .75f;
	timeSinceLastLaunch = 10.f;
	jumpState = EJumpState::Destination1;
	JumpHeight = 1.f;
	JumpHeight2 = 1.f;
}

void AShooterJumpMovement::Tick(float DeltaSeconds)
{
	AShooterGameState* GameState = Cast<AShooterGameState>(GetWorld()->GameState);

	if (!GameState)
		return;

	if (GameState->GetMatchState() != MatchState::InProgress)
		return;

	CalculateJumpPathAlpha();

	timeSinceLastLaunch += DeltaSeconds;

	RotateJumppad(DeltaSeconds);

	if (PendingJumpCharacters.Num() > 0)
	{
		AShooterCharacter* Jumper = PendingJumpCharacters[0];
		if (timeSinceLastLaunch > JumpDisabledTime)
		{
			if (Jumper->IsAlive())
			{
				if (AShooterBot* bot = Cast<AShooterBot>(Jumper))
				{
					bot->OnStopRunning();
					//bot->GetMovementComponent()->StopActiveMovement();
				}

				if (Jumper->IsInTankTransition)
				{
					return;
				}

				PlaySoundAndAnim();
				Jumper->JumpComponent->Init(this, Jumper, SlowDownPoint, TimeDilation);
				//Jumper->JumpComponent->Start();
				//if (!Cast<AShooterBot>(Jumper))
				{
					timeSinceLastLaunch = 0.f;
				}
				if (Role == ROLE_Authority)
				{
					if (AShooterLevelScriptActor* levelActor = Cast<AShooterLevelScriptActor>(GetWorld()->GetLevelScriptActor()))
					{
						levelActor->JumpPadUsed(this, Jumper);
					}
				}
			}
			PendingJumpCharacters.RemoveAt(0);
		}
		else
		{
			if (AShooterBot* bot = Cast<AShooterBot>(Jumper))
			{
				bot->OnStopRunning();
				bot->GetMovementComponent()->StopActiveMovement();
			}
		}
	}

	Super::Tick(DeltaSeconds);
}

void AShooterJumpMovement::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if (JumpVisualizerPathMesh)
	{
		JumpVisualizer->SourceComponent->SetStaticMesh(JumpVisualizerPathMesh);
	}

	if (JumpVisualizerTargetMesh)
	{
		JumpVisualizer->DestinationVisualizer->SetStaticMesh(JumpVisualizerTargetMesh);
	}

	// This is only for editor at this moment
	JumpIndicatorComp->SetHiddenInGame(true, true);

	// Draw the jump trajectory only for once when game begins
	JumpVisualizer->InitializeParms();
	JumpVisualizer->Draw();

	// Initially, it is hidden until any pawn gets near it
	JumpVisualizer->SetHiddenInGameAll(true);

	JumpVisualizer->SetDestinationVisualizerScale(DestinationVisualizerScale);
	JumpVisualizer->SetIndicationVisualizerScale(IndicationVisualizerScale);

	JumpVisualizer->SetDestinationVisualizerRotator(DestinationVisualizerRotator);
	JumpVisualizer->SetIndicationVisualizerRotater(IndicationVisualizerRotator);

	FVector newForward = FVector::ZeroVector;
	if (LaunchDestinationActor)
	{
		JumpVisualizer->SetDestinationVisualizerLoc(LaunchDestinationActor->GetActorLocation());
		newForward = LaunchDestinationActor->GetActorLocation() - GetActorLocation();
	}
	newForward.Normalize();

	NewRotation = newForward.Rotation();// FRotationMatrix::MakeFromX(newForward - GetActorForwardVector()).Rotator(); //See FindLookAtRotation in KismetMathLibrary
	NewRotation.Pitch = 0.f;
}

#if WITH_EDITOR
void AShooterJumpMovement::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (JumpType == EJumpType::Launch)
	{
		SplineComp->SetVisibility(false, true);
		JumpIndicatorComp->SetVisibility(true, true);
	}
	else if (JumpType == EJumpType::Spline)
	{
		SplineComp->SetVisibility(true, true);
		JumpIndicatorComp->SetVisibility(false, true);
	}
}
#endif

void AShooterJumpMovement::SetCustomSpline()
{
	// If jump type is using launch method, do not calculate the spline
	if (JumpType == EJumpType::Launch)
	{
		for (int i = 0; i < SplinePoints.Num(); ++i)
		{
			SplinePoints[i] = FVector(10000.0f, 10000.0f, 10000.0f);
		}
		SplineComp->SetSplineLocalPoints(SplinePoints);
		return;
	}

	Super::SetCustomSpline();
}

void AShooterJumpMovement::SetSpline()
{
	// If jump type is using launch method, do not calculate the spline
	if (JumpType == EJumpType::Launch)
	{
		for (int i = 0; i < SplinePoints.Num(); ++i)
		{
			SplinePoints[i] = FVector(10000.0f, 10000.0f, 10000.0f);
		}
		SplineComp->SetSplineLocalPoints(SplinePoints);
		return;
	}

	FVector StartLocation = FVector::ZeroVector;
	FVector TargetLocation = FVector::ZeroVector;

	if (StartLocation == FVector::ZeroVector)
	{
		StartLocation = GetActorLocation();
	}

	if (DestinationActor)
	{
		TargetLocation = DestinationActor->GetActorLocation();
	}
	else
	{
		TargetLocation = GetActorLocation();

		// When there is no DestinationActor, also reset JumpImpulse.
		JumpImpulse = 0.0f;
	}

	//float CharacterHeightHalf = TargetCharacter ? TargetCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.0f;

	if (SplinePoints.Num() < SplinePointCount)
	{
		return;
	}

	SplinePoints[0] = StartLocation;

	SplinePoints[2] = FVector((TargetLocation.X + StartLocation.X) * 0.5f, (TargetLocation.Y + StartLocation.Y) * 0.5f, JumpImpulse);

 	SplinePoints[4] = TargetLocation + FVector(0.0f, 0.0f, 0.f);//Functionality not setup here

	SplinePoints[1] = FVector((SplinePoints[0].X + SplinePoints[2].X) * 0.5f, (SplinePoints[0].Y + SplinePoints[2].Y) * 0.5f, JumpImpulse * 0.825f + NearJumpPadSlope);

	SplinePoints[3] = FVector((SplinePoints[2].X + SplinePoints[4].X) * 0.5f, (SplinePoints[2].Y + SplinePoints[4].Y) * 0.5f, JumpImpulse * 0.825f + NearArrivalSlope);

	SplineComp->SetSplineLocalPoints(SplinePoints);
	SplineLength = SplineComp->GetSplineLength();
}

void AShooterJumpMovement::MovementImplementation(float DeltaSeconds)
{
	if (SplineCharacters.Num() > 0)
	{
		for (int CharacterItr = 0; CharacterItr < SplineCharacters.Num(); CharacterItr++)
		{
			AShooterCharacter* TargetCharacter = SplineCharacters[CharacterItr].Character;

			if (TargetCharacter)
			{
				SplineCharacters[CharacterItr].CurrentPlayedTime += DeltaSeconds;

				FVector2D GraphTrimRange = FVector2D(-0.8f, 0.8f);
				float RemappedValue = FMath::GetMappedRangeValueClamped(FVector2D(0.0f, Time), GraphTrimRange, FMath::Clamp(SplineCharacters[CharacterItr].CurrentPlayedTime, -0.0001f, Time + 0.0001f));

				float MinGraphHeuristic = atanh(GraphTrimRange.X);
				float MaxGraphHeuristic = atanh(GraphTrimRange.Y);
				FVector2D TimeRange = FVector2D(MinGraphHeuristic, MaxGraphHeuristic);

				FVector2D TargetRange = FVector2D(0.0f, SplineLength);

				bool IsCharacterCloseEnoughToEndPoint = FVector::Dist(TargetCharacter->GetActorLocation(), SplineComp->GetWorldLocationAtDistanceAlongSpline(SplineComp->GetSplineLength())) < MinimumDistThreshold;

				if (SplineCharacters[CharacterItr].CurrentPlayedTime >= Time || IsCharacterCloseEnoughToEndPoint)
				{
					FinishSplineMovement(TargetCharacter, SplineCharacters[CharacterItr].CharacterMovementMode);
					SplineCharacters.RemoveAt(CharacterItr);
					CharacterItr--;
				}
				else
				{
					DistAtSpline = FMath::GetMappedRangeValueClamped(TimeRange, TargetRange, atanh(RemappedValue));

					FVector UpdatedPos = SplineComp->GetWorldLocationAtDistanceAlongSpline(DistAtSpline);
					TargetCharacter->SetActorLocation(UpdatedPos, false, nullptr, ETeleportType::TeleportPhysics);
				}
			}
		}
	}
}

// this is called from BP
void AShooterJumpMovement::MoveCharacterAlongSpline(AShooterCharacter* InCharacter)
{
	if (!InCharacter)
	{
		return;
	}

	if (JumpType == EJumpType::Launch)
	{
		if (Role >= ROLE_AutonomousProxy)
		{
			// Add the actor to be launched if it hasn't already
			if (!PendingJumpCharacters.Contains(InCharacter) && CanLaunch(InCharacter))
			{
				//InCharacter->SetRunning(false, false);
				InCharacter->SetIsOnSplineMovement(true);
				InCharacter->IsBoosting = false;
				if (InCharacter->IsInTank)
				{
					InCharacter->SetRunning(false, false);
				}
				PendingJumpCharacters.Add(InCharacter);
			}
		}
		return;
	}
	else
	{
		//Launch functionality is not setup
		InCharacter->SetIsOnJumpSplineMovement(true);
		InCharacter->GetCharacterMovement()->GravityScale = 1.0f;
		if (Role == ROLE_Authority)
		{
			if (AShooterLevelScriptActor* levelActor = Cast<AShooterLevelScriptActor>(GetWorld()->GetLevelScriptActor()))
			{
				levelActor->JumpPadUsed(this, InCharacter);
			}
		}
		Super::MoveCharacterAlongSpline(InCharacter);
		return;
	}
}

void AShooterJumpMovement::FinishSplineMovement(AShooterCharacter* InCharacter, EMovementMode CharacterMovementMode)
{
	InCharacter->SetIsOnJumpSplineMovement(false);
	Super::FinishSplineMovement(InCharacter, CharacterMovementMode);
}

FVector AShooterJumpMovement::CalculateJumpVelocity(AActor* JumpActor, AActor* LaunchDestination)
{
	float gravity = UPhysicsSettings::Get()->DefaultGravityZ;

	// N.B. GetWorld is NULL when exiting PIE
	if (GetWorld() && IsInGameThread())
	{
		gravity = GetWorld()->GetGravityZ();
	}

	// For Indicator purpose
	FVector JumpTarget = LaunchDestination->GetActorLocation();

	FVector Target = JumpTarget - GetActorLocation(); // JumpActor->GetActorLocation();

	//FVector Target = ActorToWorld().TransformPosition(JumpTarget) - JumpActor->GetActorLocation();

	float SizeZ = Target.Z / JumpHeight + 0.5f * -gravity * JumpHeight;
	float SizeXY = Target.Size2D() / JumpHeight;

	FVector Velocity = Target.GetSafeNormal2D() * SizeXY + FVector(0.0f, 0.0f, SizeZ);

	return Velocity;
}

float AShooterJumpMovement::CalculateJumpPathAlpha()
{
	if (!GetWorld())
		return 0.0f;

	APlayerController* playerController = GetWorld()->GetFirstLocalPlayerFromController()->PlayerController;

	if (!playerController)
		return 0.0f;

	AShooterCharacter* TestPawn = Cast<AShooterCharacter>(playerController->GetPawn());

	if (!TestPawn || TestPawn->IsOnJumpMovement())
		return 0.0f;

	check(playerController->IsLocalPlayerController());

	float DefaultVisibleDistBoundarySq = MinimumVisibleDist * MinimumVisibleDist;

	float DistSq = (GetActorLocation() - TestPawn->GetActorLocation()).SizeSquared();
	if (DistSq < DefaultVisibleDistBoundarySq)
	{
		FVector VectorPointOfInterest = TestPawn->GetControlRotation().Vector().GetSafeNormal();
		FVector VectorJumpPadToPawn = (TestPawn->GetActorLocation() - GetActorLocation()).GetSafeNormal();

		float ResultDot = FVector::DotProduct(VectorPointOfInterest, VectorJumpPadToPawn);

		static const float JUMPPAND_VIZ_FOV = -0.7f;

		if (ResultDot < JUMPPAND_VIZ_FOV && TestPawn->GetCharacterMovement() && TestPawn->GetCharacterMovement()->CurrentFloor.IsWalkableFloor())
		{
			JumpVisualizer->SetHiddenInGameAll(false);

			float CaculatedAlpha = FMath::GetMappedRangeValueClamped(FVector2D(0.0f, DefaultVisibleDistBoundarySq), FVector2D(0.65f, 0.0f), DistSq);
			return CaculatedAlpha;
		}
		else
		{
			JumpVisualizer->SetHiddenInGameAll(true);
		}
	}
	else
	{
		JumpVisualizer->SetHiddenInGameAll(true);
	}

	return 0.0f;
}

bool AShooterJumpMovement::CanLaunch(AShooterCharacter* InCharacter)
{
	return (InCharacter != NULL) && (InCharacter->Role >= ROLE_AutonomousProxy) && !InCharacter->IsOnSplineMovement();
}

void AShooterJumpMovement::PlaySoundAndAnim_Implementation()
{

}

void AShooterJumpMovement::MulticastTransitionJumppad_Implementation(EJumpState newState)
{
	if (!LaunchDestinationActor2)
		return;

	jumpState = newState;

	//Set rotation end point
	TransitionStartTime = GetWorld()->GetTimeSeconds();

	if (jumpState == EJumpState::Destination2)
	{
		FVector newForward = LaunchDestinationActor2->GetActorLocation() - GetActorLocation();
		newForward.Normalize();

		NewRotation = newForward.Rotation(); // FRotationMatrix::MakeFromX(newForward - GetActorForwardVector()).Rotator(); //See FindLookAtRotation in KismetMathLibrary
		NewRotation.Pitch = 0.f;
	}
	else
	{
		FVector newForward = LaunchDestinationActor->GetActorLocation() - GetActorLocation();
		newForward.Normalize();

		NewRotation = newForward.Rotation();// FRotationMatrix::MakeFromX(newForward - GetActorForwardVector()).Rotator(); //See FindLookAtRotation in KismetMathLibrary
		NewRotation.Pitch = 0.f;
	}

	JumpVisualizer->InitializeParms();
	JumpVisualizer->Draw();
}

void AShooterJumpMovement::RotateJumppad(float DeltaSeconds)
{
	//Lerp between rotation start and rotation end
	FRotator myRotation = GetActorRotation();
	
	if ((FMath::Abs(NewRotation.Yaw - myRotation.Yaw) > .01))
	{
		FRotator newRot = FMath::RInterpTo(myRotation, NewRotation, DeltaSeconds, TransitionTime);
		newRot.Pitch = 0.f;

		if (TransitionStartTime + TransitionTime <= GetWorld()->GetTimeSeconds())
		{
			newRot = NewRotation;
		}

		SetActorRotation(newRot);
		myRotation = GetActorRotation();
	}
}

EJumpState AShooterJumpMovement::GetJumpstate()
{
	return jumpState;
}
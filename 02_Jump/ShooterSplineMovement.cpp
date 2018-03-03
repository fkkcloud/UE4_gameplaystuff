// Fill out your copyright notice in the Description page of Project Settings.

#include "ShooterGame.h"
#include "Movement/ShooterSplineMovement.h"

AShooterSplineMovement::AShooterSplineMovement(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, Time(2.0f)
	, MoveAvailability(true)
{
	SceneComp = ObjectInitializer.CreateDefaultSubobject<USceneComponent>(this, TEXT("SceneComp"));
	RootComponent = SceneComp;

	SceneComp->Mobility = EComponentMobility::Movable;

	SplineComp = ObjectInitializer.CreateDefaultSubobject<USplineComponent>(this, TEXT("JumpPathSpline"));
	SplineComp->SetupAttachment(RootComponent);

	// Make sure all the spline comp to be absolute to world space not to this component
	SplineComp->bAbsoluteLocation = true;
	SplineComp->bAbsoluteRotation = true;
	SplineComp->bAbsoluteScale = true;

	SplinePointCount = 2;

	ResetTime = 1.0f;

	MinimumDistThreshold = 30.0f;

	PrimaryActorTick.bCanEverTick = true;

	Velocity = FVector::ZeroVector;
	StartLocation = FVector::ZeroVector;
	EndLocation = FVector::ZeroVector;
	VelocityCoefficient = 1.f;
	//SetActorTickEnabled(false);
	//RegisterAllActorTickFunctions(false, true);
}

void AShooterSplineMovement::Tick(float DeltaSeconds)
{
	MovementImplementation(DeltaSeconds);
}

void AShooterSplineMovement::PostInitProperties()
{
	Super::PostInitProperties();

	InitSplinePoints(SplinePointCount);
}

#if WITH_EDITOR
void AShooterSplineMovement::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	InitSpline();
}
#endif

void AShooterSplineMovement::MoveCharacterAlongSpline(AShooterCharacter* InCharacter)
{
	GetWorld()->GetTimerManager().SetTimer(ToggleAvailabilityHandle, this, &AShooterSplineMovement::ResetMoveAvaillable, ResetTime, false);

	// Fixing by resetting the movement mode for spline movement's initial slide error
	float ResetMovementModeTime = Time * 0.5;
	//GetWorld()->GetTimerManager().SetTimer(ToggleMovementModeHandle, this, &AShooterSplineMovement::ResetMovementToDefault<InCharacter,NULL>, ResetMovementModeTime, false);
	
	FSplineCharacters newCharacter;
	newCharacter.Character = InCharacter;
	newCharacter.Velocity = FVector::ZeroVector;

	if (!InCharacter)
	{
		return;
	}

	if (InCharacter->IsRunning())
	{
		InCharacter->SetRunning(false, false);
	}

	//MoveAvailability = false; // this is only for spline movement to avoid overlap calling the movement for same character.

	StartLocation = InCharacter->GetActorLocation();

	InitSpline();
	
	InCharacter->SetIsOnSplineMovement(true);

	newCharacter.CharacterMovementMode = InCharacter->GetCharacterMovement()->MovementMode;
	newCharacter.CurrentPlayedTime = 0.f;
	SplineCharacters.Add(newCharacter);
	InCharacter->GetCharacterMovement()->SetMovementMode(MOVE_Custom);
}

bool AShooterSplineMovement::IsMoveAvaillable()
{
	return MoveAvailability;
}

void AShooterSplineMovement::MovementImplementation(float DeltaSeconds)
{
	if (SplineCharacters.Num() > 0)
	{
		for (int CharacterItr = 0; CharacterItr < SplineCharacters.Num(); CharacterItr++)
		{
			AShooterCharacter* TargetCharacter = SplineCharacters[CharacterItr].Character;

			if (TargetCharacter)
			{
				SplineCharacters[CharacterItr].CurrentPlayedTime += DeltaSeconds;

				FVector2D TimeRange = FVector2D(0.0f, Time);
				FVector2D TargetRange = FVector2D(0.0f, SplineLength);

				if (SplineCharacters[CharacterItr].CurrentPlayedTime >= Time)
				{
					FinishSplineMovement(TargetCharacter, SplineCharacters[CharacterItr].CharacterMovementMode);
					SplineCharacters.RemoveAt(CharacterItr);
					CharacterItr--;
				}
				else
				{
					DistAtSpline = FMath::GetMappedRangeValueClamped(TimeRange, TargetRange, SplineCharacters[CharacterItr].CurrentPlayedTime);
					TargetCharacter->SetActorLocation(SplineComp->GetWorldLocationAtDistanceAlongSpline(DistAtSpline));
				}
			}
		}
	}
}

void AShooterSplineMovement::InitSpline()
{
	SplineComp->ClearSplinePoints();
	if (!bUseCustomSpline)
	{
		// Init the spline with predefined value
		InitSplinePoints(SplinePointCount);
		SetSpline();
	}
	else
	{
		// Spline point count will dynamically change so InitSplinePoints internally
		SetCustomSpline();
	}
	SplineComp->UpdateSpline();
	SplineLength = SplineComp->GetSplineLength();
}

void AShooterSplineMovement::SetSpline()
{
	if (StartLocation == FVector::ZeroVector)
	{
		StartLocation = GetActorLocation();
	}

	if (DestinationActor)
	{
		EndLocation = DestinationActor->GetActorLocation();
	}
	else
	{
		EndLocation = GetActorLocation();
	}

	if (SplinePoints.Num() < SplinePointCount)
	{
		return;
	}

	SplinePoints[0] = StartLocation;

	SplinePoints[1] = EndLocation;

	SplineComp->SetSplineLocalPoints(SplinePoints);
}

void AShooterSplineMovement::SetCustomSpline()
{
	if (!CustomSpline)
	{
		return;
	}

	if (StartLocation == FVector::ZeroVector)
	{
		StartLocation = GetActorLocation();
	}

	// Look for USplineComp that has name "SplineMesh"
	USplineComponent* CustomSplineComp = NULL;
	USceneComponent* CustomSceneComp = CustomSpline->GetRootComponent();
	for (int32 i = 0; i < CustomSceneComp->GetNumChildrenComponents(); ++i)
	{
		if (CustomSceneComp->GetChildComponent(i))
		{
			if (CustomSceneComp->GetChildComponent(i)->GetName() == "SplineMesh")
			{
				CustomSplineComp = Cast<USplineComponent>(CustomSceneComp->GetChildComponent(i));
			}
		}
	}

	// Make sure the custom spline comp is there and its points to be bigger than 1 (line need at least 2 points)
	if (CustomSplineComp && CustomSplineComp->GetNumberOfSplinePoints() > 1)
	{
		InitSplinePoints(CustomSplineComp->GetNumberOfSplinePoints());

		TArray<FVector> NewSplinePoints;
		NewSplinePoints.Reserve(CustomSplineComp->GetNumberOfSplinePoints());

		for (int32 i = 0; i < CustomSplineComp->GetNumberOfSplinePoints(); ++i)
		{
			// For the start point, make sure it starts from the right position
			if (i == 0)
			{
				NewSplinePoints.Add(CustomSplineComp->GetWorldLocationAtSplinePoint(i));
				/*
				FVector Offset = FVector::ZeroVector;
				CalculateOffset(TargetCharacter, Offset);
				FVector StartLocation = GetActorLocation() + Offset;
				NewSplinePoints.Add(StartLocation);
				*/
			}
			else
			{
				NewSplinePoints.Add(CustomSplineComp->GetWorldLocationAtSplinePoint(i));
			}
		}

		SplineComp->SetSplineLocalPoints(NewSplinePoints);
	}
}

void AShooterSplineMovement::CalculateOffset(const AShooterCharacter* InCharacter, FVector & InVector)
{
	float MinCharacterHeightHalf = 86.0f;
	float CharacterHeightHalf = InCharacter ? InCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : MinCharacterHeightHalf;

	InVector.Z = CharacterHeightHalf;
}

void AShooterSplineMovement::InitSplinePoints(int32 PointCount)
{
	if (SplinePoints.Num() > 0)
	{
		SplinePoints.Empty();
	}

	SplineComp->ClearSplinePoints();

	for (int32 i = 0; i < PointCount; ++i)
	{
		SplinePoints.Add(FVector::ZeroVector);
		SplineComp->AddSplineLocalPoint(SplinePoints[i]);
	}
}

void AShooterSplineMovement::ResetMoveAvaillable()
{
	// For default spline movement, it will take 0.5 sec to be reset so that every player can't jump at once.
	MoveAvailability = true;
}

void AShooterSplineMovement::FinishSplineMovement(AShooterCharacter* InCharacter, EMovementMode CharacterMovementMode)
{
	if (InCharacter &&
		InCharacter->IsActiveAndFullyReplicated &&
		InCharacter->IsAlive() &&
		!InCharacter->IsDead)
	{
		//InCharacter->SetAutonomousProxy(true);
		InCharacter->GetCharacterMovement()->SetMovementMode(CharacterMovementMode);

		InCharacter->SetIsOnSplineMovement(false);
		InCharacter->GetCharacterMovement()->Velocity = Velocity*VelocityCoefficient;
	}
}

void AShooterSplineMovement::ResetMovementToDefault(AShooterCharacter* InCharacter, EMovementMode CharacterMovementMode)
{
	if (InCharacter)
	{
		InCharacter->GetCharacterMovement()->SetMovementMode(CharacterMovementMode);
	}
}

// Fill out your copyright notice in the Description page of Project Settings.

#include "ShooterGame.h"
#include "ShooterCharacterData.h"
#include "ShooterSound.h"
#include "Movement/ShooterZiplineMovement.h"


AShooterZiplineMovement::AShooterZiplineMovement(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ZipLineMeshComp2 = ObjectInitializer.CreateDefaultSubobject<USkeletalMeshComponent>(this, TEXT("ZipLineMeshComp2"));
	ZipLineMeshComp2->SetMobility(EComponentMobility::Movable);

	ZipLineIndicator = ObjectInitializer.CreateDefaultSubobject<UStaticMeshComponent>(this, TEXT("ZipLineIndicator"));
	ZipLineIndicator->SetMobility(EComponentMobility::Static);

	// ResetTime has to be reset for Zipline because, it will not be available until the
	// zipline movement is done unless player jump in middle.
	ResetTime = Time;
	ZipLineMeshOffset = 0.0f;
	ZipLineStartOffset = 250.0f;
	ZipLineArrialOffset = 10.0f;
	ZipLineIndicatorPosition = FVector(0.0f, -50.0f, 287.5f);
	Spacing = 3.f;
}

void AShooterZiplineMovement::FinishSplineMovement(AShooterCharacter* InCharacter, EMovementMode CharacterMovementMode)
{
	StopZipLineSound(InCharacter);

	if (InCharacter->IsActiveAndFullyReplicated &&
		InCharacter->IsAlive() &&
		!InCharacter->IsDead)
	{
		InCharacter->GetCapsuleComponent()->SetCollisionResponseToChannel(ECollisionChannel::ECC_WorldStatic, ECollisionResponse::ECR_Block);

		AShooterCharacterData* CharacterData = InCharacter->CurrentCharacterData;
		check(CharacterData);

		FString Proxy	   = UShooterStatics::GetProxyAsString(InCharacter);
		FString ClientName = InCharacter->GetShortPlayerName();
		FString ShortCode  = CharacterData->ShortCode.ToString();

		// TODO: Use CharacterData->PlayAnimation();
		if (InCharacter->IsFirstPerson())
		{
			// TODO: HACK: Since IsActiveAndFullyReplicated is true, we should NEVER have to check this is NULL
			if (UAnimInstance* AnimInstance = Cast<UAnimInstance>(InCharacter->Mesh1P->GetAnimInstance()))
			{
				// TODO: Need to add this check to the IsValid check in ShooterCharacterData
				if (UAnimMontage* Anim = CharacterData->ZiplineAnims.Anim1P)
				{
					AnimInstance->Montage_Stop(0.25f, Anim);
				}
				else
				{
					UE_LOG(LogShooter, Warning, TEXT("FinishSplineMovement (%s): ZiplineAnims.Anim1P is NULL on Client: %s using Character: %s"), *Proxy, *ClientName, *ShortCode);
				}
			}
			else
			{
				UE_LOG(LogShooter, Warning, TEXT("FinishSplineMovement (%s): AnimInstance on Mesh1P is NULL on Client: %s using Character: %s"), *Proxy, *ClientName, *ShortCode);
			}
		}
		else
		{
			// TODO: HACK: Since IsActiveAndFullyReplicated is true, we should NEVER have to check this is NULL
			if (UAnimInstance* AnimInstance = Cast<UAnimInstance>(InCharacter->GetMesh()->GetAnimInstance()))
			{
				// TODO: Need to add this check to the IsValid check in ShooterCharacterData
				if (UAnimMontage* Anim = CharacterData->ZiplineAnims.Anim3P)
				{
					AnimInstance->Montage_Stop(0.25f, Anim);
				}
				else
				{
					UE_LOG(LogShooter, Warning, TEXT("FinishSplineMovement (%s): ZiplineAnims.Anim3P is NULL on Client: %s using Character: %s"), *Proxy, *ClientName, *ShortCode);
				}
			}
			else
			{
				UE_LOG(LogShooter, Warning, TEXT("FinishSplineMovement (%s): AnimInstance on Mesh3P is NULL on Client: %s using Character: %s"), *Proxy, *ClientName, *ShortCode);
			}
		}
	}
	Super::FinishSplineMovement(InCharacter, CharacterMovementMode);
}

#if WITH_EDITOR
void AShooterZiplineMovement::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	check(ZipLineMeshComp2 && ZipLineIndicator);

	InitZiplineHelpers();

	ResetTime = Time;

	ZipLineIndicator->SetWorldRotation(ZipLineIndicatorRotation);
}
#endif

void AShooterZiplineMovement::PostInitProperties()
{
	Super::PostInitProperties();

	check(ZipLineMeshComp2 && ZipLineIndicator);

	InitZiplineHelpers();

	ZipLineIndicator->SetWorldRotation(ZipLineIndicatorRotation);
}

void AShooterZiplineMovement::OnConstruction(const FTransform& Transform)
{
	
	float SplineLength = SplineComp->GetSplineLength();
	float NumberOfMeshes = SplineLength / Spacing;
	SplineMeshes.Empty();
	for (int i = 0; i < NumberOfMeshes; ++i)
	{
		USplineMeshComponent* SplineMesh = NewObject<USplineMeshComponent>(this);

		SplineMesh->CreationMethod = EComponentCreationMethod::UserConstructionScript;
		SplineMesh->SetMobility(EComponentMobility::Movable);
		SplineMesh->SetupAttachment(SplineComp);

		SplineMesh->bCastDynamicShadow = false;
		SplineMesh->SetStaticMesh(myMesh);
		
		SplineMesh->SetMaterial(0, myMaterial);
		SplineMesh->SetForwardAxis(ESplineMeshAxis::Y);
		//SplineMesh->SetEndScale(FVector2D(.25f, .25f));
		//SplineMesh->SetStartScale(FVector2D(.25f, .25f));
		FVector pointLocationStart, pointLocationEnd, pointWorldDirStart, pointWorldDirEnd, point, pointTangentStart, pointTangentEnd;
		pointLocationStart = SplineComp->GetWorldLocationAtDistanceAlongSpline(i*Spacing);
		pointLocationEnd = SplineComp->GetWorldLocationAtDistanceAlongSpline((i + 1)*Spacing);

		pointWorldDirStart = SplineComp->GetWorldDirectionAtDistanceAlongSpline(i*Spacing) * TangentLength;
		pointWorldDirEnd = SplineComp->GetWorldDirectionAtDistanceAlongSpline((i + 1) * Spacing) * TangentLength;

		pointTangentStart = pointWorldDirStart;
		pointTangentStart.Normalize();
		if (i == floor(NumberOfMeshes))
		{
			pointLocationEnd = SplineComp->GetWorldLocationAtDistanceAlongSpline(SplineLength);
			pointWorldDirEnd = SplineComp->GetWorldDirectionAtDistanceAlongSpline(SplineLength) * TangentLength;
		}
		pointTangentEnd = pointWorldDirEnd;
		pointTangentEnd.Normalize();
		
		SplineMesh->SetStartAndEnd(pointLocationStart, pointWorldDirStart, pointLocationEnd, pointWorldDirEnd);
		SplineMesh->RegisterComponent();
		SplineMeshes.Add(SplineMesh);
	}//*/
	RegisterAllComponents();
}

void AShooterZiplineMovement::BeginPlay()
{
	Super::BeginPlay();
}

void AShooterZiplineMovement::MovementImplementation(float DeltaSeconds)
{
	if (SplineCharacters.Num() > 0)
	{
		for (int CharacterItr = 0; CharacterItr < SplineCharacters.Num(); CharacterItr++)
		{
			AShooterCharacter* TargetCharacter = SplineCharacters[CharacterItr].Character;

			if (TargetCharacter)
			{
				FVector oldPos = TargetCharacter->GetActorLocation();

				SplineCharacters[CharacterItr].CurrentPlayedTime += DeltaSeconds;

				FVector2D TimeRange = FVector2D(0.0f, Time);
				FVector2D TargetRange = FVector2D(0.0f, SplineLength);

				bool IsCharacterCloseEnoughToEndPoint = FVector::Dist(TargetCharacter->GetActorLocation(), SplineComp->GetWorldLocationAtDistanceAlongSpline(SplineComp->GetSplineLength())) < MinimumDistThreshold;
				if (SplineCharacters[CharacterItr].CurrentPlayedTime >= Time || IsCharacterCloseEnoughToEndPoint)
				{
					FinishSplineMovement(TargetCharacter, SplineCharacters[CharacterItr].CharacterMovementMode);
					SplineCharacters.RemoveAt(CharacterItr);
					if (Role != ROLE_Authority)
					{
						TargetCharacter->GetCharacterMovement()->bIgnoreClientMovementErrorChecksAndCorrection = false;
					}
					CharacterItr--;
					ResetZiplineFeatures();
				}
				else
				{
					if (TargetCharacter->IsJumpPressed)
					{
						FinishSplineMovement(TargetCharacter, SplineCharacters[CharacterItr].CharacterMovementMode);
						SplineCharacters.RemoveAt(CharacterItr);
						if(Role != ROLE_Authority)
						{
							TargetCharacter->GetCharacterMovement()->bIgnoreClientMovementErrorChecksAndCorrection = false;
						}
						CharacterItr--;
						ResetZiplineFeatures();
						return;
					}

					DistAtSpline = FMath::GetMappedRangeValueClamped(TimeRange, TargetRange, SplineCharacters[CharacterItr].CurrentPlayedTime);

					FVector AtSplineLocation = SplineComp->GetWorldLocationAtDistanceAlongSpline(DistAtSpline);

					float CharacterHeightHalf = TargetCharacter->CurrentCharacterData ? TargetCharacter->CurrentCharacterData->CapsuleHalfHeight : TargetCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

					// Offseet for character and carrier
					if (TargetCharacter->IsActiveAndFullyReplicated)
					{
						FVector CharacterLocation = AtSplineLocation + TargetCharacter->CurrentCharacterData->ZiplineOffset;
						FVector ZipLineMeshLocation = AtSplineLocation + FVector(0.0f, 0.0f, ZipLineMeshOffset);

						TargetCharacter->SetActorLocation(CharacterLocation);
						ZipLineMeshComp2->SetWorldLocation(ZipLineMeshLocation);

						ZipLineMeshComp2->SetWorldRotation(ZipLineMeshRotation);

						Velocity = (TargetCharacter->GetActorLocation() - oldPos) / DeltaSeconds;
					}
				}
			}
		}
	}
}

void AShooterZiplineMovement::SetSpline()
{
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
	}

	AShooterCharacter* TargetCharacter = NULL;
	if (SplineCharacters.Num()>0)
	{
		TargetCharacter = SplineCharacters[0].Character;
	}

	// Get capsule half height
	float CharacterHeightHalf = TargetCharacter ? TargetCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.0f;

	SplinePoints[0] = StartLocation + FVector(0.0f, 0.0f, ZipLineStartOffset - CharacterHeightHalf);

	SplinePoints[1] = TargetLocation + FVector(0.0f, 0.0f, (CharacterHeightHalf * 2.0f) + ZipLineArrialOffset);

	SplineComp->SetSplineLocalPoints(SplinePoints);
	SplineLength = SplineComp->GetSplineLength();
}


void AShooterZiplineMovement::MulticastSplineMovement_Implementation(AShooterCharacter* InCharacter)
{
	if (Role != ROLE_Authority)
	{
		InCharacter->GetCharacterMovement()->bIgnoreClientMovementErrorChecksAndCorrection = true;
		if (InCharacter)
		{
			for (int itr = 0; itr < SplineCharacters.Num(); itr++)
			{
				if (InCharacter == SplineCharacters[itr].Character)
				{
					return;
				}
			}

		}

		Super::MoveCharacterAlongSpline(InCharacter);

		// When zipline is activated, zipline magnet is visibility on, indicator to be off
		ZipLineMeshComp2->SetVisibility(true, true);
		ZipLineIndicator->SetVisibility(false, true);

		check(InCharacter);

		InCharacter->IsJumpPressed = false;
		InCharacter->GetCapsuleComponent()->SetCollisionResponseToChannel(ECollisionChannel::ECC_WorldStatic, ECollisionResponse::ECR_Ignore);
		PlayZipLineSound(InCharacter);

		if (InCharacter->IsActiveAndFullyReplicated &&
			InCharacter->CurrentCharacterData)
		{
			if (InCharacter->IsFirstPerson())
			{
				ZipLineMeshComp2->SetHiddenInGame(true, true);
				if (InCharacter->CurrentCharacterData->ZiplineAnims.Anim1P)
				{
					if (InCharacter->GetSpecifcPawnMesh(true) && InCharacter->GetSpecifcPawnMesh(true)->GetAnimInstance())
					{
						InCharacter->GetSpecifcPawnMesh(true)->GetAnimInstance()->Montage_Play(InCharacter->CurrentCharacterData->ZiplineAnims.Anim1P, 1.0f);
					}
				}

			}
			else
			{
				if (InCharacter->CurrentCharacterData->ZiplineAnims.Anim3P)
				{
					if (InCharacter->GetMesh() && InCharacter->GetMesh()->GetAnimInstance())
					{
						InCharacter->GetMesh()->GetAnimInstance()->Montage_Play(InCharacter->CurrentCharacterData->ZiplineAnims.Anim3P, 1.0f);
					}
				}
			}
		}
	}
}


void AShooterZiplineMovement::CalculateOffset(const AShooterCharacter* InCharacter, FVector & InVector)
{
	InVector.Z = ZipLineStartOffset;
}

void AShooterZiplineMovement::MoveCharacterAlongSpline(AShooterCharacter* InCharacter)
{
	if (Role == ROLE_Authority)
	{
		if (InCharacter)
		{
			for (int itr = 0; itr < SplineCharacters.Num(); itr++)
			{
				if (InCharacter == SplineCharacters[itr].Character)
				{
					return;
				}
			}

		}

		Super::MoveCharacterAlongSpline(InCharacter);

		// When zipline is activated, zipline magnet is visibility on, indicator to be off
		ZipLineMeshComp2->SetVisibility(true, true);
		ZipLineIndicator->SetVisibility(false, true);

		check(InCharacter);

		InCharacter->IsJumpPressed = false;
		InCharacter->GetCapsuleComponent()->SetCollisionResponseToChannel(ECollisionChannel::ECC_WorldStatic, ECollisionResponse::ECR_Ignore);
		PlayZipLineSound(InCharacter);
		MulticastSplineMovement(InCharacter);
		if (InCharacter->IsActiveAndFullyReplicated &&
			InCharacter->CurrentCharacterData)
		{
			if (InCharacter->IsFirstPerson())
			{
				ZipLineMeshComp2->SetHiddenInGame(true, true);
				if (InCharacter->CurrentCharacterData->ZiplineAnims.Anim1P)
				{
					if (InCharacter->GetSpecifcPawnMesh(true) && InCharacter->GetSpecifcPawnMesh(true)->GetAnimInstance())
					{
						InCharacter->GetSpecifcPawnMesh(true)->GetAnimInstance()->Montage_Play(InCharacter->CurrentCharacterData->ZiplineAnims.Anim1P, 1.0f);
					}
				}

				if (InCharacter->CurrentCharacterData->ZiplineVOSounds.Sound1P)
				{
					AShooterGameState* MyGameState = Cast<AShooterGameState>(GetWorld()->GetGameState());
					ZiplineVOSound = MyGameState->AllocateAndAttachVOSound(InCharacter->CurrentCharacterData->ZiplineVOSounds.Sound1P, InCharacter, true, true, false, false);
					if (ZiplineVOSound)
						ZiplineVOSound->OnSoundDeallocate.AddDynamic(this, &AShooterZiplineMovement::ClearSound);
				}
			}
			else
			{
				if (InCharacter->CurrentCharacterData->ZiplineAnims.Anim3P)
				{
					if (InCharacter->GetMesh() && InCharacter->GetMesh()->GetAnimInstance())
					{
						InCharacter->GetMesh()->GetAnimInstance()->Montage_Play(InCharacter->CurrentCharacterData->ZiplineAnims.Anim3P, 1.0f);
					}
				}
			}
		}
	}
}


void AShooterZiplineMovement::InitZiplineHelpers()
{
	// Default setting for zipline magnet is visibility off, indicator to be on
	ZipLineMeshComp2->SetVisibility(false, true);
	ZipLineIndicator->SetVisibility(true, true);
	
	ZipLineIndicator->SetWorldLocation(GetActorLocation() + FVector(0.0f, 0.0f, ZipLineMeshOffset) + ZipLineIndicatorPosition);
}

void AShooterZiplineMovement::ResetZiplineFeatures()
{
	// Make visible the Zip line indicator when zipline is done.
	ZipLineMeshComp2->SetVisibility(false, true);
	ZipLineIndicator->SetVisibility(true, true);

	// When player jumps before zipline movement is done, it will reset the zipline availability
	ResetMoveAvaillable();
}

void AShooterZiplineMovement::PlayZipLineSound(AShooterCharacter* InCharacter)
{
	AShooterGameState* GameState = Cast<AShooterGameState>(GetWorld()->GameState);

	if (!GameState || !InCharacter)
	{
		return;
	}
	if(!ZipLineSound)
	{
		return;
	}

	if (InCharacter->IsFirstPerson())
	{
		bool bIs1PSound = true;
		bool bLooping = true;
		bool bDelay = false;
		ZipSound = GameState->AllocateAndAttachSound(ZipLineSound, InCharacter, bIs1PSound, bLooping, bDelay);
	}
}

void AShooterZiplineMovement::StopZipLineSound(AShooterCharacter* InCharacter)
{
	AShooterGameState* GameState = Cast<AShooterGameState>(GetWorld()->GameState);

	if (!GameState)
		return;

	const bool IsFirstPerson = UShooterStatics::IsControlledByClient(InCharacter);

	if (IsFirstPerson)
	{
		if (ZipSound)
		{
			GameState->DeAllocateSound(ZipSound);
		}
	}
}

void AShooterZiplineMovement::LoadMaterials()
{
	UMaterialInstanceDynamic* MI = UMaterialInstanceDynamic::Create(myMaterial, NULL);
	MI->SetVectorParameterValue(TEXT("Color"), FLinearColor::Black);
	for (int i = 0; i < SplineMeshes.Num(); i++)
	{
		SplineMeshes[i]->SetMaterial(0, MI);
	}
}

void AShooterZiplineMovement::ClearSound(AShooterSound* Sound)
{
	if (Sound == ZiplineVOSound) {
		ZiplineVOSound = NULL;
	}
}
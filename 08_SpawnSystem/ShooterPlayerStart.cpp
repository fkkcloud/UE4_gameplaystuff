
#include "ShooterGame.h"
#include "ShooterPlayerStart.h"

/**
* Heuristic values for spawn logic.
* - declared as static in .h to be shared through instances and save memory.
*/
const float AShooterPlayerStart::SCORE_DOTPRODUCT_MULT = 700.0; 
const float AShooterPlayerStart::SCORE_SEARCH_RANGE = 25.0f;
const float AShooterPlayerStart::SCORE_REGEN_RATE = 450.0f;
const float AShooterPlayerStart::SCORE_RECENT_SPAWN_MAX = 500.0f;
const float AShooterPlayerStart::SCORE_RECENT_SPAWN_DECREASE = 50.0f;
const float AShooterPlayerStart::SCORE_SPAWN_MIN = -1000.0f;
const float AShooterPlayerStart::SCORE_SPAWN_MAX = 1000.0f;
const float AShooterPlayerStart::SCORE_NEARBY_DIST = 2500.0f;
const float AShooterPlayerStart::SCORE_FOV = 0.25;

AShooterPlayerStart::AShooterPlayerStart(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, CheckRadius(3000.0f)
	, CheckTimeStep(0.5f)
	, SpawnScore(0.0f)
	, RecentSpawnWeight(0.0f)
{
	// Enables calling ReceiveTick()
	//PrimaryActorTick.bCanEverTick = true;

#if WITH_EDITOR
	// Define capsule component attributes
	CheckRadiusComponent = ObjectInitializer.CreateDefaultSubobject<USphereComponent>(this, TEXT("CheckRadiusSphere"));
	CheckRadiusComponent->ShapeColor = FColor(10, 138, 255, 200);
	CheckRadiusComponent->bDrawOnlyIfSelected = true;
	CheckRadiusComponent->InitSphereRadius(CheckRadius);
	//CheckRadiusComponent->BodyInstance.bEnableCollision_DEPRECATED = false;
	CheckRadiusComponent->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	CheckRadiusComponent->bShouldCollideWhenPlacing = false;
	CheckRadiusComponent->bShouldUpdatePhysicsVolume = false;
	CheckRadiusComponent->Mobility = EComponentMobility::Static;
	CheckRadiusComponent->SetWorldLocation(GetCapsuleComponent()->GetComponentLocation());
	CheckRadiusComponent->AttachToComponent(GetCapsuleComponent(), FAttachmentTransformRules::KeepRelativeTransform);
#endif

#if !UE_BUILD_SHIPPING
	//// Materials for ScoreTag
	//static ConstructorHelpers::FObjectFinder<UMaterialInstanceConstant> ScoreTagMICOb(TEXT("MaterialInstanceConstant'/Game/UI/HUD/mtl_name_tag_friendly_Inst.mtl_name_tag_friendly_Inst'"));
	//ScoreTagMaterial = ScoreTagMICOb.Object;
	//static ConstructorHelpers::FObjectFinder<UMaterialInstanceConstant> ScoreHitTagMICOb(TEXT("MaterialInstanceConstant'/Game/UI/HUD/mtl_name_tag_enemy_inst.mtl_name_tag_enemy_inst'"));
	//ScoreHitTagMaterial = ScoreHitTagMICOb.Object;
	//static ConstructorHelpers::FObjectFinder<UFont> FontMICOb(TEXT("MaterialInstanceConstant'/Game/UI/HUD/RobotoDistanceField_RS'"));
	//UFont* ScoreTagFont = FontMICOb.Object;

	//TextRenderComponent = PCIP.CreateDefaultSubobject<UTextRenderComponent>(this, TEXT("ScoreTextComp"));
	//if (TextRenderComponent)
	//{
	//	TextRenderComponent->SetFont(ScoreTagFont);
	//	TextRenderComponent->SetWorldScale3D(FVector(1.5f, 1.5f, 1.5f));
	//	TextRenderComponent->SetVisibility(false);
	//	TextRenderComponent->SetText(FString(TEXT("Default")));
	//}
	bRayHit = false;
#endif
}

void AShooterPlayerStart::OnPlayerStartAddedToGameMode()
{
	GetWorld()->GetTimerManager().SetTimer(UpdateScoreHandle, this, &AShooterPlayerStart::UpdateScore, CheckTimeStep, true);
}

///** To update any property every tick. */
//void AShooterPlayerStart::ReceiveTick(float DeltaSeconds)
//{
//	#if !UE_BUILD_SHIPPING
//	AShooterGameMode* GameMode = Cast<AShooterGameMode>(GetWorld()->GetAuthGameMode());
//	//if (GameMode)
//	//{
//	//	TextRenderComponent->SetVisibility(GameMode->bDebugSpawnLogicTC);
//	//}
//	if (GameMode && GameMode->bDebugSpawnLogicDT)
//	{
//		GameMode->DisplaySpawnLogicDT(GetActorLocation(), SpawnScore);
//	}
//	#endif
//}

/** Update for spawn score */
void AShooterPlayerStart::UpdateScore()
{
#if !UE_BUILD_SHIPPING
	bRayHit = false; // for debug dot product
#endif

	AShooterGameState* GameState = Cast<AShooterGameState>(GetWorld()->GameState);

	if (!GameState || !GameState->IsMatchInProgress())
		return;

	CalculateRecentSpawnedState();

	for (FConstPawnIterator It = GetWorld()->GetPawnIterator(); It; ++It)
	{
		ACharacter* TestPawn = Cast<ACharacter>(*It);
		if (TestPawn)
		{
			// Calculation to give different weight for how recent the spawn point was used.
			if (IsSpawnPointNearBy(TestPawn))
			{
				SpawnScore -= SCORE_SEARCH_RANGE;
				if (IsSpawnPointVisible(TestPawn))
				{
					SpawnScore -= SCORE_DOTPRODUCT_MULT;
				}
				SpawnScore = FMath::Clamp(SpawnScore, SCORE_SPAWN_MIN, SCORE_SPAWN_MAX);
			}
		}
	}

	// if there was not pawn nearby the spawn point, it gets more points.
	if (IsRegenable())
	{
		SpawnScore += SCORE_REGEN_RATE;
		SpawnScore = FMath::Clamp(SpawnScore, SCORE_SPAWN_MIN, SCORE_SPAWN_MAX);
	}

#if !UE_BUILD_SHIPPING
	//SetScoreTag(FString::Printf(TEXT("Score : %.2f"), SpawnScore)); // debug
#endif
}

bool AShooterPlayerStart::operator<(const AShooterPlayerStart& rhs) const
{
	return GetSpawnScore() < rhs.GetSpawnScore();
}

/** Spawned weight to be full. */
void AShooterPlayerStart::Spawned()
{
	RecentSpawnWeight = SCORE_RECENT_SPAWN_MAX;
}

/** Calculate the score for recently spawned points */
void AShooterPlayerStart::CalculateRecentSpawnedState()
{
	// RecentSpawnWeight updates
	if (RecentSpawnWeight > 0.0f)
	{
		RecentSpawnWeight -= SCORE_RECENT_SPAWN_DECREASE;
		RecentSpawnWeight = FMath::Clamp(RecentSpawnWeight, 0.0f, SCORE_RECENT_SPAWN_MAX);

		SpawnScore -= RecentSpawnWeight;
		SpawnScore = FMath::Clamp(SpawnScore, SCORE_SPAWN_MIN, SCORE_SPAWN_MAX);
	}
}

/**
* When it spawns it will update near by spawn points to affect their spawn points
* Meant to be called in game mode.
*/
void AShooterPlayerStart::AffectNearSpawnPoints(TArray<APlayerStart*> & PlayerStarts)
{
	float SCORE_NEARBY_DISTSQ = SCORE_NEARBY_DIST * SCORE_NEARBY_DIST;
	for (int32 i = 0; i < PlayerStarts.Num(); i++)
	{
		APlayerStart* PlayerStart = PlayerStarts[i];
		AShooterPlayerStart* TestSpawnPoint = Cast<AShooterPlayerStart>(PlayerStart);
		{
			if (TestSpawnPoint)
			{
				float DistSq = (GetActorLocation() - TestSpawnPoint->GetActorLocation()).SizeSquared();
				if (DistSq < SCORE_NEARBY_DISTSQ)
				{
					TestSpawnPoint->AddToSpawnScore(SCORE_RECENT_SPAWN_MAX * 1.5f);
				}
			}
		}
	}
}

#if WITH_EDITOR
void AShooterPlayerStart::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	CheckRadiusComponent->SetSphereRadius(CheckRadius);
}

void AShooterPlayerStart::PostLoad()
{
	Super::PostLoad();

	CheckRadiusComponent->SetSphereRadius(CheckRadius);
}
#endif

/**
* Calculate NearBy for spawn score,
*	  1. distance check for XY plane.
*     2. height check for Z Axis.
* @ TestPawn - Pawn to update the spawn score
* @ Return - Did it find any near by pawn to calculate
*/
bool AShooterPlayerStart::IsSpawnPointNearBy(ACharacter* TestPawn)
{
	const FVector SpawnLocation = GetActorLocation();
	FVector2D SpawnLocation2D = FVector2D(SpawnLocation.X, SpawnLocation.Y);
	FVector2D TestPawnLocation2D = FVector2D(TestPawn->GetActorLocation().X, TestPawn->GetActorLocation().Y);

	float DistSq = (SpawnLocation2D - TestPawnLocation2D).SizeSquared();
	float TestRadiusSq = CheckRadius * CheckRadius;

	if (DistSq < TestRadiusSq)
	{
		// TestHeight - between any pawn and the spawn point there have 
		// to be at least 1 pawn-height space available.
		const float TestHeight = TestPawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 4.0f;

		// check if player is near by the spawn point by Z axis
		if (FMath::Abs(SpawnLocation.Z - TestPawn->GetActorLocation().Z) < TestHeight)
		{
			return true;
		}
	}
	return false;
}

/**
* Calculate Visibility for spawn score,
*     1. check if it is looking towards the spawn point. - dot product
*     2. check if it succeeds for ray - casting. - UNavigationSystem::NavigationRayCast  - TEMP
* @ TestPawn - Pawn to update the spawn score
* @ Return - Did it actually looked at the spawn point with given FOV
*/
bool AShooterPlayerStart::IsSpawnPointVisible(ACharacter* TestPawn)
{
	FVector PointOfInterest = TestPawn->GetControlRotation().Vector().GetSafeNormal();
	FVector SpawnPointToPawn = (TestPawn->GetActorLocation() - GetActorLocation()).GetSafeNormal();

	float ResultDot = FVector::DotProduct(PointOfInterest, SpawnPointToPawn);

	if (ResultDot < SCORE_FOV)
	{
		// SpawnScore += (ResultDot * SCORE_DOTPRODUCT_MULT); // ResultDot itself will be negative value so adding it to SpawnScore
#if !UE_BUILD_SHIPPING
		bRayHit = true; // for debug dot product
#endif
		return true;
		/*
		FVector HitLoc = FVector::ZeroVector;
		if (!UNavigationSystem::NavigationRaycast(this, SpawnLocation, TestPawn->GetActorLocation(), HitLoc))
		{
		// Navmesh would not do correct LOS since it rays only on the NaveMesh which is not correct.
		}
		*/
	}
	return false;
}

/**
* Calculate for spawn score,
*     1. check if all the pawn is near by the spawn point
2. If none are near by, regen the spawn point
* @ TestPawn - Pawn to update the spawn score
* @ Return - Was there any neayby character
*/
bool AShooterPlayerStart::IsRegenable()
{
	for (FConstPawnIterator It = GetWorld()->GetPawnIterator(); It; ++It)
	{
		AShooterCharacter* TestPawn = Cast<AShooterCharacter>(*It);
		if (TestPawn)
		{
			if (IsSpawnPointNearBy(TestPawn))
			{
				return false;
			}
		}
	}
	return true;
}

// Re-enable AddPlayerStart/RemovePlayerStart functionality that was removed by Epic's GameMode.h
void AShooterPlayerStart::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	AShooterGameMode* gameMode = Cast<AShooterGameMode>(GetWorld()->GetAuthGameMode());
	if (!IsPendingKill() && gameMode)
	{
		gameMode->AddPlayerStart(this);
	}
}

void AShooterPlayerStart::PostUnregisterAllComponents()
{
	Super::PostUnregisterAllComponents();

	UWorld* ActorWorld = GetWorld();
	if (ActorWorld)
	{
		AShooterGameMode* gameMode = Cast<AShooterGameMode>(ActorWorld->GetAuthGameMode());

		if (gameMode)
			gameMode->RemovePlayerStart(this);
	}
}

#if !UE_BUILD_SHIPPING
/**
* Record if there was any player walked through.
* If there was players within 4 seconds, it will decrease spawn score.
*/
void AShooterPlayerStart::Record()
{
	/*
	FString PawnLocations;
	PawnLocations.Append(FString(TEXT("New Spawn")));

	// Add new pawn's spawn location as id'0' element of the TArray "PawnLocations"
	// So it will be treated differently
	FString SpawnPoint = FString::Printf(TEXT("%f %f %f\n"), GetActorLocation().X, GetActorLocation().Y, GetActorLocation().Z);
	PawnLocations.Append(SpawnPoint);

	// Add other pawn's current location to the TArray "PawnLocations"
	for (FConstPawnIterator It = GetWorld()->GetPawnIterator(); It; ++It)
	{
	ACharacter* TestPawn = Cast<ACharacter>(*It);
	SpawnPoint = FString::Printf(TEXT("%f %f %f\n"),
	TestPawn->GetActorLocation().X,
	TestPawn->GetActorLocation().Y,
	TestPawn->GetActorLocation().Z);
	PawnLocations.Append(SpawnPoint);
	}
	FFileHelper::SaveStringToFile(PawnLocations, SL_FILENAME);
	*/
}

/**
* Show spawn score above the spawn point
* Color changes to red when its ray casting failed.
*/
//void AShooterPlayerStart::SetScoreTag(const FString& Contents)
//{
//	if (TextRenderComponent)
//	{
//		FVector ScoreTagLocation = GetActorLocation();
//		ScoreTagLocation.Z += 100.0f;
//		TextRenderComponent->SetWorldLocation(ScoreTagLocation);
//		TextRenderComponent->SetText(Contents);
//		TextRenderComponent->SetTextMaterial(bRayHit ? ScoreHitTagMaterial : ScoreTagMaterial); // for debug dot product
//	}
//}
#endif // end of !UE_BUILD_SHIPPING

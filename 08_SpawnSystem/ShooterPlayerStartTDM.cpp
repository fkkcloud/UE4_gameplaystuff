// Fill out your copyright notice in the Description page of Project Settings.

#include "ShooterGame.h"
#include "ShooterPlayerStartTDM.h"
#include "ShooterGame_TeamDeathMatch.h"


AShooterPlayerStartTDM::AShooterPlayerStartTDM(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	TDMState = ETDMState::Neutral;

#if WITH_EDITORONLY_DATA
	// Structure to hold one-time initialization
	struct FConstructorStaticsTDM
	{
		// A helper class object we use to find target UTexture2D object in resource package
		ConstructorHelpers::FObjectFinderOptional<UTexture2D> TDMTextureObjectASide;
		ConstructorHelpers::FObjectFinderOptional<UTexture2D> TDMTextureObjectBSide;
		ConstructorHelpers::FObjectFinderOptional<UTexture2D> TDMTextureObjectNeuturalSide;
		ConstructorHelpers::FObjectFinderOptional<UTexture2D> TDMDefaultObjectSide;

		// Icon sprite category name
		FName ID_TDMIcon;

		// Icon sprite display name
		FText NAME_TDMIcon;

		FConstructorStaticsTDM()
			// Use helper class object to find the texture
			// "/Engine/EditorResources/S_Note" is resource path
			: TDMTextureObjectASide(TEXT("/Game/UI/GameModeHelper/TDMIconA"))
			, TDMTextureObjectBSide(TEXT("/Game/UI/GameModeHelper/TDMIconB"))
			, TDMTextureObjectNeuturalSide(TEXT("/Game/UI/GameModeHelper/TDMIconNeutural"))
			, TDMDefaultObjectSide(TEXT("/Game/UI/GameModeHelper/TDMIcon"))
			, ID_TDMIcon(TEXT("TDMIcon"))
			, NAME_TDMIcon(NSLOCTEXT("SpriteCategory", "TDMIcon", "TDMIcon"))
		{
		}
	};
	static FConstructorStaticsTDM ConstructorStaticsTDM;

	// CreateEditorOnlyDefaultSubobject
	SpriteComponentASide = ObjectInitializer.CreateEditorOnlyDefaultSubobject<UBillboardComponent>(this, TEXT("ASideSprite"));
	if (SpriteComponentASide)
	{
		SpriteComponentASide->Sprite = ConstructorStaticsTDM.TDMTextureObjectASide.Get();		// Get the sprite texture from helper class object
		SpriteComponentASide->SpriteInfo.Category = ConstructorStaticsTDM.ID_TDMIcon;		// Assign sprite category name
		SpriteComponentASide->SpriteInfo.DisplayName = ConstructorStaticsTDM.NAME_TDMIcon;	// Assign sprite display name
		SpriteComponentASide->Mobility = EComponentMobility::Static;
		SpriteComponentASide->bHiddenInGame = true;
		SpriteComponentASide->RelativeScale3D = FVector(0.75f, 0.75f, 0.75f);
		SpriteComponentASide->bAbsoluteScale = true;
		SpriteComponentASide->SetupAttachment(GetCapsuleComponent());
		SpriteComponentASide->bIsScreenSizeScaled = true;
		SpriteComponentASide->SetVisibility(false);
	}

	SpriteComponentBSide = ObjectInitializer.CreateEditorOnlyDefaultSubobject<UBillboardComponent>(this, TEXT("BSideSprite"));
	if (SpriteComponentBSide)
	{
		SpriteComponentBSide->Sprite = ConstructorStaticsTDM.TDMTextureObjectBSide.Get();		// Get the sprite texture from helper class object
		SpriteComponentBSide->SpriteInfo.Category = ConstructorStaticsTDM.ID_TDMIcon;		// Assign sprite category name
		SpriteComponentBSide->SpriteInfo.DisplayName = ConstructorStaticsTDM.NAME_TDMIcon;	// Assign sprite display name
		SpriteComponentBSide->Mobility = EComponentMobility::Static;
		SpriteComponentBSide->bHiddenInGame = true;
		SpriteComponentBSide->RelativeScale3D = FVector(0.75f, 0.75f, 0.75f);
		SpriteComponentBSide->bAbsoluteScale = true;
		SpriteComponentBSide->SetupAttachment(GetCapsuleComponent());
		SpriteComponentBSide->bIsScreenSizeScaled = true;
		SpriteComponentBSide->SetVisibility(false);
	}

	SpriteComponentNeuturalSide = ObjectInitializer.CreateEditorOnlyDefaultSubobject<UBillboardComponent>(this, TEXT("NeuturalSprite"));
	if (SpriteComponentNeuturalSide)
	{
		SpriteComponentNeuturalSide->Sprite = ConstructorStaticsTDM.TDMTextureObjectNeuturalSide.Get();		// Get the sprite texture from helper class object
		SpriteComponentNeuturalSide->SpriteInfo.Category = ConstructorStaticsTDM.ID_TDMIcon;		// Assign sprite category name
		SpriteComponentNeuturalSide->SpriteInfo.DisplayName = ConstructorStaticsTDM.NAME_TDMIcon;	// Assign sprite display name
		SpriteComponentNeuturalSide->Mobility = EComponentMobility::Static;
		SpriteComponentNeuturalSide->bHiddenInGame = true;
		SpriteComponentNeuturalSide->RelativeScale3D = FVector(0.75f, 0.75f, 0.75f);
		SpriteComponentNeuturalSide->bAbsoluteScale = true;
		SpriteComponentNeuturalSide->SetupAttachment(GetCapsuleComponent());
		SpriteComponentNeuturalSide->bIsScreenSizeScaled = true;
		SpriteComponentNeuturalSide->SetVisibility(true);
	}

	SpriteComponentTDM = ObjectInitializer.CreateEditorOnlyDefaultSubobject<UBillboardComponent>(this, TEXT("TDMSprite"));
	if (SpriteComponentTDM)
	{
		SpriteComponentTDM->Sprite = ConstructorStaticsTDM.TDMDefaultObjectSide.Get();		// Get the sprite texture from helper class object
		SpriteComponentTDM->SpriteInfo.Category = ConstructorStaticsTDM.ID_TDMIcon;		// Assign sprite category name
		SpriteComponentTDM->SpriteInfo.DisplayName = ConstructorStaticsTDM.NAME_TDMIcon;	// Assign sprite display name
		SpriteComponentTDM->Mobility = EComponentMobility::Static;
		SpriteComponentTDM->bHiddenInGame = true;
		SpriteComponentTDM->RelativeScale3D = FVector(0.75f, 0.75f, 0.75f);
		SpriteComponentTDM->bAbsoluteScale = true;
		SpriteComponentTDM->SetupAttachment(GetCapsuleComponent());
		SpriteComponentTDM->bIsScreenSizeScaled = true;
		SpriteComponentTDM->SetVisibility(false);
	}

#endif // WITH_EDITORONLY_DATA
}

#if WITH_EDITOR
void AShooterPlayerStartTDM::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (TDMState == ETDMState::ASide)
	{
		SpriteComponentASide->SetVisibility(true);
		SpriteComponentBSide->SetVisibility(false);
		SpriteComponentNeuturalSide->SetVisibility(false);
		SpriteComponentTDM->SetVisibility(false);
	}
	else if (TDMState == ETDMState::BSide)
	{
		SpriteComponentASide->SetVisibility(false);
		SpriteComponentBSide->SetVisibility(true);
		SpriteComponentNeuturalSide->SetVisibility(false);
		SpriteComponentTDM->SetVisibility(false);
	}
	else if (TDMState == ETDMState::Neutral)
	{
		SpriteComponentASide->SetVisibility(false);
		SpriteComponentBSide->SetVisibility(false);
		SpriteComponentNeuturalSide->SetVisibility(true);
		SpriteComponentTDM->SetVisibility(false);
	}
	else
	{
		SpriteComponentASide->SetVisibility(false);
		SpriteComponentBSide->SetVisibility(false);
		SpriteComponentNeuturalSide->SetVisibility(false);
		SpriteComponentTDM->SetVisibility(true);
	}
}
#endif

void AShooterPlayerStartTDM::UpdateScore()
{

#if !UE_BUILD_SHIPPING
		bRayHit = false; // for debug dot product
#endif
	CalculateRecentSpawnedState();

	for (FConstPawnIterator It = GetWorld()->GetPawnIterator(); It; ++It)
	{
		AShooterCharacter* TestPawn = Cast<AShooterCharacter>(*It);
		if (TestPawn)
		{
			AShooterPlayerState* PawnState = Cast<AShooterPlayerState>(TestPawn->PlayerState);

			int32 SpawnPointTeamNum = GetSpawnPointTeamNum();

			if (PawnState && PawnState->GetTeamNum() != SpawnPointTeamNum)
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

bool AShooterPlayerStartTDM::IsRegenable()
{
	for (FConstPawnIterator It = GetWorld()->GetPawnIterator(); It; ++It)
	{
		AShooterCharacter* TestPawn = Cast<AShooterCharacter>(*It);
		if (TestPawn)
		{
			AShooterPlayerState* PawnState = Cast<AShooterPlayerState>(TestPawn->PlayerState);

			int32 SpawnPointTeamNum = GetSpawnPointTeamNum();

			// -1 is temp-value for non-team
			if (SpawnPointTeamNum != -1 && PawnState && PawnState->GetTeamNum() != SpawnPointTeamNum)
			{
				if (IsSpawnPointNearBy(TestPawn))
				{
					return false;
				}
			}
		}
	}
	return true;
}

int32 AShooterPlayerStartTDM::GetSpawnPointTeamNum()
{
	AShooterGame_TeamDeathMatch* GameModeTDM = Cast<AShooterGame_TeamDeathMatch>(GetWorld()->GetAuthGameMode());

	if (GameModeTDM)
	{
		if (TDMState == GameModeTDM->USSpawnSide)
		{
			return 0; // 0 is US Team Number (not yet ENumerated)
		}
		else if (TDMState == GameModeTDM->GRSpawnSide)
		{
			return 1; // 1 is GR Team Number (not yet ENumerated)
		}
	}
	return -1; // -1 is non-team 
}

// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "ShooterGame.h"
#include "ShooterGameStats.h"
#include "CoroutineScheduler.h"
#include "Online/ShooterPlayerState.h"
#include "ShooterGameInstance.h"
#include "Animation/SkeletalMeshActor.h"
#include "ShooterMapLoadStart.h"
#include "ShooterGameData.h"
#include "ShooterGameEventData.h"
#include "ShooterDataMapping.h"
#include "ShooterProjectile.h"
#include "ShooterEmitter.h"
#include "ShooterPickup_Class.h"
#include "ShooterDamageType.h"
#include "ShooterDamageType_Melee.h"
#include "ShooterDamageType_Normal.h"
#include "ShooterDamageType_Piercing.h"
#include "ShooterDamageType_Siege.h"
#include "ShooterDamageType_Special.h"
#include "ShooterDamageType_RanOver.h"
#include "ShooterDamageType_Rocket.h"
#include "ShooterDamageType_Bomb.h"
#include "ShooterDamageType_Radiation.h"
#include "ShooterDamageType_Piano.h"
#include "ShooterDamageData.h"
#include "ShooterCharacterData.h"
#include "ShooterCharacterMeshSkin.h"
#include "ShooterCharacterMaterialSkin.h"
#include "ShooterHatData.h"
#include "ShooterProjectileData.h"
#include "ShooterTankData.h"
#include "ShooterTankMaterialSkin.h"
#include "ShooterAntennaData.h"
#include "ShooterDestructible.h"
#include "IHeadMountedDisplay.h"
#include "ShooterBot.h"
#include "ShooterWeapon_Simulated.h"
#include "ShooterSound.h"
#include "ShooterAIController.h"
#include "ShooterLevelScriptActor.h"
#include "ShooterEffectsFlipBook.h"
#include "ShooterEndMatchActor.h"
#include "GameFramework/RsGameSingleton.h"
#include "ShooterGame_SinglePlayer.h"
#include "ReloadCVars.h"

DEFINE_LOG_CATEGORY_STATIC(LogEmitterPool, Log, All);
DEFINE_LOG_CATEGORY_STATIC(LogFlipbookPool, Log, All);
DEFINE_LOG_CATEGORY_STATIC(LogSoundPool, Log, All);

DECLARE_CYCLE_STAT(TEXT("ShooterGameState"), STAT_ShooterGameState, STATGROUP_ShooterGame);
DECLARE_CYCLE_STAT(TEXT("UpdatePlayerStateMapping"), STAT_UpdatePlayerStateMapping, STATGROUP_ShooterGameState);
DECLARE_CYCLE_STAT(TEXT("UpdateBotPlayerStateMapping"), STAT_UpdateBotPlayerStateMapping, STATGROUP_ShooterGameState);
DECLARE_CYCLE_STAT(TEXT("HandleClientForceWarmUpLinkedPawn"), STAT_HandleClientForceWarmUpLinkedPawn, STATGROUP_ShooterGameState);
DECLARE_CYCLE_STAT(TEXT("HandleClientInstantWarmUpLinkedPawn"), STAT_HandleClientInstantWarmUpLinkedPawn, STATGROUP_ShooterGameState);
DECLARE_CYCLE_STAT(TEXT("HandleActorPool"), STAT_HandleActorPool, STATGROUP_ShooterGameState);
DECLARE_CYCLE_STAT(TEXT("HandleSoundPool"), STAT_HandleSoundPool, STATGROUP_ShooterGameState);
DECLARE_CYCLE_STAT(TEXT("HandleEmitterPool"), STAT_HandleEmitterPool, STATGROUP_ShooterGameState);
DECLARE_CYCLE_STAT(TEXT("HandlePickupClass"), STAT_HandlePickupClass, STATGROUP_ShooterGameState);
DECLARE_CYCLE_STAT(TEXT("HandleMeshPool"), STAT_HandleMeshPool, STATGROUP_ShooterGameState);
DECLARE_CYCLE_STAT(TEXT("HandleSkeletalMeshPool"), STAT_HandleSkeletalMeshPool, STATGROUP_ShooterGameState);
DECLARE_CYCLE_STAT(TEXT("HandleText"), STAT_HandleText, STATGROUP_ShooterGameState);
DECLARE_CYCLE_STAT(TEXT("HandleProjectilesToDeActivate"), STAT_HandleProjectilesToDeActivate, STATGROUP_ShooterGameState);

static FAutoConsoleVariable CVarSoundPoolCount(
	TEXT("sound.soundpoolcount"),
	0,
	TEXT("To set different amount of memory for sound per map."),
	ECVF_ReadOnly
	);

AShooterGameState::AShooterGameState(const FObjectInitializer& ObjectInitializer) 
	: Super(ObjectInitializer)
{
	NumTeams = 0;
	RemainingTime = 0;
	bTimerPaused = false;

#if WITH_RELOAD_STUDIOS
	// When editor is enabled, pause time so we don't do map rotation
#if WITH_EDITOR
	if (GIsEditor)
	{
		//bTimerPaused = true;
	}
#endif // #if WITH_EDITOR

	PrimaryActorTick.bCanEverTick = true;

	static ConstructorHelpers::FClassFinder<AShooterDamageData> DamageDataOb(TEXT("/Game/Blueprints/bp_damage_data"));
	DamageData = DamageDataOb.Class.GetDefaultObject();

	// Emitter
	static ConstructorHelpers::FClassFinder<AShooterEmitter> EmptyEmitterOb(TEXT("/Game/Effects/bp_empty_emitter"));
	EmptyEmitter = EmptyEmitterOb.Class;

	// Sound
	static ConstructorHelpers::FClassFinder<AShooterSound> EmptySoundOb(TEXT("/Game/Sounds/bp_empty_sound"));
	EmptySound = EmptySoundOb.Class;

	// Projectile
	static ConstructorHelpers::FClassFinder<AShooterProjectile> EmptyProjectileOb(TEXT("/Game/Projectiles/bp_base_proj"));
	EmptyProjectile = EmptyProjectileOb.Class;

	// Pickup Class
	static ConstructorHelpers::FClassFinder<AShooterPickup_Class> BasePickupClassOb(TEXT("/Game/Pickups/bp_pickup_class"));
	BasePickupClass = BasePickupClassOb.Class;

	// Hit Marker Mesh
	static ConstructorHelpers::FObjectFinder<UStaticMesh> HitMarkerMeshOb(TEXT("StaticMesh'/Game/UI/HUD/ui_hitmarker.ui_hitmarker'"));
	HitMarkerMesh = HitMarkerMeshOb.Object;

	// Kill Confirmed Icon Mesh
	static ConstructorHelpers::FObjectFinder<UStaticMesh> KillConfirmedIconOb(TEXT("StaticMesh'/Game/UI/HUD/ui_kill_confirmed_icon.ui_kill_confirmed_icon'"));
	KillConfirmedIconMesh = KillConfirmedIconOb.Object;

	// Text

		// Hit Player
	static ConstructorHelpers::FObjectFinder<UMaterialInstanceConstant> HitPlayerOb(TEXT("MaterialInstanceConstant'/Game/UI/HUD/Popups/mtl_hit_player_inst.mtl_hit_player_inst'"));
	TextHitPlayerMIC = HitPlayerOb.Object;

		// Killed Player
	static ConstructorHelpers::FObjectFinder<UMaterialInstanceConstant> KilledPlayerOb(TEXT("MaterialInstanceConstant'/Game/UI/HUD/Popups/mtl_killed_player_inst.mtl_killed_player_inst'"));
	TextKilledPlayerMIC = KilledPlayerOb.Object;

		// Respawn Victim
	static ConstructorHelpers::FObjectFinder<UMaterialInstanceConstant> RespawnVictimOb(TEXT("MaterialInstanceConstant'/Game/UI/HUD/Popups/mtl_respawn_victim_inst.mtl_respawn_victim_inst'"));
	TextRespawnVictimMIC = RespawnVictimOb.Object;

		// Respawn Killer
	static ConstructorHelpers::FObjectFinder<UMaterialInstanceConstant> RespawnKillerOb(TEXT("MaterialInstanceConstant'/Game/UI/HUD/Popups/mtl_respawn_killer_inst.mtl_respawn_killer_inst'"));
	TextRespawnKillerMIC = RespawnKillerOb.Object;

	// Physics Materials

		// Dirt
	static ConstructorHelpers::FObjectFinder<UPhysicalMaterial> PhysicsDirtOb(TEXT("PhysicalMaterial'/Game/Materials/PhysMaterial/Dirt.Dirt'"));
	PhysicsMaterials[(int32)ESurfaceType::Dirt] = PhysicsDirtOb.Object;
		// Metal
	static ConstructorHelpers::FObjectFinder<UPhysicalMaterial> PhysicsMetalOb(TEXT("PhysicalMaterial'/Game/Materials/PhysMaterial/Metal.Metal'"));
	PhysicsMaterials[(int32)ESurfaceType::Metal] = PhysicsMetalOb.Object;
		// Wood
	static ConstructorHelpers::FObjectFinder<UPhysicalMaterial> PhysicsWoodOb(TEXT("PhysicalMaterial'/Game/Materials/PhysMaterial/Wood.Wood'"));
	PhysicsMaterials[(int32)ESurfaceType::Wood] = PhysicsWoodOb.Object;
		// Stone
	static ConstructorHelpers::FObjectFinder<UPhysicalMaterial> PhysicsStoneOb(TEXT("PhysicalMaterial'/Game/Materials/PhysMaterial/Stone.Stone'"));
	PhysicsMaterials[(int32)ESurfaceType::Stone] = PhysicsStoneOb.Object;
		// Glass
	static ConstructorHelpers::FObjectFinder<UPhysicalMaterial> PhysicsGlassOb(TEXT("PhysicalMaterial'/Game/Materials/PhysMaterial/Glass.Glass'"));
	PhysicsMaterials[(int32)ESurfaceType::Glass] = PhysicsGlassOb.Object;
		// Organic
	static ConstructorHelpers::FObjectFinder<UPhysicalMaterial> PhysicsOrganicOb(TEXT("PhysicalMaterial'/Game/Materials/PhysMaterial/Organic.Organic'"));
	PhysicsMaterials[(int32)ESurfaceType::Organic] = PhysicsOrganicOb.Object;
		// Cloth
	static ConstructorHelpers::FObjectFinder<UPhysicalMaterial> PhysicsClothOb(TEXT("PhysicalMaterial'/Game/Materials/PhysMaterial/Cloth.Cloth'"));
	PhysicsMaterials[(int32)ESurfaceType::Cloth] = PhysicsClothOb.Object;
	
	bEnableNameTags = true;

	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		GameData = URsGameSingleton::Get()->GetGameData();
		GameEventData = CastChecked<AShooterGameEventData>(GameData->GameEventData.GetDefaultObject());

		DataMapping = URsGameSingleton::Get()->GetDataMapping();
	}

	// Game Mode Data

	CurrentTeamCapturingPoint = INDEX_NONE;
	CurrentCapturePointState  = ECapturePointState::Active;
	ProjectilePoolIndex = 0;

	BotPoolFilled = false;

#endif // #if WITH_RELOAD_STUDIOS
#if !UE_BUILD_SHIPPING
	bUseFXDebug = false;
#endif // !UE_BUILD_SHIPPING

	//Read in info for pointIndex
	LoadPointValues();
}

void AShooterGameState::GetLifetimeReplicatedProps( TArray< FLifetimeProperty > & OutLifetimeProps ) const
{
	Super::GetLifetimeReplicatedProps( OutLifetimeProps );

	DOREPLIFETIME( AShooterGameState, NumTeams );
	DOREPLIFETIME( AShooterGameState, RemainingTime );
	DOREPLIFETIME( AShooterGameState, bTimerPaused );
	DOREPLIFETIME(AShooterGameState, TeamAScore);
	DOREPLIFETIME(AShooterGameState, TeamBScore);
#if WITH_RELOAD_STUDIOS
	DOREPLIFETIME( AShooterGameState, bIsLanGame );
	DOREPLIFETIME( AShooterGameState, UseCharacterSimpleCollision );
	DOREPLIFETIME( AShooterGameState, CharacterPool );
	DOREPLIFETIME( AShooterGameState, BotCharacterPool );
	DOREPLIFETIME( AShooterGameState, PlayerStateMapping );
	DOREPLIFETIME( AShooterGameState, BotPlayerStateMapping );
	DOREPLIFETIME( AShooterGameState, PickupClassPool );
	DOREPLIFETIME( AShooterGameState, TimeToCapturePoint );
	DOREPLIFETIME( AShooterGameState, CurrentCapturePointState );
	DOREPLIFETIME( AShooterGameState, CurrentTeamCapturingPoint );
	DOREPLIFETIME( AShooterGameState, CurrentIdolState );
	DOREPLIFETIME( AShooterGameState, CurrentIdolLocation );
	DOREPLIFETIME( AShooterGameState, CooldownTimeBetweenCaptures );
	DOREPLIFETIME( AShooterGameState, HasFinishedMatch );
	DOREPLIFETIME(AShooterGameState, RoundTime);
	DOREPLIFETIME(AShooterGameState, LockInState);
	DOREPLIFETIME(AShooterGameState, bLockInComplete);
	DOREPLIFETIME(AShooterGameState, CurrentObjectiveTarget);
	DOREPLIFETIME(AShooterGameState, ObjectiveTargetA);
	DOREPLIFETIME(AShooterGameState, ObjectiveTargetB);
#endif // #if WITH_RELOAD_STUDIOS

#if WITH_RELOAD_STUDIOS
#if !UE_BUILD_SHIPPING
	DOREPLIFETIME( AShooterGameState, bEnableNameTags );
#endif // #if !UE_BUILD_SHIPPING
#endif // #if WITH_RELOAD_STUDIOS
}

void AShooterGameState::GetRankedMap(int32 TeamIndex, RankedPlayerMap& OutRankedMap) const
{
	OutRankedMap.Empty();

	//first, we need to go over all the PlayerStates, grab their score, and rank them
	TMultiMap<int32, AShooterPlayerState*> SortedMap;
	for(int32 i = 0; i < PlayerArray.Num(); ++i)
	{
		int32 Score = 0;
		AShooterPlayerState* CurPlayerState = Cast<AShooterPlayerState>(PlayerArray[i]);
		if (CurPlayerState && (CurPlayerState->GetTeamNum() == TeamIndex))
		{
			SortedMap.Add(FMath::TruncToInt(CurPlayerState->Score), CurPlayerState);
		}
	}
	
	//sort by the keys
	SortedMap.KeySort(TGreater<int32>());

	//now, add them back to the ranked map
	OutRankedMap.Empty();

	int32 Rank = 0;
	for(TMultiMap<int32, AShooterPlayerState*>::TIterator It(SortedMap); It; ++It)
	{
		OutRankedMap.Add(Rank++, It.Value());
	}
}

void AShooterGameState::RequestFinishAndExitToMainMenu()
{
	if (AuthorityGameMode)
	{
		// we are server, tell the gamemode
		AShooterGameMode* const GameMode = Cast<AShooterGameMode>(AuthorityGameMode);
		if (GameMode)
		{
			GameMode->RequestFinishAndExitToMainMenu();
		}
	}
	else
	{
		// we are client, handle our own business
		UShooterGameInstance* GameInstance = Cast<UShooterGameInstance>(GetGameInstance());
		if (GameInstance)
		{
			GameInstance->RemoveSplitScreenPlayers();
		}

		AShooterPlayerController* const PrimaryPC = Cast<AShooterPlayerController>(GetGameInstance()->GetFirstLocalPlayerController());
		if (PrimaryPC)
		{
			check(PrimaryPC->GetNetMode() == ENetMode::NM_Client);
			PrimaryPC->HandleReturnToMainMenu();
		}
	}

}

//#if WITH_RELOAD_STUDIOS
bool AShooterGameState::IsOnSameTeam(int32 TeamIndex, int32 TeamIndexToCompare) const
{
	if (NumTeams == 0)
	{
		//	disabled check as this will throw an assert in basic training
		//		after switching BasicTraining mode from TeamDeathmatch to SinglePlayer
		//check(TeamIndex == TeamIndexToCompare);
		return false;
	}

	return (TeamIndex == TeamIndexToCompare);
}

bool AShooterGameState::IsOnSameTeam(const AShooterCharacter *character1, const AShooterCharacter *character2) const
{
	if (!character1 || !character2)
		return false;

	AShooterPlayerState *ps1 = Cast<AShooterPlayerState>(character1->PlayerState);
	AShooterPlayerState *ps2 = Cast<AShooterPlayerState>(character2->PlayerState);

	if (!ps1 || !ps2)
		return false;

	return IsOnSameTeam(ps1->GetTeamNum(), ps2->GetTeamNum());
}

void AShooterGameState::PostActorCreated()
{
	Super::PostActorCreated();

	FActorSpawnParameters SpawnInfo;
	SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnInfo.ObjectFlags |= RF_Transient;

	// Coroutine Scheduler
	CoroutineScheduler			= GetWorld()->SpawnActor<ACoroutineScheduler>(SpawnInfo);
	CoroutineScheduler->MyOwner = this;

	int32 MaxCount = 32;

	CharacterWarmUpQueue.Reserve(MaxCount);
	CharacterWarmUpStartTimes.Reserve(MaxCount);
	PlayerStateWarmUpQueue.Reserve(MaxCount);

	for (int32 Index = 0; Index < MaxCount; Index++)
	{
		CharacterWarmUpQueue.Add(NULL);
		CharacterWarmUpStartTimes.Add(0.0f);
		PlayerStateWarmUpQueue.Add(NULL);
	}

	// Actor Pool

	MaxCount = 16;
	ActorPool.Reserve(MaxCount);
	ActorTimes.Reserve(MaxCount);
	ActorStartTimes.Reserve(MaxCount);

	for (int32 Index = 0; Index < MaxCount; Index++)
	{
		AActor* Actor = GetWorld()->SpawnActor<ATargetPoint>(SpawnInfo);
		Actor->SetReplicates(false);
		Actor->Role = ROLE_None;
		GetWorld()->RemoveNetworkActor(Actor);

		Actor->SetActorHiddenInGame(true);
		Actor->SetActorTickEnabled(false);

		ActorPool.Add(Actor);
		ActorTimes.Add(0.0f);
		ActorStartTimes.Add(0.0f);
	}

	// Character Pool
	MaxCount = 10;

	if (Role == ROLE_Authority)
	{
		AShooterGameMode* ShooterGameMode = Cast<AShooterGameMode>(GetWorld()->GetAuthGameMode());

		if (ShooterGameMode)
		{
			CharacterPool.Reserve(MaxCount);
			for (int32 Index = 0; Index < MaxCount; Index++)
			{
				AShooterCharacter* Character = GetWorld()->SpawnActor<AShooterCharacter>(ShooterGameMode->DefaultPawnClass, FVector(0.0f), FRotator(0.0f), SpawnInfo);

				Character->DeActivate();
				Character->IndexInPool = Index;
				CharacterPool.Add(Character);
			}
		}
	}

	
	// Flipbook Pool
	const int32 MAX_FLIPBOOK_COUNT = 128;

	EffectsFlipBookArray.Reserve(MAX_FLIPBOOK_COUNT);
	for (int32 i = 0; i < MAX_FLIPBOOK_COUNT; i++)
	{
		AShooterEffectsFlipBook* Flipbook = GetWorld()->SpawnActor<AShooterEffectsFlipBook>(SpawnInfo);
		Flipbook->SetReplicates(false);
		Flipbook->Role = ROLE_None;
		GetWorld()->RemoveNetworkActor(Flipbook);

		EffectsFlipBookArray.Add(Flipbook);
	}

	// Scoreboard
	Scoreboard = GetWorld()->SpawnActor<ARsUMGActorScoreboard>();

	// Emitter Pool
	const int32 MAX_EMITTER_COUNT = 256;

	EmitterArray.Reserve(MAX_EMITTER_COUNT);
	for (int32 i = 0; i < MAX_EMITTER_COUNT; i++)
	{
		AShooterEmitter* Emitter = GetWorld()->SpawnActor<AShooterEmitter>(EmptyEmitter, SpawnInfo);
		Emitter->SetReplicates(false);
		Emitter->Role = ROLE_None;
		GetWorld()->RemoveNetworkActor(Emitter);
		
		Emitter->ResetEmitter();

		EmitterArray.Add(Emitter);
	}

	// Sound Pool
	MaxConcurrentSoundCount = 128;
	MaxConcurrentSoundCount = CVarSoundPoolCount->GetInt() > MaxConcurrentSoundCount ? CVarSoundPoolCount->GetInt() : MaxConcurrentSoundCount;

	MaxCount = MaxConcurrentSoundCount;
	SoundPool.Reserve(MaxCount);

	for (int32 Index = 0; Index < MaxCount; Index++)
	{
		AShooterSound* Sound = GetWorld()->SpawnActor<AShooterSound>(EmptySound, SpawnInfo);
		Sound->SetReplicates(false);
		Sound->Role = ROLE_None;
		GetWorld()->RemoveNetworkActor(Sound);
		SoundPool.Add(Sound);
		ResetSound(Sound);
	}

	// Projectile Pool

	MaxCount = 400;
	ProjectilePool.Reserve(MaxCount);

	for (int32 Index = 0; Index < MaxCount; Index++)
	{
		AShooterProjectile* Projectile = GetWorld()->SpawnActor<AShooterProjectile>(SpawnInfo);

		Projectile->IsActive = false;
		Projectile->IsInPool = true;
		Projectile->DeActivate();

		Projectile->SetReplicates(false);
		Projectile->Role = ROLE_None;
		GetWorld()->RemoveNetworkActor(Projectile);

		ProjectilePool.Add(Projectile);
		ProjectilesToDeActivate.Add(Projectile);

		while (ProjectilesToDeActivate.Num() > 0)
			OnTick_HandleProjectilesToDeActivate();
	}

	// Fake Projectile Pool

	MaxCount = 400;
	FakeProjectilePool.Reserve(MaxCount);

	for (int32 Index = 0; Index < MaxCount; Index++)
	{
		AShooterProjectile* Projectile = GetWorld()->SpawnActor<AShooterProjectile>(SpawnInfo);

		Projectile->IsActive = false;
		Projectile->IsInPool = true;
		Projectile->IsFake   = true;
		Projectile->DeActivate();

		Projectile->SetReplicates(false);
		Projectile->Role = ROLE_None;
		GetWorld()->RemoveNetworkActor(Projectile);

		FakeProjectilePool.Add(Projectile);
		ProjectilesToDeActivate.Add(Projectile);

		while (ProjectilesToDeActivate.Num() > 0)
			OnTick_HandleProjectilesToDeActivate();
	}

	if (Role == ROLE_Authority)
	{
		// Pickup Class Pool

		MaxCount = 32;
		PickupClassPool.Reserve(MaxCount);

		for (int32 Index = 0; Index < MaxCount; Index++)
		{
			AShooterPickup_Class* Pickup = GetWorld()->SpawnActor<AShooterPickup_Class>(BasePickupClass, SpawnInfo);

			PickupClassPool.Add(Pickup);
		}
	}

	// Mesh Pool

	MaxCount = 128;
	MeshPool.Reserve(MaxCount);
	MeshTimes.Reserve(MaxCount);
	MeshStartTimes.Reserve(MaxCount);
	MeshTypes.Reserve(MaxCount);
	MeshHasOwnerList.Reserve(MaxCount);
	MeshDrawDistances.Reserve(MaxCount);
	HitMarkerTypes.Reserve(MaxCount);

	for (int32 Index = 0; Index < MaxCount; Index++)
	{
		AStaticMeshActor* Mesh = GetWorld()->SpawnActor<AStaticMeshActor>(SpawnInfo);
		Mesh->SetReplicates(false);
		Mesh->Role = ROLE_None;
		GetWorld()->RemoveNetworkActor(Mesh);

		Mesh->SetMobility(EComponentMobility::Movable);
		Mesh->SetActorHiddenInGame(true);
		Mesh->SetActorTickEnabled(false);
		Mesh->GetStaticMeshComponent()->SetCollisionResponseToAllChannels(ECR_Ignore);
		Mesh->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->GetStaticMeshComponent()->SetRenderCustomDepth(true);
		Mesh->GetStaticMeshComponent()->bGenerateOverlapEvents = false;

		MeshPool.Add(Mesh);
		MeshTimes.Add(0.0f);
		MeshStartTimes.Add(0.0f);
		MeshTypes.Add(EMeshPoolType::EMeshPoolType_MAX);
		MeshHasOwnerList.Add(false);
		MeshDrawDistances.Add(3000.0f * 3000.0f);
		HitMarkerTypes.Add(EHitMarkerType::EHitMarkerType_MAX);
	}

	// Skeletal Mesh Pool

	MaxCount = 96;
	SkeletalMeshPool.Reserve(MaxCount);
	SkeletalMeshTimes.Reserve(MaxCount);
	SkeletalMeshStartTimes.Reserve(MaxCount);
	SkeletalMeshAvailableList.Reserve(MaxCount);
	SkeletalMeshHasOwnerList.Reserve(MaxCount);
	SkeletalMeshDrawDistances.Reserve(MaxCount);
	//AngelDeathInstances.Reserve(MaxCount);
	AngelDeathDataList.Reserve(MaxCount);
	AngelDeathStartTimes.Reserve(MaxCount);
	AngelDeathStartLocations.Reserve(MaxCount);
	AngelDeathTypes.Reserve(MaxCount);

	//SpawnInfo.ObjectFlags = RF_Transactional;

	for (int32 Index = 0; Index < MaxCount; Index++)
	{
		ASkeletalMeshActor* Mesh = GetWorld()->SpawnActor<ASkeletalMeshActor>(SpawnInfo);
		Mesh->SetReplicates(false);
		Mesh->Role = ROLE_None;
		GetWorld()->RemoveNetworkActor(Mesh);

		Mesh->GetSkeletalMeshComponent()->SetCastShadow(false);
		Mesh->GetSkeletalMeshComponent()->bCastDynamicShadow = false;
		Mesh->SetActorHiddenInGame(true);
		Mesh->SetActorTickEnabled(false);
		Mesh->GetSkeletalMeshComponent()->PrimaryComponentTick.bStartWithTickEnabled = false;
		Mesh->GetSkeletalMeshComponent()->SetCollisionObjectType(ECC_Pawn);
		Mesh->GetSkeletalMeshComponent()->SetCollisionResponseToAllChannels(ECR_Ignore);
		Mesh->GetSkeletalMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->GetSkeletalMeshComponent()->SetComponentTickEnabled(false);
		Mesh->GetSkeletalMeshComponent()->bGenerateOverlapEvents = false;
		Mesh->GetSkeletalMeshComponent()->SetRenderCustomDepth(true);

		SkeletalMeshPool.Add(Mesh);
		SkeletalMeshPoolIndexMapping.Add(Mesh) = Index;
		SkeletalMeshTimes.Add(0.0f);
		SkeletalMeshStartTimes.Add(0.0f);
		SkeletalMeshAvailableList.Add(true);
		SkeletalMeshHasOwnerList.Add(false);
		SkeletalMeshDrawDistances.Add(3000.0f * 3000.0f);
		SkeletalMeshBlendToRagdollList.Add(false);
		//AngelDeathInstances.Add(NULL);
		AngelDeathDataList.Add(NULL);
		AngelDeathStartTimes.Add(0.0f);
		AngelDeathStartLocations.Add(FVector::ZeroVector);
		AngelDeathTypes.Add(EAngelDeathType::EAngelDeathType_MAX);
	}

	// Text

	MaxCount = 16;
	TextPool.Reserve(MaxCount);
	TextTimes.Reserve(MaxCount);
	TextStartTimes.Reserve(MaxCount);
	TextTypes.Reserve(MaxCount);
	TextHasOwnerList.Reserve(MaxCount);

	for (int32 Index = 0; Index < MaxCount; Index++)
	{
		ATextRenderActor* Text = GetWorld()->SpawnActor<ATextRenderActor>(SpawnInfo);
		Text->SetReplicates(false);
		Text->Role = ROLE_None;
		GetWorld()->RemoveNetworkActor(Text);

		Text->GetTextRender()->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
		Text->GetTextRender()->SetComponentTickEnabled(false);
		Text->SetActorHiddenInGame(true);
		Text->SetActorTickEnabled(false);

		TextPool.Add(Text);
		TextTimes.Add(0.0f);
		TextStartTimes.Add(0.0f);
		TextTypes.Add(ETextType::ETextType_MAX);
		TextHasOwnerList.Add(false);
	}

	AllPoolsHaveBeenCreated = true;
}

void AShooterGameState::SeamlessTravelTransitionCheckpoint(bool bToTransitionMap)
{
	Super::SeamlessTravelTransitionCheckpoint(bToTransitionMap);

	// make sure we are not holding on to any pooled assets
	for (int32 playerIndex = 0; playerIndex < PlayerArray.Num(); ++playerIndex)
	{
		check(PlayerArray[playerIndex]);
		PlayerArray[playerIndex]->Reset();
	}

	AllPoolsHaveBeenCreated = false;

	if (MatchEndActor && !MatchEndActor->IsPendingKill())
	{
		MatchEndActor->RemoveEndMatchActor();
		MatchEndActor->Destroy();
		MatchEndActor = NULL;
	}

	if (CoroutineScheduler && !CoroutineScheduler->IsPendingKill())
	{
		CoroutineScheduler->EndAll();
		CoroutineScheduler->MyOwner = NULL;
		CoroutineScheduler->MarkPendingKill();
		CoroutineScheduler = NULL;
	}

	if (Role == ROLE_Authority)
	{
		int32 CharacterPoolCount = CharacterPool.Num();

		for (int32 characterPoolIndex = CharacterPoolCount - 1; characterPoolIndex >= 0; --characterPoolIndex)
		{
			if (CharacterPool[characterPoolIndex] && !CharacterPool[characterPoolIndex]->IsPendingKill())
			{
				CharacterPool[characterPoolIndex]->Destroy(true);
			}
		}
		CharacterPool.Empty();

		int32 BotPoolCount = BotCharacterPool.Num();

		for (int32 Index = BotPoolCount - 1; Index >= 0; --Index)
		{
			AShooterBot* Bot = BotCharacterPool[Index];
			if (Bot && !Bot->IsPendingKill())
			{
				Bot->Destroy(true);
			}
			
		}

		BotCharacterPool.Empty();
	}

	// Emitters
	int32 Count = EmitterArray.Num();
	for (int32 Index = Count - 1; Index >= 0; --Index)
	{
		check(EmitterArray[Index]);
		EmitterArray[Index]->Destroy(true);
	}

	EmitterArray.Empty();

	Count = EffectsFlipBookArray.Num();
	for (int32 Index = Count - 1; Index >= 0; --Index)
	{
		check(EffectsFlipBookArray[Index]);
		EffectsFlipBookArray[Index]->Destroy(true);
	}

	EffectsFlipBookArray.Empty();

	

	// Sounds
	Count = SoundPool.Num();
	for (int32 Index = Count - 1; Index >= 0; --Index)
	{
		check(SoundPool[Index]);
		SoundPool[Index]->Destroy(true);
	}

	SoundPool.Empty();
	PlayingSounds.Empty();
	VOSounds.Empty();

	// Projectiles
	Count = ProjectilePool.Num();
	for (int32 Index = Count - 1; Index >= 0; --Index)
	{
		check(ProjectilePool[Index]);
		ProjectilePool[Index]->Destroy(true);
	}

	ProjectilePool.Empty();
	ProjectilesToDeActivate.Empty();

	// Fake Projectiles
	Count = FakeProjectilePool.Num();
	for (int32 Index = Count - 1; Index >= 0; --Index)
	{
		check(FakeProjectilePool[Index]);
		FakeProjectilePool[Index]->Destroy(true);
	}

	FakeProjectilePool.Empty();

	// Pickup Class
	AShooterGameMode* GameMode = GetWorld()->GetAuthGameMode<AShooterGameMode>();

	if (GameMode)
	{
		Count = PickupClassPool.Num();
		for (int32 Index = Count - 1; Index >= 0; --Index)
		{
			check(PickupClassPool[Index]);
			PickupClassPool[Index]->Destroy(true);
		}

		PickupClassPool.Empty();
	}

	// Meshes
	Count = MeshPool.Num();
	for (int32 Index = Count - 1; Index >= 0; --Index)
	{
		check(MeshPool[Index]);
		MeshPool[Index]->Destroy(true);
	}

	MeshPool.Empty();

	// SkeletalMeshes
	Count = SkeletalMeshPool.Num();
	for (int32 Index = Count - 1; Index >= 0; --Index)
	{
		check(SkeletalMeshPool[Index]);
		SkeletalMeshPool[Index]->Destroy(true);
	}

	SkeletalMeshPool.Empty();
	SkeletalMeshPoolIndexMapping.Empty();

	// Texts
	Count = TextPool.Num();
	for (int32 Index = Count - 1; Index >= 0; --Index)
	{
		check(TextPool[Index]);
		TextPool[Index]->Destroy(true);
	}

	TextPool.Empty();
}

void AShooterGameState::Explode(FVector Location, FExplosionParameters Parameters)
{
	Explode(Location, Parameters.radius, Parameters.damage, Parameters.Instigator, Parameters.bSelfDamage, Parameters.bAlliedDamage, Parameters.DamageType, Parameters.ExplosionParticleSystem, Parameters.ExplosionSound);
}

void AShooterGameState::Explode(FVector Location, float Radius, int32 Damage, AShooterCharacter* Instigator, bool bSelfDamage, bool bAlliedDamage, TEnumAsByte<EDamageType::Type> DamageType, UParticleSystem* ExplosionParticleSystem, USoundCue* ExplosionSoundCue)
{
	check(Role == ROLE_Authority);

	if (Radius <= 0.f)
	{
		return;
	}

	if (!(GetWorld() && GetWorld()->GameState))
	{
		return;
	}

	MulticastExplodeFX(Location, Radius, ExplosionParticleSystem, ExplosionSoundCue);

	//explosion damage event handling done on server
	for (FConstPawnIterator iterator = GetWorld()->GetPawnIterator(); iterator; ++iterator)
	{
		AShooterPlayerState* InstigatorPlayerState = Instigator ? Cast<AShooterPlayerState>(Instigator->PlayerState) : NULL;
		AController* InstigatorController = Instigator ? Instigator->GetController() : NULL;
		AShooterCharacter* OtherPawn = Cast<AShooterCharacter>(*iterator);

		if (!OtherPawn) {
			continue;
		}

		AShooterPlayerState* OtherPlayerState = Cast<AShooterPlayerState>(OtherPawn->PlayerState);

		if (!OtherPlayerState) {
			continue;
		}

		if (InstigatorPlayerState && OtherPlayerState == InstigatorPlayerState && !bSelfDamage) {
			continue;
		}
		if (InstigatorPlayerState && InstigatorPlayerState != OtherPlayerState && UShooterStatics::IsOnSameTeam(GetWorld(), InstigatorPlayerState, OtherPawn) && !bAlliedDamage) {
			continue;
		}

		const FVector OtherLocation = OtherPawn->GetCapsuleComponent()->GetComponentLocation();
		if (500.f > 0.f)
		{
			FVector zeroedLocation = FVector(Location.X, Location.Y, 0.f);
			FVector zeroedOtherLocation = FVector(OtherLocation.X, OtherLocation.Y, 0.f);
			if (FVector::DistSquared(zeroedLocation, zeroedOtherLocation) < FMath::Square(Radius) &&
				FMath::Abs(Location.Z - OtherLocation.Z) < 500.f)
			{
				AShooterGameState* GameState = Cast<AShooterGameState>(GetWorld()->GameState);
				TSubclassOf<class UDamageType> DamageClass = GameState->GetDamageClassFromType(DamageType);

				FHitResult SweepResult;
				FPointDamageEvent DamageEvent(Damage, SweepResult, -SweepResult.Normal, DamageClass);

				OtherPawn->TakeDamage(Damage, DamageEvent, InstigatorController, this);
			}
		}
		else if (FVector::DistSquared(Location, OtherLocation) < FMath::Square(Radius))
		{
			AShooterGameState* GameState = Cast<AShooterGameState>(GetWorld()->GameState);
			TSubclassOf<class UDamageType> DamageClass = GameState->GetDamageClassFromType(DamageType);

			FHitResult SweepResult;
			FPointDamageEvent DamageEvent(Damage, SweepResult, -SweepResult.Normal, DamageClass);

			OtherPawn->TakeDamage(Damage, DamageEvent, InstigatorController, this);
		}
	}
}

void AShooterGameState::MulticastExplodeFX_Implementation(FVector Location, float Radius, UParticleSystem* ExplosionParticleSystem, USoundCue* ExplosionSound)
{

	AShooterEmitter* Emitter = NULL;
	AShooterSound* Sound = NULL;
	UParticleSystem* ps = ExplosionParticleSystem ? ExplosionParticleSystem : GameData->DefaultExplosionParticleSystem;
	USoundCue* soundCue = ExplosionSound ? ExplosionSound : GameData->DefaultExplosionSound;
	Emitter = AllocateAndActivateEmitter(ps, 10.f);
	if (Emitter)
	{
		/*
		if (Emitter->GetParticleSystemComponent())
		{
			Emitter->GetParticleSystemComponent()->SetRelativeScale3D(FVector(Radius/100.f));
		}
		*/
		Emitter->DrawDistance = 20000.0f *20000.0f;
		Emitter->TeleportTo(Location, FRotator::ZeroRotator);
	}

	bool bIs1PSound = false;
	bool bLooping = false;
	bool bDelay = false;
	bool bSpatialized = false;
	Sound = AllocateSound(soundCue, NULL, bIs1PSound, bLooping, bDelay, bSpatialized, Location);
}

void AShooterGameState::Reset()
{
	for (TActorIterator<AShooterDestructible> Itr(GetWorld()); Itr; ++Itr)
	{
		if (Itr)
		{
			Itr->Reset();
		}
	}
}

void AShooterGameState::Tick(float DeltaSeconds)
{
	SCOPE_CYCLE_COUNTER(STAT_ShooterGameState);

	if (CoroutineScheduler &&
		!CoroutineScheduler->IsPendingKill())
		CoroutineScheduler->OnTick_Update();

	if (Role == ROLE_Authority)
	{
		AShooterGameMode* GameMode		   = Cast<AShooterGameMode>(GetWorld()->GetAuthGameMode());
		UShooterGameInstance* GameInstance = Cast<UShooterGameInstance>(GameMode->GetGameInstance());
		bIsLanGame						   = GameInstance->GetIsLanGame();
	}

	if (MatchState == MatchState::InProgress)
	{
		AShooterPlayerController* MachineClientController = UShooterStatics::GetMachineClientController(GetWorld());
		AShooterPlayerState* ps = MachineClientController ? Cast<AShooterPlayerState>(MachineClientController->PlayerState) : nullptr;
		if (ps)
		{
			if (RemainingTime == RoundTime - 5 && !bHasPlayedObjectiveVO)
			{
				if (ps->GetTeamNum() == 0)
				{
					if (GameData->ObjectiveUSVO)
					{
						bool bInttrupt = false;
						AShooterSound* sound = AllocateVOSound(GameData->ObjectiveUSVO, ps->GetOwner(), bInttrupt);
					}
				}
				else
				{
					if (GameData->ObjectiveGRVO)
					{
						bool bInttrupt = false;
						AShooterSound* sound = AllocateVOSound(GameData->ObjectiveGRVO, ps->GetOwner(), bInttrupt);
					}
				}
				bHasPlayedObjectiveVO = true;
			}

			if (RemainingTime * 2 <= RoundTime && !bHasPlayedHalfwayVO)
			{

				if (ps->GetTeamNum() == 0)
				{
					if (GameData->HalfDoneUSVO)
					{
						bool bInttrupt = false;
						AShooterSound* sound = AllocateVOSound(GameData->HalfDoneUSVO, ps->GetOwner(), bInttrupt);
					}
				}
				else
				{
					if (GameData->HalfDoneGRVO)
					{
						bool bInttrupt = false;
						AShooterSound* sound = AllocateVOSound(GameData->HalfDoneGRVO, ps->GetOwner(), bInttrupt);
					}
				}
				bHasPlayedHalfwayVO = true;
			}

			if (RemainingTime == 10 && !bHasPlayedTenSecondsVO)
			{

				if (ps->GetTeamNum() == 0)
				{
					if (GameData->TenSecondsUSVO)
					{
						bool bInttrupt = false;
						AShooterSound* sound = AllocateVOSound(GameData->TenSecondsUSVO, ps->GetOwner(), bInttrupt);
					}
				}
				else
				{
					if (GameData->TenSecondsGRVO)
					{
						bool bInttrupt = false;
						AShooterSound* sound = AllocateVOSound(GameData->TenSecondsGRVO, ps->GetOwner(), bInttrupt);
					}
				}
				bHasPlayedTenSecondsVO = true;
			}

			// special case. handle fading manually because time dilation may be enabled
			if (HasFinishedMatch)
			{
				if (!HasPlayedEndMatchActor)
				{
					SpawnEndMatchActor();
					HasPlayedEndMatchActor = true;
				}
				// TODO: pull this value from gamemode data?
				const float FinishMatchTime = 15.0f;
				const float FadeTime = 2.0f;

				float realDeltaTime = GetWorld()->RealTimeSeconds - FinishMatchStartTime;
				float realRemainingTime = FinishMatchTime - realDeltaTime;

				if (realRemainingTime < FadeTime)
				{
					float fadeAlpha = (realRemainingTime > 0) ? 1.0f - FMath::Clamp(realRemainingTime / FadeTime, 0.0f, 1.0f) : 1.0f;

					check(MachineClientController->PlayerCameraManager);
					MachineClientController->PlayerCameraManager->SetManualCameraFade(fadeAlpha, FLinearColor::Black, false);
				}
			}
		}
	}

	if (!AllPoolsHaveBeenCreated)
		return;

	if (GetMatchState() != MatchState::InProgress && GetMatchState() != MatchState::WaitingPostMatch)
		return;

	OnTick_UpdatePlayerStateMapping();
	OnTick_UpdateBotPlayerStateMapping();
	OnTick_HandleClientForceWarmUpLinkedPawn();
	OnTick_HandleClientInstantWarmUpLinkedPawn();
	OnTick_UpdateReplicatedPlayerStateMappingIds();
	OnTick_HandleActorPool(DeltaSeconds);
	OnTick_HandleSoundPool(DeltaSeconds);
	OnTick_HandleEmitterPool(DeltaSeconds);
	OnTick_HandlePickupClass(DeltaSeconds);
	OnTick_HandleMeshPool(DeltaSeconds);
	OnTick_HandleSkeletalMeshPool(DeltaSeconds);
	OnTick_HandleText();
	OnTick_HandleProjectilesToDeActivate();
		
	if (Role != ROLE_Authority)
		return; 

#if !UE_BUILD_SHIPPING
	// DEBUG - Check Character Pool

	int32 Count = CharacterPool.Num();

	for (int32 Index = 0; Index < Count; Index++)
	{
		if (!Cast<AShooterCharacter>(CharacterPool[Index]))
		{
			UE_LOG(LogShooter, Warning, TEXT("Character from CharacterPool was deleted / destroyed and the pointer at Index % d in the CharacterPool is invalid or null"), Index);
			check(0);
		}
	}

	// DEBUG - Check Bot Pool

	Count = BotCharacterPool.Num();

	for (int32 Index = 0; Index < Count; Index++)
	{
		if (!Cast<AShooterCharacter>(BotCharacterPool[Index]))
		{
			UE_LOG(LogShooter, Warning, TEXT("Character from BotCharacterPool was deleted / destroyed and the pointer at Index % d in the BotCharacterPool is invalid or null"), Index);
			check(0);
		}
	}
#endif // #if !UE_BUILD_SHIPPING
}

void AShooterGameState::OnRep_MatchState()
{
	UpdateFadeInOutState();

	Super::OnRep_MatchState();

	if (MatchState == MatchState::InProgress)
	{
		if (GetWorld())
		{
			// LevelScriptActors are put on the stack next
			for (ULevel* Level : GetWorld()->GetLevels())
			{
				AShooterLevelScriptActor* shooterScriptActor = Cast<AShooterLevelScriptActor>(Level->GetLevelScriptActor());
				if (shooterScriptActor)
				{
					shooterScriptActor->OnEndPregame();
				}
			}
		}
	}
	if (MatchState == MatchState::WaitingPostMatch)
	{
		if (GetWorld())
		{
			// LevelScriptActors are put on the stack next
			for (ULevel* Level : GetWorld()->GetLevels())
			{
				AShooterLevelScriptActor* shooterScriptActor = Cast<AShooterLevelScriptActor>(Level->GetLevelScriptActor());
				if (shooterScriptActor)
				{
					shooterScriptActor->OnEndMatch();
				}
			}
		}
	}
}

void AShooterGameState::HandleMatchIsWaitingToStart()
{
	Super::HandleMatchIsWaitingToStart();

	if (GetWorld())
	{
		// LevelScriptActors are put on the stack next
		for (ULevel* Level : GetWorld()->GetLevels())
		{
			AShooterLevelScriptActor* shooterScriptActor = Cast<AShooterLevelScriptActor>(Level->GetLevelScriptActor());
			if (shooterScriptActor)
			{
				shooterScriptActor->OnWaitingPrematch();
			}
		}
	}
}

void AShooterGameState::HandleMatchHasStarted()
{
	Super::HandleMatchHasStarted();

	if (GetWorld())
	{
		// LevelScriptActors are put on the stack next
		for (ULevel* Level : GetWorld()->GetLevels())
		{
			AShooterLevelScriptActor* shooterScriptActor = Cast<AShooterLevelScriptActor>(Level->GetLevelScriptActor());
			if (shooterScriptActor)
			{
				shooterScriptActor->OnMatchInProgress();
			}
		}
	}

	UBackendServicesManager::Get()->AnalyticsRecordEvent(TEXT("Gameplay:MatchStart"));
}

void AShooterGameState::HandleMatchHasEnded()
{
	Super::HandleMatchHasEnded();

	if (GetWorld())
	{
		// LevelScriptActors are put on the stack next
		for (ULevel* Level : GetWorld()->GetLevels())
		{
			AShooterLevelScriptActor* shooterScriptActor = Cast<AShooterLevelScriptActor>(Level->GetLevelScriptActor());
			if (shooterScriptActor)
			{
				shooterScriptActor->OnWaitingPostmatch();
			}
		}
	}

	for (int32 Index = 0; Index < CharacterPool.Num(); ++Index)
	{
		AShooterCharacter* Character = CharacterPool[Index];

		if (!Character || Character->IsPendingKill())
			continue;
		Character->DeActivate();
	}

	for (int32 Index = 0; Index < BotCharacterPool.Num(); ++Index)
	{
		AShooterBot* Bot = BotCharacterPool[Index];

		if (!Bot || Bot->IsPendingKill())
			continue;
		Bot->DeActivate();
	}

	UBackendServicesManager::Get()->AnalyticsRecordEvent(TEXT("Gameplay:MatchEnd"));
}

void AShooterGameState::HandleLeavingMap()
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (AShooterPlayerController* PlayerController = Cast<AShooterPlayerController>(*It))
		{
			PlayerController->ReleaseCameraOverride();
		}
	}

	Super::HandleMatchHasEnded();
}

// Game Mode Data
#pragma region

void AShooterGameState::OnRep_TeamAScore()
{
	AShooterPlayerController* MachineClientController = UShooterStatics::GetMachineClientController(GetWorld());

	if (MachineClientController)
	{
		MachineClientController->OnTeamScored.Broadcast();
	}
}

void AShooterGameState::OnRep_TeamBScore()
{
	AShooterPlayerController* MachineClientController = UShooterStatics::GetMachineClientController(GetWorld());

	if (MachineClientController)
	{
		MachineClientController->OnTeamScored.Broadcast();

	}
	if (GetWorld())
	{
		// LevelScriptActors are put on the stack next
		for (ULevel* Level : GetWorld()->GetLevels())
		{
			AShooterLevelScriptActor* shooterScriptActor = Cast<AShooterLevelScriptActor>(Level->GetLevelScriptActor());
			if (shooterScriptActor)
			{
				shooterScriptActor->OnTeamScore();
			}
		}
	}
}

void AShooterGameState::OnRep_LockInState()
{
	AShooterPlayerController* MachineClientController = UShooterStatics::GetMachineClientController(GetWorld());

	if (MachineClientController)
	{
		//for showing match lockin status
	}
}

void AShooterGameState::OnRep_LockInComplete()
{
	if (GetWorld())
	{
		// LevelScriptActors are put on the stack next
		for (ULevel* Level : GetWorld()->GetLevels())
		{
			AShooterLevelScriptActor* shooterScriptActor = Cast<AShooterLevelScriptActor>(Level->GetLevelScriptActor());
			if (shooterScriptActor)
			{
				shooterScriptActor->OnLockInComplete();
			}
		}
	}
}

void AShooterGameState::IncrementTeamScore(int32 TeamIndex)
{
	if (TeamIndex == 0)
	{
		TeamAScore++;
		OnRep_TeamAScore();
	}
	else if (TeamIndex == 1)
	{
		TeamBScore++;
		OnRep_TeamBScore();
	}
}

void AShooterGameState::AddTeamScore(int32 TeamIndex, int32 Score)
{
	if (TeamIndex == 0)
	{
		TeamAScore += Score;
		OnRep_TeamAScore();
	}
	else if (TeamIndex == 1)
	{
		TeamBScore += Score;
		OnRep_TeamBScore();
	}
}

void AShooterGameState::SetTeamScore(int32 TeamIndex, int32 Score)
{
	if (TeamIndex == 0)
	{
		TeamAScore = Score;
		OnRep_TeamAScore();
	} 
	else if(TeamIndex == 1)
	{
		TeamBScore = Score;
		OnRep_TeamBScore();
	}
}

int32 AShooterGameState::GetTeamScore(int32 TeamIndex)
{
	if(TeamIndex == 0) {
		return TeamAScore;
	} else if(TeamIndex == 1) {
		return TeamBScore;
	} else {
		return 0;
	}
}

void AShooterGameState::SetLocalObjectiveTarget(AActor* ObjectiveTarget)
{
	CurrentObjectiveTarget = ObjectiveTarget;
}

void AShooterGameState::SetObjectiveTargetA(AActor* inObjectiveTargetA)
{
	ObjectiveTargetA = inObjectiveTargetA;
}

void AShooterGameState::SetObjectiveTargetB(AActor* inObjectiveTargetB)
{
	ObjectiveTargetB = inObjectiveTargetB;
}

void AShooterGameState::OnRep_CurrentCapturePointState()
{
	AShooterPlayerController* MachineClientController = UShooterStatics::GetMachineClientController(GetWorld());

	if (CurrentCapturePointState == ECapturePointState::Active)
		MachineClientController->OnCapturePointBegin.Broadcast();
	else
		MachineClientController->OnCapturePointEnd.Broadcast();
}

void AShooterGameState::OnRep_CurrentIdolState()
{
	AShooterPlayerController* MachineClientController = UShooterStatics::GetMachineClientController(GetWorld());

	if (CurrentIdolState == EIdolState::Idle) {
		MachineClientController->OnIdolRespawn.Broadcast();
	}
	else if (CurrentIdolState == EIdolState::HeldA) {
		if (CachedIdolState == EIdolState::Idle)
		{
			MachineClientController->OnIdolTaken.Broadcast();
		}
		else 
		{
			MachineClientController->OnIdolPickup.Broadcast();
		}
	}
	else if (CurrentIdolState == EIdolState::HeldB) {
		if (CachedIdolState == EIdolState::Idle)
		{
			MachineClientController->OnIdolTaken.Broadcast();
		}
		else
		{
			MachineClientController->OnIdolPickup.Broadcast();
		}
	}
	else if (CurrentIdolState == EIdolState::CapturedA) {
		MachineClientController->OnTeamScored.Broadcast();
	}
	else if (CurrentIdolState == EIdolState::CapturedB) {
		MachineClientController->OnTeamScored.Broadcast();
	}
	else if (CurrentIdolState == EIdolState::DroppedA) {
		MachineClientController->OnIdolDropped.Broadcast();
	}
	else if (CurrentIdolState == EIdolState::DroppedB) {
		MachineClientController->OnIdolDropped.Broadcast();
	}

	CachedIdolState = CurrentIdolState;
}

void AShooterGameState::OnRep_CurrentObjectiveTarget()
{
	AShooterPlayerController* MachineClientController = UShooterStatics::GetMachineClientController(GetWorld());

	AShooterCharacter* localPawn = Cast<AShooterCharacter>(MachineClientController->AcknowledgedPawn);
	if (localPawn)
	{
		if(localPawn == CurrentObjectiveTarget)
		{
			if(Cast<AShooterPlayerState>(MachineClientController->PlayerState)->GetTeamNum())
			{
				localPawn->SetObjective(ObjectiveTargetA);
			}
			else
			{
				localPawn->SetObjective(ObjectiveTargetB);
			}
		}
		else
		{
			localPawn->SetObjective(CurrentObjectiveTarget);
		}
	}
}

void AShooterGameState::OnRep_CurrentIdolLocation()
{
	AShooterPlayerController* MachineClientController = UShooterStatics::GetMachineClientController(GetWorld());

	if (CachedIdolLocation == EIdolLocation::Field && CurrentIdolLocation == EIdolLocation::Pyramid) {
		MachineClientController->OnIdolEnterPyramid.Broadcast();
	}
	else if (CachedIdolLocation == EIdolLocation::Pyramid && CurrentIdolLocation == EIdolLocation::Field) {
		MachineClientController->OnIdolLeavePyramid.Broadcast();
	}

	CachedIdolLocation = CurrentIdolLocation;
}

void AShooterGameState::OnRep_CurrentTeamCapturingPoint()
{
	AShooterPlayerController* MachineClientController = UShooterStatics::GetMachineClientController(GetWorld());

	// None
	if (CurrentTeamCapturingPoint == INDEX_NONE)
	{
		MachineClientController->OnCapturePointNeutral.Broadcast();
	}
	// Team A
	else
	if (CurrentTeamCapturingPoint == 0)
	{
		MachineClientController->OnCapturedPointTeamA.Broadcast();
	}
	// Team B
	else
	if (CurrentTeamCapturingPoint == 1)
	{
		MachineClientController->OnCapturedPointTeamB.Broadcast();
	}
}

void AShooterGameState::SetMatchHasFinished(bool bInHasFinishedMatch)
{
	HasFinishedMatch = bInHasFinishedMatch;
	OnRep_HasFinishedMatch();
}

void AShooterGameState::OnRep_HasFinishedMatch()
{
	UpdateFadeInOutState();

	if (!HasFinishedMatch)
		return;

	AShooterPlayerController* MachineClientController = UShooterStatics::GetMachineClientController(GetWorld());

	// turn hud off when match has finished
	if (MachineClientController->GetHUD() && MachineClientController->GetHUD()->bShowHUD)
		MachineClientController->GetHUD()->ShowHUD();

	MachineClientController->OnFinishMatch.Broadcast();

	FinishMatchStartTime = GetWorld()->RealTimeSeconds;
}

void AShooterGameState::UpdateFadeInOutState()
{
	AShooterPlayerController* MachineClientController = UShooterStatics::GetMachineClientController(GetWorld());

	if (MatchState == MatchState::WaitingToStart)
	{
		// just loaded map. take 2 seconds and then fade into theater
		if (PreviousMatchState != MatchState::WaitingToStart)
		{
			//DelayedFadeIn(2.0f);
		}
		// about to transition to InProgress from theater. Fade out to black.
		else if (RemainingTime == 2)
		{
			//FadeOut(2.0f, false);
		}
	}
	else if (MatchState == MatchState::InProgress)
	{
		// joined match in progress
		if (PreviousMatchState == MatchState::EnteringMap)
		{
			DelayedFadeIn(2.0f);
		}
		else if (HasFinishedMatch)
		{
			// Since we are doing a slowdown time dilation, the fade system does not take this into account
			// To remedy, I've done a SetManualCameraFade in AShooterGameState::Tick that uses RealTimeSeconds
			//DelayedFadeOut(2.0f);
		}
	}
	else if (MatchState == MatchState::WaitingPostMatch)
	{
		// just loaded back into theater, fade back in
		//if (PreviousMatchState != MatchState::WaitingPostMatch)
		//{
		//	FadeIn(2.0f, false);
		//}
		//// about to restart. Fade out to black.
		//else if (RemainingTime == 2)
		//{
		//	FadeOut(2.0f, true);
		//}
	}
}

void AShooterGameState::DelayedFadeIn(float DelayTime)
{
	AShooterPlayerController* MachineClientController = UShooterStatics::GetMachineClientController(GetWorld());

	// make sure we are already in black screen
	if (MachineClientController)
	{
		if (MachineClientController->PlayerCameraManager)
		{
			MachineClientController->PlayerCameraManager->SetManualCameraFade(1.0f, FLinearColor::Black, false);
		}
	}

	GetWorldTimerManager().SetTimer(TimerHandle_DelayedFadeIn, this, &AShooterGameState::DelayedFadeInTimer, DelayTime, false);
}

void AShooterGameState::DelayedFadeInTimer()
{
	// TODO: make configurable
	FadeIn(2.0f, false);
}

void AShooterGameState::FadeIn(float FadeTime, bool bFadeAudio)
{
	check(FadeTime >= 0.0f);

	AShooterPlayerController* MachineClientController = UShooterStatics::GetMachineClientController(GetWorld());

	if (MachineClientController)
	{
		// fade in from black
		if (MachineClientController->PlayerCameraManager)
		{
			MachineClientController->PlayerCameraManager->StartCameraFade(1.0f, 0.0f, FadeTime, FLinearColor::Black, bFadeAudio, true);
		}
	}
}

void AShooterGameState::DelayedFadeOut(float DelayTime)
{
	GetWorldTimerManager().SetTimer(TimerHandle_DelayedFadeOut, this, &AShooterGameState::DelayedFadeOutTimer, DelayTime, false);
}

void AShooterGameState::DelayedFadeOutTimer()
{
	// TODO: make configurable
	FadeOut(2.0f, false);
}

void AShooterGameState::FadeOut(float FadeTime, bool bFadeAudio)
{
	check(FadeTime >= 0.0f);

	AShooterPlayerController* MachineClientController = UShooterStatics::GetMachineClientController(GetWorld());

	if (MachineClientController)
	{
		// fade out to black
		if (MachineClientController->PlayerCameraManager)
		{
			MachineClientController->PlayerCameraManager->StartCameraFade(0.0f, 1.0f, FadeTime, FLinearColor::Black, bFadeAudio, true);
		}
	}
}

#pragma endregion Game Mode Data

void AShooterGameState::OnRep_RemainingTime()
{
	// allow pre-empt a fade out
	UpdateFadeInOutState();

	if (GetMatchState() == MatchState::InProgress)
	{
		PlayTenSecondCountdown();
	}
}

void AShooterGameState::PlayTenSecondCountdown()
{
	if (RemainingTime > 10 || RemainingTime <= 0)
		return;

	AShooterPlayerController* MachineClientController = UShooterStatics::GetMachineClientController(GetWorld());

	FVector EyeLocation;
	FRotator LookAngles;

	MachineClientController->GetPlayerViewPoint(EyeLocation, LookAngles);

	UGameplayStatics::PlaySoundAtLocation(GetWorld(), TenSecondCountdown[RemainingTime - 1], EyeLocation);
}

void AShooterGameState::SpawnEndMatchActor()
{
	FRoutine* R = nullptr;

	AShooterPlayerController* LocalController = Cast<AShooterPlayerController>(GEngine->GetFirstLocalPlayerController(GetWorld()));
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnInfo.ObjectFlags |= RF_Transient;

	if (Cast<AShooterCharacter>(LocalController->GetPawn()))
		Cast<AShooterCharacter>(LocalController->GetPawn())->DoFreezeTankAbility = true;

	MatchEndActor = GetWorld()->SpawnActor<AShooterEndMatchActor>(SpawnInfo);

	MatchEndActor->DoUpdate = true;
	MatchEndActor->SetActorTickEnabled(true);

	/*MatchEndActor->SetEndMatchActor_TeamsBase(LocalController);

	if (!CoroutineScheduler)
		return;

	R = CoroutineScheduler->Allocate(&SpawnEndMatchActor_Internal, MatchEndActor, true, false);
	R->delay = 5;
	CoroutineScheduler->StartRoutine(R);*/

}

PT_THREAD(AShooterGameState::SpawnEndMatchActor_Internal(struct FRoutine* r))
{
	AShooterEndMatchActor* a = Cast<AShooterEndMatchActor>(r->GetActor());
	ACoroutineScheduler* s   = r->scheduler;
	UWorld* w				 = s->GetWorld();
	AShooterGameState* gs    = Cast<AShooterGameState>(w->GameState);

	const float CurrentTime = w->TimeSeconds;
	const float StartTime   = r->startTime;

	COROUTINE_BEGIN(r);

	if (r->delay > 0)
	{
		COROUTINE_WAIT_UNTIL(r, CurrentTime - StartTime > r->delay);
	}

	a->DoMatchEndWipeOut = true;

	COROUTINE_WAIT_UNTIL(r, a->HasFinishedMatchEndWipeOut);

	//// TODO: HACK: Not sure if this is the best way to "reset" the TimeDilation 
	//if (gs &&
	//	gs->Role == ROLE_Authority)
	//{
	//	if (AShooterGameMode* GameMode = Cast<AShooterGameMode>(w->GetAuthGameMode()))
	//	{
	//		GameMode->FinishMatchTime = 1.0f;
	//		GameMode->SetDilation(1.0f);
	//	}
	//}

	r->End();
	COROUTINE_END(r);
}

TArray<AShooterPlayerState*> AShooterGameState::RankPlayersByScore()
{
	TArray<AShooterPlayerState*> Players;
	TArray<AShooterPlayerState*> Result;

	for (int i = 0; i < PlayerArray.Num(); i++)
	{
		if (AShooterPlayerState* ps = Cast<AShooterPlayerState>(PlayerArray[i]))
			Players.Add(ps);
	}

	// Sort by highest kills first
	Players.Sort([](AShooterPlayerState& playerA, AShooterPlayerState& playerB)
	{
		if (playerA.GetPlayerPoints() > playerB.GetPlayerPoints())
		{
			return true;
		}
		else if (playerA.GetPlayerPoints() == playerB.GetPlayerPoints())
		{
			return playerA.GetKills() > playerB.GetKills();
		}
		else if (playerA.GetKills() == playerB.GetKills())
		{
			return playerA.GetAssists() > playerB.GetAssists();
		}
		else
		{
			return false;
		}
	});

	return Players;
}

// Respawn Selection
#pragma region

void AShooterGameState::ToggleRespawnSelectionScene(bool IsActive)
{
	RespawnSelectionScene->SetActorHiddenInGame(!IsActive);
	RespawnSelectionScene->SetActorTickEnabled(IsActive);
	// Audio
	if (UAudioComponent* AudioComponent = Cast<UAudioComponent>(RespawnSelectionScene->GetComponentByClass(UAudioComponent::StaticClass())))
	{
		if (IsActive)
		{
			AudioComponent->Activate();
			AudioComponent->SetComponentTickEnabled(true);
			AudioComponent->Play();
		}
		else
		{
			AudioComponent->Deactivate();
			AudioComponent->SetComponentTickEnabled(false);
			AudioComponent->Stop();
		}
	}
	// Post Process
	if (UBoxComponent* BoxComponent = Cast<UBoxComponent>(RespawnSelectionScene->GetComponentByClass(UBoxComponent::StaticClass())))
	{
		if (IsActive)
			BoxComponent->Activate();
		else
			BoxComponent->Deactivate();
		BoxComponent->SetComponentTickEnabled(IsActive);
	}

	if (UPostProcessComponent* PostProcessComponent = Cast<UPostProcessComponent>(RespawnSelectionScene->GetComponentByClass(UPostProcessComponent::StaticClass())))
	{
		PostProcessComponent->SetHiddenInGame(!IsActive);

		if (IsActive)
			PostProcessComponent->Activate();
		else
			PostProcessComponent->Deactivate();
		PostProcessComponent->SetComponentTickEnabled(IsActive);
	}
}

#pragma endregion Respawn Selection

// Actors
#pragma region

AActor* AShooterGameState::AllocateActor(float Time)
{
	int32 OutIndex = INDEX_NONE;
	return AllocateActor(Time, OutIndex);
}

AActor* AShooterGameState::AllocateActor(float Time, int32& OutIndex)
{
	OutIndex	  = GetAllocatedActorIndex();
	AActor* Actor = OutIndex > INDEX_NONE ? ActorPool[OutIndex] : NULL;

	if (Actor)
	{
		Actor->SetActorHiddenInGame(false);
		Actor->SetActorTickEnabled(true);
		Actor->SetActorScale3D(FVector(1.0f));

		ActorTimes[OutIndex]	  = Time;
		ActorStartTimes[OutIndex] = GetWorld()->TimeSeconds;
	}
	return Actor;
}

AActor* AShooterGameState::AllocateAndAttachActor(USceneComponent* InParent, float Time)
{
	AActor* Actor = AllocateActor(Time);

	if (Actor)
	{
		Actor->AttachToComponent(InParent, FAttachmentTransformRules::KeepRelativeTransform);
	}
	return Actor;
}

AActor* AShooterGameState::AllocateAndAttachActor(AActor* InParent, float Time)
{
	return AllocateAndAttachActor(InParent->GetRootComponent(), Time);
}

void AShooterGameState::DeAllocateActor(AActor* Actor)
{
	const int32 Count = ActorPool.Num();

	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (Actor == ActorPool[Index])
		{
			DeAllocateActor(Index);
			return;
		}
	}
}

void AShooterGameState::DeAllocateActor(int32 Index)
{
	AActor* Actor = ActorPool[Index];

	const TArray<USceneComponent*>& AttachChildren = Actor->GetRootComponent()->GetAttachChildren();
	const int32 Count							   = AttachChildren.Num();

	for (int32 I = 0; I < Count; I++)
	{
		USceneComponent* Child = AttachChildren[I];

		if (Child &&
			Child->GetAttachParent() != ActorPool[Index]->GetRootComponent())
		{
			Child->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);

			AActor* ChildActor = Cast<AActor>(Child->GetOuter());

			if (ChildActor)
				DeAllocateActor(ChildActor);
		}
	}

	Actor->SetActorHiddenInGame(true);
	Actor->SetActorRelativeLocation(FVector::ZeroVector);
	Actor->SetActorRelativeRotation(FRotator::ZeroRotator);
	Actor->SetActorRelativeScale3D(FVector(1.0f));
	Actor->SetActorScale3D(FVector(1.0f));
	Actor->DetachRootComponentFromParent();
	Actor->SetActorTickEnabled(false);
	Actor->SetOwner(NULL);

	ActorTimes[Index] = 0.0f;
}

void AShooterGameState::SetActorTime(int32 Index, float Time, bool UpdateStartTime)
{
	if (UpdateStartTime)
	{
		ActorStartTimes[Index] = GetWorld()->TimeSeconds;
	}
	ActorTimes[Index] = Time;
}

int32 AShooterGameState::GetAllocatedActorIndex()
{
	const int32 Count		  = ActorPool.Num();
	const int32 PreviousIndex = ActorPoolIndex;

	if (ActorPoolIndex >= Count)
		ActorPoolIndex = 0;

	// Check from ActorPoolIndex to Count
	for (int32 Index = ActorPoolIndex; Index < Count; ++Index)
	{
		ActorPoolIndex = Index + 1;

		if (ActorTimes[Index] == 0.0f)
			return Index;
	}

	// Check from 0 to PreviousIndex ( where ActorPoolIndex started )
	if (PreviousIndex > 0)
	{
		ActorPoolIndex = 0;

		for (int32 Index = ActorPoolIndex; Index < PreviousIndex; ++Index)
		{
			ActorPoolIndex = Index + 1;

			if (ActorTimes[Index] == 0.0f)
				return Index;
		}
	}
	return INDEX_NONE;
}

void AShooterGameState::OnTick_HandleActorPool(float DeltaSeconds)
{
	SCOPE_CYCLE_COUNTER(STAT_HandleActorPool);

	const int32 Count = ActorPool.Num();

	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (ActorTimes[Index] > 0.0f)
		{
			if (GetWorld()->TimeSeconds - ActorStartTimes[Index] > ActorTimes[Index])
			{
				DeAllocateActor(Index);
			}
		}
	}
}

#pragma endregion Actors

// Characters
#pragma region

void AShooterGameState::OnRep_CharacterPool()
{
	const int32 Count = CharacterPool.Num();

	for (int32 Index = 0; Index < Count; Index++)
	{
		if (CharacterPool[Index])
			CharacterPool[Index]->IndexInPool = Index;
	}
}

void AShooterGameState::OnRep_BotCharacterPool()
{
	const int32 Count = BotCharacterPool.Num();

	for (int32 Index = 0; Index < Count; Index++)
	{
		if (BotCharacterPool[Index])
			BotCharacterPool[Index]->IndexInPool = Index;
	}
}

AShooterCharacter* AShooterGameState::AllocateCharacter(AShooterPlayerController* InPlayer)
{
	const int32 AllocatedIndex   = GetAllocatedCharacterIndex(InPlayer);
	AShooterCharacter* Character = AllocatedIndex > INDEX_NONE ? CharacterPool[AllocatedIndex] : NULL;
	check(Character);
	return Character;
}

AShooterCharacter* AShooterGameState::AllocateCharacter(AShooterPlayerController* InPlayer, FVector Location, FRotator Rotation)
{
	AShooterCharacter* Character = AllocateCharacter(InPlayer);

	check(Character);
	verify(Character->TeleportTo(Location, Rotation, false, true));

	Character->SpawnLocationAndRotation.Location = Location;
	Character->SpawnLocationAndRotation.Rotation = Rotation;

	return Character;
}

int32 AShooterGameState::GetAllocatedCharacterIndex(AShooterPlayerController* InPlayer)
{
	const int32 Count	 = CharacterPool.Num();
	int32 AllocatedIndex = INDEX_NONE;

	AShooterPlayerState* PlayerState = Cast<AShooterPlayerState>(InPlayer->PlayerState);

	if (PlayerState->LinkedPawn.IsValid() &&
		PlayerState->LinkedPawn.Get() &&
		PlayerState->LinkedPawnIndex != INVALID_LINKED_PAWN_INDEX)
	{
		if (InPlayer->LinkedPawnIndex == INVALID_LINKED_PAWN_INDEX)
		{
			InPlayer->LinkedPawn	  = PlayerState->LinkedPawn;
			InPlayer->LinkedPawnIndex = PlayerState->LinkedPawnIndex;

			FString Proxy = Role == ROLE_Authority ? TEXT("Server") : TEXT("Client");
			UE_LOG(LogShooter, Warning, TEXT("GetAllocatedCharacterIndex (%s): Player did NOT have valid LinkedPawn but it's PlayerState: %s did"), *Proxy, *PlayerState->GetShortPlayerName());
		}
		AllocatedIndex = PlayerState->LinkedPawnIndex;
	}
	check(AllocatedIndex != INVALID_LINKED_PAWN_INDEX);
	return AllocatedIndex;
}

void AShooterGameState::CreateBotPool()
{
	if (Role == ROLE_Authority)
	{
		FActorSpawnParameters SpawnInfo;
		SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnInfo.ObjectFlags |= RF_Transient;

		AShooterGameMode* ShooterGameMode = Cast<AShooterGameMode>(GetWorld()->GetAuthGameMode());
		BotCharacterPool.Reserve(9);
		for (int32 Count = 0; Count <9; Count++)
		{
			AShooterBot* Bot = GetWorld()->SpawnActor<AShooterBot>(ShooterGameMode->BotPawnClass, FVector(0.0f), FRotator(0.0f), SpawnInfo);
			Bot->DeActivate();
			BotCharacterPool.Add(Bot);
		}
		BotPoolFilled = true;
	}
}

bool AShooterGameState::IsBotInPool(AShooterBot* inPawn)
{
	for (int itr = 0; itr < BotCharacterPool.Num(); itr++)
	{
		if(inPawn == BotCharacterPool[itr])
		{
			return true;
		}
	}
	return false;
}

void AShooterGameState::SetLinkedPawnForPlayerState(AController* InController, AShooterPlayerState* InPlayerState)
{
	// TODO: HACK: For STP. Needs to generalize this better
	const bool canPossessPawn = Cast<AShooterGameMode>(GetWorld()->GetAuthGameMode())->CanPossessPawnPostInit();

	// Player
	if (AShooterPlayerController* PlayerController = Cast<AShooterPlayerController>(InController))
	{
		IsCharacterPoolEmpty = true;

		const int32 Count = CharacterPool.Num();

		for (int32 Index = 0; Index < Count; ++Index)
		{
			AShooterCharacter* Character = CharacterPool[Index];

			if (Character->LinkedController.Get() == NULL)
			{
				PlayerController->LinkedPawn	  = Character;
				PlayerController->LinkedPawnIndex = Index;

				InPlayerState->LinkedPawn	   = Character;
				InPlayerState->LinkedPawnIndex = Index;
				InPlayerState->OnLinkedPawnSetEvent.Broadcast();

				Character->LinkedController  = PlayerController;
				Character->LinkedPlayerState = InPlayerState;

				if ((!PlayerController->IsFirstSpawn || canPossessPawn) &&
					(!InPlayerState->PlayerInfo.IsValid() ||
					 !InPlayerState->PlayerInfo.SkipWarmingUpLinkedPawn))
				{
					InPlayerState->ServerSetIsWarmingUpLinkedPawn(true);
				}

				IsCharacterPoolEmpty = false;
				return;
			}
		}
	}
	// A.I.
	if (AShooterAIController* AIController = Cast<AShooterAIController>(InController))
	{
		IsBotCharacterPoolEmpty = true;

		const int32 Count = BotCharacterPool.Num();

		for (int32 Index = 0; Index < Count; ++Index)
		{
			AShooterCharacter* Character = BotCharacterPool[Index];

			if (Character->LinkedController.Get() == NULL)
			{
				AIController->LinkedPawn	  = Character;
				AIController->LinkedPawnIndex = Index;

				InPlayerState->LinkedPawn	   = Character;
				InPlayerState->LinkedPawnIndex = Index;
				InPlayerState->OnLinkedPawnSetEvent.Broadcast();

				Character->LinkedController  = AIController;
				Character->LinkedPlayerState = InPlayerState;

				if (!InPlayerState->PlayerInfo.IsValid() ||
					!InPlayerState->PlayerInfo.SkipWarmingUpLinkedPawn)
				{
					InPlayerState->ServerSetIsWarmingUpLinkedPawn(true);
				}

				IsBotCharacterPoolEmpty = false;
				return;
			}
		}
	}
}

void AShooterGameState::AddCharacterToWarmUpQueue(AShooterPlayerState* InPlayerState, AShooterCharacter* InCharacter)
{
	const int32 Count = CharacterWarmUpQueue.Num();

	for (int32 Index = 0; Index < Count; Index++)
	{
		if (!CharacterWarmUpQueue[Index])
		{
			CharacterWarmUpQueue[Index]		 = InCharacter;
			CharacterWarmUpStartTimes[Index] = GetWorld()->TimeSeconds;
			PlayerStateWarmUpQueue[Index]	 = InPlayerState;
			return;
		}
	}
}

void AShooterGameState::RemoveCharacterFromWarmUpQueue(AShooterCharacter* InCharacter)
{
	// Remove all Null references
	const int32 Count = CharacterWarmUpQueue.Num();

	for (int32 Index = 0; Index < Count - 1; Index++)
	{
		if (CharacterWarmUpQueue[Index] == InCharacter ||
			(CharacterWarmUpQueue[Index] &&
			 (!CharacterWarmUpQueue[Index]->LinkedPlayerState.IsValid() ||
			  !CharacterWarmUpQueue[Index]->LinkedPlayerState.Get())))
		{
			CharacterWarmUpQueue[Index]		 = NULL;
			CharacterWarmUpStartTimes[Index] = 0.0f;
			PlayerStateWarmUpQueue[Index].Reset();
			PlayerStateWarmUpQueue[Index] = NULL;
		}
		
		// Character
		AShooterCharacter* Current = CharacterWarmUpQueue[Index];
		AShooterCharacter* Next	   = CharacterWarmUpQueue[Index + 1];
		// Start Time
		float NextStartTime = CharacterWarmUpStartTimes[Index + 1];
		// PlayerState
		AShooterPlayerState* NextPlayerState = PlayerStateWarmUpQueue[Index + 1].IsValid() ? PlayerStateWarmUpQueue[Index + 1].Get() : NULL;

		// If Current is NULL, replace current with Next and make Next NULL
		if (!Current)
		{
			// Character
			CharacterWarmUpQueue[Index]	    = Next;
			CharacterWarmUpQueue[Index + 1] = NULL;
			// Start Time
			CharacterWarmUpStartTimes[Index]	 = NextStartTime;
			CharacterWarmUpStartTimes[Index + 1] = 0.0f;
			// PlayerState
			PlayerStateWarmUpQueue[Index] = NextPlayerState;
			PlayerStateWarmUpQueue[Index + 1] = NULL;
		}

		if (CharacterWarmUpQueue[Index] == InCharacter ||
			(CharacterWarmUpQueue[Index] &&
			 (!CharacterWarmUpQueue[Index]->LinkedPlayerState.IsValid() ||
			  !CharacterWarmUpQueue[Index]->LinkedPlayerState.Get())))
		{
			AShooterPlayerState* Last_PlayerState = PlayerStateWarmUpQueue[Index].IsValid() ? PlayerStateWarmUpQueue[Index].Get() : NULL;
			CharacterWarmUpQueue[Index]		 = NULL;
			CharacterWarmUpStartTimes[Index] = 0.0f;
			PlayerStateWarmUpQueue[Index].Reset();
			PlayerStateWarmUpQueue[Index] = NULL;

			if (Last_PlayerState)
			{
				UE_LOG(LogShooter, Warning, TEXT("Character was added twice to the WarmUpQueue for Player: %s. Need to investigate why this is happenening"), *Last_PlayerState->GetShortPlayerName());
			}
			else
			{
				UE_LOG(LogShooter, Warning, TEXT("Character was added twice to the WarmUpQueue. Need to investigate why this is happenening"));
			}
		}
	}

	if (CharacterWarmUpQueue[Count - 1] == InCharacter)
	{
		CharacterWarmUpQueue[Count - 1]		 = NULL;
		CharacterWarmUpStartTimes[Count - 1] = 0.0f;
		PlayerStateWarmUpQueue[Count - 1].Reset();
		PlayerStateWarmUpQueue[Count - 1] = NULL;
	}
}

void AShooterGameState::InsertCharacterToWarmUpQueue(AShooterPlayerState* InPlayerState, AShooterCharacter* InCharacter, int32 InsertIndex)
{
	const int32 Count = CharacterWarmUpQueue.Num();

	if (InsertIndex < 0 || InsertIndex >= Count)
		return;

	// Character
	AShooterCharacter* LastCharacter  = CharacterWarmUpQueue[InsertIndex];
	CharacterWarmUpQueue[InsertIndex] = InCharacter;
	// Start Time
	float LastStartTime					   = GetWorld()->TimeSeconds;
	CharacterWarmUpStartTimes[InsertIndex] = LastStartTime;
	// PlayerState
	AShooterPlayerState* LastPlayerState = PlayerStateWarmUpQueue[InsertIndex].IsValid() ? PlayerStateWarmUpQueue[InsertIndex].Get() : NULL;
	PlayerStateWarmUpQueue[InsertIndex]  = InPlayerState;


	for (int32 Index = InsertIndex; Index < Count - 1; Index++)
	{
		// Character
		AShooterCharacter* NextCharacter = CharacterWarmUpQueue[Index + 1];
		CharacterWarmUpQueue[Index + 1]  = LastCharacter;
		LastCharacter				     = NextCharacter;
		// Start Time
		float NextStartTime					 = CharacterWarmUpStartTimes[Index + 1];
		CharacterWarmUpStartTimes[Index + 1] = LastStartTime;
		LastStartTime						 = NextStartTime;
		// PlayerState
		AShooterPlayerState* NextPlayerState = PlayerStateWarmUpQueue[Index + 1].IsValid() ? PlayerStateWarmUpQueue[Index + 1].Get() : NULL;
		PlayerStateWarmUpQueue[Index + 1]    = LastPlayerState;
		LastPlayerState						 = NextPlayerState;

		if (LastCharacter == NULL)
			return;
	}
}

bool AShooterGameState::IsCharacterReadyToWarmUp(AShooterCharacter* InCharacter)
{
	CheckCharactersInWarmUpQueue();

	return CharacterWarmUpQueue[0] == InCharacter;
}

void AShooterGameState::CheckCharactersInWarmUpQueue()
{
	APlayerController* MachineClientController = UShooterStatics::GetMachineClientController(GetWorld());
	AShooterPlayerState* ClientPlayerState	   = MachineClientController ? Cast<AShooterPlayerState>(MachineClientController->PlayerState) : NULL;

	FString Proxy = UShooterStatics::GetProxyAsString(this);

	const int32 Count					= CharacterWarmUpQueue.Num();
	const float CharacterWarmUp_Timeout = 2.0f;

	// For Warm Ups that have timed out, warm up the pawn immediately
	for (int32 Index = 0; Index < Count; Index++)
	{
		AShooterCharacter* Character = CharacterWarmUpQueue[Index];

		if (!Character)
			continue;

		if (!PlayerStateWarmUpQueue[Index].IsValid() || !PlayerStateWarmUpQueue[Index].Get())
			continue;

		AShooterPlayerState* PlayerState = PlayerStateWarmUpQueue[Index].Get();

		if (!PlayerState->HasFinishedLinkingPawn())
			continue;

		const bool IsBot = Cast<AShooterBot>(Character) != NULL;

		if (!IsPlayerStateFullyMapped(PlayerState, IsBot))
			continue;

		if (CharacterWarmUpStartTimes[Index] > 0.0f &&
			GetWorld()->TimeSeconds - CharacterWarmUpStartTimes[Index] > CharacterWarmUp_Timeout)
		{
			// Normal WarmUp
			if (PlayerState->PlayerInfo.IsWarmingUpLinkedPawn)
			{
				while (Character &&
					   !Character->IsWarmingUp_Completion_Sent)
				{
					Character->OnTick_HandleIsWarmingUp(PlayerState);
				}
			}
			// Instant WarmUp
			else
			if (PlayerState->PlayerInfo.SkipWarmingUpLinkedPawn)
			{
				Character->DoInstantWarmUp(PlayerState);
			}
			else
			{
				continue;
			}

			if (ClientPlayerState)
			{
				UE_LOG(LogShooter, Warning, TEXT("CheckCharactersInWarmUpQueue (%s): Force Immediate Warming Up of Pawn for Player: %s on Client: %s"), *Proxy, *PlayerState->GetShortPlayerName(), *ClientPlayerState->GetShortPlayerName());
			}
			else
			{
				UE_LOG(LogShooter, Warning, TEXT("CheckCharactersInWarmUpQueue (%s): Force Immediate Warming Up of Pawn for Player: %s"), *Proxy, *PlayerState->GetShortPlayerName());
			}
			Index--;
		}
	}
}

AShooterCharacter* AShooterGameState::GetCharacter(uint8 PawnMappingId, bool IsBot)
{
	if (PawnMappingId == INVALID_PAWN)
		return NULL;

	if ((!IsBot && PawnMappingId >= CharacterPool.Num()) ||
		(IsBot && PawnMappingId >= BotCharacterPool.Num()))
		return NULL;

	return IsBot ? BotCharacterPool[PawnMappingId] : CharacterPool[PawnMappingId];
}

uint8 AShooterGameState::GetCharacterMappingId(AShooterCharacter* InCharacter, bool IsBot)
{
	if (!InCharacter)
		return INVALID_PAWN;
	return IsBot ? BotCharacterPool.Find(Cast<AShooterBot>(InCharacter)) : CharacterPool.Find(InCharacter);
}

void AShooterGameState::SetHumanPlayerStateCount()
{
	HumanPlayerStateCount = 0;
	const int32 Count = PlayerArray.Num();

	for (int32 Index = 0; Index < Count; Index++)
	{
		if (!PlayerArray[Index])
			continue;

		if (Cast<AShooterPlayerController>(PlayerArray[Index]->GetOwner()))
			HumanPlayerStateCount++;
	}
}

void AShooterGameState::AddPlayerState(class APlayerState* PlayerState)
{
	Super::AddPlayerState(PlayerState);
	UShooterGameInstance* sgi = Cast<UShooterGameInstance>(GetGameInstance());

	if (Role == ROLE_Authority)
	{
		if (Cast<AAIController>(PlayerState->GetOwner()) && !PlayerState->bIsInactive)
		{
			if (!sgi->AvailableBotNames.IsEmpty())
			{
				PlayerState->PlayerName = sgi->AvailableBotNames.PullName(Cast<AShooterPlayerState>(PlayerState)->GetTeamNum());
			}
			else
			{
				// ran out of bot names, give generic names
				static uint8 botCount = 0;
				PlayerState->PlayerName = FString::Printf(TEXT("Bot-%03d"), botCount++);
			}
		}
		SetHumanPlayerStateCount();
	}
}

void AShooterGameState::RemovePlayerState(class APlayerState* PlayerState)
{
	UShooterGameInstance* sgi = Cast<UShooterGameInstance>(GetGameInstance());
	
	if (Role == ROLE_Authority)
	{
		if (Cast<AAIController>(PlayerState->GetOwner()) && PlayerState->PlayerName.Len())
		{
			// return non-generic names
			if (PlayerState->PlayerName.Left(4) != TEXT("Bot-"))
			{
				sgi->AvailableBotNames.ReturnName(Cast<AShooterPlayerState>(PlayerState)->GetTeamNum(), PlayerState->PlayerName);
			}

			PlayerState->PlayerName = TEXT("");
		}
	}

	Super::RemovePlayerState(PlayerState);

	if (Role == ROLE_Authority)
	{
		SetHumanPlayerStateCount();

		// TODO: DeActivate anything related to the old PlayerState (i.e. Pickup Class)
	}

	if (AShooterPlayerState* ShooterPlayerState = Cast<AShooterPlayerState>(PlayerState))
	{
		ShooterPlayerState->OnRemovePlayerState();
	}
}

TArray<AShooterCharacter*> AShooterGameState::GetAlivePawns()
{
	TArray<AShooterCharacter*> Pawns;

	const int32 Count = PlayerArray.Num();

	for (int32 Index = 0; Index < Count; Index++)
	{
		AShooterPlayerState* PlayerState = Cast<AShooterPlayerState>(PlayerArray[Index]);

		if (!PlayerState->LinkedPawn.IsValid() || !PlayerState->LinkedPawn.Get())
			continue;

		if (!PlayerState->LinkedPawn->IsActive || PlayerState->LinkedPawn->Health <= 0 || PlayerState->LinkedPawn->IsDead)
			continue;

		Pawns.Add(PlayerState->LinkedPawn.Get());
	}
	return Pawns;
}

TArray<AShooterCharacter*> AShooterGameState::GetAlivePlayerPawns()
{
	TArray<AShooterCharacter*> Pawns;

	const int32 Count = PlayerArray.Num();

	for (int32 Index = 0; Index < Count; Index++)
	{
		AShooterPlayerState* PlayerState = Cast<AShooterPlayerState>(PlayerArray[Index]);

		if (!PlayerState->LinkedPawn.IsValid() || !PlayerState->LinkedPawn.Get())
			continue;

		if (!PlayerState->LinkedPawn->IsActive || PlayerState->LinkedPawn->Health <= 0 || PlayerState->LinkedPawn->IsDead)
			continue;

		if (Cast<AShooterBot>(PlayerState->LinkedPawn.Get()))
			continue;

		Pawns.Add(PlayerState->LinkedPawn.Get());
	}
	return Pawns;
}

TArray<AShooterCharacter*> AShooterGameState::GetAliveBotPawns()
{
	TArray<AShooterCharacter*> Pawns;

	const int32 Count = PlayerArray.Num();

	for (int32 Index = 0; Index < Count; Index++)
	{
		AShooterPlayerState* PlayerState = Cast<AShooterPlayerState>(PlayerArray[Index]);

		if (!PlayerState->LinkedPawn.IsValid() || !PlayerState->LinkedPawn.Get())
			continue;

		if (!PlayerState->LinkedPawn->IsActive || PlayerState->LinkedPawn->Health <= 0 || PlayerState->LinkedPawn->IsDead)
			continue;

		if (!Cast<AShooterBot>(PlayerState->LinkedPawn.Get()))
			continue;

		Pawns.Add(PlayerState->LinkedPawn.Get());
	}
	return Pawns;
}

// Player State Mapping
#pragma region

void AShooterGameState::OnRep_PlayerStateMapping()
{
	PlayerStateMappingCount = 0;

	for (int32 Index = 0; Index < MAX_PLAYER_STATES; Index++)
	{
		if (PlayerStateMapping[Index] == TEXT(""))
			return;
		PlayerStateMappingCount++;
	}
}

void AShooterGameState::OnTick_UpdatePlayerStateMapping()
{
	SCOPE_CYCLE_COUNTER(STAT_UpdatePlayerStateMapping);

	if (Role < ROLE_Authority)
		return;

	const int32 Count = PlayerArray.Num();

	for (int32 Index = 0; Index < Count; Index++)
	{
		AShooterPlayerState* PlayerState = Cast<AShooterPlayerState>(PlayerArray[Index]);

		if (!PlayerState)
			continue;

		if (Cast<AShooterPlayerController>(PlayerState->GetOwner()))
		{
			FName UniqueId = NAME_None;
			
			if (bIsLanGame)
			{
				if (PlayerState->LanUniqueNetId == NAME_None)
				{
					// Taken from FUserOnlineAccountNull::GenerateRandomUserId
					uint64 randomUid = FMath::Rand();
					randomUid	    |= ((uint64)FMath::Rand()) << 16;
					randomUid		|= ((uint64)FMath::Rand()) << 32;

					// don't use full 64 bit range so that there is even less chance of conflicting with steam since steam uses full 64bit range
					randomUid		|= ((uint64)FMath::Rand()) << 47;

					PlayerState->LanUniqueNetId = FName(*FString::Printf(TEXT("%llu"), randomUid));
				}
				UniqueId = PlayerState->LanUniqueNetId;
			}
			else
			{
				UniqueId = PlayerState->UniqueId.IsValid() ? FName(*PlayerState->UniqueId->ToString()) : INVALID_PLAYER_STATE_UNIQUE_ID;
			}

			if (UniqueId != INVALID_PLAYER_STATE_UNIQUE_ID &&
				UniqueId != NAME_None &&
				UniqueId != FNAME_EMPTY)
			{
				// Check if the Id already exists
				if (GetIndexFromPlayerStateMapping(UniqueId) == INDEX_NONE)
				{
					PlayerState->UniqueMappingId				= PlayerStateMappingCount;
					PlayerStateMapping[PlayerStateMappingCount] = UniqueId;

					UShooterStatics::PlayerStateMappingItem_MinHeap_Add(PlayerStateMapping_MinHeap, PlayerStateMappingCount, UniqueId, PlayerState);

					PlayerStateMappingCount++;
				}
			}
		}
	}
}

int32 AShooterGameState::GetIndexFromPlayerStateMapping(FName UniqueId, bool IsBot)
{
	if (UniqueId == NAME_None || UniqueId == FNAME_EMPTY || UniqueId == INVALID_PLAYER_STATE_UNIQUE_ID)
		return INDEX_NONE;

	// Check Heap for Index
	TArray<FPlayerStateMappingItem>* MinHeap = IsBot ? &BotPlayerStateMapping_MinHeap : &PlayerStateMapping_MinHeap;
	FPlayerStateMappingItem* Item			 = UShooterStatics::PlayerStateMappingItem_MinHeap_Find(*MinHeap, UniqueId);

	if (Item)
		return Item->MappingId;

	// Go through PlayerStateMapping array if the the UniqueId does NOT exist in the Heap
	FName(*Mapping)[MAX_PLAYER_STATES] = IsBot ? &BotPlayerStateMapping : &PlayerStateMapping;
	const int32 Count				   = IsBot ? BotPlayerStateMappingCount : PlayerStateMappingCount;

	for (int32 Index = 0; Index < Count; Index++)
	{
		if ((*Mapping)[Index] == UniqueId)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

AShooterPlayerState* AShooterGameState::GetPlayerState(uint8 PlayerStateMappingId, bool IsBot)
{
	if (PlayerStateMappingId == INVALID_PLAYER_STATE)
		return NULL;

	// Check Heap for the PlayerState
	TArray<FPlayerStateMappingItem>* MinHeap = IsBot ? &BotPlayerStateMapping_MinHeap : &PlayerStateMapping_MinHeap;

	FPlayerStateMappingItem* Item = UShooterStatics::PlayerStateMappingItem_MinHeap_Find(*MinHeap, PlayerStateMappingId);

	if (Item && Item->PlayerState.IsValid() && Item->PlayerState.Get())
	{
		if (Cast<AShooterPlayerState>(Item->PlayerState.Get()))
			return Cast<AShooterPlayerState>(Item->PlayerState.Get());

		Item->PlayerState.Reset();
		Item->PlayerState = NULL;
	}

	// Go through PlayerState array if the the PlayerState does NOT exist in the Heap
	FName(*Mapping)[MAX_PLAYER_STATES] = IsBot ? &BotPlayerStateMapping : &PlayerStateMapping;

	AShooterPlayerState* PlayerState = NULL;

	const int32 Count = PlayerArray.Num();

	for (int32 Index = 0; Index < Count; Index++)
	{
		PlayerState = Cast<AShooterPlayerState>(PlayerArray[Index]);

		// N.B. there are scenarios where PlayerArray[Index] is not yet AShooterPlayerState while loading
		if (!PlayerState || !PlayerState->GetLinkedPawn())
			continue;

		const FName UniqueId = PlayerState->GetLanUniqueNetId(IsBot, bIsLanGame);

		if (UniqueId != INVALID_PLAYER_STATE_UNIQUE_ID &&
			UniqueId != NAME_None &&
			UniqueId != FNAME_EMPTY &&
			UniqueId == (*Mapping)[PlayerStateMappingId])
		{
			if (Item)
			{
				Item->PlayerState = PlayerState;
			}
			else
			{
				UShooterStatics::PlayerStateMappingItem_MinHeap_Add(*MinHeap, PlayerStateMappingId, UniqueId, PlayerState);
			}
			return PlayerState;
		}
	}
	return NULL;
}

uint8 AShooterGameState::GetPlayerStateMappingId(AShooterPlayerState* InPlayerState, bool IsBot)
{
	if (!InPlayerState)
		return INVALID_PLAYER_STATE;
	
	FName(*Mapping)[MAX_PLAYER_STATES]   = IsBot ? &BotPlayerStateMapping : &PlayerStateMapping;
	const int32 Count					 = IsBot ? BotPlayerStateMappingCount : PlayerStateMappingCount;
	const FName UniqueId				 = InPlayerState->GetLanUniqueNetId(IsBot, bIsLanGame);

	if (UniqueId == NAME_None || UniqueId == FNAME_EMPTY || UniqueId == INVALID_PLAYER_STATE_UNIQUE_ID)
		return INVALID_PLAYER_STATE;

	if (Count == 0)
		return INVALID_PLAYER_STATE;
	// Check Min Heap
	TArray<FPlayerStateMappingItem>* MinHeap = IsBot ? &BotPlayerStateMapping_MinHeap : &PlayerStateMapping_MinHeap;
	FPlayerStateMappingItem* Item			 = UShooterStatics::PlayerStateMappingItem_MinHeap_Find(*MinHeap, InPlayerState);

	if (Item)
		return Item->MappingId;
	// Check List - TODO: Remove
	for (int32 Index = 0; Index < Count; Index++)
	{
		if ((*Mapping)[Index] == UniqueId)
		{
			return Index;
		}
	}
	return INVALID_PLAYER_STATE;
}

bool AShooterGameState::UpdatePlayerStateCharacterInfoMapping(TEnumAsByte<EFaction::Type> InFaction, AShooterPlayerState* InPlayerState, bool IsBot)
{
	const FName UniqueId   = InPlayerState->GetLanUniqueNetId(IsBot, bIsLanGame);
	int32 Index			   = UniqueId == NAME_None || UniqueId == FNAME_EMPTY || UniqueId == INVALID_PLAYER_STATE_UNIQUE_ID ? INDEX_NONE : GetIndexFromPlayerStateMapping(UniqueId, IsBot);

	// Check if the PlayerState is in the Mapping list
	if (Index != INDEX_NONE)
	{
		TArray<FCharacterInfo>* CopyToInfos = NULL;

		if (IsBot)
		{
			TMap<uint8, TArray<FCharacterInfo>>& InfoMapping = InFaction == EFaction::GR ? AxisBotPlayerStateCharacterInfoMapping : AllyBotPlayerStateCharacterInfoMapping;
			CopyToInfos										 = InfoMapping.Find(Index);
		}
		else
		{
			TMap<uint8, TArray<FCharacterInfo>>& InfoMapping = InFaction == EFaction::GR ? AxisPlayerStateCharacterInfoMapping : AllyPlayerStateCharacterInfoMapping;
			CopyToInfos										 = InfoMapping.Find(Index);
		}

		// Check if the Mapping for the PlayerState exists, 
		// if NOT, create / add it
		if (!CopyToInfos)
		{
			TArray<FCharacterInfo> Infos;

			const int32 Count = (int32)ECharacterClass::ECharacterClass_MAX;

			for (int32 I = 0; I < Count; I++)
			{
				FCharacterInfo Info;
				Infos.Add(Info);
			}

			if (IsBot)
			{
				TMap<uint8, TArray<FCharacterInfo>>& InfoMapping = InFaction == EFaction::GR ? AxisBotPlayerStateCharacterInfoMapping : AllyBotPlayerStateCharacterInfoMapping;
				InfoMapping.Add(Index, Infos);
				CopyToInfos = InfoMapping.Find(Index);
			}
			else
			{
				TMap<uint8, TArray<FCharacterInfo>>& InfoMapping = InFaction == EFaction::GR ? AxisPlayerStateCharacterInfoMapping : AllyPlayerStateCharacterInfoMapping;
				InfoMapping.Add(Index, Infos);
				CopyToInfos = InfoMapping.Find(Index);
			}
		}

		// Copy Character Info from PlayerState
		FCharacterInfo(*CopyFromInfos)[ECharacterClass::ECharacterClass_MAX] = InFaction == EFaction::GR ? &InPlayerState->AxisCharacterInfos : &InPlayerState->AllyCharacterInfos;

		const int32 Count = (int32)ECharacterClass::ECharacterClass_MAX;

		for (int32 I = 0; I < Count; I++)
		{
			FCharacterInfo& CopyFromInfo = (*CopyFromInfos)[I];

			if (!CopyFromInfo.IsActive)
				continue;

			FCharacterInfo& Info = (*CopyToInfos)[I];
			Info = CopyFromInfo;
		}
		return true;
	}
	return false;
}

FCharacterInfo* AShooterGameState::GetCharacterInfo(TEnumAsByte<EFaction::Type> InFaction, TEnumAsByte<ECharacterClass::Type> InCharacterClass, uint8 PlayerStateMappingId, bool IsBot)
{
	if (InFaction == EFaction::EFaction_MAX)
		return NULL;

	if (InCharacterClass == ECharacterClass::ECharacterClass_MAX)
		return NULL;

	if (PlayerStateMappingId == INVALID_PLAYER_STATE)
		return NULL;

	TArray<FCharacterInfo>* Infos = NULL;

	if (IsBot)
	{
		TMap<uint8, TArray<FCharacterInfo>>& InfoMapping = InFaction == EFaction::GR ? AxisBotPlayerStateCharacterInfoMapping : AllyBotPlayerStateCharacterInfoMapping;
		Infos											 = InfoMapping.Find(PlayerStateMappingId);
	}
	else
	{
		TMap<uint8, TArray<FCharacterInfo>>& InfoMapping = InFaction == EFaction::GR ? AxisPlayerStateCharacterInfoMapping : AllyPlayerStateCharacterInfoMapping;
		Infos											 = InfoMapping.Find(PlayerStateMappingId);
	}

	if (!Infos)
		return NULL;

	if ((int32)InCharacterClass >= Infos->Num())
		return NULL;

	return &((*Infos)[InCharacterClass]);
}

bool AShooterGameState::IsPlayerStateFullyMapped(AShooterPlayerState* InPlayerState, bool IsBot)
{
	uint8 OutMappingId = INVALID_PLAYER_STATE;
	return IsPlayerStateFullyMapped(InPlayerState, IsBot, OutMappingId);
}

bool AShooterGameState::IsPlayerStateFullyMapped(AShooterPlayerState* InPlayerState, bool IsBot, uint8& OutMappingId)
{
	OutMappingId = GetPlayerStateMappingId(InPlayerState, IsBot);

	if (OutMappingId == INVALID_PLAYER_STATE)
		return false;

	if (!GetPlayerState(OutMappingId, IsBot))
		return false;

	for (int32 I = 0; I < 2; I++)
	{
		TArray<FCharacterInfo>* Infos = NULL;

		if (IsBot)
		{
			TMap<uint8, TArray<FCharacterInfo>>& InfoMapping = I == 0 ? AxisBotPlayerStateCharacterInfoMapping : AllyBotPlayerStateCharacterInfoMapping;
			Infos = InfoMapping.Find(OutMappingId);
		}
		else
		{
			TMap<uint8, TArray<FCharacterInfo>>& InfoMapping = I == 0 ? AxisPlayerStateCharacterInfoMapping : AllyPlayerStateCharacterInfoMapping;
			Infos = InfoMapping.Find(OutMappingId);
		}

		if (!Infos)
			return false;

		const int32 Count = (int32)ECharacterClass::ECharacterClass_MAX;

		if (Infos->Num() < Count)
			return false;

		for (int32 J = 0; J < Count; J++)
		{
			if (!(*Infos)[J].IsValid())
				return false;
		}
	}
	return true;
}

void AShooterGameState::OnTick_UpdateReplicatedPlayerStateMappingIds()
{
	APlayerController* MachineClientController = UShooterStatics::GetMachineClientController(GetWorld());
	AShooterPlayerState* ClientPlayerState	   = MachineClientController ? Cast<AShooterPlayerState>(MachineClientController->PlayerState) : NULL;

	if (!ClientPlayerState)
		return;

	const int32 Count = PlayerArray.Num();

	if (Count == 0)
		return;

	int32 Index						 = Last_UpdateReplicatedPlayerStateMappingIds_Index % Count;
	AShooterPlayerState* PlayerState = Cast<AShooterPlayerState>(PlayerArray[Index]);

	if (!PlayerState)
	{
		Last_UpdateReplicatedPlayerStateMappingIds_Index++;
		return;
	}

	const float UpdateFrequency = 0.1f;

	if (GetWorld()->TimeSeconds - Last_UpdateReplicatedPlayerStateMappingIds_Time < UpdateFrequency)
	{
		return;
	}

	Last_UpdateReplicatedPlayerStateMappingIds_Time = GetWorld()->TimeSeconds;

	PlayerState->OnTick_UpdateReplicatedPlayerStateMappingIds();
	PlayerState->OnTick_UpdateReplicatedBotPlayerStateMappingIds();

	Last_UpdateReplicatedPlayerStateMappingIds_Index++;
}

#pragma endregion Player State Mapping

// Bot Player State Mapping
#pragma region

void AShooterGameState::OnRep_BotPlayerStateMapping()
{
	BotPlayerStateMappingCount = 0;

	for (int32 Index = 0; Index < MAX_PLAYER_STATES; Index++)
	{
		if (BotPlayerStateMapping[Index] == TEXT(""))
			return;
		BotPlayerStateMappingCount++;
	}
}

void AShooterGameState::OnTick_UpdateBotPlayerStateMapping()
{
	SCOPE_CYCLE_COUNTER(STAT_UpdateBotPlayerStateMapping);

	if (Role < ROLE_Authority)
		return;

	const int32 Count = PlayerArray.Num();

	for (int32 Index = 0; Index < Count; Index++)
	{
		AShooterPlayerState* PlayerState = Cast<AShooterPlayerState>(PlayerArray[Index]);

		// N.B. there are scenarios where PlayerArray[Index] is not yet AShooterPlayerState while loading
		if (PlayerState && Cast<AShooterAIController>(PlayerState->GetOwner()))
		{
			FName UniqueId = FName(*PlayerState->GetShortPlayerName());

			if (UniqueId != INVALID_PLAYER_STATE_UNIQUE_ID &&
				UniqueId != NAME_None &&
				UniqueId != FNAME_EMPTY)
			{
				// Check if the Id already exists
				if (GetIndexFromPlayerStateMapping(UniqueId, true) == INDEX_NONE)
				{
					PlayerState->UniqueMappingId					  = BotPlayerStateMappingCount;
					BotPlayerStateMapping[BotPlayerStateMappingCount] = UniqueId;

					UShooterStatics::PlayerStateMappingItem_MinHeap_Add(BotPlayerStateMapping_MinHeap, BotPlayerStateMappingCount, UniqueId, PlayerState);

					BotPlayerStateMappingCount++;
				}
			}
		}
	}
}

#pragma endregion Bot Player State Mapping

void AShooterGameState::Setup_Queue_ClientForceWarmUpLinkedPawn(AShooterPlayerState* InClient, AShooterPlayerState* InPlayerState)
{
	if (Queue_ClientForceWarmUpLinkedPawn_Clients.Find(InClient) == INDEX_NONE)
	{
		Queue_ClientForceWarmUpLinkedPawn_Clients.Add(InClient);
		Queue_ClientForceWarmUpLinkedPawn_PlayerStates.Add(InPlayerState);
	}
	else
	{
		const int32 Count     = Queue_ClientForceWarmUpLinkedPawn_Clients.Num();
		bool PlayerStateFound = false;

		for (int32 Index = 0; Index < Count; Index++)
		{
			if (Queue_ClientForceWarmUpLinkedPawn_Clients[Index] == InClient &&
				Queue_ClientForceWarmUpLinkedPawn_PlayerStates[Index] == InPlayerState)
			{
				PlayerStateFound = true;
				break;
			}
		}

		if (!PlayerStateFound)
		{
			Queue_ClientForceWarmUpLinkedPawn_Clients.Add(InClient);
			Queue_ClientForceWarmUpLinkedPawn_PlayerStates.Add(InPlayerState);
		}
	}
}

void AShooterGameState::OnTick_HandleClientForceWarmUpLinkedPawn()
{
	SCOPE_CYCLE_COUNTER(STAT_HandleClientForceWarmUpLinkedPawn);

	const int32 Count = Queue_ClientForceWarmUpLinkedPawn_Clients.Num();

	if (Count == 0)
		return;

	for (int32 Index = Count - 1; Index >= 0; Index--)
	{
		if (!Queue_ClientForceWarmUpLinkedPawn_Clients[Index].IsValid() ||
			!Queue_ClientForceWarmUpLinkedPawn_Clients[Index].Get() ||
			!Queue_ClientForceWarmUpLinkedPawn_PlayerStates[Index].IsValid() ||
			!Queue_ClientForceWarmUpLinkedPawn_PlayerStates[Index].Get())
		{
			Queue_ClientForceWarmUpLinkedPawn_Clients.RemoveAt(Index);
			Queue_ClientForceWarmUpLinkedPawn_PlayerStates.RemoveAt(Index);
		}
		else
		{
			AShooterPlayerState* Client		 = Queue_ClientForceWarmUpLinkedPawn_Clients[Index].Get();
			AShooterPlayerState* PlayerState = Queue_ClientForceWarmUpLinkedPawn_PlayerStates[Index].Get();
			AShooterCharacter* Pawn			 = PlayerState->GetLinkedPawn();

			if (!Pawn)
				continue;

			const bool IsBot		= Cast<AShooterBot>(Pawn) != NULL;
			bool PerformForceWarmUp = IsBot ? Client->GetReplicatedBotPlayerStateMappingId(PlayerState) != INVALID_PLAYER_STATE : Client->GetReplicatedPlayerStateMappingId(PlayerState) != INVALID_PLAYER_STATE;

			if (PerformForceWarmUp)
			{
				Client->ClientForceWarmUpLinkedPawn(PlayerState);
				Queue_ClientForceWarmUpLinkedPawn_Clients.RemoveAt(Index);
				Queue_ClientForceWarmUpLinkedPawn_PlayerStates.RemoveAt(Index);
			}
		}
	}
}

void AShooterGameState::Setup_Queue_ClientInstantWarmUpLinkedPawn(AShooterPlayerState* InClient, AShooterPlayerState* InPlayerState)
{
	if (Queue_ClientInstantWarmUpLinkedPawn_Clients.Find(InClient) == INDEX_NONE)
	{
		Queue_ClientInstantWarmUpLinkedPawn_Clients.Add(InClient);
		Queue_ClientInstantWarmUpLinkedPawn_PlayerStates.Add(InPlayerState);
	}
	else
	{
		const int32 Count	  = Queue_ClientInstantWarmUpLinkedPawn_Clients.Num();
		bool PlayerStateFound = false;

		for (int32 Index = 0; Index < Count; Index++)
		{
			if (Queue_ClientInstantWarmUpLinkedPawn_Clients[Index] == InClient &&
				Queue_ClientInstantWarmUpLinkedPawn_PlayerStates[Index] == InPlayerState)
			{
				PlayerStateFound = true;
				break;
			}
		}

		if (!PlayerStateFound)
		{
			Queue_ClientInstantWarmUpLinkedPawn_Clients.Add(InClient);
			Queue_ClientInstantWarmUpLinkedPawn_PlayerStates.Add(InPlayerState);
		}
	}
}

void AShooterGameState::OnTick_HandleClientInstantWarmUpLinkedPawn()
{
	SCOPE_CYCLE_COUNTER(STAT_HandleClientInstantWarmUpLinkedPawn);

	const int32 Count = Queue_ClientInstantWarmUpLinkedPawn_Clients.Num();

	if (Count == 0)
		return;

	for (int32 Index = Count - 1; Index >= 0; Index--)
	{
		if (!Queue_ClientInstantWarmUpLinkedPawn_Clients[Index].IsValid() ||
			!Queue_ClientInstantWarmUpLinkedPawn_Clients[Index].Get() ||
			!Queue_ClientInstantWarmUpLinkedPawn_PlayerStates[Index].IsValid() ||
			!Queue_ClientInstantWarmUpLinkedPawn_PlayerStates[Index].Get())
		{
			Queue_ClientInstantWarmUpLinkedPawn_Clients.RemoveAt(Index);
			Queue_ClientInstantWarmUpLinkedPawn_PlayerStates.RemoveAt(Index);
		}
		else
		{
			AShooterPlayerState* Client		 = Queue_ClientInstantWarmUpLinkedPawn_Clients[Index].Get();
			AShooterPlayerState* PlayerState = Queue_ClientInstantWarmUpLinkedPawn_PlayerStates[Index].Get();
			AShooterCharacter* Pawn			 = PlayerState->GetLinkedPawn();

			if (!Pawn)
				continue;

			const bool IsBot		  = Cast<AShooterBot>(Pawn) != NULL;
			bool PerformInstantWarmUp = IsBot ? Client->GetReplicatedBotPlayerStateMappingId(PlayerState) != INVALID_PLAYER_STATE : Client->GetReplicatedPlayerStateMappingId(PlayerState) != INVALID_PLAYER_STATE;

			if (PerformInstantWarmUp)
			{
				Client->ClientInstantWarmUpLinkedPawn(PlayerState);
				Queue_ClientInstantWarmUpLinkedPawn_Clients.RemoveAt(Index);
				Queue_ClientInstantWarmUpLinkedPawn_PlayerStates.RemoveAt(Index);
			}
		}
	}
}

AShooterBot* AShooterGameState::AllocateBotCharacter(AShooterAIController* InAIController)
{
	check(Role == ROLE_Authority);

	if (!BotCharacterPool.Num())
	{
		CreateBotPool();
	}

	const int32 AllocatedIndex = GetAllocatedBotCharacterIndex(InAIController);
	AShooterBot* Character	   = AllocatedIndex > INDEX_NONE ? BotCharacterPool[AllocatedIndex] : NULL;

	check(Character);

	return Character;
}

AShooterBot* AShooterGameState::AllocateBotCharacter(AShooterAIController* InAIController, FVector Location, FRotator Rotation)
{
	AShooterBot* Character = AllocateBotCharacter(InAIController);

	if (Character)
	{
		if (Character->IsPlacedInLevel)
		{
			if (Character->IsFirstSpawn)
			{
				Location = Character->SpawnLocation;
				Rotation = Character->SpawnRotation;
			}
			else
			{
				Location = Character->playerRespawn ? Character->playerRespawn->GetActorLocation() : Character->SpawnLocation;
				Rotation = Character->playerRespawn ? Character->playerRespawn->GetActorRotation() : Character->SpawnRotation;
			}
		}

		Character->TeleportTo(Location, Rotation, false, true);

		Character->SpawnLocation = Location;
		Character->SpawnRotation = Rotation;
	}
	return Character;
}

int32 AShooterGameState::GetAllocatedBotCharacterIndex(AShooterAIController* InAIController)
{
	const int32 Count	 = BotCharacterPool.Num();
	int32 AllocatedIndex = INDEX_NONE;

	AShooterPlayerState* PlayerState = Cast<AShooterPlayerState>(InAIController->PlayerState);

	if (PlayerState->LinkedPawn.IsValid() &&
		PlayerState->LinkedPawn.Get() &&
		PlayerState->LinkedPawnIndex != INVALID_LINKED_PAWN_INDEX)
	{
		if (InAIController->LinkedPawnIndex == INVALID_LINKED_PAWN_INDEX)
		{
			InAIController->LinkedPawn		= PlayerState->LinkedPawn;
			InAIController->LinkedPawnIndex = PlayerState->LinkedPawnIndex;

			FString Proxy = Role == ROLE_Authority ? TEXT("Server") : TEXT("Client");
			UE_LOG(LogShooter, Warning, TEXT("GetAllocatedBotCharacterIndex (%s): AI Controller did NOT have valid LinkedPawn but it's PlayerState: %s did"), *Proxy, *PlayerState->GetShortPlayerName());
		}
		AllocatedIndex = PlayerState->LinkedPawnIndex;
	}
	check(AllocatedIndex != INVALID_LINKED_PAWN_INDEX);
	return AllocatedIndex;
}

#pragma endregion Characters

// Flipbook
#pragma region

AShooterEffectsFlipBook* AShooterGameState::AllocateEffectsFlipBook()
{
	AShooterEffectsFlipBook* AvailableFlipBook;

	int effectsQuality = Scalability::GetQualityLevels().EffectsQuality;
	float effectsScaler = (effectsQuality + 1) / 4.0f;
	const int32 MAX_FLIPBOOK_COUNT = 128;

	const int32 MaxUseFlipBooks = FMath::Max<int32>(MAX_FLIPBOOK_COUNT, EffectsFlipBookArray.Num() * effectsScaler);

	for (int32 i = 0; i < MaxUseFlipBooks; ++i)
	{
		check(EffectsFlipBookArray[i]);

		if (EffectsFlipBookArray[i]->IsAvailable)
		{
			AvailableFlipBook = EffectsFlipBookArray[i];
#if !UE_BUILD_SHIPPING
			//UE_LOG(LogFlipbookPool, Warning, TEXT("Allocating flipbook id : %d"), i);
#endif // #if !UE_BUILD_SHIPPING
			return AvailableFlipBook;
		}
	}

#if !UE_BUILD_SHIPPING
	UE_LOG(LogFlipbookPool, Warning, TEXT("There is no available flippbook in the pool."));
#endif // #if !UE_BUILD_SHIPPING

	// find "LiveEnough" emitter and use it
	uint32 OldestFlipBookID = 0;
	for (int32 i = 0; i < MaxUseFlipBooks; ++i)
	{
		check(EffectsFlipBookArray[i]);

		// Get the oldest flipbook ID
		float CurrentFlipBookSpendLife = GetWorld()->TimeSeconds - EffectsFlipBookArray[i]->SpawnTime;
		float OldestFlipBookSpendLife = GetWorld()->TimeSeconds - EffectsFlipBookArray[OldestFlipBookID]->SpawnTime;
		if (CurrentFlipBookSpendLife > OldestFlipBookSpendLife)
		{
			OldestFlipBookID = i;
		}

		// Make sure to use multiplier instead of divider to check the emitter's 
		// spend lifetime is larger than 9/10 of its lifetime
		bool bIsLivedEnough(CurrentFlipBookSpendLife > (EffectsFlipBookArray[i]->LifeTime * 0.9f));
		if (bIsLivedEnough)
		{
#if !UE_BUILD_SHIPPING
			UE_LOG(LogFlipbookPool, Warning, TEXT("bIsLivedEnough flipbook is used : %.2f"), CurrentFlipBookSpendLife / (float)EffectsFlipBookArray[i]->LifeTime);
#endif // #if !UE_BUILD_SHIPPING

			EffectsFlipBookArray[i]->DeallocateFromPool();
			AvailableFlipBook = EffectsFlipBookArray[i];
			return AvailableFlipBook;
		}
	}

	// If most of the emitter's are not lived enough, get the oldest emitter
	// UE4 [] operator will assert when there is out-number for array
	EffectsFlipBookArray[OldestFlipBookID]->DeallocateFromPool();
	AvailableFlipBook = EffectsFlipBookArray[OldestFlipBookID];

#if !UE_BUILD_SHIPPING
	UE_LOG(LogFlipbookPool, Warning, TEXT("OldestFlipBookID is used : %d"), OldestFlipBookID);
#endif // #if !UE_BUILD_SHIPPING

	return AvailableFlipBook;
}

AShooterEffectsFlipBook* AShooterGameState::AllocateAndAttachFlipBook(FEffectsFlipBook* EffectsFlipbook, AActor* InOwner)
{
	AShooterEffectsFlipBook* Flipbook = AllocateEffectsFlipBook();

	/* Take care of Attaching or Hiding based on drawdist */
	if (Flipbook)
	{
		Flipbook->AllocateFromPool(EffectsFlipbook, InOwner);

		// Attaching to Pawn
		AShooterCharacter* OwningPawn = Cast<AShooterCharacter>(InOwner);
		if (OwningPawn)
		{
			Flipbook->DrawDistance = UShooterStatics::IsControlledByClient(OwningPawn) ? EffectsFlipbook->DrawDistances.Distance1P : EffectsFlipbook->DrawDistances.Distance3P;
		}

		// Attaching to Projectile
		AShooterProjectile* OwningProjectile = Cast<AShooterProjectile>(InOwner);
		if (OwningProjectile)
		{
			if (OwningProjectile->Instigator)
			{
				OwningPawn = Cast<AShooterCharacter>(OwningProjectile->Instigator);

				if (OwningPawn)
					Flipbook->DrawDistance = UShooterStatics::IsControlledByClient(OwningPawn) ? EffectsFlipbook->DrawDistances.Distance1P : EffectsFlipbook->DrawDistances.Distance3P;
			}
		}

		// Turn on its ActorTick to be true only when the emitter is actually allocated
		Flipbook->PrimaryActorTick.bCanEverTick = true;

		if (Flipbook->DrawDistance > 0.0f)
		{
			float DistanceSq = UShooterStatics::GetSquaredDistanceToLocalControllerEye(GetWorld(), Flipbook->GetActorLocation());

			if (DistanceSq > Flipbook->DrawDistance * Flipbook->DrawDistance)
				Flipbook->SetActorHiddenInGame(true);
		}
	}
	return Flipbook;
}
#pragma endregion Flipbook

// Emitter
#pragma region

AShooterEmitter* AShooterGameState::AllocateEmitter()
{
	AShooterEmitter* AvailableEmitter;

	int effectsQuality = Scalability::GetQualityLevels().EffectsQuality;
	float effectsScaler = (effectsQuality + 1) / 4.0f;

	const int32 MaxUseEmitters = FMath::Max<int32>(128, EmitterArray.Num() * effectsScaler);

	for (int32 i = 0; i < MaxUseEmitters; ++i)
	{
		check(EmitterArray[i]);

		if (EmitterArray[i]->IsAvailable)
		{
			AvailableEmitter = EmitterArray[i];
#if !UE_BUILD_SHIPPING
			UE_LOG(LogEmitterPool, Warning, TEXT("Allocating emitter id : %d"), i);
#endif // #if !UE_BUILD_SHIPPING
			return AvailableEmitter;
		}
	}

#if !UE_BUILD_SHIPPING
	UE_LOG(LogEmitterPool, Warning, TEXT("There is no available emitter in the pool."));
#endif // #if !UE_BUILD_SHIPPING

	 // find "LiveEnough" emitter and use it
	uint32 OldestEmitterID = 0;
	for (int32 i = 0; i < MaxUseEmitters; ++i)
	{
		check(EmitterArray[i]);

		// Get the oldest emitter ID
		float CurrentEmitterSpendLife = GetWorld()->TimeSeconds - EmitterArray[i]->SpawnTime;
		float OldestEmitterSpendLife = GetWorld()->TimeSeconds - EmitterArray[OldestEmitterID]->SpawnTime;
		if (CurrentEmitterSpendLife > OldestEmitterSpendLife)
		{
			OldestEmitterID = i;
		}

		// Make sure to use multiplier instead of divider to check the emitter's 
		// spend lifetime is larger than 3/4 of its lifetime
		bool bIsLivedEnough(CurrentEmitterSpendLife > (EmitterArray[i]->LifeTime * 0.9f));
		if (bIsLivedEnough)
		{
#if !UE_BUILD_SHIPPING
			UE_LOG(LogEmitterPool, Warning, TEXT("bIsLivedEnough Emitter is used : %.2f"), CurrentEmitterSpendLife / (float)EmitterArray[i]->LifeTime);
#endif // #if !UE_BUILD_SHIPPING

			EmitterArray[i]->DeallocateFromPool();
			AvailableEmitter = EmitterArray[i];
			return AvailableEmitter;
		}
	}

	// If most of the emitter's are not lived enough, get the oldest emitter
	// UE4 [] operator will assert when there is out-number for array
	EmitterArray[OldestEmitterID]->DeallocateFromPool();
	AvailableEmitter = EmitterArray[OldestEmitterID];

#if !UE_BUILD_SHIPPING
	UE_LOG(LogEmitterPool, Warning, TEXT("OldestEmitterID is used : %d"), OldestEmitterID);
#endif // #if !UE_BUILD_SHIPPING

	return AvailableEmitter;
}

AShooterEmitter* AShooterGameState::AllocateEmitter(FEffectsElement* EffectsElement, FVector Location)
{
	AShooterEmitter* Emitter = AllocateEmitter();

	if (Emitter)
	{
		Emitter->SetActorHiddenInGame(false);
		Emitter->SetTemplate(EffectsElement->ParticleSystem);
		Emitter->IsAvailable = false;
		Emitter->IsLooping   = false;

		Emitter->SpawnTime	  = GetWorld()->TimeSeconds;
		Emitter->LifeTime	  = EffectsElement->LifeTime;
		Emitter->DeathTime	  = EffectsElement->DeathTime;
		Emitter->IsAttachedFX = false;
		Emitter->DrawDistance = EffectsElement->DrawDistances.Distance3P;

		Emitter->SetActorScale3D(EffectsElement->Scale * FVector(1.0f));
		Emitter->TeleportTo(Location, FRotator::ZeroRotator, false, true);

		if (Emitter->DrawDistance > 0.0f)
		{
			float DistanceSq = UShooterStatics::GetSquaredDistanceToLocalControllerEye(GetWorld(), Emitter->GetActorLocation());

			Emitter->SetActorHiddenInGame(DistanceSq > Emitter->DrawDistance * Emitter->DrawDistance);

		}
		if (Emitter->GetParticleSystemComponent())
		{
			Emitter->GetParticleSystemComponent()->SetVisibility(true);
			Emitter->GetParticleSystemComponent()->SetHiddenInGame(false);
			Emitter->GetParticleSystemComponent()->Activate();
			Emitter->bCurrentlyActive = true;
		}

		// Turn on its ActorTick to be true only when the emitter is actually allocated
		Emitter->PrimaryActorTick.bCanEverTick = true;
	}
	return Emitter;
}

AShooterEmitter* AShooterGameState::AllocateAndActivateEmitter(UParticleSystem* Template, float Lifetime, bool IsAttachedFX, AActor* Parent, FName BoneName, float Scale)
{
	AShooterEmitter* Emitter = AllocateEmitter();

	check(Emitter);

	Emitter->AllocateFromPool(Template, Lifetime, IsAttachedFX, Parent, BoneName, Scale);

	return Emitter;
}

AShooterEmitter* AShooterGameState::AllocateAndAttachEmitter(const FEffectsElement* EffectsElement, USceneComponent* InParent, AActor* InOwner, bool IsLooping)
{
	return AllocateAndAttachEmitter(EffectsElement, InParent, InOwner, EffectsElement->LifeTime, IsLooping);
}

AShooterEmitter* AShooterGameState::AllocateAndAttachEmitter(const FEffectsElement* EffectsElement, USceneComponent* InParent, AActor* InOwner, float Time, bool IsLooping)
{
	if (!InParent)
		return NULL;

	if (!InOwner)
		return NULL;

	AShooterEmitter* Emitter = AllocateEmitter();

	if (Emitter)
	{
		Emitter->SetActorHiddenInGame(false);
		Emitter->SetTemplate(EffectsElement->ParticleSystem);
		Emitter->IsAvailable = false;
		Emitter->IsLooping   = IsLooping;

		Emitter->SpawnTime	  = GetWorld()->TimeSeconds;
		Emitter->LifeTime	  = Time;
		Emitter->DeathTime	  = EffectsElement->DeathTime;
		Emitter->IsAttachedFX = true;
		Emitter->DrawDistance = EffectsElement->DrawDistances.Distance3P;

		// Attaching to Pawn
		AShooterCharacter* OwningPawn = Cast<AShooterCharacter>(InOwner);

		if (OwningPawn)
		{
			Emitter->DrawDistance = UShooterStatics::IsControlledByClient(OwningPawn) ? EffectsElement->DrawDistances.Distance1P : EffectsElement->DrawDistances.Distance3P;
		}

		// Attaching to Projectile
		AShooterProjectile* OwningProjectile = Cast<AShooterProjectile>(InOwner);

		if (OwningProjectile)
		{
			if (OwningProjectile->Instigator)
			{
				OwningPawn = Cast<AShooterCharacter>(OwningProjectile->Instigator);

				if (OwningPawn)
					Emitter->DrawDistance = UShooterStatics::IsControlledByClient(OwningPawn) ? EffectsElement->DrawDistances.Distance1P : EffectsElement->DrawDistances.Distance3P;
			}
		}

		Emitter->SetActorRelativeLocation(EffectsElement->LocationOffset);
		Emitter->SetActorRelativeRotation(EffectsElement->RotationOffset);
		Emitter->AttachToComponent(InParent, FAttachmentTransformRules::KeepRelativeTransform, EffectsElement->Bone);

		Emitter->HasOwner = true;
		Emitter->SetOwner(InOwner);

		if (Emitter->GetParticleSystemComponent())
		{
			Emitter->GetParticleSystemComponent()->SetVisibility(true);
			Emitter->GetParticleSystemComponent()->SetHiddenInGame(false);
			Emitter->GetParticleSystemComponent()->Activate();
			Emitter->bCurrentlyActive = true;
		}

		Emitter->SetActorScale3D(FVector(1.0f));

		// Turn on its ActorTick to be true only when the emitter is actually allocated
		Emitter->PrimaryActorTick.bCanEverTick = true;

		if (Emitter->DrawDistance > 0.0f)
		{
			float DistanceSq = UShooterStatics::GetSquaredDistanceToLocalControllerEye(GetWorld(), Emitter->GetActorLocation());

			if (DistanceSq > Emitter->DrawDistance * Emitter->DrawDistance)
				Emitter->SetActorHiddenInGame(true);
		}
	}
	return Emitter;
}

AShooterEmitter* AShooterGameState::AllocateAndAttachEmitter(const FEffectsElement* EffectsElement, AActor* InParent, bool IsLooping)
{
	return AllocateAndAttachEmitter(EffectsElement, InParent, EffectsElement->LifeTime, IsLooping);
}

AShooterEmitter* AShooterGameState::AllocateAndAttachEmitter(FEffectsElement* EffectsElement, AActor* InParent, bool IsLooping)
{
	return AllocateAndAttachEmitter(EffectsElement, InParent->GetRootComponent(), 0.0f, IsLooping);
}

AShooterEmitter* AShooterGameState::AllocateAndAttachEmitter(const FEffectsElement* EffectsElement, UObject* InParent, float Time, bool IsLooping)
{
	if (!InParent)
		return NULL;

	if (!Cast<AActor>(InParent) && !Cast<USceneComponent>(InParent))
		return NULL;

	AShooterEmitter* Emitter = AllocateEmitter();

	if (Emitter)
	{
		Emitter->SetActorHiddenInGame(false);
		Emitter->SetTemplate(EffectsElement->ParticleSystem);
		Emitter->IsAvailable = false;
		Emitter->IsLooping = IsLooping;

		Emitter->SpawnTime = GetWorld()->TimeSeconds;
		Emitter->LifeTime = Time > 0.0f ? Time : EffectsElement->LifeTime;
		Emitter->DeathTime = EffectsElement->DeathTime;
		Emitter->IsAttachedFX = true;
		Emitter->DrawDistance = EffectsElement->DrawDistances.Distance3P;

		if (Emitter->GetParticleSystemComponent())
		{
			Emitter->GetParticleSystemComponent()->CustomTimeDilation = 1.f;
		}

		// Attaching to Pawn
		AShooterCharacter* OwningPawn = Cast<AShooterCharacter>(InParent);

		if (OwningPawn)
		{
			Emitter->DrawDistance = UShooterStatics::IsControlledByClient(OwningPawn) ? EffectsElement->DrawDistances.Distance1P : EffectsElement->DrawDistances.Distance3P;
		}

		// Attaching to Projectile
		AShooterProjectile* OwningProjectile = Cast<AShooterProjectile>(InParent);

		if (OwningProjectile)
		{
			if (OwningProjectile->Instigator)
			{
				OwningPawn = Cast<AShooterCharacter>(OwningProjectile->Instigator);

				if (OwningPawn)
					Emitter->DrawDistance = UShooterStatics::IsControlledByClient(OwningPawn) ? EffectsElement->DrawDistances.Distance1P : EffectsElement->DrawDistances.Distance3P;
			}
		}

		Emitter->SetActorRelativeLocation(EffectsElement->LocationOffset);
		Emitter->SetActorRelativeRotation(EffectsElement->RotationOffset);

		if (Cast<AActor>(InParent))
		{
			Emitter->AttachToActor(Cast<AActor>(InParent), FAttachmentTransformRules::KeepRelativeTransform, EffectsElement->Bone);
			Emitter->HasOwner = true;
			Emitter->SetOwner(Cast<AActor>(InParent));
		}
		
		if (Cast<USceneComponent>(InParent))
		{
			Emitter->AttachToComponent(Cast<USceneComponent>(InParent), FAttachmentTransformRules::KeepRelativeTransform, EffectsElement->Bone);
		}

		if (Emitter->GetParticleSystemComponent())
		{
			Emitter->GetParticleSystemComponent()->SetVisibility(true);
			Emitter->GetParticleSystemComponent()->SetHiddenInGame(false);
			Emitter->GetParticleSystemComponent()->Activate();
			Emitter->bCurrentlyActive = true;
		}

		// Turn on its ActorTick to be true only when the emitter is actually allocated
		Emitter->PrimaryActorTick.bCanEverTick = true;

		if (Emitter->DrawDistance > 0.0f)
		{
			float DistanceSq = UShooterStatics::GetSquaredDistanceToLocalControllerEye(GetWorld(), Emitter->GetActorLocation());

			Emitter->SetActorHiddenInGame(DistanceSq > Emitter->DrawDistance * Emitter->DrawDistance);
		}
	}
	return Emitter;
}

AShooterEmitter* AShooterGameState::AllocateAndAttachEmitter(const FEffectsElement* EffectsElement, AActor* InParent, float Time, bool IsLooping)
{
	if (!InParent)
		return NULL;

	AShooterEmitter* Emitter = AllocateEmitter();

	if (Emitter)
	{
		Emitter->SetActorHiddenInGame(false);
		Emitter->SetTemplate(EffectsElement->ParticleSystem);
		Emitter->IsAvailable = false;
		Emitter->IsLooping	 = IsLooping;

		Emitter->SpawnTime	  = GetWorld()->TimeSeconds;
		Emitter->LifeTime	  = Time;
		Emitter->DeathTime	  = EffectsElement->DeathTime;
		Emitter->IsAttachedFX = true;
		Emitter->DrawDistance = EffectsElement->DrawDistances.Distance3P;

		// Attaching to Pawn
		AShooterCharacter* OwningPawn = Cast<AShooterCharacter>(InParent);

		if (OwningPawn)
		{
			Emitter->DrawDistance = UShooterStatics::IsControlledByClient(OwningPawn) ? EffectsElement->DrawDistances.Distance1P : EffectsElement->DrawDistances.Distance3P;
		}

		// Attaching to Projectile
		AShooterProjectile* OwningProjectile = Cast<AShooterProjectile>(InParent);

		if (OwningProjectile)
		{
			if (OwningProjectile->Instigator)
			{
				OwningPawn = Cast<AShooterCharacter>(OwningProjectile->Instigator);

				if (OwningPawn)
					Emitter->DrawDistance = UShooterStatics::IsControlledByClient(OwningPawn) ? EffectsElement->DrawDistances.Distance1P : EffectsElement->DrawDistances.Distance3P;
			}
		}

		Emitter->SetActorRelativeLocation(EffectsElement->LocationOffset);
		Emitter->SetActorRelativeRotation(EffectsElement->RotationOffset);
		Emitter->AttachToActor(InParent, FAttachmentTransformRules::KeepRelativeTransform, EffectsElement->Bone);

		Emitter->HasOwner = true;
		Emitter->SetOwner(InParent);

		if (Emitter->GetParticleSystemComponent())
		{
			Emitter->GetParticleSystemComponent()->SetVisibility(true);
			Emitter->GetParticleSystemComponent()->SetHiddenInGame(false);
			Emitter->GetParticleSystemComponent()->Activate();
			Emitter->bCurrentlyActive = true;
		}

		Emitter->SetActorScale3D(FVector(1.0f));

		// Turn on its ActorTick to be true only when the emitter is actually allocated
		Emitter->PrimaryActorTick.bCanEverTick = true;

		if (Emitter->DrawDistance > 0.0f)
		{
			float DistanceSq = UShooterStatics::GetSquaredDistanceToLocalControllerEye(GetWorld(), Emitter->GetActorLocation());

			Emitter->SetActorHiddenInGame(DistanceSq > Emitter->DrawDistance * Emitter->DrawDistance);
		}
	}
	return Emitter;
}

AShooterEmitter* AShooterGameState::AllocateAttachAndActivateMuzzleEffect(TEnumAsByte<EViewType::Type> InViewType, FCharacterInfo& InCharacterInfo, AActor* InParent, bool IsTank, float Scale)
{
	AShooterTankData* TankData = GetTankData(InCharacterInfo.TankDataShortCode);
	AShooterWeaponData* Data   = IsTank ? TankData->WeaponData->GetDefaultObject<AShooterWeaponData>() : GetWeaponData(InCharacterInfo.WeaponDataShortCode);
	FEffectsElement* Effect    = Data->MuzzleFXs.Get(InViewType);

	AShooterEmitter* Emitter = AllocateAndActivateEmitter(Effect->ParticleSystem, Effect->LifeTime, true, InParent, Effect->Bone, Scale * Effect->Scale);
	Emitter->DeathTime		 = Effect->DeathTime;
	return Emitter;
}

AShooterEmitter* AShooterGameState::AllocateAttachAndActivateProjectileImpactEffect(AShooterProjectileData* InProjectileData, AActor* InParent, FName BoneName, TEnumAsByte<ESurfaceType::Type> SurfaceType, float Scale)
{
	FEffectsElement* Effect  = InProjectileData->HitImpactEffects.Get(SurfaceType);
	AShooterEmitter* Emitter = AllocateAndActivateEmitter(Effect->ParticleSystem, Effect->LifeTime, false, NULL, BoneName, Scale * Effect->Scale);
	Emitter->DeathTime		 = Effect->DeathTime;
	return Emitter;
}

AShooterEmitter* AShooterGameState::AllocateAttachAndActivateProjectileImpactEffect(FCharacterInfo& InCharacterInfo, AActor* InParent, FName BoneName, bool IsTank, TEnumAsByte<ESurfaceType::Type> SurfaceType, float Scale)
{
	AShooterWeaponData* WeaponData = NULL;

	if (IsTank)
	{
		AShooterTankData* TankData = GetTankData(InCharacterInfo.TankDataShortCode);
		WeaponData				   = TankData->WeaponData->GetDefaultObject<AShooterWeaponData>();
	}
	else
	{
		WeaponData = GetWeaponData(InCharacterInfo.WeaponDataShortCode);
	}
	AShooterProjectileData* ProjectileData = WeaponData->ProjectileDataClass->GetDefaultObject<AShooterProjectileData>();

	return AllocateAttachAndActivateProjectileImpactEffect(ProjectileData, InParent, BoneName, SurfaceType, Scale);
}

void AShooterGameState::OnTick_HandleEmitterPool(float DeltaSeconds)
{
	SCOPE_CYCLE_COUNTER(STAT_HandleEmitterPool);

	const int32 Count = EmitterArray.Num();

	for (int32 Index = 0; Index < Count; Index++)
	{
		if (!EmitterArray[Index]->IsAvailable)
			EmitterArray[Index]->Tick_Internal(DeltaSeconds);
	}
}

#pragma endregion Emitter

// Sound
#pragma region

AShooterSound* AShooterGameState::AllocateSound(USoundCue* Cue, AActor* InOwnerActor, bool bIs1PSound /*=false*/, bool bLooping /*=false*/, bool bDelay /*=false*/, bool bSpatialized /*=false*/, FVector Location /*= FVector::ZeroVector*/)
{
	if (!Cue)
		return NULL;
	
	// Find any available ShooterSound
	for (int Index = 0; Index < SoundPool.Num(); ++Index)
	{
		AShooterSound* Sound = SoundPool[Index];
		check(Sound);

		if (Sound->bIsBeingUsed)
			continue;

		if (Sound->OnSoundDeallocate.IsBound())
			continue;

		// TODO : 
		// owner is needed only cases that we check its for 1P sound for non-pawn attached sound
		// since it will not play any sound due to its sound tick where it look for its owner but sound for projectile OnHit,
		// sound gets deallocated when projectile(owner of the sound) gets deallocated.
		if (InOwnerActor && bIs1PSound) {
			Sound->SetOwner(InOwnerActor);
			Sound->HasOwner = true;
		}

		if (bSpatialized){
			bool bActivated = Sound->ActivateWithLocation(Cue, bIs1PSound, bLooping, bDelay, Location);
			if (!bActivated) {
#if !UE_BUILD_SHIPPING
				//UE_LOG(LogSoundPool, Warning, TEXT("Allocating spatialized failed %s at sound id : %d"), Sound->AudioComponent->Sound, Index);
#endif // #if !UE_BUILD_SHIPPING
				return NULL;
			}
				

#if !UE_BUILD_SHIPPING
			//UE_LOG(LogSoundPool, Warning, TEXT("Allocating spatialized %s at sound id : %d"), Sound->AudioComponent->Sound, Index);
#endif // #if !UE_BUILD_SHIPPING

			return Sound;
		}
		else {
			bool bActivated = Sound->Activate(Cue, bIs1PSound, bLooping, bDelay);
			if (!bActivated) {
#if !UE_BUILD_SHIPPING
				//UE_LOG(LogSoundPool, Warning, TEXT("Allocating failed sound id : %d"), Index);
#endif // #if !UE_BUILD_SHIPPING
				return NULL;
			}
				

#if !UE_BUILD_SHIPPING
			//UE_LOG(LogSoundPool, Warning, TEXT("Allocating sound id : %d"), Index);
#endif // #if !UE_BUILD_SHIPPING

			return Sound;
		}
	}

	// If None is found, try to look for oldest sound and deactivate it and actiavte it with new cue
	AShooterSound* OldestSound = NULL;
	for (int Index = 0; Index < SoundPool.Num(); ++Index)
	{
		AShooterSound* Sound = SoundPool[Index];
		check(Sound);

		if (!OldestSound) // for the first time in the loop
		{
			OldestSound = Sound;
		}
		else if (OldestSound->SpawnTime < Sound->SpawnTime) // try to find the oldest one
		{
			OldestSound = Sound;
		}
	}

	OldestSound->DeActivate();

	if (InOwnerActor && bIs1PSound) {
		OldestSound->SetOwner(InOwnerActor);
		OldestSound->HasOwner = true;
	}


	if (bSpatialized){
		OldestSound->ActivateWithLocation(Cue, bIs1PSound, bLooping, bDelay, Location);
	}
	else {
		OldestSound->Activate(Cue, bIs1PSound, bLooping, bDelay);
	}

	return OldestSound;
}

AShooterSound* AShooterGameState::AllocateSound(USoundCue* Cue, AActor* InOwner, FVector Location)
{
	return AllocateSound(Cue, InOwner, false, false, false, false, Location);
}

AShooterSound* AShooterGameState::AllocateDramaticSound(USoundCue* Cue, AActor* InOwner, bool bIs1PSound /*=false */, bool bLooping /*=false*/, bool bDelay /*=false*/, bool bSpatialized /*=false*/, FVector Location /*= FVector::ZeroVector*/)
{
	if (!Cue)
		return NULL;

	// volume down all the other sounds
	int PlayingSoundCount = PlayingSounds.Num();
	for (int Index = 0; Index < PlayingSoundCount; Index++)
	{
		AShooterSound* Sound = PlayingSounds[Index];
		Sound->AudioComponent->SetVolumeMultiplier(.2f);
	}

	// Allocate the dramatic sound
	AShooterSound* DramaticSound = AllocateSound(Cue, InOwner, bIs1PSound, bLooping, bSpatialized, bDelay, Location);
	DramaticSound->bMustPlay = true;

	// revert the volume after the dramatic sounds as dramatic sound is being finished
	FTimerHandle RevertSoundVolumeTimerHandle;
	GetWorld()->GetTimerManager().SetTimer(RevertSoundVolumeTimerHandle, this, &AShooterGameState::RevertAllSoundMultiplier, DramaticSound->LifeTime, false);

	return DramaticSound;
}

void AShooterGameState::RevertAllSoundMultiplier()
{
	int PlayingSoundCount = PlayingSounds.Num();
	for (int Index = 0; Index < PlayingSoundCount; Index++)
	{
		AShooterSound* Sound = PlayingSounds[Index];
		Sound->AudioComponent->SetVolumeMultiplier(1.f);
	}
}

AShooterSound* AShooterGameState::AllocateAndAttachSound(USoundCue* Cue, AActor* InOwner, bool bIs1PSound /*=false */, bool bLooping /*=false*/, bool bDelay /*=false*/)
{
	if (!Cue)
		return NULL;

	bool bSpatialized = true; // attached sound is never 2d sound so set it to be spatialized.
	AShooterSound* Sound = AllocateSound(Cue, InOwner, bIs1PSound, bLooping, bDelay, bSpatialized, InOwner->GetActorLocation());

	if (Sound)
	{
		Sound->SetActorRelativeLocation(FVector::ZeroVector);
		Sound->AttachToActor(InOwner, FAttachmentTransformRules::KeepRelativeTransform, NAME_None);

		if (InOwner) {
			Sound->SetOwner(InOwner);
			Sound->HasOwner = true;
		}
	}
	return Sound;
}

AShooterSound* AShooterGameState::AllocateVOSound(USoundCue* Cue, AActor* InOwner, bool bInterrupt /*=false*/, bool bIs1PSound /*=false */, bool bLooping /*=false*/, bool bDelay /*=false*/, bool bSpatialized /* =false */, FVector Location /*= FVector::ZeroVector*/)
{
	if (!Cue)
		return NULL;

	AShooterSound* Sound = AllocateSound(Cue, InOwner, bIs1PSound, bLooping, bDelay, bSpatialized, Location);

	if (Sound)
	{
		if (bInterrupt)
		{
			// turn off all the VO sounds playing if it has to interrupt
			for (int i = 0; i < VOSounds.Num(); i++) {
				AShooterSound* VOSound = VOSounds[i];

				// if the sound is current activate-waiting sound, skip it!
				if (Sound == VOSound)
					continue;
				
				VOSound->DeActivate();
			}
		}
		VOSounds.Push(Sound);
	}
	return Sound;
}

AShooterSound* AShooterGameState::AllocateAndAttachVOSound(USoundCue* Cue, AActor* InOwner, bool bInterrupt /*=false*/, bool bIs1PSound /*=true */, bool bLooping /*=false*/, bool bDelay /*=false*/)
{
	if (!Cue)
		return NULL;

	bool bSpatialized = true; // attached sound is never 2d sound so set it to be spatialized.
	AShooterSound* Sound = AllocateVOSound(Cue, InOwner, bInterrupt, bIs1PSound, bLooping, bDelay, bSpatialized);

	if (Sound)
	{
		Sound->SetActorRelativeLocation(FVector::ZeroVector);
		Sound->AttachToActor(InOwner, FAttachmentTransformRules::KeepRelativeTransform, NAME_None);

		if (InOwner) {
			Sound->SetOwner(InOwner);
			Sound->HasOwner = true;
		}
	}
	return Sound;
}

static FString GetSoundDescription(AShooterSound *sound)
{
	check(sound);

	FString OwnerName = sound->GetOwner() ? sound->GetOwner()->GetFullName() : TEXT("nullptr");
	FString CueName(TEXT("None"));

	USoundBase* soundBase = sound->AudioComponent ? sound->AudioComponent->Sound : nullptr;

	if (soundBase)
	{
		CueName = soundBase->GetFullName();
	}

	return FString::Printf(TEXT("Used: %d '%s', Looping: %d, SpawnTime: %.2f, HasOwner: %d, Owner: '%s'"), (int)sound->bIsBeingUsed, *CueName, (int)sound->bIsLooping, sound->SpawnTime, (int)sound->HasOwner, *OwnerName);
}

void AShooterGameState::DumpActiveSounds()
{
	UE_LOG(LogShooter, Log, TEXT("Dumping sound pool at time: %f"), GetWorld()->GetTimeSeconds());

	// dump all allocated sounds
	for (int Index = 0; Index < SoundPool.Num(); ++Index)
	{
		AShooterSound* Sound = SoundPool[Index];

		check(Sound);

		UE_LOG(LogShooter, Log, TEXT("%d: %s"), Index + 1, *GetSoundDescription(Sound));
	}
}

void AShooterGameState::ResetSound(AShooterSound *InShooterSound)
{
	if (GetMatchState() == MatchState::InProgress)
	{
		if (SoundPool.Num() && (SoundPool.Find(InShooterSound) == INDEX_NONE))
		{
			DumpActiveSounds();
			checkf(0, TEXT("Attempted to return a sound not in pool: %s"), *GetSoundDescription(InShooterSound));
		}
	}
	InShooterSound->Reset();
}

void AShooterGameState::DeAllocateSound(AShooterSound *InShooterSound)
{
	if (GetMatchState() == MatchState::InProgress)
	{
		if (SoundPool.Num() && (SoundPool.Find(InShooterSound) == INDEX_NONE))
		{
			DumpActiveSounds();
			checkf(0, TEXT("Attempted to return a sound not in pool: %s"), *GetSoundDescription(InShooterSound));
		}
	}

	InShooterSound->DeActivate();
}

AShooterSound* AShooterGameState::AllocateAttachAndPlayCharacter1PSound(FClassSounds inSoundStruct, EFaction::Type inFaction, AActor* inAttachParent, bool bIs1PSound /* = true */, bool bInterrupt /* = false */)
{
	if (inFaction == EFaction::US)
	{
		if (inSoundStruct.ClassSoundsUS.Sounds1P.Default)
		{
			bool bLooping = false;
			bool bInterruptArg = bInterrupt;
			AShooterSound* sfx = AllocateAndAttachVOSound(inSoundStruct.ClassSoundsUS.Sounds1P.Default, inAttachParent, bInterruptArg, bIs1PSound, bLooping);

			return sfx;
		}
	}
	else if (inFaction == EFaction::GR)
	{
		if (inSoundStruct.ClassSoundsGR.Sounds1P.Default)
		{
			bool bLooping = false;
			bool bInterruptArg = bInterrupt;
			AShooterSound* sfx = AllocateAndAttachVOSound(inSoundStruct.ClassSoundsGR.Sounds1P.Default, inAttachParent, bInterruptArg, bIs1PSound, bLooping);

			return sfx;
		}
	}
	return NULL;
}

bool AShooterGameState::RequestPlaySound(AShooterSound* Sound)
{
	if (!Sound->bIsBeingUsed)
		return false;

	/*
	if (!Sound->bCanPlay)
		return false;
	*/

	int CurrentlyPlayingSoundCount = PlayingSounds.Num();

	// CASE 1 - if there is room to play sound right now - play
	if (CurrentlyPlayingSoundCount < MaxConcurrentSoundCount)
	{
		// play the current sound
		float SoundContinueTime = GetWorld()->TimeSeconds - Sound->SpawnTime; // TODO: delayed sound might not be played when the delayed time was too long!
		SoundContinueTime = SoundContinueTime > 0.0f ? SoundContinueTime : 0.0f;
		Sound->Play(SoundContinueTime);
		Sound->bIsPlayingSound = true;
		PlayingSounds.Push(Sound);
		return true;
	}

	// CASE 2 - if playing sound count is at max but the sound bMustPlay is true - get rid of the oldest one and play the sound
	if (CurrentlyPlayingSoundCount >= MaxConcurrentSoundCount && Sound->bMustPlay)
	{
		// find oldest sound and remove it
		AShooterSound* OldestSound = PlayingSounds[0];

		for (int32 i = 0; i < PlayingSounds.Num(); ++i)
		{
			AShooterSound* CurrentSound = PlayingSounds[i];
			check(CurrentSound);

			// Get the biggest sound life ratio - since all the sound's life duration is different
			float CurrentSoundLifeRatio = (GetWorld()->TimeSeconds - CurrentSound->SpawnTime) / CurrentSound->LifeTime;
			float OldestSoundLifeRatio = (GetWorld()->TimeSeconds - OldestSound->SpawnTime) / CurrentSound->LifeTime;
			if (CurrentSoundLifeRatio > OldestSoundLifeRatio && !CurrentSound->bMustPlay) //  TODO : if all sounds in PlayingSounds are bMustPlay true? what should it do?
			{
				OldestSound = CurrentSound;
			}
		}

		// get rid of the oldest sound
		PlayingSounds.Remove(OldestSound);
		OldestSound->DeActivate();

		// play the current sound
		float SoundContinueTime = GetWorld()->TimeSeconds - Sound->SpawnTime;
		Sound->Play(SoundContinueTime);
		Sound->bIsPlayingSound = true;
		PlayingSounds.Push(Sound);

		return true;
	}

	// CASE 3 - if playing sound count is at max and the sound's bMustPlay is not true- sound have to wait.
	return false;
}

void AShooterGameState::DeAllocateSounds(AActor* InOwner)
{
	int32 Count = SoundPool.Num();
	
	for (int32 Index = 0; Index < Count; Index++)
	{
		AShooterSound* Sound = SoundPool[Index];

		if (Sound->HasOwner &&
			Sound->GetOwner() == InOwner)
		{
			DeAllocateSound(Sound);
		}
	}
}

void AShooterGameState::OnTick_HandleSoundPool(float DeltaSeconds)
{
	SCOPE_CYCLE_COUNTER(STAT_HandleSoundPool);

	const int32 SoundPoolCount = SoundPool.Num();

	for (int32 Index = 0; Index < SoundPoolCount; Index++)
	{
		AShooterSound* Sound= SoundPool[Index];

		check(Sound);

		if (!Sound->bIsBeingUsed)
			continue;

		Sound->Tick_Internal(DeltaSeconds);

		if (Sound->bIsPlayingSound)
			continue;

		bool bSoundPlayed = RequestPlaySound(Sound); // can use bSoundPlayed bool to debug
	}
}

#pragma endregion Sound

// Damage
#pragma region

TSubclassOf<class UDamageType> AShooterGameState::GetDamageClassFromType(TEnumAsByte<EDamageType::Type> DamageType)
{
	switch (DamageType)
	{
	case EDamageType::Melee:
		return UShooterDamageType_Melee::StaticClass();
	case EDamageType::Normal:
		return UShooterDamageType_Normal::StaticClass();
	case EDamageType::Piercing:
		return UShooterDamageType_Piercing::StaticClass();
	case EDamageType::Siege:
		return UShooterDamageType_Siege::StaticClass();
	case EDamageType::Special:
		return UShooterDamageType_Special::StaticClass();
	case EDamageType::RanOver:
		return UShooterDamageType_RanOver::StaticClass();
	case EDamageType::Rocket:
		return UShooterDamageType_Rocket::StaticClass();
	case EDamageType::Bomb:
		return UShooterDamageType_Bomb::StaticClass();
	case EDamageType::Piano:
		return UShooterDamageType_Piano::StaticClass();
	case EDamageType::Radiation:
		return UShooterDamageType_Radiation::StaticClass();
	default:
		return UShooterDamageType::StaticClass();
	}
	return UShooterDamageType::StaticClass();
}

TEnumAsByte<EDamageType::Type> AShooterGameState::GetDamageTypeFromClass(TSubclassOf<class UDamageType> DamageClass)
{
	if (DamageClass == UShooterDamageType_Melee::StaticClass())
		return EDamageType::Melee;
	if (DamageClass == UShooterDamageType_Normal::StaticClass())
		return EDamageType::Normal;
	if (DamageClass == UShooterDamageType_Piercing::StaticClass())
		return EDamageType::Piercing;
	if (DamageClass == UShooterDamageType_Siege::StaticClass())
		return EDamageType::Siege;
	if (DamageClass == UShooterDamageType_Special::StaticClass())
		return EDamageType::Special;
	 if (DamageClass == UShooterDamageType_RanOver::StaticClass())
		return EDamageType::RanOver;
	if (DamageClass == UShooterDamageType_Rocket::StaticClass())
		return EDamageType::Rocket;
	if (DamageClass == UShooterDamageType_Bomb::StaticClass())
		return EDamageType::Bomb;
	if (DamageClass == UShooterDamageType_Piano::StaticClass())
		return EDamageType::Piano;
	if (DamageClass == UShooterDamageType_Radiation::StaticClass())
		return EDamageType::Radiation;

	return EDamageType::Normal;
}

int32 AShooterGameState::GetAdjustedDamage(int32 BaseDamage, TEnumAsByte<EDamageType::Type> DamageType, TEnumAsByte<EArmorType::Type> ArmorType)
{
	return FMath::FloorToInt(BaseDamage * DamageData->Data[DamageType].Armors[ArmorType].Value);
}

int32 AShooterGameState::GetAdjustedDamage(int32 BaseDamage, TSubclassOf<class UDamageType> DamageClass, TEnumAsByte<EArmorType::Type> ArmorType)
{
	TEnumAsByte<EDamageType::Type> DamageType = GetDamageTypeFromClass(DamageClass);

	return FMath::FloorToInt(BaseDamage * DamageData->Data[DamageType].Armors[ArmorType].Value);
}

#pragma endregion Damage

// Projectile
#pragma region

AShooterProjectile* AShooterGameState::SimpleFireProjectileSimulated(FVector Origin, FVector Trajectory, AShooterProjectileData* InProjectileData, AActor* InOwner, AActor* InInstigator, const TArray<AActor*> IgnoreActors)
{	
	AShooterProjectile* projectile = AllocateProjectile(InOwner, InInstigator, InProjectileData, IgnoreActors);
	check(projectile);

	projectile->InitVelocity(Trajectory);
	projectile->TeleportTo(Origin, Trajectory.Rotation(), false, true);

#if DEBUG_PROJECTILE_COLLISION
	projectile->InitDebugCollision();
#endif

	AShooterProjectile* FakeProjectile = AllocateFakeProjectile(InOwner, InInstigator, InProjectileData, IgnoreActors);
	if (FakeProjectile)
	{
		FakeProjectile->InitVelocity(Trajectory);
		verify(FakeProjectile->TeleportTo(Origin, Trajectory.Rotation(), false, true));
		projectile->FakeProjectile = FakeProjectile;
	}
	if (Role == ROLE_Authority)
	{
		ReplicateFireSimpleProjectile(Origin, Trajectory, InProjectileData, InOwner, InInstigator, IgnoreActors);
	}
	return projectile;
}

void AShooterGameState::ReplicateFireSimpleProjectile_Implementation(FVector Origin, FVector Trajectory, AShooterProjectileData* InProjectileData, AActor* InOwner, AActor* InInstigator, const TArray<AActor*>& IgnoreActors)
{
	if(Role < ROLE_Authority)
	{
		SimpleFireProjectileSimulated(Origin, Trajectory, InProjectileData, InOwner, InInstigator, IgnoreActors);
	}
}

AShooterProjectile *AShooterGameState::AllocateProjectile(AActor *InOwner, AActor*InInstigator, AShooterProjectileData* InProjectileData, const TArray<AActor*> IgnoreActors)
{
	AShooterProjectile *projectile = NULL;
	const int32 Count = ProjectilePool.Num();

	// Check from ProjectilePoolIndex to Count, then wrap around
	int32 poolIndex = 0;
	for (int32 countIndex = 0; countIndex < Count; ++countIndex)
	{
		poolIndex = (ProjectilePoolIndex + countIndex) % Count;

		if (!ProjectilePool[poolIndex]->IsActive && ProjectilePool[poolIndex]->DeActivateState == EProjectileDeActivate::EProjectileDeActivate_MAX)
		{
			projectile = ProjectilePool[poolIndex];
			break;
		}

		// make sure to choose at least one projectile from the pool.
		// in the case that all the projectiles are taken, choose the one with the earliest start time
		if (!projectile || ProjectilePool[poolIndex]->ActiveStartTime < projectile->ActiveStartTime)
		{
			projectile = ProjectilePool[poolIndex];

			int32 Index = ProjectilesToDeActivate.Find(projectile);
			if (Index != INDEX_NONE)
				ProjectilesToDeActivate.RemoveAt(Index);
		}
	}

	// get the pool ready for next allocation request
	ProjectilePoolIndex = (poolIndex + 1) % Count;

	check(projectile);

#if !UE_BUILD_SHIPPING
	if (projectile->IsActive)
	{
		UE_LOG(LogShooter, Log, TEXT("Exhausted inactive projectile pool of %d"), ProjectilePool.Num());
	}
#endif // !UE_BUILD_SHIPPING

	projectile->SetOwner(InOwner);
	projectile->ActivateOnAllocation(InInstigator, InProjectileData);

	for (int i = 0; i < IgnoreActors.Num(); i++)
	{
		projectile->IgnoreActor(IgnoreActors[i]);
	}

	check(!projectile->IsPendingKill());
	return projectile;
}

AShooterProjectile *AShooterGameState::AllocateProjectile(AActor *InOwner, APawn *InInstigator, AShooterWeaponData *InWeaponData)
{
	TArray<AActor*> emptyList;
	return AllocateProjectile(InOwner, InInstigator, InWeaponData, emptyList);
}

AShooterProjectile *AShooterGameState::AllocateProjectile(AActor *InOwner, APawn *InInstigator, AShooterWeaponData *InWeaponData, const TArray<AActor*> IgnoreActors)
{
	AShooterProjectile *projectile = NULL;
	const int32 Count			   = ProjectilePool.Num();

	// Check from ProjectilePoolIndex to Count, then wrap around
	int32 poolIndex = 0;
	for (int32 countIndex = 0; countIndex < Count; ++countIndex)
	{
		poolIndex = (ProjectilePoolIndex + countIndex) % Count;

		if (!ProjectilePool[poolIndex]->IsActive && ProjectilePool[poolIndex]->DeActivateState == EProjectileDeActivate::EProjectileDeActivate_MAX)
		{
			projectile = ProjectilePool[poolIndex];
			break;
		}

		// make sure to choose at least one projectile from the pool.
		// in the case that all the projectiles are taken, choose the one with the earliest start time
		if (!projectile || ProjectilePool[poolIndex]->ActiveStartTime < projectile->ActiveStartTime)
		{
			projectile = ProjectilePool[poolIndex];

			int32 Index = ProjectilesToDeActivate.Find(projectile);
			if (Index != INDEX_NONE)
				ProjectilesToDeActivate.RemoveAt(Index);
		}
	}

	// get the pool ready for next allocation request
	ProjectilePoolIndex = (poolIndex + 1) % Count;

	check(projectile);

#if !UE_BUILD_SHIPPING
	if (projectile->IsActive)
	{
		UE_LOG(LogShooter, Log, TEXT("Exhausted inactive projectile pool of %d"), ProjectilePool.Num());
	}
#endif // !UE_BUILD_SHIPPING

	projectile->SetOwner(InOwner);
	projectile->Instigator = InInstigator;
	projectile->ActivateOnAllocation(InWeaponData);

	for (int i = 0; i < IgnoreActors.Num(); i++)
	{
		projectile->IgnoreActor(IgnoreActors[i]);
	}

	check(!projectile->IsPendingKill());
	return projectile;
}

void AShooterGameState::DeAllocateProjectile(AShooterProjectile *projectile)
{
	check(projectile);
	// shouldn't return projectiles not in the pool
	check(ProjectilePool.Find(projectile) != INDEX_NONE);

	ProjectilesToDeActivate.Add(projectile);
	projectile->DeActivate();
}

AShooterProjectile *AShooterGameState::AllocateFakeProjectile(AActor *InOwner, AActor *InInstigator, AShooterProjectileData *InProjectileData, const TArray<AActor*> IgnoreActors)
{
	AShooterProjectile *projectile = NULL;
	for (int32 poolIndex = 0; poolIndex < FakeProjectilePool.Num(); ++poolIndex)
	{
		if (!FakeProjectilePool[poolIndex]->IsActive && ProjectilePool[poolIndex]->DeActivateState == EProjectileDeActivate::EProjectileDeActivate_MAX)
		{
			projectile = FakeProjectilePool[poolIndex];
			break;
		}

		// make sure to choose at least one projectile from the pool.
		// in the case that all the projectiles are taken, choose the one with the earliest start time
		if (!projectile || FakeProjectilePool[poolIndex]->ActiveStartTime < projectile->ActiveStartTime)
		{
			projectile = FakeProjectilePool[poolIndex];

			int32 Index = ProjectilesToDeActivate.Find(projectile);
			if (Index != INDEX_NONE)
				ProjectilesToDeActivate.RemoveAt(Index);
		}
	}

	check(projectile);

#if !UE_BUILD_SHIPPING
	if (projectile->IsActive)
	{
		UE_LOG(LogShooter, Log, TEXT("Exhausted inactive fake pool %d"), FakeProjectilePool.Num());
	}
#endif // !UE_BUILD_SHIPPING

	projectile->SetOwner(nullptr);
	projectile->ActivateFakeOnAllocation(InInstigator, InProjectileData);

	for (int i = 0; i < IgnoreActors.Num(); i++)
	{
		projectile->IgnoreActor(IgnoreActors[i]);
	}

	check(!projectile->IsPendingKill());
	return projectile;
}

AShooterProjectile *AShooterGameState::AllocateFakeProjectile(AActor *InOwner, APawn *InInstigator, AShooterWeaponData *InWeaponData)
{
	TArray<AActor*> emptyList;
	return AllocateFakeProjectile(InOwner, InInstigator, InWeaponData, emptyList);
}

AShooterProjectile *AShooterGameState::AllocateFakeProjectile(AActor *InOwner, APawn *InInstigator, AShooterWeaponData *InWeaponData, const TArray<AActor*> IgnoreActors)
{
	AShooterProjectile *projectile = NULL;
	for (int32 poolIndex = 0; poolIndex < FakeProjectilePool.Num(); ++poolIndex)
	{
		if (!FakeProjectilePool[poolIndex]->IsActive && ProjectilePool[poolIndex]->DeActivateState == EProjectileDeActivate::EProjectileDeActivate_MAX)
		{
			projectile = FakeProjectilePool[poolIndex];
			break;
		}

		// make sure to choose at least one projectile from the pool.
		// in the case that all the projectiles are taken, choose the one with the earliest start time
		if (!projectile || FakeProjectilePool[poolIndex]->ActiveStartTime < projectile->ActiveStartTime)
		{
			projectile = FakeProjectilePool[poolIndex];

			int32 Index = ProjectilesToDeActivate.Find(projectile);
			if (Index != INDEX_NONE)
				ProjectilesToDeActivate.RemoveAt(Index);
		}
	}

	check(projectile);

#if !UE_BUILD_SHIPPING
	if (projectile->IsActive)
	{
		UE_LOG(LogShooter, Log, TEXT("Exhausted inactive fake pool %d"), FakeProjectilePool.Num());
	}
#endif // !UE_BUILD_SHIPPING

	projectile->SetOwner(nullptr);
	projectile->Instigator = InInstigator;
	projectile->ActivateFakeOnAllocation(InWeaponData);

	for (int i = 0; i < IgnoreActors.Num(); i++)
	{
		projectile->IgnoreActor(IgnoreActors[i]);
	}

	check(!projectile->IsPendingKill());
	return projectile;
}

void AShooterGameState::DeAllocateFakeProjectile(AShooterProjectile *projectile)
{
	check(projectile);
	// shouldn't return projectiles not in the pool
	check(FakeProjectilePool.Find(projectile) != INDEX_NONE);

	ProjectilesToDeActivate.Add(projectile);
	projectile->DeActivate();
}

void AShooterGameState::OnTick_HandleProjectilesToDeActivate()
{
	SCOPE_CYCLE_COUNTER(STAT_HandleProjectilesToDeActivate);

	if (ProjectilesToDeActivate.Num() == 0)
		return;

	ProjectilesToDeActivate[0]->OnTick_HandleDeActivate();

	if (ProjectilesToDeActivate[0]->DeActivateState == EProjectileDeActivate::EProjectileDeActivate_MAX)
		ProjectilesToDeActivate.RemoveAt(0);
}

#pragma endregion Projectile

// Pickup Class
#pragma region

void AShooterGameState::AllocatePickup_Class(TEnumAsByte<EFaction::Type> InFaction, TEnumAsByte<ECharacterClass::Type> InCharacterClass, uint8 PlayerStateMappingId, bool IsBot)
{
	AShooterPickup_Class* Pickup = NULL;

	const int32 Count = PickupClassPool.Num();

	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (!PickupClassPool[Index]->IsPickupActive)
		{
			Pickup = PickupClassPool[Index];
			break;
		}
		
		if (!Pickup || 
			(!PickupClassPool[Index]->IsPickupActive && PickupClassPool[Index]->ActiveStartTime < Pickup->ActiveStartTime))
		{
			Pickup = PickupClassPool[Index];
		}
	}
	
	if (!Pickup)
		return;

	AShooterPlayerState* PlayerState = GetPlayerState(PlayerStateMappingId, IsBot);

	AShooterBot* Bot = Cast<AShooterBot>(PlayerState->LinkedPawn.Get());

	if (Bot)
	{
		Pickup->AllowBotToPickup = Bot->AllowBotToPickupClassOnDeath;
	}

	Pickup->NetUpdateFrequency = CVarNetUpdateFrequencyPickups->GetFloat();
	Pickup->ForceNetUpdate();
	Pickup->MulticastDrop((uint8)InFaction, (uint8)InCharacterClass, PlayerStateMappingId, IsBot);
}

void AShooterGameState::DeAllocatePickup_Class(AShooterPickup_Class* Pickup)
{
	Pickup->DeActivate();
	Pickup->NetUpdateFrequency = 0.1f;
}

uint8 AShooterGameState::GetPickupClassMappingId(AShooterPickup_Class* InPickupClass)
{
	const int32 Index = PickupClassPool.Find(InPickupClass);

	return Index == INDEX_NONE ? INVALID_PICKUP_CLASS : Index;
}

void AShooterGameState::OnTick_HandlePickupClass(float DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_HandlePickupClass);

	// Pickup Class

	const int32 Count = PickupClassPool.Num();

	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (Role == ROLE_Authority)
		{
			if (PickupClassPool[Index]->IsPickupActive &&
				!PickupClassPool[Index]->IsActiveForever &&
				GetWorld()->TimeSeconds - PickupClassPool[Index]->ActiveStartTime > PickupClassPool[Index]->LifeTime)
			{
				DeAllocatePickup_Class(PickupClassPool[Index]);
			}
		}
		/*
		if (PickupClassPool[Index])
			PickupClassPool[Index]->Tick_Internal(DeltaTime);
			*/
	}
}

#pragma endregion Pickup Class

// Mesh
#pragma region

AStaticMeshActor* AShooterGameState::AllocateMesh(float Time)
{
	const int32 AllocatedIndex = GetAllocatedMeshIndex();
	AStaticMeshActor* Mesh	   = AllocatedIndex > INDEX_NONE ? MeshPool[AllocatedIndex] : NULL;

	if (Mesh)
	{
		Mesh->SetActorHiddenInGame(false);
		Mesh->SetActorTickEnabled(true);
		Mesh->SetActorScale3D(FVector(1.0f));

		MeshTimes[AllocatedIndex]	   = Time;
		MeshStartTimes[AllocatedIndex] = GetWorld()->TimeSeconds;
	}
	return Mesh;
}

AStaticMeshActor* AShooterGameState::AllocateMesh(AShooterCharacter* OwningPawn, UStaticMesh* InMesh, float Time)
{
	const int32 AllocatedIndex = GetAllocatedMeshIndex();
	AStaticMeshActor* Mesh	   = AllocatedIndex > INDEX_NONE ? MeshPool[AllocatedIndex] : NULL;

	if (Mesh)
	{
		Mesh->SetActorHiddenInGame(false);
		Mesh->SetActorTickEnabled(true);

		Mesh->GetStaticMeshComponent()->SetStaticMesh(InMesh);

		const int32 Count = InMesh->Materials.Num();

		for (int32 Index = 0; Index < Count; Index++)
		{
			Mesh->GetStaticMeshComponent()->SetMaterial(Index, InMesh->Materials[Index]);
		}

		if (OwningPawn)
		{
			Mesh->SetOwner(OwningPawn);

			MeshHasOwnerList[AllocatedIndex] = true;
		}

		Mesh->SetActorScale3D(FVector(1.0f));

		MeshTimes[AllocatedIndex]      = Time;
		MeshStartTimes[AllocatedIndex] = GetWorld()->TimeSeconds;
	}
	return Mesh;
}

AStaticMeshActor* AShooterGameState::AllocateMesh(AShooterCharacter* OwningPawn, TEnumAsByte<EMeshPoolType::Type> MeshType, float Time, TEnumAsByte<EHitMarkerType::Type> HitMarkerType)
{
	const int32 AllocatedIndex = GetAllocatedMeshIndex();
	AStaticMeshActor* Mesh	   = AllocatedIndex > INDEX_NONE ? MeshPool[AllocatedIndex] : NULL;
	
	if (Mesh)
	{
		Mesh->SetActorHiddenInGame(false);
		Mesh->SetActorTickEnabled(true);

		if (OwningPawn)
		{
			Mesh->SetOwner(OwningPawn);

			MeshHasOwnerList[AllocatedIndex] = true;
		}

		Mesh->SetActorScale3D(FVector(1.0f));

		MeshTypes[AllocatedIndex]	   = MeshType;
		MeshTimes[AllocatedIndex]	   = Time;
		MeshStartTimes[AllocatedIndex] = GetWorld()->TimeSeconds;

		switch (MeshType)
		{
			case EMeshPoolType::HitMarker:
				HitMarkerTypes[AllocatedIndex] = HitMarkerType;
				break;
		}
	}
	return Mesh;
}

AStaticMeshActor* AShooterGameState::AllocateMesh(FShooterStaticMesh* MeshData, float Time)
{
	const int32 AllocatedIndex = GetAllocatedMeshIndex();
	AStaticMeshActor* Mesh	   = AllocatedIndex > INDEX_NONE ? MeshPool[AllocatedIndex] : NULL;

	if (Mesh)
	{
		Mesh->SetActorHiddenInGame(false);
		Mesh->SetActorTickEnabled(true);

		Mesh->GetStaticMeshComponent()->SetStaticMesh(MeshData->Mesh);

		const int32 Count = MeshData->Mesh->Materials.Num();

		for (int32 Index = 0; Index < Count; Index++)
		{
			Mesh->GetStaticMeshComponent()->SetMaterial(Index, MeshData->Mesh->Materials[Index]);
		}

		Mesh->SetActorScale3D(MeshData->Scale);

		MeshTimes[AllocatedIndex]	      = Time;
		MeshStartTimes[AllocatedIndex]    = GetWorld()->TimeSeconds;
		MeshDrawDistances[AllocatedIndex] = MeshData->DrawDistance * MeshData->DrawDistance;
	}
	return Mesh;
}

AStaticMeshActor* AShooterGameState::AllocateMesh(const FShooterStaticMesh* MeshData, float Time)
{
	const int32 AllocatedIndex = GetAllocatedMeshIndex();
	AStaticMeshActor* Mesh = AllocatedIndex > INDEX_NONE ? MeshPool[AllocatedIndex] : NULL;

	if (Mesh)
	{
		Mesh->SetActorHiddenInGame(false);
		Mesh->SetActorTickEnabled(true);

		Mesh->GetStaticMeshComponent()->SetStaticMesh(MeshData->Mesh);

		const int32 Count = MeshData->Mesh->Materials.Num();

		for (int32 Index = 0; Index < Count; Index++)
		{
			Mesh->GetStaticMeshComponent()->SetMaterial(Index, MeshData->Mesh->Materials[Index]);
		}

		Mesh->SetActorScale3D(MeshData->Scale);

		MeshTimes[AllocatedIndex]		  = Time;
		MeshStartTimes[AllocatedIndex]	  = GetWorld()->TimeSeconds;
		MeshDrawDistances[AllocatedIndex] = MeshData->DrawDistance * MeshData->DrawDistance;
	}
	return Mesh;
}

AStaticMeshActor* AShooterGameState::AllocateMesh(UStaticMesh* InMesh, float Time)
{
	const int32 AllocatedIndex = GetAllocatedMeshIndex();
	AStaticMeshActor* Mesh	   = AllocatedIndex > INDEX_NONE ? MeshPool[AllocatedIndex] : NULL;

	if (Mesh)
	{
		Mesh->SetActorHiddenInGame(false);
		Mesh->SetActorTickEnabled(true);

		Mesh->GetStaticMeshComponent()->SetStaticMesh(InMesh);

		const int32 Count = InMesh->Materials.Num();

		for (int32 Index = 0; Index < Count; Index++)
		{
			Mesh->GetStaticMeshComponent()->SetMaterial(Index, InMesh->Materials[Index]);
		}

		MeshTimes[AllocatedIndex]	   = Time;
		MeshStartTimes[AllocatedIndex] = GetWorld()->TimeSeconds;
	}
	return Mesh;
}

AStaticMeshActor* AShooterGameState::AllocateMesh(UStaticMesh* InMesh, float Time, int32 & OutIndex)
{
	OutIndex			   = GetAllocatedMeshIndex();
	AStaticMeshActor* Mesh = OutIndex > INDEX_NONE ? MeshPool[OutIndex] : NULL;

	if (Mesh)
	{
		Mesh->SetActorHiddenInGame(false);
		Mesh->SetActorTickEnabled(true);

		Mesh->GetStaticMeshComponent()->SetStaticMesh(InMesh);

		const int32 Count = InMesh->Materials.Num();

		for (int32 Index = 0; Index < Count; Index++)
		{
			Mesh->GetStaticMeshComponent()->SetMaterial(Index, InMesh->Materials[Index]);
		}

		MeshTimes[OutIndex]		 = Time;
		MeshStartTimes[OutIndex] = GetWorld()->TimeSeconds;
	}
	return Mesh;
}

AStaticMeshActor* AShooterGameState::AllocateAndAttachMesh(FShooterStaticMesh* MeshData, AShooterCharacter* InOwner, USceneComponent* InParent, float Time)
{
	const int32 AllocatedIndex = GetAllocatedMeshIndex();
	AStaticMeshActor* Mesh	   = AllocatedIndex > INDEX_NONE ? MeshPool[AllocatedIndex] : NULL;

	if (Mesh)
	{
		Mesh->SetActorHiddenInGame(false);
		Mesh->SetActorTickEnabled(true);

		Mesh->GetStaticMeshComponent()->SetStaticMesh(MeshData->Mesh);

		const int32 Count = MeshData->Mesh->Materials.Num();

		for (int32 Index = 0; Index < Count; Index++)
		{
			Mesh->GetStaticMeshComponent()->SetMaterial(Index, MeshData->Mesh->Materials[Index]);
		}

		if (InParent)
		{
			Mesh->SetActorRelativeLocation(MeshData->Location);
			Mesh->SetActorRelativeRotation(MeshData->Rotation);
			Mesh->SetActorRelativeScale3D(MeshData->Scale);
			Mesh->AttachToComponent(InParent, FAttachmentTransformRules::KeepRelativeTransform, MeshData->Bone);
		}

		if (InOwner)
		{
			Mesh->SetOwner(InOwner);

			MeshHasOwnerList[AllocatedIndex] = true;
		}

		MeshTimes[AllocatedIndex]		  = Time;
		MeshStartTimes[AllocatedIndex]	  = GetWorld()->TimeSeconds;
		MeshDrawDistances[AllocatedIndex] = MeshData->DrawDistance * MeshData->DrawDistance;
	}
	return Mesh;
}

AStaticMeshActor* AShooterGameState::AllocateAndAttachMesh(const FShooterStaticMesh* MeshData, AShooterCharacter* InOwner, USceneComponent* InParent, float Time)
{
	const int32 AllocatedIndex = GetAllocatedMeshIndex();
	AStaticMeshActor* Mesh = AllocatedIndex > INDEX_NONE ? MeshPool[AllocatedIndex] : NULL;

	if (Mesh)
	{
		Mesh->SetActorHiddenInGame(false);
		Mesh->SetActorTickEnabled(true);

		Mesh->GetStaticMeshComponent()->SetStaticMesh(MeshData->Mesh);

		const int32 Count = MeshData->Mesh->Materials.Num();

		for (int32 Index = 0; Index < Count; Index++)
		{
			Mesh->GetStaticMeshComponent()->SetMaterial(Index, MeshData->Mesh->Materials[Index]);
		}

		if (InParent)
		{
			Mesh->SetActorRelativeLocation(MeshData->Location);
			Mesh->SetActorRelativeRotation(MeshData->Rotation);
			Mesh->SetActorRelativeScale3D(MeshData->Scale);
			Mesh->AttachToComponent(InParent, FAttachmentTransformRules::KeepRelativeTransform, MeshData->Bone);
		}

		if (InOwner)
		{
			Mesh->SetOwner(InOwner);

			MeshHasOwnerList[AllocatedIndex] = true;
		}

		MeshTimes[AllocatedIndex] = Time;
		MeshStartTimes[AllocatedIndex] = GetWorld()->TimeSeconds;
		MeshDrawDistances[AllocatedIndex] = MeshData->DrawDistance * MeshData->DrawDistance;
	}
	return Mesh;
}

AStaticMeshActor* AShooterGameState::AllocateAndAttachMesh(UStaticMesh* InMesh, USceneComponent* InParent, float Time)
{
	const int32 AllocatedIndex = GetAllocatedMeshIndex();
	AStaticMeshActor* Mesh = AllocatedIndex > INDEX_NONE ? MeshPool[AllocatedIndex] : NULL;

	if (Mesh)
	{
		Mesh->SetActorHiddenInGame(false);
		Mesh->SetActorTickEnabled(true);

		Mesh->GetStaticMeshComponent()->SetStaticMesh(InMesh);

		const int32 Count = InMesh->Materials.Num();

		for (int32 Index = 0; Index < Count; Index++)
		{
			Mesh->GetStaticMeshComponent()->SetMaterial(Index, InMesh->Materials[Index]);
		}

		if (InParent)
		{
			Mesh->AttachToComponent(InParent, FAttachmentTransformRules::KeepRelativeTransform, NAME_None);
		}

		MeshTimes[AllocatedIndex]	   = Time;
		MeshStartTimes[AllocatedIndex] = GetWorld()->TimeSeconds;
	}
	return Mesh;
}

AStaticMeshActor* AShooterGameState::AllocateAndAttachMesh(UStaticMesh* InMesh, AActor* InParent, float Time)
{
	return AllocateAndAttachMesh(InMesh, InParent->GetRootComponent(), Time);
}

AStaticMeshActor* AShooterGameState::AllocateProjectileMesh(FCharacterInfo& InCharacterInfo, bool IsTank,float Time)
{ 
	AShooterTankData* TankData			   = GetTankData(InCharacterInfo.TankDataShortCode);
	AShooterWeaponData* WeaponData		   = IsTank ? TankData->WeaponData->GetDefaultObject<AShooterWeaponData>() : GetWeaponData(InCharacterInfo.WeaponDataShortCode);
	AShooterProjectileData* ProjectileData = WeaponData->ProjectileDataClass->GetDefaultObject<AShooterProjectileData>();
	const bool IsTracer					   = ProjectileData->IsTrailFXAttachedMesh;

	return AllocateMesh(IsTracer ? ProjectileData->TracerMesh : ProjectileData->Mesh, Time);
}

AStaticMeshActor* AShooterGameState::AllocateAndAttachProjectileMesh(FCharacterInfo& InCharacterInfo, USceneComponent* InParent, bool IsTank, float Time)
{
	AStaticMeshActor* MeshActor = AllocateProjectileMesh(InCharacterInfo, IsTank, Time);

	if (MeshActor)
	{
		MeshActor->AttachToComponent(InParent, FAttachmentTransformRules::KeepRelativeTransform);
	}
	return MeshActor;
}

AStaticMeshActor* AShooterGameState::AllocateAndAttachProjectileMesh(FCharacterInfo& InCharacterInfo, AActor* InParent, bool IsTank, float Time)
{
	return AllocateAndAttachProjectileMesh(InCharacterInfo, InParent->GetRootComponent(), IsTank, Time);
}

void AShooterGameState::DeAllocateMesh(AStaticMeshActor* Mesh)
{
	const int32 Count = MeshPool.Num();

	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (Mesh == MeshPool[Index])
		{
			DeAllocateMesh(Index);
			return;
		}
	}
}

void AShooterGameState::DeAllocateMesh(int32 Index)
{
	AStaticMeshActor* Mesh = MeshPool[Index];

	const TArray<USceneComponent*>& AttachChildren = Mesh->GetStaticMeshComponent()->GetAttachChildren();
	const int32 Count							   = AttachChildren.Num();

	for (int32 I = 0; I < Count; I++)
	{
		USceneComponent* Child = AttachChildren[I];

		if (Child &&
			Child->GetAttachParent() != MeshPool[Index]->GetRootComponent())
		{
			Child->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);

			AStaticMeshActor* ChildMesh = Cast<AStaticMeshActor>(Child->GetOuter());

			if (ChildMesh)
				DeAllocateMesh(ChildMesh);
		}
	}

	Mesh->SetActorHiddenInGame(true);
	Mesh->SetActorRelativeLocation(FVector::ZeroVector);
	Mesh->SetActorRelativeRotation(FRotator::ZeroRotator);
	Mesh->SetActorRelativeScale3D(FVector(1.0f));
	Mesh->SetActorScale3D(FVector(1.0f));
	Mesh->DetachRootComponentFromParent();
	UShooterStatics::ClearOverrideMaterials(Mesh->GetStaticMeshComponent());
	Mesh->GetStaticMeshComponent()->SetStaticMesh(NULL);
	Mesh->GetStaticMeshComponent()->SetOnlyOwnerSee(false);
	Mesh->GetStaticMeshComponent()->SetOwnerNoSee(false);
	Mesh->GetStaticMeshComponent()->bGenerateOverlapEvents = false;
	Mesh->SetActorTickEnabled(false);
	Mesh->SetOwner(NULL);

	MeshTypes[Index]		 = EMeshPoolType::EMeshPoolType_MAX;
	MeshTimes[Index]		 = 0.0f;
	MeshHasOwnerList[Index]  = false;
	MeshDrawDistances[Index] = 3000.0f *3000.0f;

	HitMarkerTypes[Index] = EHitMarkerType::EHitMarkerType_MAX;
}

AStaticMeshActor* AShooterGameState::AllocateMesh_HitMarker(AShooterCharacter* OwningPawn, FVector Location, TEnumAsByte<EHitMarkerType::Type> HitMarkerType)
{
	const float HitMarkTime = 1.0f;

	AStaticMeshActor* Mesh = AllocateMesh(OwningPawn, EMeshPoolType::HitMarker, HitMarkTime, HitMarkerType);

	if (Mesh)
	{
		Mesh->GetStaticMeshComponent()->SetOnlyOwnerSee(true);
		Mesh->GetStaticMeshComponent()->SetOwnerNoSee(false);
		Mesh->GetStaticMeshComponent()->SetStaticMesh(HitMarkerMesh);

		const int32 Count = HitMarkerMesh->Materials.Num();

		for (int32 Index = 0; Index < Count; Index++)
		{
			Mesh->GetStaticMeshComponent()->SetMaterial(Index, HitMarkerMesh->Materials[Index]);
		}

		FRotator Rotation = OwningPawn->GetViewRotation();
		Rotation.Roll     = 45.0f;

		Mesh->TeleportTo(Location, Rotation, false, true);
	}
	return Mesh;
}

AStaticMeshActor* AShooterGameState::AllocateMesh_KillConfirmedIcon(AShooterCharacter* OwningPawn, FVector Location)
{
	const float KillConfirmedIconTime = 3.0f;

	AStaticMeshActor* Mesh = AllocateMesh(OwningPawn, EMeshPoolType::KillConfirmedIcon, KillConfirmedIconTime);

	if (Mesh)
	{
		Mesh->GetStaticMeshComponent()->SetOnlyOwnerSee(true);
		Mesh->GetStaticMeshComponent()->SetOwnerNoSee(false);
		Mesh->GetStaticMeshComponent()->SetStaticMesh(KillConfirmedIconMesh);

		const int32 Count = KillConfirmedIconMesh->Materials.Num();

		for (int32 Index = 0; Index < Count; Index++)
		{
			Mesh->GetStaticMeshComponent()->SetMaterial(Index, KillConfirmedIconMesh->Materials[Index]);
		}

		Mesh->TeleportTo(Location, FRotator::ZeroRotator, false, true);
	}
	return Mesh;
}

AStaticMeshActor* AShooterGameState::AllocateMesh_Medal(AShooterCharacter* OwningPawn, FVector Location, TEnumAsByte<EGameEvent::Type> GameEvent)
{
	const float MedalTime = 3.0f;

	AStaticMeshActor* Mesh = AllocateMesh(OwningPawn, EMeshPoolType::Medal, MedalTime);

	if (Mesh)
	{
		Mesh->GetStaticMeshComponent()->SetOnlyOwnerSee(true);
		Mesh->GetStaticMeshComponent()->SetOwnerNoSee(false);
		Mesh->GetStaticMeshComponent()->SetStaticMesh(GameEventData->Events[GameEvent].Mesh);
		Mesh->TeleportTo(Location, FRotator::ZeroRotator, false, true);
	}
	return Mesh;
}

int32 AShooterGameState::ReturnMeshIndex(AStaticMeshActor* InMesh)
{
	const int32 Count = MeshPool.Num();

	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (InMesh == MeshPool[Index])
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

void AShooterGameState::SetMeshTime(int32 Index, float Time, bool UpdateStartTime)
{
	if (UpdateStartTime)
	{
		MeshStartTimes[Index] = GetWorld()->TimeSeconds;
	}
	MeshTimes[Index] = Time;
}

int32 AShooterGameState::GetAllocatedMeshIndex()
{
	const int32 Count		  = MeshPool.Num();
	const int32 PreviousIndex = MeshPoolIndex;

	if (MeshPoolIndex >= Count)
		MeshPoolIndex = 0;
	
	// Check from MeshPoolIndex to Count
	for (int32 Index = MeshPoolIndex; Index < Count; ++Index)
	{
		MeshPoolIndex = Index + 1;

		if (MeshTimes[Index] == 0.0f)
			return Index;
	}

	// Check from 0 to PreviousIndex ( where MeshPoolIndex started )
	if (PreviousIndex > 0)
	{
		MeshPoolIndex = 0;

		for (int32 Index = MeshPoolIndex; Index < PreviousIndex; ++Index)
		{
			MeshPoolIndex = Index + 1;

			if (MeshTimes[Index] == 0.0f)
				return Index;
		}
	}
	UE_LOG(LogShooter, Warning, TEXT("GetAllocatedMeshIndex: All Static Meshes from the pool have been allocated"));
	return INDEX_NONE;
}

void AShooterGameState::OnTick_HandleMeshPool(float DeltaSeconds)
{
	SCOPE_CYCLE_COUNTER(STAT_HandleMeshPool);

	const int32 Count = MeshPool.Num();

	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (MeshTimes[Index] > 0.0f)
		{
			AShooterCharacter* MeshOwner = Cast<AShooterCharacter>(MeshPool[Index]->GetOwner());

			if ((MeshHasOwnerList[Index] &&
				 (!MeshOwner ||
				  !MeshOwner->IsAlive())) ||
				GetWorld()->TimeSeconds - MeshStartTimes[Index] > MeshTimes[Index])
			{
				DeAllocateMesh(Index);
			}
			else
			{
				const float DistanceSq = UShooterStatics::GetSquaredDistanceToLocalControllerEye(GetWorld(), MeshPool[Index]->GetActorLocation());
				const bool HideMesh    = DistanceSq > MeshDrawDistances[Index];

				if (MeshOwner)
				{
					float Scale			   = 1.0f;
					const float Distance   = FVector::Dist(MeshPool[Index]->GetActorLocation(), MeshOwner->GetEyeLocation());
					
					// Hit Marker
					if (MeshTypes[Index] == EMeshPoolType::HitMarker)
					{
						Scale = HitMarkerTypes[Index] == EHitMarkerType::Tank ? 0.006f : 0.004f;

						MeshPool[Index]->SetActorScale3D(Scale * Distance * FVector(2.0f, 1.0f, 1.0f));
						MeshPool[Index]->SetActorRotation(FRotator(MeshOwner->GetViewRotation().Pitch, MeshOwner->GetViewRotation().Yaw, 45.0f));
					}
					// Kill Confirmed Icon - TODO: Remove. No longer used
					else
					if (MeshTypes[Index] == EMeshPoolType::KillConfirmedIcon)
					{
						const FRotator Rotation = DeltaSeconds * FRotator(0.0f, 100.0f, 0.0f);
						MeshPool[Index]->AddActorLocalRotation(Rotation);

						Scale = 0.0035f;

						MeshPool[Index]->SetActorScale3D(Scale * Distance * FVector(1.0f));
					}
					// Medal
					else
					if (MeshTypes[Index] == EMeshPoolType::Medal)
					{
						MeshPool[Index]->SetActorRotation(FRotator(0.0f, MeshOwner->GetViewRotation().Yaw + 90.0f, 0.0f));

						Scale = 0.0005f;

						MeshPool[Index]->SetActorScale3D(Scale * Distance * FVector(1.0f));
					}
					else
					{
						if (MeshPool[Index]->bHidden != HideMesh)
							MeshPool[Index]->SetActorHiddenInGame(HideMesh);
					}
				}
				else
				{
					if (MeshPool[Index]->bHidden != HideMesh)
						MeshPool[Index]->SetActorHiddenInGame(HideMesh);
				}
			}
		}
	}
}

#pragma endregion Mesh

// Skeletal Mesh
#pragma region

ASkeletalMeshActor* AShooterGameState::AllocateSkeletalMesh(float Time)
{
	int32 OutIndex = INDEX_NONE;
	return AllocateSkeletalMesh(NULL, NULL, Time, OutIndex);
}

ASkeletalMeshActor* AShooterGameState::AllocateSkeletalMesh(float Time, int32& OutIndex)
{
	return AllocateSkeletalMesh(NULL, NULL, Time, OutIndex);
}

ASkeletalMeshActor* AShooterGameState::AllocateSkeletalMesh(FShooterSkeletalMesh* MeshData, float Time)
{
	const int32 AllocatedIndex = GetAllocatedSkeletalMeshIndex();
	ASkeletalMeshActor* Mesh   = AllocatedIndex > INDEX_NONE ? SkeletalMeshPool[AllocatedIndex] : NULL;

	if (Mesh)
	{
		Mesh->GetSkeletalMeshComponent()->SetAnimInstanceClass(NULL);
		Mesh->GetSkeletalMeshComponent()->SetSkeletalMesh(NULL);

		Mesh->SetActorHiddenInGame(false);
		Mesh->SetActorTickEnabled(true);

		Mesh->GetSkeletalMeshComponent()->SetComponentTickEnabled(true);
		Mesh->GetSkeletalMeshComponent()->SetSkeletalMesh(MeshData->Mesh);
		Mesh->GetSkeletalMeshComponent()->SetCastShadow(MeshData->bCastShadow);

		if (MeshData->bCastShadow)
		{
			Mesh->GetSkeletalMeshComponent()->bCastDynamicShadow = true;
		}

		const int32 Count = MeshData->Mesh->Materials.Num();

		for (int32 Index = 0; Index < Count; Index++)
		{
			Mesh->GetSkeletalMeshComponent()->SetMaterial(Index, MeshData->Mesh->Materials[Index].MaterialInterface);
		}

		if (MeshData->AnimBlueprint)
		{
			Mesh->GetSkeletalMeshComponent()->SetAnimInstanceClass(MeshData->AnimBlueprint);
		}
		
		Mesh->SetActorScale3D(MeshData->Scale);

		SkeletalMeshTimes[AllocatedIndex]		  = Time;
		SkeletalMeshStartTimes[AllocatedIndex]	  = GetWorld()->TimeSeconds;
		SkeletalMeshAvailableList[AllocatedIndex] = false;
		SkeletalMeshDrawDistances[AllocatedIndex] = MeshData->DrawDistance * MeshData->DrawDistance;
	}
	return Mesh;
}

ASkeletalMeshActor* AShooterGameState::AllocateSkeletalMesh(USkeletalMesh* InMesh, float Time)
{
	int32 OutIndex = INDEX_NONE;
	return AllocateSkeletalMesh(InMesh, NULL, Time, OutIndex);
}

ASkeletalMeshActor* AShooterGameState::AllocateSkeletalMesh(USkeletalMesh* InMesh, float Time, int32& OutIndex)
{
	return AllocateSkeletalMesh(InMesh, NULL, Time, OutIndex);
}

ASkeletalMeshActor* AShooterGameState::AllocateSkeletalMesh(USkeletalMesh* InMesh, AShooterCharacter* InOwner, float Time, int32& OutIndex)
{
	OutIndex				 = GetAllocatedSkeletalMeshIndex();
	ASkeletalMeshActor* Mesh = OutIndex > INDEX_NONE ? SkeletalMeshPool[OutIndex] : NULL;

	if (Mesh)
	{
		Mesh->GetSkeletalMeshComponent()->SetAnimInstanceClass(NULL);
		Mesh->GetSkeletalMeshComponent()->SetSkeletalMesh(NULL);

		Mesh->SetActorHiddenInGame(false);
		Mesh->SetActorTickEnabled(true);

		Mesh->GetSkeletalMeshComponent()->SetComponentTickEnabled(true);

		if (InMesh)
		{
			Mesh->GetSkeletalMeshComponent()->SetSkeletalMesh(InMesh);

			const int32 Count = InMesh->Materials.Num();

			for (int32 Index = 0; Index < Count; Index++)
			{
				Mesh->GetSkeletalMeshComponent()->SetMaterial(Index, InMesh->Materials[Index].MaterialInterface);
			}
		}

		if (InOwner)
		{
			Mesh->SetOwner(InOwner);

			SkeletalMeshHasOwnerList[OutIndex] = true;
		}

		SkeletalMeshTimes[OutIndex]			= Time;
		SkeletalMeshStartTimes[OutIndex]	= GetWorld()->TimeSeconds;
		SkeletalMeshAvailableList[OutIndex] = false;
	}
	return Mesh;
}

ASkeletalMeshActor* AShooterGameState::AllocateSkeletalMesh(USkeletalMesh* InMesh, AShooterCharacter* InOwner, float Time)
{
	int32 OutIndex = INDEX_NONE;
	return AllocateSkeletalMesh(InMesh, InOwner, Time, OutIndex);
}

ASkeletalMeshActor* AShooterGameState::AllocateSkeletalMesh(const FShooterSkeletalMesh* MeshData, float Time)
{
	const int32 AllocatedIndex = GetAllocatedSkeletalMeshIndex();
	ASkeletalMeshActor* Mesh   = AllocatedIndex > INDEX_NONE ? SkeletalMeshPool[AllocatedIndex] : NULL;

	if (Mesh)
	{
		Mesh->GetSkeletalMeshComponent()->SetAnimInstanceClass(NULL);
		Mesh->GetSkeletalMeshComponent()->SetSkeletalMesh(NULL);

		Mesh->SetActorHiddenInGame(false);
		Mesh->SetActorTickEnabled(true);

		Mesh->GetSkeletalMeshComponent()->SetComponentTickEnabled(true);
		Mesh->GetSkeletalMeshComponent()->SetSkeletalMesh(MeshData->Mesh);
		Mesh->GetSkeletalMeshComponent()->SetCastShadow(MeshData->bCastShadow);

		if (MeshData->bCastShadow)
		{
			Mesh->GetSkeletalMeshComponent()->bCastDynamicShadow = true;
		}

		const int32 Count = MeshData->Mesh->Materials.Num();

		for (int32 Index = 0; Index < Count; Index++)
		{
			Mesh->GetSkeletalMeshComponent()->SetMaterial(Index, MeshData->Mesh->Materials[Index].MaterialInterface);
		}

		if (MeshData->AnimBlueprint)
		{
			Mesh->GetSkeletalMeshComponent()->SetAnimInstanceClass(MeshData->AnimBlueprint);
		}

		Mesh->SetActorScale3D(MeshData->Scale);

		SkeletalMeshTimes[AllocatedIndex]		  = Time;
		SkeletalMeshStartTimes[AllocatedIndex]	  = GetWorld()->TimeSeconds;
		SkeletalMeshAvailableList[AllocatedIndex] = false;
		SkeletalMeshDrawDistances[AllocatedIndex] = MeshData->DrawDistance * MeshData->DrawDistance;
	}
	return Mesh;
}

ASkeletalMeshActor* AShooterGameState::AllocateSkeletalMesh(AShooterCharacter* InOwner, float Time, int32& OutIndex)
{
	return AllocateSkeletalMesh(NULL, InOwner, Time, OutIndex);
}

ASkeletalMeshActor* AShooterGameState::AllocateSkeletalMesh(AShooterCharacter* InOwner, float Time)
{
	int32 OutIndex = INDEX_NONE;
	return AllocateSkeletalMesh(NULL, InOwner, Time, OutIndex);
}

ASkeletalMeshActor* AShooterGameState::AllocateAndAttachSkeletalMesh(FShooterSkeletalMesh* MeshData, AShooterCharacter* InOwner, USceneComponent* InParent, float Time)
{
	const int32 AllocatedIndex = GetAllocatedSkeletalMeshIndex();
	ASkeletalMeshActor* Mesh   = AllocatedIndex > INDEX_NONE ? SkeletalMeshPool[AllocatedIndex] : NULL;

	if (Mesh)
	{
		Mesh->GetSkeletalMeshComponent()->SetAnimInstanceClass(NULL);
		Mesh->GetSkeletalMeshComponent()->SetSkeletalMesh(NULL);

		Mesh->SetActorHiddenInGame(false);
		Mesh->SetActorTickEnabled(true);

		Mesh->GetSkeletalMeshComponent()->SetComponentTickEnabled(true);
		Mesh->GetSkeletalMeshComponent()->SetSkeletalMesh(MeshData->Mesh);
		Mesh->GetSkeletalMeshComponent()->SetCastShadow(MeshData->bCastShadow);

		if (MeshData->OverlapWithProjectiles)
		{
			Mesh->GetSkeletalMeshComponent()->bGenerateOverlapEvents = true;
			Mesh->GetSkeletalMeshComponent()->SetCollisionResponseToChannel(COLLISION_PROJECTILE, ECR_Block);
			Mesh->GetSkeletalMeshComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		}

		if (MeshData->bCastShadow)
		{
			Mesh->GetSkeletalMeshComponent()->bCastDynamicShadow = true;
		}

		const int32 Count = MeshData->Mesh->Materials.Num();

		for (int32 Index = 0; Index < Count; Index++)
		{
			Mesh->GetSkeletalMeshComponent()->SetMaterial(Index, MeshData->Mesh->Materials[Index].MaterialInterface);
		}

		if (InParent)
		{
			Mesh->SetActorRelativeLocation(MeshData->Location);
			Mesh->SetActorRelativeRotation(MeshData->Rotation);
			Mesh->SetActorRelativeScale3D(MeshData->Scale);
			Mesh->AttachToComponent(InParent, FAttachmentTransformRules::KeepRelativeTransform, MeshData->Bone);
		}

		if (InOwner)
		{
			Mesh->SetOwner(InOwner);

			SkeletalMeshHasOwnerList[AllocatedIndex] = true;
		}

		if (MeshData->AnimBlueprint)
		{
			Mesh->GetSkeletalMeshComponent()->SetAnimInstanceClass(MeshData->AnimBlueprint);
		}

		SkeletalMeshTimes[AllocatedIndex]		  = Time;
		SkeletalMeshStartTimes[AllocatedIndex]	  = GetWorld()->TimeSeconds;
		SkeletalMeshAvailableList[AllocatedIndex] = false;
		SkeletalMeshDrawDistances[AllocatedIndex] = MeshData->DrawDistance * MeshData->DrawDistance;
	}
	return Mesh;
}

ASkeletalMeshActor* AShooterGameState::AllocateAndAttachSkeletalMesh(const FShooterSkeletalMesh* MeshData, AShooterCharacter* InOwner, USceneComponent* InParent, float Time)
{
	const int32 AllocatedIndex = GetAllocatedSkeletalMeshIndex();
	ASkeletalMeshActor* Mesh   = AllocatedIndex > INDEX_NONE ? SkeletalMeshPool[AllocatedIndex] : NULL;
	
	if (Mesh)
	{
		Mesh->GetSkeletalMeshComponent()->SetAnimInstanceClass(NULL);
		Mesh->GetSkeletalMeshComponent()->SetSkeletalMesh(NULL);

		Mesh->SetActorHiddenInGame(false);
		Mesh->SetActorTickEnabled(true);

		Mesh->GetSkeletalMeshComponent()->SetComponentTickEnabled(true);
		Mesh->GetSkeletalMeshComponent()->SetSkeletalMesh(MeshData->Mesh);
		Mesh->GetSkeletalMeshComponent()->SetCastShadow(MeshData->bCastShadow);

		if (MeshData->OverlapWithProjectiles)
		{
			Mesh->GetSkeletalMeshComponent()->bGenerateOverlapEvents = true;
			Mesh->GetSkeletalMeshComponent()->SetCollisionResponseToChannel(COLLISION_PROJECTILE, ECR_Block);
			Mesh->GetSkeletalMeshComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		}

		if (MeshData->bCastShadow)
		{
			Mesh->GetSkeletalMeshComponent()->bCastDynamicShadow = true;
		}

		const int32 Count = MeshData->Mesh->Materials.Num();

		for (int32 Index = 0; Index < Count; Index++)
		{
			Mesh->GetSkeletalMeshComponent()->SetMaterial(Index, MeshData->Mesh->Materials[Index].MaterialInterface);
		}

		if (InParent)
		{
			Mesh->SetActorRelativeLocation(MeshData->Location);
			Mesh->SetActorRelativeRotation(MeshData->Rotation);
			Mesh->SetActorRelativeScale3D(MeshData->Scale);
			Mesh->AttachToComponent(InParent, FAttachmentTransformRules::KeepRelativeTransform, MeshData->Bone);
		}

		if (InOwner)
		{
			Mesh->SetOwner(InOwner);

			SkeletalMeshHasOwnerList[AllocatedIndex] = true;
		}

		if (MeshData->AnimBlueprint)
		{
			Mesh->GetSkeletalMeshComponent()->SetAnimInstanceClass(MeshData->AnimBlueprint);
		}

		SkeletalMeshTimes[AllocatedIndex]		  = Time;
		SkeletalMeshStartTimes[AllocatedIndex]    = GetWorld()->TimeSeconds;
		SkeletalMeshAvailableList[AllocatedIndex] = false;
		SkeletalMeshDrawDistances[AllocatedIndex] = MeshData->DrawDistance * MeshData->DrawDistance;
	}
	return Mesh;
}

ASkeletalMeshActor* AShooterGameState::AllocateAndAttachSkeletalMesh(USkeletalMesh* InMesh, USceneComponent* InParent, float Time)
{
	ASkeletalMeshActor* MeshActor = AllocateSkeletalMesh(InMesh, Time);

	if (MeshActor)
	{
		MeshActor->AttachToComponent(InParent, FAttachmentTransformRules::KeepRelativeTransform, NAME_None);
		return MeshActor;
	}
	return NULL;
}

ASkeletalMeshActor* AShooterGameState::AllocateAndAttachSkeletalMesh(USkeletalMesh* InMesh, AActor* InParent, float Time)
{
	return AllocateAndAttachSkeletalMesh(InMesh, InParent->GetRootComponent(), Time);
}

ASkeletalMeshActor* AShooterGameState::AllocateAndAttachRagdoll(USkeletalMesh* MeshTemplate, AShooterCharacter* InOwner, USceneComponent* InParent, FName AttachingBone, FName AttachToBone, float Time)
{
	const int32 AllocatedIndex = GetAllocatedSkeletalMeshIndex();
	ASkeletalMeshActor* Mesh   = AllocatedIndex > INDEX_NONE ? SkeletalMeshPool[AllocatedIndex] : NULL;

	if (Mesh)
	{
		Mesh->GetSkeletalMeshComponent()->SetAnimInstanceClass(NULL);
		Mesh->GetSkeletalMeshComponent()->SetSkeletalMesh(NULL);

		Mesh->SetActorHiddenInGame(false);
		Mesh->SetActorTickEnabled(true);

		Mesh->GetSkeletalMeshComponent()->SetComponentTickEnabled(true);
		Mesh->GetSkeletalMeshComponent()->SetSkeletalMesh(MeshTemplate);

		const int32 Count = MeshTemplate->Materials.Num();

		for (int32 Index = 0; Index < Count; Index++)
		{
			Mesh->GetSkeletalMeshComponent()->SetMaterial(Index, MeshTemplate->Materials[Index].MaterialInterface);
		}

		if (InOwner)
		{
			/*
			Mesh->TeleportTo(InParent->GetSocketLocation(Bone) + FVector(0.0f, 0.0f, 100.0f), InParent->GetSocketRotation(Bone), false, true);

			Mesh->GetSkeletalMeshComponent()->SetAllBodiesSimulatePhysics(true);
			Mesh->GetSkeletalMeshComponent()->SetSimulatePhysics(true);
			Mesh->GetSkeletalMeshComponent()->WakeAllRigidBodies();
			Mesh->GetSkeletalMeshComponent()->bBlendPhysics = true;
			*/
			//Mesh->SetActorRelativeLocation(-1.0f * Mesh->GetSkeletalMeshComponent()->GetBoneLocation(AttachingBone, EBoneSpaces::ComponentSpace) + FVector(0.0f, 0.0f, 32.0f));
			Mesh->SetActorRelativeLocation(FVector(0.0f, -8.0f, 0.0f));

			const float Pitch = FMath::RandRange(0, 1) == 1 ? FMath::FRandRange(-60.0f, -45.0f) : FMath::FRandRange(45.0f, 60.0f);

			Mesh->SetActorRelativeRotation(FRotator(Pitch, 0.0f, 0.0f));
			Mesh->AttachToComponent(InParent, FAttachmentTransformRules::KeepRelativeTransform, AttachToBone);
			Mesh->SetOwner(InOwner);

			SkeletalMeshHasOwnerList[AllocatedIndex] = true;
		}
		
		/*
		Mesh->GetSkeletalMeshComponent()->SetCollisionObjectType(ECC_PhysicsBody);
		Mesh->GetSkeletalMeshComponent()->SetCollisionResponseToAllChannels(ECR_Block);
		Mesh->GetSkeletalMeshComponent()->SetCollisionResponseToChannel(COLLISION_PROJECTILE, ECR_Overlap);
		Mesh->GetSkeletalMeshComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		*/
		SkeletalMeshTimes[AllocatedIndex]		  = Time;
		SkeletalMeshStartTimes[AllocatedIndex]	  = GetWorld()->TimeSeconds;
		SkeletalMeshAvailableList[AllocatedIndex] = false;
		SkeletalMeshDrawDistances[AllocatedIndex] = 20000.0f * 20000.0f;
	}
	return Mesh;
}

// Character

ASkeletalMeshActor* AShooterGameState::AllocateCharacterMesh(TEnumAsByte<ECharacterSkin::Type> InSkinType, FCharacterInfo& InCharacterInfo, float Time, int32& OutIndex)
{
	ASkeletalMeshActor* MeshActor = AllocateSkeletalMesh(Time, OutIndex);

	AShooterCharacterMeshSkin* MeshSkin = GetCharacterMeshSkin(InCharacterInfo.CharacterMeshSkinShortCode);

	MeshSkin->SetMesh(MeshActor, InSkinType);

	AShooterCharacterMaterialSkin* MaterialSkin = GetCharacterMaterialSkin(InCharacterInfo.CharacterMaterialSkinShortCode);

	MaterialSkin->SetMaterials(MeshActor, InSkinType);

	return MeshActor;
}

ASkeletalMeshActor* AShooterGameState::AllocateCharacterMesh(TEnumAsByte<ECharacterSkin::Type> InSkinType, FCharacterInfo& InCharacterInfo, float Time)
{
	int32 OutIndex = INDEX_NONE;
	return AllocateCharacterMesh(InSkinType, InCharacterInfo, Time, OutIndex);
}

ASkeletalMeshActor* AShooterGameState::AllocateCharacterMesh(TEnumAsByte<ECharacterSkin::Type> InSkinType, AShooterPlayerState* InPlayerState, TEnumAsByte<EFaction::Type> InFaction, TEnumAsByte<ECharacterClass::Type> InCharacterClass, float Time, int32 & OutIndex)
{
	FCharacterInfo(*Infos)[ECharacterClass::ECharacterClass_MAX] = InFaction == EFaction::GR ? &InPlayerState->AxisCharacterInfos : &InPlayerState->AllyCharacterInfos;
	FCharacterInfo& Info										= (*Infos)[(int32)InCharacterClass];

	return AllocateCharacterMesh(InSkinType, Info, Time, OutIndex);
}

ASkeletalMeshActor* AShooterGameState::AllocateAndAttachCharacterMesh(TEnumAsByte<ECharacterSkin::Type> InSkinType, FCharacterInfo& InCharacterInfo, USceneComponent* InParent, float Time, int32& OutIndex)
{
	ASkeletalMeshActor* MeshActor = AllocateCharacterMesh(InSkinType, InCharacterInfo, Time, OutIndex);

	if (MeshActor)
	{
		MeshActor->AttachToComponent(InParent, FAttachmentTransformRules::KeepRelativeTransform);
	}
	return MeshActor;
}

ASkeletalMeshActor* AShooterGameState::AllocateAndAttachCharacterMesh(TEnumAsByte<ECharacterSkin::Type> InSkinType, FCharacterInfo& InCharacterInfo, USceneComponent* InParent, float Time)
{
	int32 OutIndex = INDEX_NONE;
	return AllocateAndAttachCharacterMesh(InSkinType, InCharacterInfo, InParent, Time, OutIndex);
}

ASkeletalMeshActor* AShooterGameState::AllocateAndAttachCharacterMesh(TEnumAsByte<ECharacterSkin::Type> InSkinType, FCharacterInfo& InCharacterInfo, AActor* InParent, float Time, int32& OutIndex)
{
	return AllocateAndAttachCharacterMesh(InSkinType, InCharacterInfo, InParent->GetRootComponent(), Time, OutIndex);
}

ASkeletalMeshActor* AShooterGameState::AllocateAndAttachCharacterMesh(TEnumAsByte<ECharacterSkin::Type> InSkinType, FCharacterInfo& InCharacterInfo, AActor* InParent, float Time)
{
	int32 OutIndex = INDEX_NONE;
	return AllocateAndAttachCharacterMesh(InSkinType, InCharacterInfo, InParent, Time, OutIndex);
}

// Hat

ASkeletalMeshActor* AShooterGameState::AllocateAndAttachHatMesh(TEnumAsByte<EHatMaterial::Type> InHatMaterial, FCharacterInfo& InCharacterInfo, USceneComponent* InParent, float Time, int32& OutIndex)
{
	AShooterHatData* Hat = GetHat(InCharacterInfo.HatShortCode);

	if (Hat->IsDefault)
		return NULL;

	ASkeletalMeshActor* MeshActor = AllocateSkeletalMesh(Time, OutIndex);

	if (MeshActor)
	{
		if (Cast<USkeletalMeshComponent>(InParent))
		{
			AShooterCharacterData* CharacterData = GetCharacterData(InCharacterInfo.CharacterDataShortCode);
			Hat->SetAndAttach(MeshActor->GetSkeletalMeshComponent(), InCharacterInfo.CharacterDataShortCode, Cast<USkeletalMeshComponent>(InParent), CharacterData->HatBoneName, InHatMaterial);
		}
		else
		{
			Hat->Set(MeshActor, InHatMaterial);
			Hat->AttachToComponent(InParent, FAttachmentTransformRules::KeepRelativeTransform);
		}
	}
	return MeshActor;
}

ASkeletalMeshActor* AShooterGameState::AllocateAndAttachHatMesh(TEnumAsByte<EHatMaterial::Type> InHatMaterial, FCharacterInfo& InCharacterInfo, AActor* InParent, float Time, int32& OutIndex)
{
	return AllocateAndAttachHatMesh(InHatMaterial, InCharacterInfo, InParent->GetRootComponent(), Time, OutIndex);
}

ASkeletalMeshActor* AShooterGameState::AllocateAndAttachHatMesh(TEnumAsByte<EHatMaterial::Type> InHatMaterial, FCharacterInfo& InCharacterInfo, AActor* InParent, float Time)
{
	int32 OutIndex = INDEX_NONE;
	return AllocateAndAttachHatMesh(InHatMaterial, InCharacterInfo, InParent, Time, OutIndex);
}

// Weapon

ASkeletalMeshActor* AShooterGameState::AllocateWeaponMesh(TEnumAsByte<EViewType::Type> InViewType, FCharacterInfo& InCharacterInfo, float Time, int32& OutIndex, bool IsLow)
{
	return AllocateWeaponMesh(InViewType, GetWeaponData(InCharacterInfo.WeaponDataShortCode), Time, OutIndex, IsLow);
}

ASkeletalMeshActor* AShooterGameState::AllocateWeaponMesh(TEnumAsByte<EViewType::Type> InViewType, FCharacterInfo& InCharacterInfo, float Time, bool IsLow)
{
	return AllocateWeaponMesh(InViewType, GetWeaponData(InCharacterInfo.WeaponDataShortCode), Time, IsLow);
}

ASkeletalMeshActor* AShooterGameState::AllocateWeaponMesh(TEnumAsByte<EViewType::Type> InViewType, AShooterWeaponData* InWeaponData, AShooterCharacter* InOwner, float Time, int32& OutIndex, bool IsLow)
{
	ASkeletalMeshActor* MeshActor = InOwner ? AllocateSkeletalMesh(InOwner, Time, OutIndex) : AllocateSkeletalMesh(Time, OutIndex);

	InWeaponData->SetMeshAndMaterials(MeshActor, InViewType, IsLow);

	return MeshActor;
}

ASkeletalMeshActor* AShooterGameState::AllocateWeaponMesh(TEnumAsByte<EViewType::Type> InViewType, AShooterWeaponData* InWeaponData, float Time, int32& OutIndex, bool IsLow)
{
	return AllocateWeaponMesh(InViewType, InWeaponData, NULL, Time, OutIndex, IsLow);
}

ASkeletalMeshActor* AShooterGameState::AllocateWeaponMesh(TEnumAsByte<EViewType::Type> InViewType, AShooterWeaponData* InWeaponData, float Time, bool IsLow)
{
	int32 OutIndex = INDEX_NONE;
	return AllocateWeaponMesh(InViewType, InWeaponData, Time, OutIndex, IsLow);
}

ASkeletalMeshActor* AShooterGameState::AllocateWeaponMesh(TEnumAsByte<EViewType::Type> InViewType, AShooterPlayerState* InPlayerState, TEnumAsByte<EFaction::Type> InFaction, TEnumAsByte<ECharacterClass::Type> InCharacterClass, float Time, int32 & OutIndex, bool IsLow)
{
	FCharacterInfo(*Infos)[ECharacterClass::ECharacterClass_MAX] = InFaction == EFaction::GR ? &InPlayerState->AxisCharacterInfos : &InPlayerState->AllyCharacterInfos;
	FCharacterInfo& Info										 = (*Infos)[(int32)InCharacterClass];

	AShooterWeaponData* WeaponData = GetWeaponData(Info.WeaponDataShortCode);

	return AllocateWeaponMesh(InViewType, WeaponData, Time, OutIndex, IsLow);
}

ASkeletalMeshActor* AShooterGameState::AllocateAndAttachWeaponMesh(TEnumAsByte<EViewType::Type> InViewType, AShooterCharacterData* InCharacterData, AShooterWeaponData* InWeaponData, AShooterCharacter* InOwner, USceneComponent* InParent, float Time, int32& OutIndex, bool IsLow)
{
	ASkeletalMeshActor* MeshActor = AllocateWeaponMesh(InViewType, InWeaponData, Time, OutIndex, IsLow);

	if (MeshActor)
	{
		const bool UseMasterPoseComponent = InViewType == EViewType::ThirdPerson ? InWeaponData->bUseMasterPoseForTransform3P : InWeaponData->bUseMasterPoseForTransform1P;

		if (UseMasterPoseComponent &&
			Cast<USkeletalMeshComponent>(InParent))
		{
			MeshActor->GetSkeletalMeshComponent()->AttachToComponent(InParent, FAttachmentTransformRules::KeepRelativeTransform);
			MeshActor->GetSkeletalMeshComponent()->SetMasterPoseComponent(Cast<USkeletalMeshComponent>(InParent));
		}
		else
		{
			MeshActor->AttachToComponent(InParent, FAttachmentTransformRules::SnapToTargetIncludingScale, InCharacterData->WeaponAttachPoint);
		}
	}
	return MeshActor;
}

ASkeletalMeshActor* AShooterGameState::AllocateAndAttachWeaponMesh(TEnumAsByte<EViewType::Type> InViewType, AShooterCharacterData* InCharacterData, AShooterWeaponData* InWeaponData, AShooterCharacter* InOwner, USceneComponent* InParent, float Time, bool IsLow)
{
	int32 OutIndex = INDEX_NONE;
	return AllocateAndAttachWeaponMesh(InViewType, InCharacterData, InWeaponData, InOwner, InParent, Time, OutIndex, IsLow);
}

ASkeletalMeshActor* AShooterGameState::AllocateAndAttachWeaponMesh(TEnumAsByte<EViewType::Type> InViewType, FCharacterInfo& InCharacterInfo, USceneComponent* InParent, float Time, int32& OutIndex, bool IsLow)
{
	ASkeletalMeshActor* MeshActor = AllocateWeaponMesh(InViewType, InCharacterInfo, Time, OutIndex, IsLow);

	if (MeshActor)
	{
		AShooterWeaponData* WeaponData = GetWeaponData(InCharacterInfo.WeaponDataShortCode);
		const bool UseMasterPoseComponent = InViewType == EViewType::ThirdPerson ? WeaponData->bUseMasterPoseForTransform3P : WeaponData->bUseMasterPoseForTransform1P;

		if (UseMasterPoseComponent &&
			Cast<USkeletalMeshComponent>(InParent))
		{
			MeshActor->GetSkeletalMeshComponent()->AttachToComponent(InParent, FAttachmentTransformRules::KeepRelativeTransform);
			MeshActor->GetSkeletalMeshComponent()->SetMasterPoseComponent(Cast<USkeletalMeshComponent>(InParent));
		}
		else
		{
			AShooterCharacterData* Data = GetCharacterData(InCharacterInfo.CharacterDataShortCode);

			MeshActor->AttachToComponent(InParent, FAttachmentTransformRules::SnapToTargetIncludingScale, Data->WeaponAttachPoint);
		}
	}
	return MeshActor;
}

ASkeletalMeshActor* AShooterGameState::AllocateAndAttachWeaponMesh(TEnumAsByte<EViewType::Type> InViewType, FCharacterInfo& InCharacterInfo, USceneComponent* InParent, float Time, bool IsLow)
{
	int32 OutIndex = INDEX_NONE;
	return AllocateAndAttachWeaponMesh(InViewType, InCharacterInfo, InParent, Time, OutIndex, IsLow);
}

ASkeletalMeshActor* AShooterGameState::AllocateAndAttachWeaponMesh(TEnumAsByte<EViewType::Type> InViewType, FCharacterInfo& InCharacterInfo, AActor* InParent, float Time, int32& OutIndex, bool IsLow)
{
	return AllocateAndAttachWeaponMesh(InViewType, InCharacterInfo, InParent->GetRootComponent(), Time, OutIndex, IsLow);
}

ASkeletalMeshActor* AShooterGameState::AllocateAndAttachWeaponMesh(TEnumAsByte<EViewType::Type> InViewType, FCharacterInfo& InCharacterInfo, AActor* InParent, float Time, bool IsLow)
{
	return AllocateAndAttachWeaponMesh(InViewType, InCharacterInfo, InParent->GetRootComponent(), Time, IsLow);
}

ASkeletalMeshActor* AShooterGameState::AllocateRagdoll(FCharacterInfo & InCharacterInfo)
{
	AShooterCharacterData* CharacterData = GetCharacterData(InCharacterInfo.Faction, InCharacterInfo.CharacterDataShortCode);

	if (!CharacterData)
	{
		UE_LOG(LogShooter, Warning, TEXT("AllocateRagdoll: CharacterData with ShortCode: %s is not loaded"), *InCharacterInfo.CharacterDataShortCode.ToString());
		return NULL;
	}

	if (CharacterData->Class != InCharacterInfo.CharacterClass)
	{
		FString DataClass = UShooterStatics::CharacterClassToString(CharacterData->Class);
		FString InfoClass = UShooterStatics::CharacterClassToString(InCharacterInfo.CharacterClass);

		UE_LOG(LogShooter, Warning, TEXT("AllocateRagdoll: Character Class mismatch. Data's Class is: and Info's Class is: %s"), *DataClass, *InfoClass);
		return NULL;
	}

	FName CharacterMeshSkinShortCode	 = InCharacterInfo.CharacterMeshSkinShortCode;
	FName CharacterMaterialSkinShortCode = InCharacterInfo.CharacterMaterialSkinShortCode;
	AShooterWeaponData* WeaponData		 = GetWeaponData(InCharacterInfo.WeaponDataShortCode);

	// Allocate the Skeletal Mesh Actor for Character
	USkeletalMesh* MeshTemplate   = GetCharacterMesh(ECharacterSkin::Mesh3P, InCharacterInfo);
	const int32 AllocatedIndex	  = GetAllocatedSkeletalMeshIndex();
	ASkeletalMeshActor* MeshActor = AllocatedIndex > INDEX_NONE ? SkeletalMeshPool[AllocatedIndex] : NULL;

	if (!MeshActor)
	{
		UE_LOG(LogShooter, Warning, TEXT("Failed Allocating Ragdoll: Not enough actors in Skeletal Mesh Pool"));
		return NULL;
	}

	USkeletalMeshComponent* Mesh = MeshActor->GetSkeletalMeshComponent();

	Mesh->SetAnimInstanceClass(CharacterData->EmptyAnimBlueprints.Blueprint3P);
	Mesh->SetSkeletalMesh(MeshTemplate);

	const int32 Count = MeshTemplate->Materials.Num();

	for (int32 Index = 0; Index < Count; Index++)
	{
		Mesh->SetMaterial(Index, GetCharacterDeathMaterial(Index, InCharacterInfo.Faction, CharacterMaterialSkinShortCode));
	}

	const float VeryLongTime = 1000.0f;

	SkeletalMeshTimes[AllocatedIndex]		  = GetMatchState() == MatchState::InProgress ? RemainingTime : VeryLongTime;
	SkeletalMeshStartTimes[AllocatedIndex]    = GetWorld()->TimeSeconds;
	SkeletalMeshAvailableList[AllocatedIndex] = false;

	return MeshActor;
}

PT_THREAD(AShooterGameState::HandleSkeletalMeshRagdoll(struct FRoutine* r))
{
	ASkeletalMeshActor* sm = Cast<ASkeletalMeshActor>(r->GetActor());
	ACoroutineScheduler* s = r->scheduler;
	UWorld* w			    = s->GetWorld();
	AShooterGameState* gs   = Cast<AShooterGameState>(w->GameState);

	const float CurrentTime = w->TimeSeconds;
	const float StartTime   = r->startTime;
	const int32 Index	    = r->ints[0]; // AllocatedIndex

	USkeletalMeshComponent* Mesh = gs->SkeletalMeshPool[Index]->GetSkeletalMeshComponent();

	COROUTINE_BEGIN(r);

	if (!Mesh)
	{
		r->End();
		COROUTINE_EXIT(r);
	}

	if (r->delay > 0)
		COROUTINE_WAIT_UNTIL(r, CurrentTime - StartTime > r->delay);
	
	static float BlendWeight = 1.0f;
	Mesh->SetPhysicsBlendWeight(BlendWeight);

	// ClothTickFunction.bStartWithTickEnabled is set to false as optimization. Only needed for ragdoll
	// so when we go into ragdoll, we need to manually renable ticking
	Mesh->ClothTickFunction.SetTickFunctionEnable(true);

	// initialize physics/etc
	Mesh->SetAllBodiesSimulatePhysics(true);
	Mesh->SetSimulatePhysics(true);
	Mesh->WakeAllRigidBodies();
	Mesh->bBlendPhysics = true;

	UAnimInstance* AnimInstance = Mesh->GetAnimInstance();

	if (AnimInstance)
		Mesh->SetAnimInstanceClass(NULL);
	else
		Mesh->Stop();
	
	COROUTINE_END(r);
}

// Death

ASkeletalMeshActor* AShooterGameState::AllocateCharacterDeathMesh(FCharacterInfo&  InCharacterInfo, float Time)
{
	ASkeletalMeshActor* MeshActor = AllocateSkeletalMesh(Time);
	SetCharacterMesh(MeshActor->GetSkeletalMeshComponent(), ECharacterSkin::Death, InCharacterInfo);

	return MeshActor;
}

ASkeletalMeshActor* AShooterGameState::AllocateAngelDeathMesh(TEnumAsByte<EViewType::Type> InViewType, FCharacterInfo& InCharacterInfo, int32& OutIndex, ASkeletalMeshActor*& OutHat, int32& OutHatIndex, TArray<ASkeletalMeshActor*>& OutAttachments, TArray<int32>& OutAttachmentIndices, float Time, bool AllocateEffects)
{
	USkeletalMesh* Mesh							= GetCharacterMesh(InViewType == EViewType::ThirdPerson ? ECharacterSkin::Angel3P : ECharacterSkin::Angel1P, InCharacterInfo);
	ASkeletalMeshActor* MeshActor				= AllocateSkeletalMesh(Mesh, Time, OutIndex);
	AShooterCharacterData* Data					= GetCharacterData(InCharacterInfo.CharacterDataShortCode);
	AShooterCharacterMeshSkin* MeshSkin			= GetCharacterMeshSkin(InCharacterInfo.CharacterMeshSkinShortCode);
	AShooterCharacterMaterialSkin* MaterialSkin = GetCharacterMaterialSkin(InCharacterInfo.CharacterMaterialSkinShortCode);

	// Assign Materials
	MaterialSkin->SetMaterials(MeshActor, InViewType == EViewType::ThirdPerson ? ECharacterSkin::Angel3P : ECharacterSkin::Angel1P);

	// Attach Hat if it not the Default Hat
	AShooterHatData* HatData = NULL;

	if (InViewType == EViewType::ThirdPerson)
	{
		HatData = GetHat(InCharacterInfo.HatShortCode);
		// TODO: Later need to rig the Custom Angel Mesh to have a bone to attach the Hat to
		if (!HatData->IsDefault && !MeshSkin->HasCustomAngelMesh)
		{
			OutHat = AllocateSkeletalMesh(Time, OutHatIndex);
			HatData->SetAndAttach(OutHat, InCharacterInfo.CharacterDataShortCode, MeshActor, Data->HatBoneName, EHatMaterial::Angel);
		}
	}

	// Add Attachments - Only 3rd Person
	if (InViewType == EViewType::ThirdPerson &&
		Data->AngelDeathProps.Num() > 0)
	{
		const int32 Count = Data->AngelDeathProps.Num();

		for (int32 Index = 0; Index < Count; Index++)
		{
			if (Data->AngelDeathProps[Index].SkeletalMesh)
			{
				int32 OutAttachmentIndex		 = INDEX_NONE;
				ASkeletalMeshActor* AttachedMesh = AllocateSkeletalMesh(Time, OutAttachmentIndex);

				if (AttachedMesh)
				{
					AttachedMesh->GetSkeletalMeshComponent()->SetSkeletalMesh(Data->AngelDeathProps[Index].SkeletalMesh);

					FName BoneName		  = NAME_None;
					const int32 HaloIndex = 1;

					if (Index == HaloIndex)
					{
						BoneName = HatData->IsDefault || MeshSkin->HasCustomAngelMesh ? Data->HatBoneName : FName("hat_tip");
					}
					else
					{
						BoneName = Data->AngelDeathProps[Index].BoneAttachPoint;
					}
					ASkeletalMeshActor* Parent = OutHat && Index == HaloIndex ? OutHat : MeshActor;

					AttachedMesh->AttachToActor(Parent, FAttachmentTransformRules::SnapToTargetIncludingScale, BoneName);

					if (Data->AngelDeathProps[Index].Anim)
						AttachedMesh->GetSkeletalMeshComponent()->PlayAnimation(Data->AngelDeathProps[Index].Anim, true);
					
					OutAttachments.Add(AttachedMesh);
					OutAttachmentIndices.Add(OutAttachmentIndex);
				}
			}
		}
	}

	// Add Effect Attachments
	if (AllocateEffects &&
		Data->AngelDeathFXs.Num() > 0)
	{
		const int32 Count = Data->AngelDeathFXs.Num();

		for (int32 Index = 0; Index < Count; Index++)
		{
			FAngelDeathEffect* AngelDeathFX = Data->AngelDeathFXs[Index].Get(InViewType);

			if (AngelDeathFX->ParticleSystem)
			{
				if (AShooterEmitter* Emitter = AllocateAndActivateEmitter(AngelDeathFX->ParticleSystem, AngelDeathFX->LifeTime))
				{
					Emitter->AttachToActor(MeshActor, FAttachmentTransformRules::SnapToTargetIncludingScale, AngelDeathFX->BoneAttachPoint);
				}
			}
		}
	}
	return MeshActor;
}

ASkeletalMeshActor* AShooterGameState::AllocateAngelDeathMesh(TEnumAsByte<EViewType::Type> InViewType, FCharacterInfo& InCharacterInfo, int32& OutIndex, float Time, bool AllocateEffects)
{
	ASkeletalMeshActor* OutHat = NULL;
	int32 OutHatIndex = INDEX_NONE;
	TArray<ASkeletalMeshActor*> OutAttachments;
	TArray<int32> OutAttachmentIndices;
	return AllocateAngelDeathMesh(InViewType, InCharacterInfo, OutIndex, OutHat, OutHatIndex, OutAttachments, OutAttachmentIndices, Time, AllocateEffects);
}

ASkeletalMeshActor* AShooterGameState::AllocateAngelDeathMesh(TEnumAsByte<EViewType::Type> InViewType, FCharacterInfo& InCharacterInfo, float Time, bool AllocateEffects)
{
	int32 OutIndex = INDEX_NONE;
	return AllocateAngelDeathMesh(InViewType, InCharacterInfo, OutIndex, Time, AllocateEffects);
}

ASkeletalMeshActor* AShooterGameState::AllocateAttachAndPlayAngelDeathMesh(TEnumAsByte<EViewType::Type> InViewType, FCharacterInfo& InCharacterInfo, AActor* InParent, float PlayRate, float Time)
{
	ASkeletalMeshActor* MeshActor = AllocateAngelDeathMesh(InViewType, InCharacterInfo, Time);

	if (MeshActor)
	{
		MeshActor->AttachToActor(InParent, FAttachmentTransformRules::KeepRelativeTransform, NAME_None);

		AShooterCharacterData* Data = GetCharacterData(InCharacterInfo.CharacterDataShortCode);
		AShooterHatData* HatData	= GetHat(InCharacterInfo.HatShortCode);
		Data->InitAndPlayAngelDeath(InViewType, MeshActor, PlayRate, !HatData->IsDefault);
	}
	return MeshActor;
}

// Tank

ASkeletalMeshActor* AShooterGameState::AllocateTankMesh(TEnumAsByte<EViewType::Type> InViewType, FCharacterInfo& InCharacterInfo, float Time)
{
	ASkeletalMeshActor* MeshActor = AllocateSkeletalMesh(Time);

	if (MeshActor)
	{
		AShooterTankData* Data					     = GetTankData(InCharacterInfo.TankDataShortCode);
		AShooterTankMaterialSkin* Skin				 = GetTankMaterialSkin(InCharacterInfo.TankMaterialSkinShortCode);
		USkeletalMesh* Mesh							 = InViewType == EViewType::ThirdPerson ? Data->Mesh3P : Data->Mesh1P;
		TArray<UMaterialInstanceConstant*>& Materials = InViewType == EViewType::ThirdPerson ? Skin->Materials3P : Skin->Materials1P;

		UShooterStatics::SetSkeletalMeshAndMaterials(MeshActor->GetSkeletalMeshComponent(), Mesh, Materials);
	}
	return MeshActor;
}

ASkeletalMeshActor* AShooterGameState::AllocateAndAttachTankMesh(TEnumAsByte<EViewType::Type> InViewType, FCharacterInfo& InCharacterInfo, USceneComponent* InParent, float Time)
{
	ASkeletalMeshActor* MeshActor = AllocateTankMesh(InViewType, InCharacterInfo, Time);

	if (MeshActor)
	{
		MeshActor->AttachToComponent(InParent, FAttachmentTransformRules::KeepRelativeTransform);
	}
	return MeshActor;
}

ASkeletalMeshActor* AShooterGameState::AllocateAndAttachTankMesh(TEnumAsByte<EViewType::Type> InViewType, FCharacterInfo& InCharacterInfo, AActor* InParent, float Time)
{
	return AllocateAndAttachTankMesh(InViewType, InCharacterInfo, InParent->GetRootComponent(), Time);
}

ASkeletalMeshActor* AShooterGameState::AllocateAndAttachTankHatchMesh(FCharacterInfo& InCharacterInfo, ASkeletalMeshActor* InParent, float Time)
{
	ASkeletalMeshActor* MeshActor = AllocateSkeletalMesh(Time);

	if (MeshActor)
	{
		AShooterTankData* Data = GetTankData(InCharacterInfo.TankDataShortCode);

		Data->SetAndAttachHatch(InParent, MeshActor);
	}
	return MeshActor;
}

void AShooterGameState::AllocateAndHandleCharacterDeath(AShooterCharacter* InCharacter)
{
	AShooterCharacterData* CharacterData = InCharacter->Last_CurrentCharacterData;

	if (!CharacterData)
		return;

	FName CharacterMeshSkinShortCode	 = InCharacter->Last_CharacterInfo.CharacterMeshSkinShortCode;
	FName CharacterMaterialSkinShortCode = InCharacter->Last_CharacterInfo.CharacterMaterialSkinShortCode; 
	AShooterWeaponData* WeaponData		 = GetWeaponData(InCharacter->Last_CharacterInfo.WeaponDataShortCode);
	
	FHitInfo_Killed & LastHitInfo_Killed = InCharacter->LastHitInfo_Killed;
	
	FVector Location  = InCharacter->Last_Location_Mesh3P;
	FRotator Rotation = InCharacter->Last_Rotation_Mesh3P;
	
	const bool ForceImmediateRagdoll = LastHitInfo_Killed.ForceRagdollDeath;

	if (!ForceImmediateRagdoll && !InCharacter->OnDeathShowMesh3P)
		return;

	// Allocate the Skeletal Mesh Actor for Character
	USkeletalMesh* MeshTemplate   = GetCharacterMesh(ECharacterSkin::Mesh3P, InCharacter->Last_CharacterInfo);
	const int32 AllocatedIndex    = GetAllocatedSkeletalMeshIndex();
	ASkeletalMeshActor* MeshActor = AllocatedIndex > INDEX_NONE ? SkeletalMeshPool[AllocatedIndex] : NULL;

	if (!MeshActor)
	{
		UE_LOG(LogShooter, Warning, TEXT("Failed Handling Character Death Anim / Ragdoll: Not enough actors in Skeletal Mesh Pool"));
		return;
	}

	USkeletalMeshComponent* Mesh = MeshActor->GetSkeletalMeshComponent();

	Mesh->SetAnimInstanceClass(NULL);
	Mesh->SetSkeletalMesh(NULL);

	MeshActor->SetActorHiddenInGame(false);
	MeshActor->SetActorTickEnabled(true);

	Mesh->SetComponentTickEnabled(true);
	Mesh->SetAnimInstanceClass(CharacterData->EmptyAnimBlueprints.Blueprint3P);
	Mesh->SetSkeletalMesh(MeshTemplate);

	const int32 Count = MeshTemplate->Materials.Num();

	for (int32 Index = 0; Index < Count; Index++)
	{
		Mesh->SetMaterial(Index, GetCharacterDeathMaterial(Index, InCharacter->Last_CharacterInfo.Faction, CharacterMaterialSkinShortCode));
	}

	const float DeathTime = 5.0f;

	SkeletalMeshTimes[AllocatedIndex]		  = DeathTime;
	SkeletalMeshStartTimes[AllocatedIndex]    = GetWorld()->TimeSeconds;
	SkeletalMeshAvailableList[AllocatedIndex] = false;

	MeshActor->TeleportTo(Location, Rotation, false, true);

	// Setup profile for Ragdoll
	static FName CollisionProfileName(TEXT("Ragdoll"));
	Mesh->SetCollisionProfileName(CollisionProfileName);

	Mesh->SetCollisionObjectType(ECC_Pawn);
	Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Mesh->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
	Mesh->SetCollisionResponseToChannel(COLLISION_PROJECTILE, ECR_Block);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	// Setup the "Dissolve"
	/*
	const float DissolveFXDelayTime = 1.5f;
	FTimerHandle PassSkeletalMeshAnimateDissolve;
	GetWorld()->GetTimerManager().SetTimer(PassSkeletalMeshAnimateDissolve, this, &AShooterGameState::PassSkeletalMeshAnimateDissolve, DissolveFXDelayTime, false);
	*/

	if (!ForceImmediateRagdoll && InCharacter->OnDeathShowMesh3P)
	{
		UAnimMontage *DeathAnim = CharacterData->DeathAnims.Anim3P;

		static FName HideWeapon("HideWeapon");
		const float HideWeaponTime = UShooterStatics::GetLastAnimNotifyTriggerTime(DeathAnim, HideWeapon);

		Mesh->GetAnimInstance()->Montage_Play(DeathAnim);

		// Allocate the Skeletal Mesh Actor for Character
		if (HideWeaponTime > 0.0f)
		{
			ASkeletalMeshActor* WeaponActor = AllocateAndAttachWeaponMesh(EViewType::ThirdPerson, CharacterData, WeaponData, NULL, Mesh, HideWeaponTime);
			
			WeaponActor->GetSkeletalMeshComponent()->SetAnimInstanceClass(WeaponData->AnimBlueprints.Blueprint3P);
			WeaponActor->GetSkeletalMeshComponent()->GetAnimInstance()->Montage_Play(WeaponData->DeathAnims.Anim3P);
		}

		// start ragdoll with anim notify?
		static FName StartRagdoll("StartRagdoll");
		const float RagdollTime = UShooterStatics::GetLastAnimNotifyTriggerTime(DeathAnim, StartRagdoll);

		if (RagdollTime > 0.0f)
		{
			if (RagdollTime < DeathTime)
			{
				SkeletalMeshBlendToRagdollList[AllocatedIndex] = true;

				Coroutine Function		    = &AShooterGameState::HandleSkeletalMeshRagdoll;
				CoroutineStopCondition Stop = &UShooterStatics::CoroutineStopCondition_CheckActor;

				FRoutine* R = CoroutineScheduler->Allocate(Function, Stop, SkeletalMeshPool[AllocatedIndex], true, false);

				R->timers[0] = CoroutineScheduler->GetWorld()->TimeSeconds;
				R->ints[0]   = AllocatedIndex;
				R->delay     = RagdollTime;

				CoroutineScheduler->StartRoutine(R);
			}
			else
			{
				UE_LOG(LogShooter, Warning, TEXT("StartRagdoll(%f) >= DeathTimeCharacter(%f), skipping ragdoll"), RagdollTime, DeathTime);
			}
		}
	}
	// not on ground
	else
	{
		SkeletalMeshSetRagdollPhysics(AllocatedIndex);

		// died from radial explosion
		if (LastHitInfo_Killed.DamageType != EDamageType::RanOver)
		{
			// send the 3p mesh flying upwards.
			// TODO: Does design want it to be location/direction aware?
			static float upMagnitude = 10000.0f;

			if (LastHitInfo_Killed.ForceRagdollDeath)
			{
				FVector UpImpulse(0.0f, 0.0f, upMagnitude);
				Mesh->AddImpulse(UpImpulse, NAME_None, true);
			}
		}
		// died from tank burst collision
		else
		{
			FVector impulseDirection = Location- LastHitInfo_Killed.HitLocation;
			impulseDirection.Z	     = 0.0f;
			impulseDirection.Normalize();

			static float awayMagnitude = 17500.0f;
			static float upMagnitude   = 2500.0f;

			impulseDirection *= awayMagnitude;
			impulseDirection += FVector(0.0f, 0.0f, upMagnitude);
			Mesh->AddImpulse(impulseDirection, NAME_None, true);
		}
	}
}

void AShooterGameState::SkeletalMeshSetRagdollPhysics(int32 Index)
{
	USkeletalMeshComponent* Mesh = SkeletalMeshPool[Index]->GetSkeletalMeshComponent();

	bool DoRagdoll = false;

	if (!Mesh->GetPhysicsAsset())
	{
		DoRagdoll = false;
	}
	else
	{
		// ClothTickFunction.bStartWithTickEnabled is set to false as optimization. Only needed for ragdoll
		// so when we go into ragdoll, we need to manually renable ticking
		Mesh->ClothTickFunction.SetTickFunctionEnable(true);

		// initialize physics/etc
		Mesh->SetAllBodiesSimulatePhysics(true);
		Mesh->SetSimulatePhysics(true);
		Mesh->WakeAllRigidBodies();
		Mesh->bBlendPhysics = true;

		DoRagdoll = true;
	}

	if (DoRagdoll)
	{
		UAnimInstance* AnimInstance = Mesh->GetAnimInstance();

		if (AnimInstance)
			Mesh->SetAnimInstanceClass(NULL);
		else
			Mesh->Stop();
	}
}

// Death Dissolve Effects
/*
void AShooterGameState::PassSkeletalMeshAnimateDissolve()
{
	SkeletalMeshAnimateDissolve(0.2f, 1.25f, 25);
}
*/

/* To animate dissolve of an object
@ float MinTime   : Minimum time interval between function stacks
@ float MaxTime   : Maximum time interval between function stacks
@ uint32 MaxCount : Maximum count of function stacks
*/
/*
void AShooterGameState::SkeletalMeshAnimateDissolve(int32 Index, const float MinTime, const float MaxTime, int32 MaxCount)
{
	int32 Count = 0;

	float Stepsize;

	while (Count < MaxCount && SkeletalMeshAvailableList[Index])
	{
		Stepsize = UShooterStatics::MapValueNonLinear(FVector2D(0, MaxCount), FVector2D(MinTime, MaxTime), Count, EGraphType::EaseIn);

		FTimerHandle SkeletalMeshOnTimerHandle; // Handle have to be created every time it sets timer to be stacked
		Count += 1;
		GetWorld()->GetTimerManager().SetTimer(SkeletalMeshOnTimerHandle, this, &AShooterGameState::SkeletalMeshVisibilityOn, Stepsize, false);

		Stepsize = UShooterStatics::MapValueNonLinear(FVector2D(0, MaxCount), FVector2D(MinTime, MaxTime), Count, EGraphType::EaseIn);

		FTimerHandle SkeletalMeshOffTimerHandle; // Handle have to be created every time it sets timer to be stacked
		Count += 1;
		GetWorld()->GetTimerManager().SetTimer(SkeletalMeshOffTimerHandle, this, &AShooterGameState::SkeletalaMeshVisibilityOff, Stepsize, false);
	}
}

void AShooterGameState::SkeletalMeshVisibilityOn(int32 Index)
{
	if (SkeletalMeshAvailableList[Index])
		SkeletalMeshPool[Index]->GetSkeletalMeshComponent()->SetVisibility(true, true);
}

void AShooterGameState::SkeletalaMeshVisibilityOff(int32 Index)
{
	if (SkeletalMeshAvailableList[Index])
		SkeletalMeshPool[Index]->GetSkeletalMeshComponent()->SetVisibility(false, true);
}
*/

void AShooterGameState::DeAllocateSkeletalMesh(ASkeletalMeshActor* Mesh)
{
	if (!Mesh)
		return;

	int32 skelMeshPoolIndex = SkeletalMeshPoolIndexMapping.FindChecked(Mesh);
	DeAllocateSkeletalMesh(skelMeshPoolIndex);
}

void AShooterGameState::DeAllocateSkeletalMesh(int32 Index)
{
	check(SkeletalMeshPool.IsValidIndex(Index));

	ASkeletalMeshActor* skelMeshActor = SkeletalMeshPool[Index];
	check(skelMeshActor);

	UAnimInstance* AnimInstance = skelMeshActor->GetSkeletalMeshComponent()->GetAnimInstance();

	if (AnimInstance)
	{
		AnimInstance->Montage_Stop(0.0f);
		skelMeshActor->GetSkeletalMeshComponent()->SetAnimInstanceClass(NULL);
	}
	else
	{
		skelMeshActor->GetSkeletalMeshComponent()->Stop();
	}

	skelMeshActor->GetSkeletalMeshComponent()->SetCastShadow(false);
	skelMeshActor->GetSkeletalMeshComponent()->bCastDynamicShadow = false;

	skelMeshActor->DetachRootComponentFromParent();
	
	if (skelMeshActor->GetRootComponent())
	{
		const TArray<USceneComponent*>& AttachChildren = skelMeshActor->GetRootComponent()->GetAttachChildren();
		const int32 AttachedCount					   = AttachChildren.Num();

		for (int32 attachIndex = AttachedCount - 1; attachIndex >= 0; attachIndex--)
		{
			USceneComponent* Child = AttachChildren[attachIndex];

			if (Child &&
				Child != skelMeshActor->GetSkeletalMeshComponent() &&
				skelMeshActor != Cast<ASkeletalMeshActor>(Child->GetOuter()))
			{
				if (Child->GetAttachParent() != skelMeshActor->GetRootComponent())
				{
					Child->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);

					ASkeletalMeshActor* Mesh = Cast<ASkeletalMeshActor>(Child->GetOuter());

					if (Mesh)
						DeAllocateSkeletalMesh(Mesh);
				}

				AShooterEmitter* emitter = Cast<AShooterEmitter>(Child->GetOuter());
				if (emitter)
					emitter->DeallocateFromPool();
			}
		}
	}

	const TArray<USceneComponent*>& AttachChildren = skelMeshActor->GetRootComponent()->GetAttachChildren();
	const int32 Count							   = AttachChildren.Num();

	for (int32 I = 0; I < Count; I++)
	{
		USceneComponent* Child = AttachChildren[I];

		if (Child &&
			Child->GetAttachParent() != skelMeshActor->GetSkeletalMeshComponent() &&
			skelMeshActor != Cast<ASkeletalMeshActor>(Child->GetOuter()))
		{
			Child->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);

			ASkeletalMeshActor* Mesh = Cast<ASkeletalMeshActor>(Child->GetOuter());

			if (Mesh)
			{
				DeAllocateSkeletalMesh(Mesh);
			}
			else
			{
				AShooterEmitter* emitter = Cast<AShooterEmitter>(Child->GetOuter());
				if (emitter)
					emitter->DeallocateFromPool();
			}
		}
	}

	skelMeshActor->SetActorRelativeLocation(FVector::ZeroVector);
	skelMeshActor->SetActorRelativeRotation(FRotator::ZeroRotator);
	skelMeshActor->SetActorRelativeScale3D(FVector(1.0f));
	skelMeshActor->SetActorScale3D(FVector(1.0f));
	skelMeshActor->SetActorHiddenInGame(true);
	skelMeshActor->SetActorTickEnabled(false);

	skelMeshActor->GetSkeletalMeshComponent()->ClothTickFunction.SetTickFunctionEnable(true);
	skelMeshActor->GetSkeletalMeshComponent()->SetComponentTickEnabled(false);
	skelMeshActor->GetSkeletalMeshComponent()->SetAllBodiesSimulatePhysics(false);
	skelMeshActor->GetSkeletalMeshComponent()->SetSimulatePhysics(false);
	skelMeshActor->GetSkeletalMeshComponent()->bBlendPhysics = false;

	skelMeshActor->GetSkeletalMeshComponent()->bGenerateOverlapEvents = false;

	skelMeshActor->GetSkeletalMeshComponent()->SetCollisionObjectType(ECC_Pawn);
	skelMeshActor->GetSkeletalMeshComponent()->SetCollisionResponseToAllChannels(ECR_Ignore);
	skelMeshActor->GetSkeletalMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	skelMeshActor->GetSkeletalMeshComponent()->SetRenderCustomDepth(false);
	skelMeshActor->GetSkeletalMeshComponent()->SetCustomDepthStencilValue(0);

	skelMeshActor->GetSkeletalMeshComponent()->SetMasterPoseComponent(nullptr);

	UShooterStatics::ClearOverrideMaterials(skelMeshActor->GetSkeletalMeshComponent());
	skelMeshActor->GetSkeletalMeshComponent()->SetSkeletalMesh(nullptr);
	skelMeshActor->SetOwner(NULL);

	SkeletalMeshTimes[Index]			  = 0.0f;
	SkeletalMeshAvailableList[Index]	  = true;
	SkeletalMeshHasOwnerList[Index]		  = false;
	SkeletalMeshDrawDistances[Index]	  = 3000.0f * 3000.0f;
	SkeletalMeshBlendToRagdollList[Index] = false;
	AngelDeathDataList[Index]			  = NULL;
	AngelDeathTypes[Index]				  = EAngelDeathType::EAngelDeathType_MAX;
}

int32 AShooterGameState::GetAllocatedSkeletalMeshIndex()
{
	const int32 Count		  = SkeletalMeshPool.Num();
	const int32 PreviousIndex = SkeletalMeshPoolIndex;

	if (SkeletalMeshPoolIndex >= Count)
		SkeletalMeshPoolIndex = 0;

	// Check from SkeletalMeshPoolIndex to Count
	for (int32 Index = SkeletalMeshPoolIndex; Index < Count; ++Index)
	{
		SkeletalMeshPoolIndex = Index + 1;

		if (SkeletalMeshAvailableList[Index])
			return Index;
	}

	// Check from 0 to PreviousIndex ( where SkeletalMeshPoolIndex started )
	if (PreviousIndex > 0)
	{
		SkeletalMeshPoolIndex = 0;

		for (int32 Index = SkeletalMeshPoolIndex; Index < PreviousIndex; ++Index)
		{
			SkeletalMeshPoolIndex = Index + 1;

			if (SkeletalMeshAvailableList[Index])
				return Index;
		}
	}
	UE_LOG(LogShooter, Warning, TEXT("GetAllocatedSkeletalMeshIndex: All Skeletal Meshes from the pool have been allocated"));
	return INDEX_NONE;
}

void AShooterGameState::SetDrawDistanceSkeletalMesh(ASkeletalMeshActor* Mesh, float DrawDistance)
{
	const int32 Count = SkeletalMeshPool.Num();

	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (Mesh == SkeletalMeshPool[Index])
		{
			SkeletalMeshDrawDistances[Index] = DrawDistance * DrawDistance;
			return;
		}
	}
}

void AShooterGameState::SetDrawDistanceStaticMesh(AStaticMeshActor* Mesh, float DrawDistance)
{
	const int32 Count = MeshPool.Num();

	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (Mesh == MeshPool[Index])
		{
			MeshDrawDistances[Index] = DrawDistance * DrawDistance;
			return;
		}
	}
}

void AShooterGameState::SetSkeletalMeshTime(ASkeletalMeshActor* InMesh, float Time, bool UpdateStartTime)
{
	const int32 Count = SkeletalMeshPool.Num();

	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (InMesh == SkeletalMeshPool[Index])
		{
			SkeletalMeshTimes[Index] = Time;

			if (UpdateStartTime)
			{
				SkeletalMeshStartTimes[Index] = GetWorld()->TimeSeconds;
			}
			return;
		}
	}
}

void AShooterGameState::SetSkeletalMeshTime(int32 Index, float Time, bool UpdateStartTime)
{
	if (UpdateStartTime)
	{
		SkeletalMeshStartTimes[Index] = GetWorld()->TimeSeconds;
	}
	SkeletalMeshTimes[Index] = Time;
}

void AShooterGameState::OnTick_HandleSkeletalMeshPool(float DeltaSeconds)
{
	SCOPE_CYCLE_COUNTER(STAT_HandleSkeletalMeshPool);

	const int32 Count = SkeletalMeshPool.Num();

	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (!SkeletalMeshAvailableList[Index] &&
			!AngelDeathDataList[Index] &&
			SkeletalMeshTimes[Index] > 0.0f)
		{
			const float DistanceSq = UShooterStatics::GetSquaredDistanceToLocalControllerEye(GetWorld(), SkeletalMeshPool[Index]->GetActorLocation());
			const bool IsVisible   = DistanceSq <= SkeletalMeshDrawDistances[Index];

			if (SkeletalMeshPool[Index]->bHidden != !IsVisible)
			{
				SkeletalMeshPool[Index]->SetActorHiddenInGame(!IsVisible);
				SkeletalMeshPool[Index]->GetSkeletalMeshComponent()->SetComponentTickEnabled(!IsVisible);
			}

			if (SkeletalMeshHasOwnerList[Index])
			{
				AShooterCharacter* OwningPawn = Cast<AShooterCharacter>(SkeletalMeshPool[Index]->GetOwner());
				
				if (!OwningPawn ||
					!OwningPawn->IsActiveAndFullyReplicated ||
					!OwningPawn->IsAlive())
				{
					DeAllocateSkeletalMesh(Index);
					continue;
				}
			}

			if (GetWorld()->TimeSeconds - SkeletalMeshStartTimes[Index] > SkeletalMeshTimes[Index])
				DeAllocateSkeletalMesh(Index);
		}
	}
	OnTick_HandleAngelDeath(DeltaSeconds);
}

void AShooterGameState::AllocateAndStartAngelDeath(int32 TeamIndex, FCharacterInfo* InCharacterInfo, FVector Location, FRotator Rotation, TEnumAsByte<EAngelDeathType::Type> AngelDeathType)
{
	if (InCharacterInfo->CharacterDataShortCode == INVALID_SHORT_CODE)
		return;

	if (InCharacterInfo->CharacterMeshSkinShortCode == INVALID_SHORT_CODE)
		return;

	if (InCharacterInfo->CharacterMaterialSkinShortCode == INVALID_SHORT_CODE)
		return;

	AShooterCharacterData* Data = GetCharacterData(InCharacterInfo->Faction, InCharacterInfo->CharacterDataShortCode);

	const bool IsFirstPerson = AngelDeathType == EAngelDeathType::FirstPerson;
	Data->AngelDeathDuration    = 10.0f; // set global angel death duration
	Data->AngelDeathDeltaHeight = Data->AngelDeathDuration * Data->AngelDeathSpeed;

	int32 AllocatedIndex	 = INDEX_NONE;
	ASkeletalMeshActor* Mesh = AllocateAngelDeathMesh(IsFirstPerson ? EViewType::FirstPerson : EViewType::ThirdPerson, *InCharacterInfo, AllocatedIndex, Data->AngelDeathDuration * 1.25f, true);
	
	if (!Mesh)
		return;

	AngelDeathDataList[AllocatedIndex]		  = Data;
	AngelDeathStartTimes[AllocatedIndex]	  = GetWorld()->TimeSeconds;
	AngelDeathStartLocations[AllocatedIndex]  = Location;
	AngelDeathTypes[AllocatedIndex]			  = AngelDeathType;

	Mesh->TeleportTo(Location, Rotation, false, true);

	AShooterHatData* HatData = GetHat(InCharacterInfo->HatShortCode);

	Data->InitAndPlayAngelDeath(IsFirstPerson ? EViewType::FirstPerson : EViewType::ThirdPerson, Mesh, 1.0f, !HatData->IsDefault);
	
	TEnumAsByte<EFaction::Type> Faction = InCharacterInfo->Faction;

	// Allocate Cloud Effects
	if (!IsFirstPerson)
	{
		if (Data->CloudEffectAlly && Data->CloudEffectEnemy)
		{
			AShooterEmitter* Emitter				 = NULL;
			APlayerController* LocalPlayerController = UShooterStatics::GetMachineClientController(GetWorld());
			AShooterCharacter* ViewingPawn = LocalPlayerController ? Cast<AShooterCharacter>(LocalPlayerController->AcknowledgedPawn) : nullptr;

			if (TeamIndex != INDEX_NONE &&
				(ViewingPawn &&
				 ViewingPawn->PlayerState &&
				 Cast<AShooterPlayerState>(ViewingPawn->PlayerState)->GetFaction() == Faction))
			{
				Emitter = AllocateAndActivateEmitter(Data->CloudEffectAlly, 20.0f);
			}
			else
			{
				Emitter = AllocateAndActivateEmitter(Data->CloudEffectEnemy, 20.0f);
			}

			if (Emitter)
			{
				if (Emitter->GetParticleSystemComponent())
				{
					//Emitter->GetParticleSystemComponent()->CustomTimeDilation = 12.0f;
					//Emitter->GetParticleSystemComponent()->EmitterDelay = 5.0f;
					//Emitter->GetParticleSystemComponent()->SetRelativeScale3D(FVector(4.0f));
				}
				Emitter->DrawDistance = 20000.0f;

				float ParticleRadius = 350.0f;
				Emitter->TeleportTo(Location + FVector(0.0f, 0.0f, Data->AngelDeathDeltaHeight - ParticleRadius), FRotator::ZeroRotator);
			}
		}
	}
}

void AShooterGameState::OnAngelCameraUpdate(float inZOffset, FRotator inRotation)
{
	int32 Count = AngelDeathDataList.Num();

	for (int32 Index = 0; Index < Count; Index++)
	{
		if (AngelDeathDataList[Index] && AngelDeathTypes[Index] == EAngelDeathType::FirstPerson)
		{
			ASkeletalMeshActor* angel = SkeletalMeshPool[Index];
			FRotator lastRotation = angel->GetActorRotation();
			angel->SetActorLocation(AngelDeathStartLocations[Index] + FVector(0.0f, 0.0f, inZOffset));
			//TODO: Implement look rotation behavior for skeletal mesh
			//angel->SetActorRotation(FRotator(lastRotation.Pitch, inRotation.Yaw - 90.0f, lastRotation.Roll));
		}
	}
}

void AShooterGameState::OnTick_HandleAngelDeath(float DeltaSeconds)
{
	int32 Count = AngelDeathDataList.Num();

	for (int32 Index = 0; Index < Count; Index++)
	{
		if (AngelDeathDataList[Index])
		{
			switch (AngelDeathTypes[Index])
			{
			case EAngelDeathType::FirstPerson:
				if (GetWorld()->TimeSeconds - SkeletalMeshStartTimes[Index] > SkeletalMeshTimes[Index])
				{
					DeAllocateSkeletalMesh(Index);
				}
				break;
			case EAngelDeathType::ThirdPerson:
				// DeAllocate when we reach the top
				float elapsed = GetWorld()->TimeSeconds - AngelDeathStartTimes[Index];
				if (elapsed > AngelDeathDataList[Index]->AngelDeathDuration)
				{
					// deallocate any child skel mesh that were attached
					TArray<AActor*> AttachedActors;
					SkeletalMeshPool[Index]->GetAttachedActors(AttachedActors);

					int32 AttachedCount = AttachedActors.Num();

					for (int32 attachedIndex = AttachedCount - 1; attachedIndex >= 0; --attachedIndex)
					{
						ASkeletalMeshActor* skelMeshActor = Cast<ASkeletalMeshActor>(AttachedActors[attachedIndex]);

						if (skelMeshActor)
						{
							DeAllocateSkeletalMesh(skelMeshActor);
						}
					}

					// now deallocate the parent mesh
					DeAllocateSkeletalMesh(Index);
				}
				else
				{
						
					float startingz = AngelDeathStartLocations[Index].Z;
					FVector Location = SkeletalMeshPool[Index]->GetActorLocation();
					float currentz = Location.Z;
					float totalheight = AngelDeathDataList[Index]->AngelDeathDeltaHeight;
					
					Location.Z += DeltaSeconds * AngelDeathDataList[Index]->AngelDeathSpeed;
					FVector Scale; // = SkeletalMeshPool[Index]->GetActorScale();
					float startScaleElapsed = AngelDeathDataList[Index]->AngelDeathDuration * 0.8f;
					float alpha = FMath::Max(elapsed - startScaleElapsed, 0.0f);
					alpha = FMath::Min(alpha, 1.0f);
					Scale = FVector(FMath::Lerp(1.0f, 0.01f, alpha));
					//Scale = FVector(FMath::Max<float>(0.0f, Scale.X));

					SkeletalMeshPool[Index]->SetActorLocation(Location);
					SkeletalMeshPool[Index]->SetActorScale3D(Scale);
				}
				break;
			}
		}
	}
}

#pragma endregion Skeletal Mesh

// Text
#pragma region

ATextRenderActor* AShooterGameState::AllocateText(AShooterCharacter* InOwner, FString InText, TEnumAsByte<ETextType::Type> TextType, float Time, FVector Location)
{
	ATextRenderActor* Text = NULL;

	const int32 Count = TextPool.Num();

	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (TextTimes[Index] == 0.0f)
		{
			Text = TextPool[Index];

			Text->SetActorHiddenInGame(false);
			Text->SetActorTickEnabled(true);
			Text->SetOwner(InOwner);

			if (InOwner)
			{
				TextHasOwnerList[Index] = true;
			}

			Text->GetTextRender()->SetComponentTickEnabled(true);
			Text->SetActorLocation(Location);

			switch (TextType)
			{
				case ETextType::HitPlayer:
					Text->GetTextRender()->SetTextMaterial(TextHitPlayerMIC);
					break;
				case ETextType::KilledPlayer:
					Text->GetTextRender()->SetTextMaterial(TextKilledPlayerMIC);
					break;
				case ETextType::RespawnVictim:
					Text->GetTextRender()->SetTextMaterial(TextRespawnVictimMIC);
					break;
				case ETextType::RespawnKiller:
					Text->GetTextRender()->SetTextMaterial(TextRespawnKillerMIC);
					break;
			}

			Text->GetTextRender()->SetText(FText::FromString(InText));

			TextTimes[Index]	  = Time;
			TextStartTimes[Index] = GetWorld()->TimeSeconds;
			TextTypes[Index]	  = TextType;

			return TextPool[Index];
		}
	}
	return Text;
}

ATextRenderActor* AShooterGameState::AllocateText(FString InText, TEnumAsByte<ETextType::Type> TextType, float Time)
{
	return AllocateText(NULL, InText, TextType, Time, FVector::ZeroVector);
}

ATextRenderActor* AShooterGameState::AllocateAndAttachText(FString InText, TEnumAsByte<ETextType::Type> TextType, USceneComponent* InParent, float Time)
{
	ATextRenderActor* TextActor = AllocateText(NULL, InText, TextType, Time, FVector::ZeroVector);

	if (TextActor)
	{
		TextActor->AttachToComponent(InParent, FAttachmentTransformRules::KeepRelativeTransform);
	}
	return TextActor;
}

ATextRenderActor* AShooterGameState::AllocateAndAttachText(FString InText, TEnumAsByte<ETextType::Type> TextType, AActor* InParent, float Time)
{
	return AllocateAndAttachText(InText, TextType, InParent->GetRootComponent(), Time);
}

void AShooterGameState::DeAllocateText(ATextRenderActor* Text)
{
	Text->GetTextRender()->SetComponentTickEnabled(false);
	Text->SetActorHiddenInGame(true);
	Text->SetActorTickEnabled(false);
	Text->SetOwner(NULL);
}

void AShooterGameState::OnTick_HandleText()
{
	SCOPE_CYCLE_COUNTER(STAT_HandleText);

	const int32 Count = TextPool.Num();

	for (int32 Index = 0; Index < Count; ++Index)
	{
		if ((TextHasOwnerList[Index] && !TextPool[Index]->GetOwner()) ||
			(TextTimes[Index] > 0.0f &&
			 GetWorld()->TimeSeconds - TextStartTimes[Index] > TextTimes[Index]))
		{
			DeAllocateText(TextPool[Index]);

			TextTimes[Index] = 0.0f;
			TextTypes[Index] = ETextType::ETextType_MAX;
		}

		if (TextTimes[Index] > 0.0f &&
			GetWorld()->TimeSeconds - TextStartTimes[Index] < TextTimes[Index])
		{
			AShooterPlayerController* MachineClientController = UShooterStatics::GetMachineClientController(GetWorld());

			FVector Origin    = FVector::ZeroVector;
			FRotator Rotation = FRotator::ZeroRotator;

			MachineClientController->GetPlayerViewPoint(Origin, Rotation);

			const float Distance = FVector::Dist(TextPool[Index]->GetActorLocation(), Origin);
			
			float Scale			 = 0.1f;
			bool AlignTextToView = false;

			switch (TextTypes[Index])
			{
				case ETextType::KilledPlayer:
				case ETextType::KilledTank:
					Scale			= 0.1f;
					AlignTextToView = true;
					break;
				case ETextType::HitPlayer:
				case ETextType::HitTank:
					Scale			= 0.05f;
					AlignTextToView = true;
					break;
			}

			if (AlignTextToView)
			{
				TextPool[Index]->GetTextRender()->SetWorldSize(Scale * Distance);
				TextPool[Index]->SetActorRotation(FRotator(-1.0f * Rotation.Pitch, Rotation.Yaw + 180.0f, 0.0f));
			}
		}
	}
}

#pragma endregion Text

#if !UE_BUILD_SHIPPING

void AShooterGameState::ToggleNameTags()
{
	bEnableNameTags = !bEnableNameTags;
}
#endif // #if !UE_BUILD_SHIPPING

void AShooterGameState::OnRep_ToggledNameTags()
{
#if !UE_BUILD_SHIPPING
	//hack: toggle prior to ToggleNameTags since that will toggle again causing it to be correct
	bEnableNameTags = !bEnableNameTags;
	ToggleNameTags();
#endif // #if !UE_BUILD_SHIPPING
}

// Loading Assets - TODO: Move to FrontendManager
#pragma region

void AShooterGameState::AddLoadedAsset(TEnumAsByte<EAssetType::Type> AssetType, FName ShortCode, AActor* Actor)
{
	// Ally Character
	if (AssetType == EAssetType::Ally_Characters)
	{
		if (LoadedAllyCharacters.Find(Cast<AShooterCharacterData>(Actor)) == INDEX_NONE)
		{
			LoadedAllyCharacterShortCodes.Add(ShortCode);
			LoadedAllyCharacters.Add(Cast<AShooterCharacterData>(Actor));
		}
	}
	// Ally Skins
	if (AssetType == EAssetType::Ally_Skins)
	{
		// Mesh Skin
		if (Cast<AShooterCharacterMeshSkin>(Actor) &&
			LoadedAllyCharacterMeshSkins.Find(Cast<AShooterCharacterMeshSkin>(Actor)) == INDEX_NONE)
		{
			LoadedAllyCharacterMeshSkinShortCodes.Add(ShortCode);
			LoadedAllyCharacterMeshSkins.Add(Cast<AShooterCharacterMeshSkin>(Actor));
		}

		// Material Skin
		if (Cast<AShooterCharacterMaterialSkin>(Actor) &&
			LoadedAllyCharacterMaterialSkins.Find(Cast<AShooterCharacterMaterialSkin>(Actor)) == INDEX_NONE)
		{
			LoadedAllyCharacterMaterialSkinShortCodes.Add(ShortCode);
			LoadedAllyCharacterMaterialSkins.Add(Cast<AShooterCharacterMaterialSkin>(Actor));
		}
	}
	// Axis Characters
	if (AssetType == EAssetType::Axis_Characters)
	{
		if (LoadedAxisCharacters.Find(Cast<AShooterCharacterData>(Actor)) == INDEX_NONE)
		{
			LoadedAxisCharacterShortCodes.Add(ShortCode);
			LoadedAxisCharacters.Add(Cast<AShooterCharacterData>(Actor));
		}
	}
	// Axis Skins
	if (AssetType == EAssetType::Axis_Skins)
	{
		// Mesh Skin
		if (Cast<AShooterCharacterMeshSkin>(Actor) &&
			LoadedAxisCharacterMeshSkins.Find(Cast<AShooterCharacterMeshSkin>(Actor)) == INDEX_NONE)
		{
			LoadedAxisCharacterMeshSkinShortCodes.Add(ShortCode);
			LoadedAxisCharacterMeshSkins.Add(Cast<AShooterCharacterMeshSkin>(Actor));
		}

		// Material Skin
		if (Cast<AShooterCharacterMaterialSkin>(Actor) &&
			LoadedAxisCharacterMaterialSkins.Find(Cast<AShooterCharacterMaterialSkin>(Actor)) == INDEX_NONE)
		{
			LoadedAxisCharacterMaterialSkinShortCodes.Add(ShortCode);
			LoadedAxisCharacterMaterialSkins.Add(Cast<AShooterCharacterMaterialSkin>(Actor));
		}
	}
	// Hats
	if (AssetType == EAssetType::Hats)
	{
		if (Cast<AShooterHatData>(Actor) &&
			LoadedHats.Find(Cast<AShooterHatData>(Actor)) == INDEX_NONE)
		{
			LoadedHatShortCodes.Add(ShortCode);
			LoadedHats.Add(Cast<AShooterHatData>(Actor));
		}
	}
	// Weapons
	if (AssetType == EAssetType::Weapons)
	{
		if (Cast<AShooterWeaponData>(Actor) &&
			LoadedWeapons.Find(Cast<AShooterWeaponData>(Actor)) == INDEX_NONE)
		{
			LoadedWeaponShortCodes.Add(ShortCode);
			LoadedWeapons.Add(Cast<AShooterWeaponData>(Actor));
		}
	}
	// Mods
	if (AssetType == EAssetType::Mods)
	{
		if (Cast<AShooterModData>(Actor) &&
			LoadedMods.Find(Cast<AShooterModData>(Actor)) == INDEX_NONE)
		{
			LoadedModShortCodes.Add(ShortCode);
			LoadedMods.Add(Cast<AShooterModData>(Actor));
		}
	}
	// Ally Tanks
	if (AssetType == EAssetType::Ally_Tanks)
	{
		if (LoadedAllyTanks.Find(Cast<AShooterTankData>(Actor)) == INDEX_NONE)
		{
			LoadedAllyTankShortCodes.Add(ShortCode);
			LoadedAllyTanks.Add(Cast<AShooterTankData>(Actor));
		}
	}
	// Ally Tank Skins
	if (AssetType == EAssetType::Ally_Tank_Skins)
	{
		if (LoadedAllyTankMaterialSkins.Find(Cast<AShooterTankMaterialSkin>(Actor)) == INDEX_NONE)
		{
			LoadedAllyTankMaterialSkinShortCodes.Add(ShortCode);
			LoadedAllyTankMaterialSkins.Add(Cast<AShooterTankMaterialSkin>(Actor));
		}
	}
	// Axis Tanks
	if (AssetType == EAssetType::Axis_Tanks)
	{
		if (LoadedAxisTanks.Find(Cast<AShooterTankData>(Actor)) == INDEX_NONE)
		{
			LoadedAxisTankShortCodes.Add(ShortCode);
			LoadedAxisTanks.Add(Cast<AShooterTankData>(Actor));
		}
	}
	// Axis Tank Skins
	if (AssetType == EAssetType::Axis_Tank_Skins)
	{
		if (LoadedAxisTankMaterialSkins.Find(Cast<AShooterTankMaterialSkin>(Actor)) == INDEX_NONE)
		{
			LoadedAxisTankMaterialSkinShortCodes.Add(ShortCode);
			LoadedAxisTankMaterialSkins.Add(Cast<AShooterTankMaterialSkin>(Actor));
		}
	}
	// Antenna
	if (AssetType == EAssetType::Antennas)
	{
		if (LoadedAntennas.Find(Cast<AShooterAntennaData>(Actor)) == INDEX_NONE)
		{
			LoadedAntennaShortCodes.Add(ShortCode);
			LoadedAntennas.Add(Cast<AShooterAntennaData>(Actor));
		}
	}
	if (LoadedAssetShortCodes.Find(ShortCode) == INDEX_NONE)
	{
		LoadedAssetShortCodes.Add(ShortCode);
		LoadedAssetObjects.Add(Actor);
	}
}

bool AShooterGameState::IsAssetLoaded(FName ShortCode)
{
	// Ally Character
	if (LoadedAllyCharacterShortCodes.Find(ShortCode) != INDEX_NONE)
		return true;
	// Ally Skins
	if (LoadedAllyCharacterMeshSkinShortCodes.Find(ShortCode) != INDEX_NONE || LoadedAllyCharacterMaterialSkinShortCodes.Find(ShortCode) != INDEX_NONE)
		return true;
	// Axis Characters
	if (LoadedAxisCharacterShortCodes.Find(ShortCode) != INDEX_NONE)
		return true;
	// Axis Skins
	if (LoadedAxisCharacterMeshSkinShortCodes.Find(ShortCode) != INDEX_NONE || LoadedAxisCharacterMaterialSkinShortCodes.Find(ShortCode) != INDEX_NONE)
		return true;
	// Hats
	if (LoadedHatShortCodes.Find(ShortCode) != INDEX_NONE || LoadedHatShortCodes.Find(ShortCode) != INDEX_NONE)
		return true;
	// Weapons
	if (LoadedWeaponShortCodes.Find(ShortCode) != INDEX_NONE)
		return true;
	// Mods
	if (LoadedModShortCodes.Find(ShortCode) != INDEX_NONE)
		return true;
	// Abilities
	if (LoadedAbilityShortCodes.Find(ShortCode) != INDEX_NONE)
		return true;
	// Ally Tanks
	if (LoadedAllyTankShortCodes.Find(ShortCode) != INDEX_NONE)
		return true;
	// Ally Tank Skins
	if (LoadedAllyTankMaterialSkinShortCodes.Find(ShortCode) != INDEX_NONE)
		return true;
	// Axis Tanks
	if (LoadedAxisTankShortCodes.Find(ShortCode) != INDEX_NONE)
		return true;
	// Axis Tank Skins
	if (LoadedAxisTankMaterialSkinShortCodes.Find(ShortCode) != INDEX_NONE)
		return true;
	return false;
}

bool AShooterGameState::IsAssetLoaded(TEnumAsByte<EAssetType::Type> AssetType, FName ShortCode)
{
	// Ally Character
	if (AssetType == EAssetType::Ally_Characters)
	{
		return LoadedAllyCharacterShortCodes.Find(ShortCode) != INDEX_NONE;
	}
	// Ally Skins
	if (AssetType == EAssetType::Ally_Skins)
	{
		return LoadedAllyCharacterMeshSkinShortCodes.Find(ShortCode) != INDEX_NONE || LoadedAllyCharacterMaterialSkinShortCodes.Find(ShortCode) != INDEX_NONE;
	}
	// Axis Characters
	if (AssetType == EAssetType::Axis_Characters)
	{
		return LoadedAxisCharacterShortCodes.Find(ShortCode) != INDEX_NONE;
	}
	// Axis Skins
	if (AssetType == EAssetType::Axis_Skins)
	{
		return LoadedAxisCharacterMeshSkinShortCodes.Find(ShortCode) != INDEX_NONE || LoadedAxisCharacterMaterialSkinShortCodes.Find(ShortCode) != INDEX_NONE;
	}
	// Hats
	if (AssetType == EAssetType::Hats)
	{
		return LoadedHatShortCodes.Find(ShortCode) != INDEX_NONE;
	}
	// Weapons
	if (AssetType == EAssetType::Weapons)
	{
		return LoadedWeaponShortCodes.Find(ShortCode) != INDEX_NONE;
	}
	// Mods
	if (AssetType == EAssetType::Mods)
	{
		return LoadedModShortCodes.Find(ShortCode) != INDEX_NONE;
	}
	// Abilities
	if (AssetType == EAssetType::Abilities)
	{
		return LoadedAbilityShortCodes.Find(ShortCode) != INDEX_NONE;
	}
	// Ally Tanks
	if (AssetType == EAssetType::Ally_Tanks)
	{
		return LoadedAllyTankShortCodes.Find(ShortCode) != INDEX_NONE;
	}
	// Ally Tank Skins
	if (AssetType == EAssetType::Ally_Tank_Skins)
	{
		return LoadedAllyTankMaterialSkinShortCodes.Find(ShortCode) != INDEX_NONE;
	}
	// Axis Tanks
	if (AssetType == EAssetType::Axis_Tanks)
	{
		return LoadedAxisTankShortCodes.Find(ShortCode) != INDEX_NONE;
	}
	// Axis Tank Skins
	if (AssetType == EAssetType::Axis_Tank_Skins)
	{
		return LoadedAxisTankMaterialSkinShortCodes.Find(ShortCode) != INDEX_NONE;
	}
	// Antennas
	if (AssetType == EAssetType::Antennas)
	{
		return LoadedAntennaShortCodes.Find(ShortCode) != INDEX_NONE;
	}
	return false;
}

FName AShooterGameState::GetLoadedAssetShortCode(AActor* Actor)
{
	const int32 Index = LoadedAssetObjects.Find(Actor);

	if (Index != INDEX_NONE)
	{
		return LoadedAssetShortCodes[Index];
	}
	return INVALID_SHORT_CODE;
}

void AShooterGameState::MulticastLoadAllCharacters_Implementation()
{
	DataMapping->LoadAllCharacters(this);

	UE_LOG(LogShooter, Warning, TEXT("MulticastLoadAllCharacters: Loading All Characters"));
}

void AShooterGameState::MulticastLoadAllCharacterSkins_Implementation()
{
	DataMapping->LoadAllCharacterSkins(this);

	UE_LOG(LogShooter, Warning, TEXT("MulticastLoadAllCharacterSkins: Loading All Character Skins"));
}

void AShooterGameState::MulticastLoadCharacterMeshSkin_Implementation(uint8 InFaction, FName ShortCode)
{
	TEnumAsByte<EFaction::Type> Faction		= InFaction == 1 ? EFaction::US : EFaction::GR;
	AShooterCharacterMeshSkin* Data		    = DataMapping->LoadCharacterMeshSkin(Faction, ShortCode);;
	TEnumAsByte<EAssetType::Type> AssetType = InFaction == 1 ? EAssetType::Ally_Skins : EAssetType::Axis_Skins;

	if (Data)
	{
		if (!IsAssetLoaded(AssetType, ShortCode))
		{
			AddLoadedAsset(AssetType, ShortCode, Data);
		}
	}
	else
	{
		UE_LOG(LogShooter, Warning, TEXT("MulticastLoadCharacterMeshSkin: Failed to load Character Mesh Skin using Short Code: %s"), *ShortCode.ToString());
		return;
	}
	UE_LOG(LogShooter, Warning, TEXT("MulticastLoadCharacterMeshSkin: Loading Character Mesh Skin with ShortCode: %s"), *ShortCode.ToString());
}

void AShooterGameState::MulticastLoadCharacterMaterialSkin_Implementation(uint8 InFaction, FName ShortCode)
{
	TEnumAsByte<EFaction::Type> Faction     = InFaction == 1 ? EFaction::US : EFaction::GR;
	AShooterCharacterMaterialSkin* Data		= DataMapping->LoadCharacterMaterialSkin(Faction, ShortCode);;
	TEnumAsByte<EAssetType::Type> AssetType = InFaction == 1 ? EAssetType::Ally_Skins : EAssetType::Axis_Skins;

	if (Data)
	{
		if (!IsAssetLoaded(AssetType, ShortCode))
		{
			AddLoadedAsset(AssetType, ShortCode, Data);
		}
	}
	else
	{
		UE_LOG(LogShooter, Warning, TEXT("MulticastLoadCharacterMaterialSkin: Failed to load Character Material Skin using Short Code: %s"), *ShortCode.ToString());
		return;
	}
	UE_LOG(LogShooter, Warning, TEXT("MulticastLoadCharacterMaterialSkin: Loading Character Material Skin with ShortCode: %s"), *ShortCode.ToString());
}

void AShooterGameState::MulticastLoadAllHats_Implementation()
{
	DataMapping->LoadAllHats(this);

	UE_LOG(LogShooter, Warning, TEXT("MulticastLoadAllHats: Loading All Characters"));
}

void AShooterGameState::MulticastLoadWeapon_Implementation(uint8 LookUpCode)
{
	FName ShortCode = DataMapping->GetWeaponDataShortCode(LookUpCode);

	if (!IsAssetLoaded(EAssetType::Weapons, ShortCode))
	{
		AShooterWeaponData* Data = DataMapping->LoadWeaponData(ShortCode);

		if (Data)
		{
			AddLoadedAsset(EAssetType::Weapons, ShortCode, Data);
		}
		else
		{
			UE_LOG(LogShooter, Warning, TEXT("MulticastLoadWeapon: Failed to load Weapon Data using Short Code: %s"), *ShortCode.ToString());
			return;
		}
		UE_LOG(LogShooter, Warning, TEXT("MulticastLoadWeapon: Loading Weapon with ShortCode: %s"), *ShortCode.ToString());
	}
}

void AShooterGameState::MulticastLoadAllWeapons_Implementation()
{
	DataMapping->LoadAllWeapons(this);

	UE_LOG(LogShooter, Warning, TEXT("MulticastLoadAllWeapons: Loading All Weapons"));
}

void AShooterGameState::MulticastLoadMod_Implementation(uint16 LookUpCode)
{
	FName ShortCode = DataMapping->GetModShortCode(LookUpCode);

	if (!IsAssetLoaded(EAssetType::Mods, ShortCode))
	{
		AShooterModData* Data = DataMapping->LoadMod(ShortCode);

		if (Data)
		{
			AddLoadedAsset(EAssetType::Mods, ShortCode, Data);
		}
		else
		{
			UE_LOG(LogShooter, Warning, TEXT("MulticastLoadMod: Failed to load Mod using Short Code: %s"), *ShortCode.ToString());
			return;
		}
		UE_LOG(LogShooter, Warning, TEXT("MulticastLoadMod: Loading Mod with ShortCode: %s"), *ShortCode.ToString());
	}
}

void AShooterGameState::MulticastLoadAllTanks_Implementation()
{
	DataMapping->LoadAllTanks(this);

	UE_LOG(LogShooter, Warning, TEXT("MulticastLoadAllTanks: Loading All Tanks"));
}

void AShooterGameState::MulticastLoadTankMaterialSkin_Implementation(uint8 InFaction, FName ShortCode)
{
	TEnumAsByte<EFaction::Type> Faction		= InFaction == 1 ? EFaction::US : EFaction::GR;
	AShooterTankMaterialSkin* Data			= DataMapping->LoadTankMaterialSkin(Faction, ShortCode);;
	TEnumAsByte<EAssetType::Type> AssetType = InFaction == 1 ? EAssetType::Ally_Tank_Skins : EAssetType::Axis_Tank_Skins;

	if (Data)
	{
		if (!IsAssetLoaded(AssetType, ShortCode))
		{
			AddLoadedAsset(AssetType, ShortCode, Data);
		}
	}
	else
	{
		UE_LOG(LogShooter, Warning, TEXT("MulticastLoadTankMaterialSkin: Failed to load Tank Material Skin using Short Code: %s"), *ShortCode.ToString());
		return;
	}
	UE_LOG(LogShooter, Warning, TEXT("MulticastLoadTankMaterialSkin: Loading Tank Material Skin with ShortCode: %s"), *ShortCode.ToString());
}

template<typename T>
T* AShooterGameState::GetData(FString FunctionName, FString DataType, TArray<T*>& Datas, TArray<FName>& ShortCodes, FName ShortCode)
{
	const int32 Count = ShortCodes.Num();

	for (int32 Index = 0; Index < Count; Index++)
	{
		if (ShortCodes[Index] == ShortCode)
		{
			return Datas[Index];
		}
	}
	FString Proxy = UShooterStatics::GetProxyAsString(this);
	UE_LOG(LogShooter, Warning, TEXT("%s (%s): Failed to find %s with ShortCode: %s"), *FunctionName, *Proxy, *DataType, *ShortCode.ToString());
	return NULL;
}

template<typename T>
T* AShooterGameState::GetData(TArray<T*>& Datas, TArray<FName>& ShortCodes, FName ShortCode)
{
	const int32 Count = ShortCodes.Num();

	for (int32 Index = 0; Index < Count; Index++)
	{
		if (ShortCodes[Index] == ShortCode)
		{
			return Datas[Index];
		}
	}
	return NULL;
}

template<typename T>
T* AShooterGameState::GetDataByCommonName(FString FunctionName, FString DataType, TArray<T*>& Datas, FName CommonName)
{
	const int32 Count = Datas.Num();

	for (int32 Index = 0; Index < Count; Index++)
	{
		AShooterData* Data = Cast<AShooterData>(Datas[Index]);

		if (Data->Name.ToString().ToLower() == CommonName.ToString().ToLower())
		{
			return Cast<T>(Data);
		}

		const int32 NumNames = Data->AlternativeNames.Num();

		for (int32 J = 0; J < NumNames; J++)
		{
			if (Data->AlternativeNames[J].ToString().ToLower() == CommonName.ToString().ToLower())
			{
				return Cast<T>(Data);
			}
		}
	}
	FString Proxy = UShooterStatics::GetProxyAsString(this);
	UE_LOG(LogShooter, Warning, TEXT("%s (%s): Failed to find %s with CommonName: %s"), *FunctionName, *Proxy, *DataType, *CommonName.ToString());
	return NULL;
}

template<typename T>
T* AShooterGameState::GetDataByCommonName(TArray<T*>& Datas, FName CommonName)
{
	const int32 Count = Datas.Num();

	for (int32 Index = 0; Index < Count; Index++)
	{
		AShooterData* Data = Cast<AShooterData>(Datas[Index]);

		if (Data->Name.ToString().ToLower() == CommonName.ToString().ToLower())
		{
			return Cast<T>(Data);
		}

		const int32 NumNames = Data->AlternativeNames.Num();

		for (int32 J = 0; J < NumNames; J++)
		{
			if (Data->AlternativeNames[J].ToString().ToLower() == CommonName.ToString().ToLower())
			{
				return Cast<T>(Data);
			}
		}
	}
	return NULL;
}

// Character
#pragma region

AShooterCharacterData* AShooterGameState::GetCharacterDataByCommonName(FName CommonName)
{
	for (int32 FactionIndex = 0; FactionIndex < 2; FactionIndex++)
	{
		TArray<AShooterCharacterData*>& LoadedCharacters = FactionIndex == 0 ? LoadedAxisCharacters : LoadedAllyCharacters;

		if (AShooterCharacterData* Data = GetDataByCommonName<AShooterCharacterData>(LoadedCharacters, CommonName))
			return Data;
	}

	FString Proxy = UShooterStatics::GetProxyAsString(this);
	UE_LOG(LogShooter, Warning, TEXT("GetCharacterDataByCommonName (%s): Failed to find Character Data with CommonName: %s"), *Proxy, *CommonName.ToString());
	return NULL;
}

AShooterCharacterData* AShooterGameState::GetCharacterDataByCommonName(TEnumAsByte<EFaction::Type> InFaction, FName CommonName)
{
	TArray<AShooterCharacterData*>& LoadedCharacters = InFaction == EFaction::GR ? LoadedAxisCharacters : LoadedAllyCharacters;

	return GetDataByCommonName<AShooterCharacterData>(TEXT("GetCharacterDataByCommonName"), TEXT("Character Data"), LoadedCharacters, CommonName);
}

AShooterCharacterData* AShooterGameState::GetCharacterData(FName ShortCode)
{
	TEnumAsByte<EFaction::Type> OutFaction = EFaction::EFaction_MAX;
	return GetCharacterData(ShortCode, OutFaction);
}

AShooterCharacterData* AShooterGameState::GetCharacterData(FName ShortCode, TEnumAsByte<EFaction::Type>& OutFaction)
{
	OutFaction = EFaction::EFaction_MAX;

	for (int32 FactionIndex = 0; FactionIndex < 2; FactionIndex++)
	{
		TArray<FName>& LoadedCharacterShortCodes		 = FactionIndex == 0 ? LoadedAxisCharacterShortCodes : LoadedAllyCharacterShortCodes;
		TArray<AShooterCharacterData*>& LoadedCharacters = FactionIndex == 0 ? LoadedAxisCharacters : LoadedAllyCharacters;

		if (AShooterCharacterData* Data = GetData<AShooterCharacterData>(LoadedCharacters, LoadedCharacterShortCodes, ShortCode))
		{
			OutFaction = (TEnumAsByte<EFaction::Type>)FactionIndex;
			return Data;
		}
	}

	FString Proxy = UShooterStatics::GetProxyAsString(this);
	UE_LOG(LogShooter, Warning, TEXT("GetCharacterData (%s): Failed to find Character Data with ShortCode: %s"), *Proxy, *ShortCode.ToString());
	return NULL;
}

AShooterCharacterData* AShooterGameState::GetCharacterData(TEnumAsByte<EFaction::Type> InFaction, FName ShortCode)
{
	TArray<FName>& LoadedCharacterShortCodes	     = InFaction == EFaction::GR ? LoadedAxisCharacterShortCodes : LoadedAllyCharacterShortCodes;
	TArray<AShooterCharacterData*>& LoadedCharacters = InFaction == EFaction::GR ? LoadedAxisCharacters : LoadedAllyCharacters;

	return GetData<AShooterCharacterData>(TEXT("GetCharacterData"), TEXT("Character Data"), LoadedCharacters, LoadedCharacterShortCodes, ShortCode);
}

FName AShooterGameState::GetRandomCharacterDataShortCode(TEnumAsByte<EFaction::Type> InFaction, TEnumAsByte<ECharacterClass::Type> InCharacterClass)
{
	TArray<FName>& LoadedCharacterShortCodes = InFaction == EFaction::GR ? LoadedAxisCharacterShortCodes : LoadedAllyCharacterShortCodes;
	TArray<AShooterCharacterData*>& LoadedCharacters = InFaction == EFaction::GR ? LoadedAxisCharacters : LoadedAllyCharacters;
	FString TeamName = InFaction == EFaction::GR ? TEXT("Axis") : TEXT("Allies");

	FString Proxy = Role == ROLE_Authority ? TEXT("Server") : TEXT("Client");

	int32 Count = LoadedCharacterShortCodes.Num();

	if (Count == 0)
	{
		UE_LOG(LogShooter, Warning, TEXT("GetRandomCharacterDataShortCode (%s): No Character Data Short Codes for %s Characters"), *Proxy, *TeamName);
		return INVALID_SHORT_CODE;
	}

	TArray<FName> CharacterShortCodes;

	for (int32 Index = 0; Index < Count; Index++)
	{
		if (LoadedCharacters[Index]->Class == InCharacterClass &&
			LoadedCharacters[Index]->PrimaryFaction == InFaction)
		{
			CharacterShortCodes.Add(LoadedCharacterShortCodes[Index]);
		}
	}

	Count = CharacterShortCodes.Num();

	if (Count == 0)
	{
		UE_LOG(LogShooter, Warning, TEXT("GetRandomCharacterDataShortCode (%s): No Character Data Short Codes for Faction: %s and Character Class: %s"), *Proxy, *UShooterStatics::FactionToString(InFaction), *UShooterStatics::CharacterClassToString(InCharacterClass));
		return INVALID_SHORT_CODE;
	}
	return CharacterShortCodes[FMath::RandRange(0, Count - 1)];
}

AShooterCharacterMeshSkin* AShooterGameState::GetCharacterMeshSkinByCommonName(FName CommonName)
{
	for (int32 FactionIndex = 0; FactionIndex < 2; FactionIndex++)
	{
		TArray<AShooterCharacterMeshSkin*>& LoadedCharacterSkins = FactionIndex == 0 ? LoadedAxisCharacterMeshSkins : LoadedAllyCharacterMeshSkins;

		if (AShooterCharacterMeshSkin* Data = GetDataByCommonName<AShooterCharacterMeshSkin>(LoadedCharacterSkins, CommonName))
			return Data;
	}

	FString Proxy = UShooterStatics::GetProxyAsString(this);
	UE_LOG(LogShooter, Warning, TEXT("GetCharacterMeshSkinByCommonName (%s): Failed to find Character Mesh Skin with CommonName: %s"), *Proxy, *CommonName.ToString());
	return NULL;
}

AShooterCharacterMeshSkin* AShooterGameState::GetCharacterMeshSkin(FName ShortCode)
{
	for (int32 FactionIndex = 0; FactionIndex < 2; FactionIndex++)
	{
		TArray<FName>& MeshSkinShortCodes			  = FactionIndex == 0 ? LoadedAxisCharacterMeshSkinShortCodes : LoadedAllyCharacterMeshSkinShortCodes;
		TArray<AShooterCharacterMeshSkin*>& MeshSkins = FactionIndex == 0 ? LoadedAxisCharacterMeshSkins : LoadedAllyCharacterMeshSkins;

		if (AShooterCharacterMeshSkin* Data = GetData<AShooterCharacterMeshSkin>(MeshSkins, MeshSkinShortCodes, ShortCode))
			return Data;
	}

	FString Proxy = UShooterStatics::GetProxyAsString(this);
	UE_LOG(LogShooter, Warning, TEXT("GetCharacterMeshSkin: Failed to find Character Mesh Skin with ShortCode: %s"), *Proxy, *ShortCode.ToString());
	return NULL;
}

AShooterCharacterMeshSkin* AShooterGameState::GetCharacterMeshSkin(TEnumAsByte<EFaction::Type> InFaction, FName ShortCode)
{
	TArray<FName>& MeshSkinShortCodes			  = InFaction == EFaction::GR ? LoadedAxisCharacterMeshSkinShortCodes : LoadedAllyCharacterMeshSkinShortCodes;
	TArray<AShooterCharacterMeshSkin*>& MeshSkins = InFaction == EFaction::GR ? LoadedAxisCharacterMeshSkins : LoadedAllyCharacterMeshSkins;

	if (AShooterCharacterMeshSkin* Data = GetData<AShooterCharacterMeshSkin>(MeshSkins, MeshSkinShortCodes, ShortCode))
		return Data;

	FString Proxy = UShooterStatics::GetProxyAsString(this);
	UE_LOG(LogShooter, Warning, TEXT("GetCharacterMeshSkin: Failed to find Character Mesh Skin with ShortCode: %s"), *Proxy, *ShortCode.ToString());
	return NULL;
}

USkeletalMesh* AShooterGameState::GetCharacterMesh(TEnumAsByte<ECharacterSkin::Type> InSkinType, TEnumAsByte<EFaction::Type> InFaction, FName ShortCode)
{
	AShooterCharacterMeshSkin* MeshSkin = GetCharacterMeshSkin(InFaction, ShortCode);
	USkeletalMesh* Mesh					= MeshSkin->GetMesh(InSkinType);

	if (Mesh)
		return Mesh;

	FString Proxy = UShooterStatics::GetProxyAsString(this);
	UE_LOG(LogShooter, Warning, TEXT("GetCharacterMesh (%s): Failed to Find Character Mesh for %s, Faction: %s, and Short Code: %s"), *Proxy, *UShooterStatics::CharacterSkinToString(InSkinType), *UShooterStatics::FactionToString(InFaction), *ShortCode.ToString());
	return NULL;
}

USkeletalMesh* AShooterGameState::GetCharacterMesh(TEnumAsByte<ECharacterSkin::Type> InSkinType, FCharacterInfo& InInfo)
{
	return GetCharacterMesh(InSkinType, InInfo.Faction, InInfo.CharacterMeshSkinShortCode);
}

USkeletalMesh* AShooterGameState::GetCharacterMesh(TEnumAsByte<ECharacterSkin::Type> InSkinType, FCharacterInfo* InInfo)
{
	return GetCharacterMesh(InSkinType, InInfo->Faction, InInfo->CharacterMeshSkinShortCode);
}

void AShooterGameState::SetCharacterMesh(USkeletalMeshComponent* InMesh, TEnumAsByte<ECharacterSkin::Type> InSkinType, TEnumAsByte<EFaction::Type> InFaction, FName MeshSkinShortCode, FName MaterialSkinShortCode)
{
	FString Proxy = UShooterStatics::GetProxyAsString(this);

	// Set Mesh
	AShooterCharacterMeshSkin* MeshSkin = GetCharacterMeshSkin(InFaction, MeshSkinShortCode);

	if (!MeshSkin)
	{
		UE_LOG(LogShooter, Warning, TEXT("SetCharacterMesh (%s): Failed to find Character Mesh Skin for %s, Faction: %s, and Short Code: %s"), *Proxy, *UShooterStatics::CharacterSkinToString(InSkinType), *UShooterStatics::FactionToString(InFaction), *MeshSkinShortCode.ToString());
		return;
	}

	MeshSkin->SetMesh(InMesh, InSkinType);

	// Set Material
	AShooterCharacterMaterialSkin* MaterialSkin = GetCharacterMaterialSkin(InFaction, MaterialSkinShortCode);

	if (!MaterialSkin)
	{
		UE_LOG(LogShooter, Warning, TEXT("SetCharacterMesh (%s): Failed to find Character Material Skin for %s, Faction: %s, and Short Code: %s"), *Proxy, *UShooterStatics::CharacterSkinToString(InSkinType), *UShooterStatics::FactionToString(InFaction), *MaterialSkinShortCode.ToString());
		return;
	}

	MaterialSkin->SetMaterials(InMesh, InSkinType);
}

void AShooterGameState::SetCharacterMesh(USkeletalMeshComponent* InMesh, TEnumAsByte<ECharacterSkin::Type> InSkinType, FCharacterInfo& InCharacterInfo)
{
	SetCharacterMesh(InMesh, InSkinType, InCharacterInfo.Faction, InCharacterInfo.CharacterMeshSkinShortCode, InCharacterInfo.CharacterMaterialSkinShortCode);
}

AShooterCharacterMaterialSkin* AShooterGameState::GetCharacterMaterialSkin(FName ShortCode)
{
	for (int32 FactionIndex = 0; FactionIndex < 2; FactionIndex++)
	{
		TArray<FName>& MaterialSkinShortCodes			      = FactionIndex == 0 ? LoadedAxisCharacterMaterialSkinShortCodes : LoadedAllyCharacterMaterialSkinShortCodes;
		TArray<AShooterCharacterMaterialSkin*>& MaterialSkins = FactionIndex == 0 ? LoadedAxisCharacterMaterialSkins : LoadedAllyCharacterMaterialSkins;

		if (AShooterCharacterMaterialSkin* Data = GetData<AShooterCharacterMaterialSkin>(MaterialSkins, MaterialSkinShortCodes, ShortCode))
			return Data;
	}

	FString Proxy = UShooterStatics::GetProxyAsString(this);
	UE_LOG(LogShooter, Warning, TEXT("GetCharacterMaterialSkin: Failed to find Character Material Skin with ShortCode: %s"), *Proxy, *ShortCode.ToString());
	return NULL;
}

AShooterCharacterMaterialSkin* AShooterGameState::GetCharacterMaterialSkin(TEnumAsByte<EFaction::Type> InFaction, FName ShortCode)
{
	TArray<FName>& MaterialSkinShortCodes			      = InFaction == EFaction::GR ? LoadedAxisCharacterMaterialSkinShortCodes : LoadedAllyCharacterMaterialSkinShortCodes;
	TArray<AShooterCharacterMaterialSkin*>& MaterialSkins = InFaction == EFaction::GR ? LoadedAxisCharacterMaterialSkins : LoadedAllyCharacterMaterialSkins;

	return GetData<AShooterCharacterMaterialSkin>(TEXT("GetCharacterMaterialSkin"), TEXT("Character Material Skin"), MaterialSkins, MaterialSkinShortCodes, ShortCode);
}

void AShooterGameState::SetMaterialsForCharacterMesh(USkeletalMeshComponent* InMesh, TEnumAsByte<ECharacterSkin::Type> InSkinType, TEnumAsByte<EFaction::Type> InFaction, FName ShortCode)
{
	FString Proxy = UShooterStatics::GetProxyAsString(this);

	AShooterCharacterMaterialSkin* MaterialSkin = GetCharacterMaterialSkin(InFaction, ShortCode);

	if (!MaterialSkin)
	{
		UE_LOG(LogShooter, Warning, TEXT("SetMaterialsForCharacterMesh (%s): Failed to find Character Material Skin for %s, Faction: %s, and Short Code: %s"), *Proxy, *UShooterStatics::CharacterSkinToString(InSkinType), *UShooterStatics::FactionToString(InFaction), *ShortCode.ToString());
		return;
	}
	MaterialSkin->SetMaterials(InMesh, InSkinType);
}

void AShooterGameState::SetMaterialsForCharacterMesh(USkeletalMeshComponent* InMesh, TEnumAsByte<ECharacterSkin::Type> InSkinType, FCharacterInfo& InInfo)
{
	SetMaterialsForCharacterMesh(InMesh, InSkinType, InInfo.Faction, InInfo.CharacterMaterialSkinShortCode);
}

int32 AShooterGameState::GetNumDefaultCharacterMaterials(TEnumAsByte<EViewType::Type> InViewType, TEnumAsByte<EFaction::Type> InFaction, FName ShortCode)
{
	TArray<FName>& LoadedCharacterMeshSkinShortCodes = InFaction == EFaction::GR ? LoadedAxisCharacterMeshSkinShortCodes : LoadedAllyCharacterMeshSkinShortCodes;
	TArray<AShooterCharacterMeshSkin*>& LoadedCharacterMeshSkins = InFaction == EFaction::GR ? LoadedAxisCharacterMeshSkins : LoadedAllyCharacterMeshSkins;

	FString Proxy = Role == ROLE_Authority ? TEXT("Server") : TEXT("Client");

	const int32 Index = LoadedCharacterMeshSkinShortCodes.Find(ShortCode);

	if (Index != INDEX_NONE)
	{
		if (InViewType == EViewType::FirstPerson || InViewType == EViewType::VR)
			return LoadedCharacterMeshSkins[Index]->Mesh1P->Materials.Num();
		if (InViewType == EViewType::ThirdPerson)
			return LoadedCharacterMeshSkins[Index]->Mesh3P->Materials.Num();
	}
	UE_LOG(LogShooter, Warning, TEXT("GetNumDefaultCharacterMaterials (%s): Failed to Find Materials for %s, Faction: %s, and Short Code: %s"), *Proxy, *UShooterStatics::ViewTypeToString(InViewType), *UShooterStatics::FactionToString(InFaction), *ShortCode.ToString());
	return INDEX_NONE;
}

UMaterialInstanceConstant* AShooterGameState::GetCharacterMaterial(int32 Index, TEnumAsByte<EViewType::Type> InViewType, TEnumAsByte<EFaction::Type> InFaction, FName ShortCode)
{
	TArray<FName>& LoadedCharacterMaterialSkinShortCodes = InFaction == EFaction::GR ? LoadedAxisCharacterMaterialSkinShortCodes : LoadedAllyCharacterMaterialSkinShortCodes;
	TArray<AShooterCharacterMaterialSkin*>& LoadedCharacterMaterialSkins = InFaction == EFaction::GR ? LoadedAxisCharacterMaterialSkins : LoadedAllyCharacterMaterialSkins;

	FString Proxy = Role == ROLE_Authority ? TEXT("Server") : TEXT("Client");

	const int32 AssetIndex = LoadedCharacterMaterialSkinShortCodes.Find(ShortCode);

	if (AssetIndex != INDEX_NONE)
	{
		if (InViewType == EViewType::FirstPerson || InViewType == EViewType::VR)
			return LoadedCharacterMaterialSkins[AssetIndex]->Materials1P[Index];
		if (InViewType == EViewType::ThirdPerson)
			return LoadedCharacterMaterialSkins[AssetIndex]->Materials3P[Index];
	}
	UE_LOG(LogShooter, Warning, TEXT("GetCharacterMaterial (%s): Failed to Find Material at Index %s for %d, Faction: %s, and Short Code: %s"), *Proxy, Index, *UShooterStatics::ViewTypeToString(InViewType), *UShooterStatics::FactionToString(InFaction), *ShortCode.ToString());
	return NULL;
}

UMaterialInstanceConstant* AShooterGameState::GetCharacterDeathMaterial(int32 Index, TEnumAsByte<EFaction::Type> InFaction, FName ShortCode)
{
	TArray<FName>& LoadedCharacterMaterialSkinShortCodes = InFaction == EFaction::GR ? LoadedAxisCharacterMaterialSkinShortCodes : LoadedAllyCharacterMaterialSkinShortCodes;
	TArray<AShooterCharacterMaterialSkin*>& LoadedCharacterMaterialSkins = InFaction == EFaction::GR ? LoadedAxisCharacterMaterialSkins : LoadedAllyCharacterMaterialSkins;

	FString Proxy = Role == ROLE_Authority ? TEXT("Server") : TEXT("Client");

	const int32 AssetIndex = LoadedCharacterMaterialSkinShortCodes.Find(ShortCode);

	if (AssetIndex != INDEX_NONE)
	{
		return LoadedCharacterMaterialSkins[AssetIndex]->DeathMaterials3P[Index];
	}
	UE_LOG(LogShooter, Warning, TEXT("GetCharacterDeathMaterial (%s): Failed to Find Death Material at Index %d for Faction: %s, and Short Code: %s"), *Proxy, Index, *UShooterStatics::FactionToString(InFaction), *ShortCode.ToString());
	return NULL;
}

int32 AShooterGameState::GetFaceMaterialIndex(TEnumAsByte<EFaction::Type> InFaction, FName ShortCode)
{
	TArray<FName>& LoadedCharacterMaterialSkinShortCodes = InFaction == EFaction::GR ? LoadedAxisCharacterMaterialSkinShortCodes : LoadedAllyCharacterMaterialSkinShortCodes;
	TArray<AShooterCharacterMaterialSkin*>& LoadedCharacterMaterialSkins = InFaction == EFaction::GR ? LoadedAxisCharacterMaterialSkins : LoadedAllyCharacterMaterialSkins;

	FString Proxy = Role == ROLE_Authority ? TEXT("Server") : TEXT("Client");

	const int32 Index = LoadedCharacterMaterialSkinShortCodes.Find(ShortCode);

	if (Index != INDEX_NONE)
	{
		return LoadedCharacterMaterialSkins[Index]->FaceMaterialIndex;
	}
	UE_LOG(LogShooter, Warning, TEXT("GetFaceMaterialIndex (%s): Failed to Find Face Material Index for Faction: %s, and Short Code: %s"), *Proxy, *UShooterStatics::FactionToString(InFaction), *ShortCode.ToString());
	return INDEX_NONE;
}

#pragma endregion Character

// Hat
#pragma region

AShooterHatData* AShooterGameState::GetHatByCommonName(FName CommonName)
{
	return GetDataByCommonName<AShooterHatData>(TEXT("GetHatByCommonName"), TEXT("Hat Data"), LoadedHats, CommonName);
}

AShooterHatData* AShooterGameState::GetHat(FName ShortCode)
{
	return GetData<AShooterHatData>(TEXT("GetHat"), TEXT("Hat Data"), LoadedHats, LoadedHatShortCodes, ShortCode);
}

#pragma endregion Hat

// Weapon
#pragma region

AShooterWeaponData* AShooterGameState::GetWeaponDataByCommonName(FName CommonName)
{
	return GetDataByCommonName<AShooterWeaponData>(TEXT("GetWeaponDataByCommonName"), TEXT("Weapon Data"), LoadedWeapons, CommonName);
}

AShooterWeaponData* AShooterGameState::GetWeaponData(FName ShortCode)
{
	return GetData<AShooterWeaponData>(TEXT("GetWeaponData"), TEXT("Weapon Data"), LoadedWeapons, LoadedWeaponShortCodes, ShortCode);
}

#pragma endregion Weapon

AShooterModData* AShooterGameState::GetMod(FName ShortCode)
{
	return GetData<AShooterModData>(TEXT("GetMod"), TEXT("Mod"), LoadedMods, LoadedModShortCodes, ShortCode);
}

// Tank
#pragma region

AShooterTankData* AShooterGameState::GetTankData(FName ShortCode)
{
	TEnumAsByte<EFaction::Type> OutFaction = EFaction::EFaction_MAX;
	return GetTankData(ShortCode, OutFaction);
}

AShooterTankData* AShooterGameState::GetTankData(FName ShortCode, TEnumAsByte<EFaction::Type>& OutFaction)
{
	OutFaction = EFaction::EFaction_MAX;

	for (int32 FactionIndex = 0; FactionIndex < 2; FactionIndex++)
	{
		TArray<FName>& LoadedTankShortCodes    = FactionIndex == 0 ? LoadedAxisTankShortCodes : LoadedAllyTankShortCodes;
		TArray<AShooterTankData*>& LoadedTanks = FactionIndex == 0 ? LoadedAxisTanks : LoadedAllyTanks;

		if (AShooterTankData* Data = GetData<AShooterTankData>(LoadedTanks, LoadedTankShortCodes, ShortCode))
		{
			OutFaction = (TEnumAsByte<EFaction::Type>)FactionIndex;
			return Data;
		}
	}

	FString Proxy = UShooterStatics::GetProxyAsString(this);
	UE_LOG(LogShooter, Warning, TEXT("GetTankData (%s): Failed to find Tank Data with ShortCode: %s"), *Proxy, *ShortCode.ToString());
	return NULL;
}

AShooterTankData* AShooterGameState::GetTankData(TEnumAsByte<EFaction::Type> InFaction, FName ShortCode)
{
	TArray<FName>& LoadedTankShortCodes    = InFaction == EFaction::GR ? LoadedAxisTankShortCodes : LoadedAllyTankShortCodes;
	TArray<AShooterTankData*>& LoadedTanks = InFaction == EFaction::GR ? LoadedAxisTanks : LoadedAllyTanks;

	if (AShooterTankData* Data = GetData<AShooterTankData>(LoadedTanks, LoadedTankShortCodes, ShortCode))
		return Data;

	FString Proxy = UShooterStatics::GetProxyAsString(this);
	UE_LOG(LogShooter, Warning, TEXT("GetTankData (%s): Failed to find Tank Data for Faction: %s with ShortCode: %s"), *Proxy, *UShooterStatics::FactionToString(InFaction), *ShortCode.ToString());
	return NULL;
}

AShooterTankData* AShooterGameState::GetTankDataByCommonName(FName CommonName)
{
	for (int32 FactionIndex = 0; FactionIndex < 2; FactionIndex++)
	{
		TArray<AShooterTankData*>& LoadedTanks = FactionIndex == 0 ? LoadedAxisTanks : LoadedAllyTanks;

		if (AShooterTankData* Data = GetDataByCommonName<AShooterTankData>(LoadedTanks, CommonName))
			return Data;
	}

	FString Proxy = UShooterStatics::GetProxyAsString(this);
	UE_LOG(LogShooter, Warning, TEXT("GetTankDataByCommonName (%s): Failed to find Tank Data with CommonName: %s"), *Proxy, *CommonName.ToString());
	return NULL;
}

AShooterTankData* AShooterGameState::GetTankDataByCommonName(TEnumAsByte<EFaction::Type> InFaction, FName CommonName)
{
	TArray<AShooterTankData*>& LoadedTanks = InFaction == EFaction::GR ? LoadedAxisTanks : LoadedAllyTanks;

	return GetDataByCommonName<AShooterTankData>(TEXT("GetTankDataByCommonName"), TEXT("Tank Data"), LoadedTanks, CommonName);
}

FName AShooterGameState::GetTankDataShortCodeByCommonName(FName CommonName)
{
	for (int32 FactionIndex = 0; FactionIndex < 2; FactionIndex++)
	{
		TArray<FName>& LoadedTankShortCodes = FactionIndex == 0 ? LoadedAxisTankShortCodes : LoadedAllyTankShortCodes;
		TArray<AShooterTankData*>& LoadedTanks = FactionIndex == 0 ? LoadedAxisTanks : LoadedAllyTanks;

		int32 Count = LoadedTanks.Num();

		for (int32 Index = 0; Index < Count; Index++)
		{
			if (LoadedTanks[Index]->Name.ToString().ToLower() == CommonName.ToString().ToLower())
			{
				return LoadedTankShortCodes[Index];
			}

			const int32 NumNames = LoadedTanks[Index]->AlternativeNames.Num();

			for (int32 J = 0; J < NumNames; J++)
			{
				if (LoadedTanks[Index]->AlternativeNames[J].ToString().ToLower() == CommonName.ToString().ToLower())
				{
					return LoadedTankShortCodes[Index];
				}
			}
		}
	}

	FString Proxy = Role == ROLE_Authority ? TEXT("Server") : TEXT("Client");
	UE_LOG(LogShooter, Warning, TEXT("GetTankDataShortCodeByCommonName (%s): Failed to find Tank Data Short Code with CommonName: %s"), *Proxy, *CommonName.ToString());
	return INVALID_SHORT_CODE;
}

class AShooterTankData* AShooterGameState::GetRandomTankDataByClass(TEnumAsByte<ETankClass::Type> InTankClass)
{
	const int32 RandomStartIndex = FMath::RandRange(0, 1);

	for (int32 FactionIndex = 0; FactionIndex < 2; FactionIndex++)
	{
		TArray<AShooterTankData*>& LoadedTanks = FactionIndex == RandomStartIndex ? LoadedAxisTanks : LoadedAllyTanks;

		const int32 Count = LoadedTanks.Num();

		for (int32 Index = 0; Index < Count; Index++)
		{
			if (LoadedTanks[Index]->Class == InTankClass)
			{
				return LoadedTanks[Index];
			}
		}
	}

	FString Proxy = Role == ROLE_Authority ? TEXT("Server") : TEXT("Client");
	UE_LOG(LogShooter, Warning, TEXT("GetRandomTankDataByClass (%s): Failed to find Tank Data for Tank Class: %s"), *Proxy, *UShooterStatics::TankClassToString(InTankClass));
	return NULL;
}

FName AShooterGameState::GetRandomTankDataShortCodeByClass(TEnumAsByte<ETankClass::Type> InTankClass)
{
	const int32 RandomStartIndex = FMath::RandRange(0, 1);

	for (int32 FactionIndex = 0; FactionIndex < 2; FactionIndex++)
	{
		TArray<FName>& LoadedTankShortCodes = FactionIndex == RandomStartIndex ? LoadedAxisTankShortCodes : LoadedAllyTankShortCodes;
		TArray<AShooterTankData*>& LoadedTanks = FactionIndex == RandomStartIndex ? LoadedAxisTanks : LoadedAllyTanks;

		const int32 Count = LoadedTanks.Num();

		for (int32 Index = 0; Index < Count; Index++)
		{
			if (LoadedTanks[Index]->Class == InTankClass)
			{
				return LoadedTankShortCodes[Index];
			}
		}
	}

	FString Proxy = Role == ROLE_Authority ? TEXT("Server") : TEXT("Client");
	UE_LOG(LogShooter, Warning, TEXT("GetRandomTankDataShortCodeByClass (%s): Failed to find Tank Data Short Code for Tank Class: %s"), *Proxy, *UShooterStatics::TankClassToString(InTankClass));
	return INVALID_SHORT_CODE;
}

void AShooterGameState::SetTankMesh(USkeletalMeshComponent* InMesh, TEnumAsByte<EViewType::Type> InViewType, FName TankDataShortCode, FName MaterialSkinShortCode)
{
	AShooterTankData* Data = GetTankData(TankDataShortCode);
	FString Proxy		   = UShooterStatics::GetProxyAsString(this);

	if (!Data)
	{
		UE_LOG(LogShooter, Warning, TEXT("SetTankMesh (%s): Failed to find Tank Data with Short Code: %s"), *Proxy, *TankDataShortCode.ToString());
		return;
	}

	USkeletalMesh* Mesh	= InViewType == EViewType::ThirdPerson ? Data->Mesh3P : Data->Mesh1P;

	if (!Mesh)
	{
		UE_LOG(LogShooter, Warning, TEXT("SetTankMesh (%s): No %s Mesh set for Tank Data: %s"), *Proxy, *UShooterStatics::ViewTypeToString(InViewType), *TankDataShortCode.ToString());
		return;
	}

	InMesh->SetSkeletalMesh(Mesh);

	AShooterTankMaterialSkin* Skin = GetTankMaterialSkin(MaterialSkinShortCode);

	if (!Skin)
	{
		UE_LOG(LogShooter, Warning, TEXT("SetTankMesh (%s): Failed to find Material Skin with Short Code: %s for Tank Data: %s"), *Proxy, *MaterialSkinShortCode.ToString(), *TankDataShortCode.ToString());
		return;
	}

	Skin->SetMaterials(InMesh, InViewType);
}

void AShooterGameState::SetTankMesh(USkeletalMeshComponent* InMesh, TEnumAsByte<EViewType::Type> InViewType, FCharacterInfo& InCharacterInfo)
{
	SetTankMesh(InMesh, InViewType, InCharacterInfo.TankDataShortCode, InCharacterInfo.TankMaterialSkinShortCode);
}

AShooterTankMaterialSkin* AShooterGameState::GetTankMaterialSkin(FName ShortCode)
{
	for (int32 FactionIndex = 0; FactionIndex < 2; FactionIndex++)
	{
		TArray<FName>& LoadedTankMaterialSkinShortCodes			   = FactionIndex == 0 ? LoadedAxisTankMaterialSkinShortCodes : LoadedAllyTankMaterialSkinShortCodes;
		TArray<AShooterTankMaterialSkin*>& LoadedTankMaterialSkins = FactionIndex == 0 ? LoadedAxisTankMaterialSkins : LoadedAllyTankMaterialSkins;

		int32 Index = LoadedTankMaterialSkinShortCodes.Find(ShortCode);

		if (Index != INDEX_NONE)
		{
			return LoadedTankMaterialSkins[Index];
		}
	}

	FString Proxy = UShooterStatics::GetProxyAsString(this);
	UE_LOG(LogShooter, Warning, TEXT("GetTankMaterialSkin (%s): Failed to find Tank Material Skin with ShortCode: %s"), *Proxy, *ShortCode.ToString());
	return NULL;
}

AShooterTankMaterialSkin* AShooterGameState::GetTankMaterialSkin(TEnumAsByte<EFaction::Type> InFaction, FName ShortCode)
{
	TArray<FName>& LoadedTankMaterialSkinShortCodes			   = InFaction == EFaction::GR ? LoadedAxisTankMaterialSkinShortCodes : LoadedAllyTankMaterialSkinShortCodes;
	TArray<AShooterTankMaterialSkin*>& LoadedTankMaterialSkins = InFaction == EFaction::GR ? LoadedAxisTankMaterialSkins : LoadedAllyTankMaterialSkins;

	int32 Index = LoadedTankMaterialSkinShortCodes.Find(ShortCode);

	if (Index != INDEX_NONE)
	{
		return LoadedTankMaterialSkins[Index];
	}

	FString Proxy = UShooterStatics::GetProxyAsString(this);
	UE_LOG(LogShooter, Warning, TEXT("GetTankMaterialSkin (%s): Failed to find Tank Material Skin for Faction: %s with ShortCode: %s"), *Proxy, *UShooterStatics::FactionToString(InFaction), *ShortCode.ToString());
	return NULL;
}

void AShooterGameState::SetMaterialsForTankMesh(USkeletalMeshComponent* InMesh, TEnumAsByte<EViewType::Type> InViewType, TEnumAsByte<EFaction::Type> InFaction, FName ShortCode)
{
	TArray<FName>& LoadedTankMaterialSkinShortCodes = InFaction == EFaction::GR ? LoadedAxisTankMaterialSkinShortCodes : LoadedAllyTankMaterialSkinShortCodes;
	TArray<AShooterTankMaterialSkin*>& LoadedTankMaterialSkins = InFaction == EFaction::GR ? LoadedAxisTankMaterialSkins : LoadedAllyTankMaterialSkins;

	FString Proxy = Role == ROLE_Authority ? TEXT("Server") : TEXT("Client");

	int32 AssetIndex = LoadedTankMaterialSkinShortCodes.Find(ShortCode);

	if (AssetIndex == INDEX_NONE)
	{
		UE_LOG(LogShooter, Warning, TEXT("SetMaterialsForTankMesh (%s): Failed to find Tank Material Skin for %s, Faction: %s, and Short Code: %s"), *Proxy, *UShooterStatics::ViewTypeToString(InViewType), *UShooterStatics::FactionToString(InFaction), *ShortCode.ToString());
		return;
	}

	// 1P
	if (InViewType == EViewType::FirstPerson || InViewType == EViewType::VR)
	{
		const int32 Count = LoadedTankMaterialSkins[AssetIndex]->Materials1P.Num();

		for (int32 Index = 0; Index < Count; Index++)
		{
			InMesh->SetMaterial(Index, LoadedTankMaterialSkins[AssetIndex]->Materials1P[Index]);
		}
	}
	// 3P
	if (InViewType == EViewType::ThirdPerson)
	{
		const int32 Count = LoadedTankMaterialSkins[AssetIndex]->Materials3P.Num();

		for (int32 Index = 0; Index < Count; Index++)
		{
			InMesh->SetMaterial(Index, LoadedTankMaterialSkins[AssetIndex]->Materials3P[Index]);
		}
	}
}

#pragma endregion Tank

class AShooterAntennaData* AShooterGameState::GetAntenna(FName ShortCode)
{
	return GetData<AShooterAntennaData>(TEXT("GetAntenna"), TEXT("Antenna Data"), LoadedAntennas, LoadedAntennaShortCodes, ShortCode);
}

#pragma endregion Loading Assets

#pragma region
void AShooterGameState::LoadPointValues()
{
	FString filename = FPaths::GameDir() + "Content/AlwaysCook/Stats/Stats.json";

	FString statsJson;
	if (FFileHelper::LoadFileToString(statsJson, *filename))
	{
		TSharedPtr<FJsonObject> JsonParsed;
		TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(statsJson);
		if (FJsonSerializer::Deserialize(JsonReader, JsonParsed) && JsonParsed.IsValid())
		{
			TArray<TSharedPtr<FJsonValue>> objArray = JsonParsed->GetArrayField("Point");

			for (int i = 0; i < objArray.Num(); i++)
			{
				TSharedPtr<FJsonValue> value = objArray[i];
				TSharedPtr<FJsonObject> obj = value->AsObject();
				FPointStruct newStruct;

				newStruct.name = FName(*obj->GetStringField("name"));
				newStruct.points = obj->GetNumberField("points");
				newStruct.text = obj->GetStringField("text");
				newStruct.category = obj->GetStringField("category");
				pointArray.Add(newStruct);
			}
		}
	}
}
#pragma endregion PointSystem

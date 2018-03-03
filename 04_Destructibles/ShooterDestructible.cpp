// Fill out your copyright notice in the Description page of Project Settings.

#include "ShooterGame.h"
#include "ShooterDestructible.h"
#include "ShooterProjectile.h"
#include "ShooterSound.h"
#include "ShooterStatics.h"

AShooterDestructible::AShooterDestructible(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SceneComp = ObjectInitializer.CreateDefaultSubobject<USceneComponent>(this, TEXT("SceneComp"));
	RootComponent = SceneComp;

	OriginalMeshComp = ObjectInitializer.CreateDefaultSubobject<UStaticMeshComponent>(this, TEXT("OriginalMeshComp"));
	OriginalMeshComp->AttachToComponent(SceneComp, FAttachmentTransformRules::KeepRelativeTransform);

	DestroyedSkeletalMeshComp = ObjectInitializer.CreateDefaultSubobject<USkeletalMeshComponent>(this, TEXT("DestroyedSkeletalMeshComp"));
	DestroyedSkeletalMeshComp->AttachToComponent(SceneComp, FAttachmentTransformRules::KeepRelativeTransform);
	DestroyedSkeletalMeshComp->MeshComponentUpdateFlag = EMeshComponentUpdateFlag::OnlyTickPoseWhenRendered;

	DestroyedStaticMeshComp = ObjectInitializer.CreateDefaultSubobject<UStaticMeshComponent>(this, TEXT("DestroyedStaticMeshComp"));
	DestroyedStaticMeshComp->AttachToComponent(SceneComp, FAttachmentTransformRules::KeepRelativeTransform);

	DestructableShadowStaticMeshComp = ObjectInitializer.CreateDefaultSubobject<UStaticMeshComponent>(this, TEXT("DestroyedShadowComp"));
	DestructableShadowStaticMeshComp->AttachToComponent(SceneComp, FAttachmentTransformRules::KeepRelativeTransform);

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
#endif

	IsRespawnable = true;
	IsAvailable = true;
	IsReadyToRespawn = true;

	DissolveTime = 1.5f;
	DissolveDelayTime = 0.1f;
	CheckRadius = 300.0f;
	CheckDurationForRespawn = 2.0f;
	ChainDestructionDelayTime = 0.25f;

	MaxRadialDamage = 60.0f;
	MinRadialDamage = 15.0f;

	MaxRadialDamageRadius = 512.0f;
	MinRadialDamageRadius = 256.0f;

	ShadowScaleMult = FVector(1.0f, 1.0f, 1.0f);

	SetReplicates(true);
	//Role = ROLE_AutonomousProxy;
}

void AShooterDestructible::PostInitProperties()
{
	Super::PostInitProperties();

	// instances in Level will try to overwrite the parent' blueprint's HP value (if its derrived from BP)
	// So saving this value for 1 time and use it over for respawnable instances
	// DefaultHP = HP;

#if !WITH_EDITOR
	SetDestructibleState(EDestructableState::UnDestroyed);
#endif
}

void AShooterDestructible::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	CurrentHP = HP;

	UWorld* ActorWorld = GetWorld();
	if (!ActorWorld)
	{
		return;
	}

	AShooterGameMode* GameMode = Cast <AShooterGameMode>(ActorWorld->GetAuthGameMode());
	if (!IsPendingKill() && GameMode)
	{
		GameMode->AddDestructibleActor(this);
	}

	if (DestroyedSkeletalMeshComp)
		DestroyedSkeletalMeshComp->SetComponentTickEnabled(false);

	// in cases, where player joins the game in the middle, make sure it shows the right mesh
	if (!IsAvailable) {
		DestroyOn();
	}
}

void AShooterDestructible::PostUnregisterAllComponents()
{
	Super::PostUnregisterAllComponents();

	UWorld* ActorWorld = GetWorld();
	if (!ActorWorld)
	{
		return;
	}

	AShooterGameMode* GameMode = Cast <AShooterGameMode>(ActorWorld->GetAuthGameMode());
	if (!IsPendingKill() && GameMode)
	{
		GameMode->RemoveDestructibleActor(this);
	}
}

#if WITH_EDITOR
void AShooterDestructible::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Set visibility on only for editor for destroyedMEsh
	

	// Dissolving time to be always smaller than DestructionLifeTime
	CheckRadiusComponent->SetSphereRadius(CheckRadius);

	CheckRadiusComponent->SetWorldLocation(GetActorLocation());
	CheckRadiusComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);

	if (DestroyedStaticMesh && DestroyedStaticMeshComp)
	{
		DestroyedStaticMeshComp->SetVisibility(true);
		DestroyedStaticMeshComp->SetHiddenInGame(true);
		DestroyedStaticMeshComp->SetWorldLocation(GetActorLocation() + DestroyedPositionOffset);
		DestroyedStaticMeshComp->SetWorldRotation(GetActorRotation() + DestroyedRotationOffset);
	}
	else if (DestroyedSkeletalMesh && DestroyedSkeletalMeshComp)
	{
		DestroyedSkeletalMeshComp->SetVisibility(true);
		DestroyedSkeletalMeshComp->SetHiddenInGame(true);
		DestroyedSkeletalMeshComp->SetWorldLocation(GetActorLocation() + DestroyedPositionOffset);
		DestroyedSkeletalMeshComp->SetWorldRotation(GetActorRotation() + DestroyedRotationOffset);
	}

	if (ShadowStaticMesh && DestructableShadowStaticMeshComp)
	{
		DestructableShadowStaticMeshComp->SetVisibility(true);
		DestructableShadowStaticMeshComp->SetHiddenInGame(true);
		DestructableShadowStaticMeshComp->SetWorldLocation(GetActorLocation() + ShadowPositionOffset);
		DestructableShadowStaticMeshComp->SetWorldRotation(GetActorRotation() + ShadowRotationOffset);
		DestructableShadowStaticMeshComp->SetWorldScale3D(ShadowScaleMult);
	}
	
	InitDestructibleMesh();
}

void AShooterDestructible::PostLoad()
{
	Super::PostLoad();

	// Set visibility on only for editor for destroyedMEsh
	if (DestroyedStaticMesh && DestroyedStaticMeshComp)
	{
		DestroyedStaticMeshComp->SetVisibility(true);
		DestroyedStaticMeshComp->SetHiddenInGame(true);
	}
	else if (DestroyedSkeletalMesh && DestroyedSkeletalMeshComp)
	{
		DestroyedSkeletalMeshComp->SetVisibility(true);
		DestroyedSkeletalMeshComp->SetHiddenInGame(true);
	}

	if (ShadowStaticMesh && DestructableShadowStaticMeshComp)
	{
		DestructableShadowStaticMeshComp->SetVisibility(true);
		DestructableShadowStaticMeshComp->SetHiddenInGame(true);
	}

	// Dissolving time to be always smaller than DestructionLifeTime
	CheckRadiusComponent->SetSphereRadius(CheckRadius);

	CheckRadiusComponent->SetWorldLocation(GetActorLocation());
	CheckRadiusComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
}

#endif // END of EDITOR_ONLY

void AShooterDestructible::Reset()
{
	// Set geometry and collision setting to UnDestroyed version
	SetDestructibleState(EDestructableState::UnDestroyed);

	CurrentHP   = HP;
	IsAvailable = true;
}

void AShooterDestructible::DoDamagePawns(AShooterDestructible* Instigator)
{
	UWorld* ActorWorld = GetWorld();
	if (!ActorWorld)
	{
		return;
	}
	AShooterGameMode* GameMode = Cast <AShooterGameMode>(ActorWorld->GetAuthGameMode());
	if (!GameMode)
	{
		return;
	}

	float InstigatorCheckRadius = Instigator->CheckRadius;
	InstigatorCheckRadius *= InstigatorCheckRadius; // make the check radius to be Sq for compare
	FVector InstigatorLocation = Instigator->GetActorLocation();

	for (int32 i = 0; i < GameMode->ShooterDestructibles.Num(); ++i)
	{
		AShooterDestructible* Destructible = GameMode->ShooterDestructibles[i];

		FVector DestructibleLocation = Destructible->GetActorLocation();
		float DistSq = FMath::Pow(DestructibleLocation.X - InstigatorLocation.X, 2) + FMath::Pow(DestructibleLocation.Y - InstigatorLocation.Y, 2);
		bool bIsWithinDist = DistSq < InstigatorCheckRadius;

		if (bIsWithinDist && GameMode->ShooterDestructibles[i]->bDamageByDestructible)
		{
			FHitResult HitResult; // empty hit result, just for function signature requirement to call it.

			FTimerDelegate AttempDestructionDelegate = FTimerDelegate::CreateUObject(Destructible, &AShooterDestructible::TimedAttemptDestruction);
			FTimerHandle UniqueHandle;
			GetWorldTimerManager().SetTimer(UniqueHandle, AttempDestructionDelegate, ChainDestructionDelayTime, false);
		}
	}
}

void AShooterDestructible::DoDamageToNearbyDestructible(AShooterDestructible* Instigator, AController* LastDamagingController)
{
	UWorld* ActorWorld = GetWorld();
	if (!ActorWorld)
	{
		return;
	}
	AShooterGameMode* GameMode = Cast <AShooterGameMode>(ActorWorld->GetAuthGameMode());
	if (!GameMode)
	{
		return;
	}

	float InstigatorCheckRadius = Instigator->CheckRadius;
	InstigatorCheckRadius *= InstigatorCheckRadius; // make the check radius to be Sq for compare
	FVector InstigatorLocation = Instigator->GetActorLocation();

	for (FConstPawnIterator It = GetWorld()->GetPawnIterator(); It; ++It)
	{
		AShooterCharacter* TestPawn = Cast<AShooterCharacter>(*It);
		if (TestPawn)
		{
			FVector DestructibleLocation = Instigator->GetActorLocation();
			FVector PawnLocation = TestPawn->GetActorLocation();
			
			float DistSq = FMath::Pow(PawnLocation.X - InstigatorLocation.X, 2) + FMath::Pow(PawnLocation.Y - InstigatorLocation.Y, 2);
			bool bIsWithinDist = DistSq < InstigatorCheckRadius;

			if (bIsWithinDist)
			{
				FShooterDamageEvent DamageEvent(EDamageType::Explosive);
				TestPawn->ShooterTakeDamage(MaxRadialDamage, DamageEvent, LastDamagingController, LastDamagingController->GetPawn());
			}
		}
	}
}

// created to send to timer - seems like
void AShooterDestructible::TimedAttemptDestruction()
{
	FHitResult HitResult; // empty hit result, just for function signature requirement to call it.
	AttemptDestruction(NULL, HitResult, MaxRadialDamage);
}

void AShooterDestructible::RunDestruction(class AShooterProjectile* Projectile, const FHitResult & SweepResult)
{
	AShooterGameState* GameState = Cast<AShooterGameState>(GetWorld()->GameState);

	if (GameState)
	{
		// Once it is activated, let it become unavailable and not ready to respawn
		IsAvailable = false;
		IsReadyToRespawn = false;

		InitDestructibleMesh();

		if (Projectile != nullptr)
		{
			Projectile->DoRadialDamageDestructible(SweepResult, MinRadialDamage, MaxRadialDamage, MinRadialDamageRadius, MaxRadialDamageRadius);

			DoDamageToNearbyDestructible(this, Projectile->GetInstigatorController());
		}

		// Blueprint Callable Event
		OnDestroyedStart();
		OnDestroyed.Broadcast(this/*DestroyedActor*/);

		if (DestroyedStaticMesh && DestroyedStaticMeshComp)
		{
			DestroyedStaticMeshComp->SetWorldLocation(GetActorLocation() + DestroyedPositionOffset);
			DestroyedStaticMeshComp->SetWorldRotation(GetActorRotation() + DestroyedRotationOffset);
		}
		else if (DestroyedSkeletalMesh && DestroyedSkeletalMeshComp)
		{
			DestroyedSkeletalMeshComp->SetWorldLocation(GetActorLocation() + DestroyedPositionOffset);
			DestroyedSkeletalMeshComp->SetWorldRotation(GetActorRotation() + DestroyedRotationOffset);
		}

		if (ShadowStaticMesh && DestructableShadowStaticMeshComp)
		{
			DestructableShadowStaticMeshComp->SetWorldLocation(GetActorLocation() + ShadowPositionOffset);
			DestructableShadowStaticMeshComp->SetWorldRotation(GetActorRotation() + ShadowRotationOffset);
			DestructableShadowStaticMeshComp->SetWorldScale3D(ShadowScaleMult);
		}

		// Play all the effects that are registered
		PlayDestroyEffects(GameState);

		// Play a single sound that is registered
		PlayDestroySound(GameState);

		// Set geometry and collision setting to Destroyed version
		SetDestructibleState(EDestructableState::Destroyed);

		// If it's respawnable, we have to starting tick to check the 
		if (IsRespawnable)
		{
			// DestructionLifeTime is always dissolve time + dissolve delayed time
			DestructionLifeTime = DissolveTime + DissolveDelayTime;

			float DelayTime = 0.01f;

			// SetTimer to call dissolve animation
			GetWorld()->GetTimerManager().SetTimer(DissolveTimeHandle, this, &AShooterDestructible::HandleDissolve, DissolveDelayTime, false);

			// SetTimer to call handle destroyed object
			GetWorld()->GetTimerManager().SetTimer(RespawnTimeHandle, this, &AShooterDestructible::RespawnDestructible, DestructionLifeTime + DelayTime, false);

			GetWorld()->GetTimerManager().SetTimer(RespawnConditionCheckHandle, this, &AShooterDestructible::CheckRespawnCondition, CheckDurationForRespawn, true);
		}
		else if (DestroyedSkeletalMeshComp && DestroyedAnimation)
		{
			GetWorld()->GetTimerManager().SetTimer(DisableSkeletalMeshTickHandle, this, &AShooterDestructible::DisableSkeletalMeshTick, DestroyedAnimation->SequenceLength, false);
		}
	}
}

// Call from public-side, attempt to damage the detructible and destruct
void AShooterDestructible::AttemptDestruction(class AShooterProjectile* Projectile, const FHitResult & SweepResult, float Damage)
{
	// If its not available for destroy and original mesh is not set, don't do anything
	if (!IsAvailable || !OriginalMesh)
	{
		return;
	}

	DoDamage(Damage);

	if (!IsHPEmpty())
	{
		return;
	}
	
	RunDestruction(Projectile, SweepResult);
}

bool AShooterDestructible::IsHPEmpty()
{
	if (CurrentHP >= 0.0f)
	{
		return false;
	}

	return true;
}

void AShooterDestructible::DoDamage(const float Damage)
{
	if (Damage <= 0.0f)
	{
		return ;
	}

	CurrentHP -= Damage;
}

bool AShooterDestructible::IsDestroyed()
{
	return !IsAvailable;
}

void AShooterDestructible::ForceTriggerDestruction()
{
	FHitResult SweepResult;
	AttemptDestruction( NULL, SweepResult, FLT_MAX );
}

float AShooterDestructible::GetCurrentHP()
{
	return CurrentHP;
}

void AShooterDestructible::SetCurrentHP( float NewHP )
{
	if( !IsDestroyed() )
	{
		CurrentHP = NewHP;
	}
}

void AShooterDestructible::CheckRespawnCondition()
{
	// When there is overlapping, and it's ready to respawn and it's respawnable,
	// let it appear the undestroyed mesh and let the tick disabled and make
	// the IsAvailable to be true so it will be once again available for destructable
	if (!IsOverlapped() && IsReadyToRespawn && !IsAvailable && IsRespawnable)
	{
		// Set geometry and collision setting to UnDestroyed version
		SetDestructibleState(EDestructableState::UnDestroyed);

		//SetActorTickEnabled(false);
		GetWorld()->GetTimerManager().ClearTimer(RespawnConditionCheckHandle);
		CurrentHP = HP;
		IsAvailable = true;
	}
}

bool AShooterDestructible::IsOverlapped()
{
	for (FConstPawnIterator It = GetWorld()->GetPawnIterator(); It; ++It)
	{
		AShooterCharacter* TestPawn = Cast<AShooterCharacter>(*It);
		if (TestPawn)
		{
			const FVector DestructibleLocation = GetActorLocation();

			float DistSq = (DestructibleLocation - TestPawn->GetActorLocation()).SizeSquared();
			float TestRadiusSq = CheckRadius * CheckRadius;

			if (DistSq < TestRadiusSq)
			{
				return true;
			}
		}
	}
	return false;
}

void AShooterDestructible::RespawnDestructible()
{
	IsReadyToRespawn = true;
}

void AShooterDestructible::HandleDissolve()
{
	AnimateDissolve(0.2f, DissolveTime, 25);
}

void AShooterDestructible::DisableSkeletalMeshTick()
{
	check(DestroyedSkeletalMeshComp);
	DestroyedSkeletalMeshComp->SetComponentTickEnabled(false);
}

void AShooterDestructible::DestroyOn()
{
	if (DestroyedStaticMesh && DestroyedStaticMeshComp)
	{
		DestroyedStaticMeshComp->SetVisibility(true, true);
	}
	else if (DestroyedSkeletalMesh && DestroyedSkeletalMeshComp)
	{
		DestroyedSkeletalMeshComp->SetVisibility(true, true);
	}

	if (ShadowStaticMesh && DestructableShadowStaticMeshComp)
	{
		DestructableShadowStaticMeshComp->SetVisibility(true, true);
	}
}

void AShooterDestructible::DestroyOff()
{
	if (DestroyedStaticMesh && DestroyedStaticMeshComp)
	{
		DestroyedStaticMeshComp->SetVisibility(false, true);
	}
	else if (DestroyedSkeletalMesh && DestroyedSkeletalMeshComp)
	{
		DestroyedSkeletalMeshComp->SetVisibility(false, true);
	}

	if (ShadowStaticMesh && DestructableShadowStaticMeshComp)
	{
		DestructableShadowStaticMeshComp->SetVisibility(false, true);
	}
}

void AShooterDestructible::InitDestructibleMesh()
{
	if (OriginalMeshComp)
	{
		OriginalMeshComp->SetStaticMesh(OriginalMesh);
	}

	if (DestroyedStaticMeshComp)
	{
		DestroyedStaticMeshComp->SetStaticMesh(DestroyedStaticMesh);
	}
	
	if (DestroyedSkeletalMeshComp)
	{
		DestroyedSkeletalMeshComp->SetSkeletalMesh(DestroyedSkeletalMesh);
	}

	if (DestructableShadowStaticMeshComp)
	{
		DestructableShadowStaticMeshComp->SetStaticMesh(ShadowStaticMesh);
	}
}

void AShooterDestructible::SetDestructibleState(EDestructableState::Type State)
{
	if (State == EDestructableState::UnDestroyed)
	{
		OriginalMeshComp->SetVisibility(true);
		OriginalMeshComp->SetHiddenInGame(false);

		if (DestroyedStaticMesh && DestroyedStaticMeshComp)
		{
			DestroyedStaticMeshComp->SetVisibility(false);
			DestroyedStaticMeshComp->SetHiddenInGame(true);
		}
		else if (DestroyedSkeletalMesh && DestroyedSkeletalMeshComp)
		{
			DestroyedSkeletalMeshComp->SetVisibility(false);
			DestroyedSkeletalMeshComp->SetHiddenInGame(true);
			DestroyedSkeletalMeshComp->SetComponentTickEnabled(false);
		}

		if (ShadowStaticMesh && DestructableShadowStaticMeshComp)
		{
			DestructableShadowStaticMeshComp->SetVisibility(false);
			DestructableShadowStaticMeshComp->SetHiddenInGame(true);
		}
		/*
		OriginalMeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		DestroyedMeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

		OriginalMeshComp->SetCollisionResponseToAllChannels(ECR_MAX);
		DestroyedMeshComp->SetCollisionResponseToAllChannels(ECR_MAX);
		*/
	}
	else if (State == EDestructableState::Destroyed)
	{
		// when its destroyed and it will not respawn, collision should be completely off for this actor.
		if (!IsRespawnable)
		{
			if (!DestroyedStaticMeshComp->StaticMesh &&  !DestroyedSkeletalMeshComp->SkeletalMesh)
			{
				OriginalMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				DestructableShadowStaticMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			}
		}

		OriginalMeshComp->SetVisibility(false);
		OriginalMeshComp->SetHiddenInGame(true);

		if (DestroyedStaticMesh && DestroyedStaticMeshComp)
		{
			DestroyedStaticMeshComp->SetVisibility(true);
			DestroyedStaticMeshComp->SetHiddenInGame(false);
		}
		else if (DestroyedSkeletalMesh && DestroyedSkeletalMeshComp)
		{
			DestroyedSkeletalMeshComp->SetComponentTickEnabled(true);
			DestroyedSkeletalMeshComp->SetVisibility(true);
			DestroyedSkeletalMeshComp->SetHiddenInGame(false);
		}

		if (ShadowStaticMesh && DestructableShadowStaticMeshComp)
		{
			DestructableShadowStaticMeshComp->SetVisibility(true);
			DestructableShadowStaticMeshComp->SetHiddenInGame(false);
		}

		// Play the destructible animation - ONLY for skeletal mesh
		if (DestroyedSkeletalMesh && DestroyedAnimation)
		{
			DestroyedSkeletalMeshComp->PlayAnimation(DestroyedAnimation, false);
		}

		/*
		// Only when there is no destroyed mesh to be swapped, collision for all the mesh will disabled.
		if (!DestroyedMeshComp->SkeletalMesh)
		{
			
			OriginalMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			DestroyedMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			
			OriginalMeshComp->SetCollisionResponseToAllChannels(ECR_Ignore);
			DestroyedMeshComp->SetCollisionResponseToAllChannels(ECR_Ignore);
			
		}
		*/
	}
}

/* To animate dissolve of an object
@ float MinTime   : Minimum time interval between function stacks
@ float MaxTime   : Maximum time interval between function stacks
@ uint32 MaxCount : Maximum count of function stacks
*/
void AShooterDestructible::AnimateDissolve(const float MinTime, const float MaxTime, int32 MaxCount)
{
	int32 Count = 0;

	float Stepsize;
	while (Count < MaxCount)
	{
		Stepsize = UShooterStatics::MapValueNonLinear(FVector2D(0, MaxCount), FVector2D(MinTime, MaxTime), Count, EGraphType::EaseIn);

		FTimerHandle DestroyOnTimerHandle; // Handle have to be created every time it sets timer to be stacked
		Count += 1;
		GetWorld()->GetTimerManager().SetTimer(DestroyOnTimerHandle, this, &AShooterDestructible::DestroyOn, Stepsize, false);

		Stepsize = UShooterStatics::MapValueNonLinear(FVector2D(0, MaxCount), FVector2D(MinTime, MaxTime), Count, EGraphType::EaseIn);

		FTimerHandle DestroyOffTimerHandle; // Handle have to be created every time it sets timer to be stacked
		Count += 1;
		GetWorld()->GetTimerManager().SetTimer(DestroyOffTimerHandle, this, &AShooterDestructible::DestroyOff, Stepsize, false);
	}
}

void AShooterDestructible::PlayDestroyEffects(AShooterGameState*  GameState)
{
	check(GameState);

	for (int32 i = 0; i < DestructionParticles.Num(); ++i)
	{
		if (!(DestructionParticles[i].ParticleSystem))
		{
			return;
		}

		AShooterEmitter* Emitter = GameState->AllocateAndActivateEmitter(DestructionParticles[i].ParticleSystem, DestructionParticles[i].LifeTime);

		check(Emitter);

		Emitter->TeleportTo(GetActorLocation(), GetActorRotation());
		Emitter->SetActorScale3D(FVector(DestructionParticles[i].Scale, DestructionParticles[i].Scale, DestructionParticles[i].Scale));
	}
}

void AShooterDestructible::PlayDestroySound(AShooterGameState* GameState)
{
	check(GameState);

	if (!DestroySound)
	{
		return;
	}

	bool bIs1PSound = false;
	bool bLooping = false;
	bool bDelay = false;
	bool bSpatialized = true;
	AShooterSound* sound = GameState->AllocateSound(DestroySound, this, bIs1PSound, bLooping, bDelay, bSpatialized, GetActorLocation());
}

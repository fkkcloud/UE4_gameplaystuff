// Fill out your copyright notice in the Description page of Project Settings.

#include "ShooterGame.h"
#include "ShooterStatics.h"
#include "ShooterProjectile.h"

DEFINE_LOG_CATEGORY_STATIC(ShooterEmitterLog, Log, All);

AShooterEmitter::AShooterEmitter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bDestroyOnSystemFinish = false;

	GetParticleSystemComponent()->PrimaryComponentTick.bStartWithTickEnabled = false;
}

void AShooterEmitter::PostActorCreated()
{
	Super::PostActorCreated();

	if (GetParticleSystemComponent())
	{
		GetParticleSystemComponent()->bAutoDestroy = false;
	}
}

#if !UE_BUILD_SHIPPING
void AShooterEmitter::BeginDestroy()
{
	// emitters in the pool should never be destroyed while game is in progress. they should be returned to pool
	if (GetWorld())
	{
		AShooterGameState* GameState = Cast<AShooterGameState>(GetWorld()->GameState);

		if (GameState)
		{
			ensure(GameState->GetMatchState() != MatchState::InProgress);
		}
	}
	Super::BeginDestroy();
}
#endif // #if !UE_BUILD_SHIPPING

void AShooterEmitter::Tick_Internal(float DeltaSeconds)
{
	if (DeathStartTime == 0.0f)
	{
		bool IsUseDrawDistance = true;

		if (IsAttachedFX)
		{
			AShooterProjectile* ParentProjectile = Cast<AShooterProjectile>(GetAttachParentActor());
			if (ParentProjectile != nullptr)
			{
				IsUseDrawDistance = ParentProjectile->IsUseDrawDistance();
			}
		}


		if (DrawDistance > 0.0f && IsUseDrawDistance)
		{
			float DistanceSq = UShooterStatics::GetSquaredDistanceToLocalControllerEye(GetWorld(), GetActorLocation());

			SetActorHiddenInGame(DistanceSq > DrawDistance * DrawDistance);
		}

		// When emitter is in use and attached to projectile, check on the parentActor
		// and if there is no parentActor is there, immediately deallocate from pool
		if (IsAttachedFX)
		{
			AActor* ParentActor = GetAttachParentActor();

			if (!ParentActor)
			{
				if (IsLooping)
				{
					DeathStartTime = GetWorld()->TimeSeconds;

					if (GetParticleSystemComponent())
					{
						GetParticleSystemComponent()->DeactivateSystem();
					}
#if !UE_BUILD_SHIPPING
					else
					{
						if (Last_Template)
						{
							UE_LOG(ShooterEmitterLog, Warning, TEXT("Emitter Tick_Internal: ParticleSystemComponent is NULL for looping emitter. Last Template was %s"), *Last_Template->GetName());
						}
						else
						{
							UE_LOG(ShooterEmitterLog, Warning, TEXT("Emitter Tick_Internal: ParticleSystemComponent is NULL for looping emitter"));
						}
					}
#endif // #if !UE_BUILD_SHIPPING
				}
				else
				{
					DeallocateFromPool();
				}

#if !UE_BUILD_SHIPPING
				if (GetWorld()->TimeSeconds - SpawnTime / (float)LifeTime < 0.02)
				{
					UE_LOG(ShooterEmitterLog, Warning, TEXT("EMITTER LOSING PARENT :: It lost its parent when it is almost born"));
				}
#endif // #if !UE_BUILD_SHIPPING
			}

			if (HasOwner)
			{
				if (!GetOwner())
				{
					if (IsLooping)
					{
						DeathStartTime = GetWorld()->TimeSeconds;

						if (GetParticleSystemComponent())
						{
							GetParticleSystemComponent()->DeactivateSystem();
						}
#if !UE_BUILD_SHIPPING
						else
						{
							if (Last_Template)
							{
								UE_LOG(ShooterEmitterLog, Warning, TEXT("Emitter Tick_Internal: ParticleSystemComponent is NULL for looping emitter. Last Template was %s"), *Last_Template->GetName());
							}
							else
							{
								UE_LOG(ShooterEmitterLog, Warning, TEXT("Emitter Tick_Internal: ParticleSystemComponent is NULL for looping emitter"));
							}
						}
#endif // #if !UE_BUILD_SHIPPING
					}
					else
					{
						DeallocateFromPool();
					}
				}
				else
				{
					AShooterCharacter* MyOwner = Cast<AShooterCharacter>(GetOwner());

					if (MyOwner && (!MyOwner->IsAlive() || !MyOwner->IsActiveAndFullyReplicated))
					{
						if (IsLooping)
						{
							DeathStartTime = GetWorld()->TimeSeconds;

							if (GetParticleSystemComponent())
							{
								GetParticleSystemComponent()->DeactivateSystem();
							}
#if !UE_BUILD_SHIPPING
							else
							{
								if (Last_Template)
								{
									UE_LOG(ShooterEmitterLog, Warning, TEXT("Emitter Tick_Internal: ParticleSystemComponent is NULL for looping emitter. Last Template was %s"), *Last_Template->GetName());
								}
								else
								{
									UE_LOG(ShooterEmitterLog, Warning, TEXT("Emitter Tick_Internal: ParticleSystemComponent is NULL for looping emitter"));
								}
							}
#endif // #if !UE_BUILD_SHIPPING
						}
						else
						{
							DeallocateFromPool();
						}
					}
				}
			}
		}
	}

	// Looping
	if (IsLooping)
	{
		// Check if the particle should slowly die out when ending
		if (DeathTime > 0.0f)
		{
			if (DeathStartTime > 0.0f)
			{
				if (GetWorld()->TimeSeconds - DeathStartTime > DeathTime)
				{
					DeallocateFromPool();
				}
			}
			else
			{
				if (LifeTime > 0.0f &&
					GetWorld()->TimeSeconds - SpawnTime > LifeTime)
				{
					DeathStartTime = GetWorld()->TimeSeconds;

					if (GetParticleSystemComponent())
					{
						GetParticleSystemComponent()->DeactivateSystem();
					}
				}
			}
		}
		else
		{
			if (LifeTime > 0.0f &&
				GetWorld()->TimeSeconds - SpawnTime > LifeTime)
			{
				DeallocateFromPool();
			}
		}
	}
	// Non-Looping
	else
	{
		if (GetWorld()->TimeSeconds - SpawnTime > LifeTime)
		{
			DeallocateFromPool();
		}
	}
}

void AShooterEmitter::ResetEmitter()
{
	if (GetParticleSystemComponent())
	{
		GetParticleSystemComponent()->SetComponentTickEnabled(false);
		GetParticleSystemComponent()->CustomTimeDilation = 1.f;
	}
#if !UE_BUILD_SHIPPING
	else
	{
		if (Last_Template)
		{
			UE_LOG(ShooterEmitterLog, Warning, TEXT("Emitter ResetEmitter: ParticleSystemComponent is NULL. Last Template was %s"), *Last_Template->GetName());
		}
		else
		{
			UE_LOG(ShooterEmitterLog, Warning, TEXT("Emitter ResetEmitter: ParticleSystemComponent is NULL"));
		}
	}
#endif // #if !UE_BUILD_SHIPPING

	SetActorRelativeLocation(FVector::ZeroVector);
	SetActorRelativeRotation(FRotator::ZeroRotator);
	SetActorRelativeScale3D(FVector(1.0f));
	SetActorScale3D(FVector(1.0f));
	SetActorLocation(FVector(10000.0f, 10000.0f, 10000.0f));
	SetActorHiddenInGame(true);
	SetTemplate(NULL);
	IsAvailable  = true;
	IsLooping = false;
	CustomTimeDilation = 1.f;

	SpawnTime      = 0.0f;
	LifeTime       = 0.0f;
	DeathTime	   = 0.0f;
	DeathStartTime = 0.0f;
	IsAttachedFX   = false;
	HasOwner	   = false;

	SetOwner(NULL);
}

void AShooterEmitter::AllocateFromPool(UParticleSystem* Template, float InLifetime, bool InIsAttachedFX, AActor* Parent, FName BoneName, float Scale)
{
	SetActorHiddenInGame(false);
	SetTemplate(Template);
	Last_Template = Template;
	IsAvailable = false;

	SpawnTime = GetWorld()->TimeSeconds;
	LifeTime = InLifetime;
	IsAttachedFX = InIsAttachedFX;

	if (IsAttachedFX)
	{
		AttachToActor(Parent, FAttachmentTransformRules::SnapToTargetIncludingScale, BoneName);
		SetOwner(Parent);

		if (Parent)
			HasOwner = true;
	}

	if (GetParticleSystemComponent())
	{
		GetParticleSystemComponent()->SetComponentTickEnabled(true);
		GetParticleSystemComponent()->SetVisibility(true);
		GetParticleSystemComponent()->SetHiddenInGame(false);
		GetParticleSystemComponent()->Activate();
		GetParticleSystemComponent()->bAutoDestroy = false;
		bCurrentlyActive = true;
	}

	if (Scale == 0.0f)
	{
		Scale = 1.0f;
	}
	SetActorScale3D(FVector(Scale, Scale, Scale));

#if !UE_BUILD_SHIPPING
	// this is debug purpose, turning off all the PS for performance profiling
	// this will hide / turn off visibility of all the dynamic particle creations / not evironment ones.
	AShooterGameState* SGS = Cast<AShooterGameState>(GetWorld()->GetGameState());
	if (SGS && SGS->bUseFXDebug)
	{
		SetActorHiddenInGame(true);
		GetParticleSystemComponent()->SetVisibility(false);
		GetParticleSystemComponent()->SetHiddenInGame(true);
	}
#endif // #if !UE_BUILD_SHIPPING
}

/*
* Deallocation from pool means here is that,
* it will make the AShooterEmitter instance's member variable 'IsAvailable' to be true,
* and make it's particle system to be deactivated until it will be assigned when "allocated" again.
*/
void AShooterEmitter::DeallocateFromPool()
{
	if (GetParticleSystemComponent())
	{
		GetParticleSystemComponent()->SetVisibility(false);
		GetParticleSystemComponent()->KillParticlesForced();
		GetParticleSystemComponent()->SetHiddenInGame(true);
		GetParticleSystemComponent()->Deactivate();
		bCurrentlyActive = false;
	}
#if !UE_BUILD_SHIPPING
	else
	{
		if (Last_Template)
		{
			UE_LOG(ShooterEmitterLog, Warning, TEXT("Emitter DeallocateFromPool: ParticleSystemComponent is NULL. Last Template was %s"), *Last_Template->GetName());
		}
		else
		{
			UE_LOG(ShooterEmitterLog, Warning, TEXT("Emitter DeallocateFromPool: ParticleSystemComponent is NULL"));
		}
	}
#endif // #if !UE_BUILD_SHIPPING

	ResetEmitter();
	DetachRootComponentFromParent();
}

void AShooterEmitter::FellOutOfWorld(const class UDamageType& dmgType)
{
#if !UE_BUILD_SHIPPING
	// * operator is used to return TCHAR
	float EmitterCurrentAge = GetWorld()->TimeSeconds - SpawnTime;
	UE_LOG(ShooterEmitterLog, Warning, TEXT("EMITTER FeelOutWorld :: Getting deallocated..\nCurrentAge:%.2f\nLocation:%s"), EmitterCurrentAge, *GetActorLocation().ToCompactString());
#endif

	// don't do killz with any emitters in pool. we manage them ourselves
	DeallocateFromPool();
}

void AShooterEmitter::OutsideWorldBounds()
{
#if !UE_BUILD_SHIPPING
	float EmitterCurrentAge = GetWorld()->TimeSeconds - SpawnTime;
	UE_LOG(ShooterEmitterLog, Warning, TEXT("EMITTER OutsideWorldBounds :: Getting deallocated..\nCurrentAge:%.2f\nLocation:%s"), EmitterCurrentAge, *GetActorLocation().ToCompactString());
#endif

	// don't do destroy() with any emitters in pool. we manage them ourselves
	DeallocateFromPool();
}

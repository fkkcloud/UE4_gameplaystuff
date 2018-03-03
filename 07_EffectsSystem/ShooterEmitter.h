#pragma once
#include "ShooterEmitter.generated.h"

/**
 * 
 */
UCLASS()
class AShooterEmitter : public AEmitter
{
	GENERATED_UCLASS_BODY()

	float SpawnTime;
	float LifeTime;
	float DrawDistance;
	float DeathTime;
	float DeathStartTime;

	bool IsAvailable;
	bool IsLooping;
	bool IsAttachedFX;
	bool HasOwner;

	class UParticleSystem* Last_Template;

#if !UE_BUILD_SHIPPING
	virtual void BeginDestroy() override;
#endif // #if !UE_BUILD_SHIPPING

	//void virtual ReceiveTick(float DeltaSeconds) override;

	virtual void PostActorCreated() override;

	// Called from ShooterGameState
	void Tick_Internal(float DeltaSeconds);

	void ResetEmitter();
	void AllocateFromPool(UParticleSystem* Template, float InLifetime, bool InIsAttachedFX, AActor* Parent, FName BoneName, float Scale);
	void DeallocateFromPool();

	/** called when the actor falls out of the world 'safely' (below KillZ and such) */
	virtual void FellOutOfWorld(const class UDamageType& dmgType) override;

	virtual void OutsideWorldBounds() override;
};

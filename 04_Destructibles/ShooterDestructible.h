// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/Actor.h"
#include "ShooterDestructible.generated.h"

/** Defining a enumeration within a namespace for destructible mesh state types */
namespace EDestructableState
{
	enum Type
	{
		UnDestroyed			UMETA(DisplayName = "Dirt"),
		Destroyed			UMETA(DisplayName = "Metal"),
		EDestructableState_MAX UMETA(Hidden),
	};
}

UCLASS()
class AShooterDestructible : public AActor
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITOR
	/** visible indicator - radius to check if any pawn is nearby */
	USphereComponent* CheckRadiusComponent;
#endif

	UPROPERTY()
	USceneComponent* SceneComp;

	UPROPERTY()
	UStaticMeshComponent* OriginalMeshComp;

	UPROPERTY()
	UStaticMeshComponent* DestroyedStaticMeshComp;

	UPROPERTY()
	UStaticMeshComponent* DestructableShadowStaticMeshComp;

	UPROPERTY()
	USkeletalMeshComponent* DestroyedSkeletalMeshComp;

	/** Original Mesh to be destroyed */
	UPROPERTY(EditAnywhere, Category = "Setting")
	UStaticMesh* OriginalMesh;

	/** Destroyed mesh without any animation - Static */
	UPROPERTY(EditAnywhere, Category = "Setting")
	UStaticMesh* DestroyedStaticMesh;

	/** Shadow of destructable mesh */
	UPROPERTY(EditAnywhere, Category = "Setting")
	UStaticMesh* ShadowStaticMesh;

	/** Destroyed mesh with animation - Skeletal */
	UPROPERTY(EditAnywhere, Category = "Setting")
	USkeletalMesh* DestroyedSkeletalMesh;

	/** Destroyed mesh's position offset */
	UPROPERTY(EditAnywhere, Category = "Setting")
	FVector DestroyedPositionOffset;

	/** Destroyed mesh's rotation offset */
	UPROPERTY(EditAnywhere, Category = "Setting")
	FRotator DestroyedRotationOffset;

	/** Destroyed mesh's shadow position offset */
	UPROPERTY(EditAnywhere, Category = "Setting")
	FVector ShadowPositionOffset;

	/** Destroyed mesh's shadow rotation offset */
	UPROPERTY(EditAnywhere, Category = "Setting")
	FRotator ShadowRotationOffset;

	/** Destroyed mesh's shadow scale multiplication */
	UPROPERTY(EditAnywhere, Category = "Setting")
	FVector ShadowScaleMult;

	/** Destroyed mesh */
	UPROPERTY(EditAnywhere, Category = "Setting")
	UAnimSequenceBase* DestroyedAnimation;

	UPROPERTY(EditDefaultsOnly, Category = "Setting")
	USoundCue* DestroySound;
	
	/** Is it respawn-able after it gets destroyed */
	UPROPERTY(EditAnywhere, Category = "Setting")
	TArray<FEffectsElement> DestructionParticles;

	/** Let blueprint know that we were launched */
	UFUNCTION(BlueprintImplementableEvent)
	void OnDestroyedStart();

	/** Let blueprint know that we were launched */
	UFUNCTION(BlueprintCallable, Category = "DestructibleEvent")
	bool IsDestroyed();

	/** Allow us to trigger the destruction of this object from with in blueprint */
	UFUNCTION(BlueprintCallable, Category = "DestructibleEvent")
	void ForceTriggerDestruction();

	/** Function to get current HP **/
	UFUNCTION(BlueprintPure, Category = "Health")
	float GetCurrentHP();

	UFUNCTION(BlueprintCallable, Category = "Health")
	void SetCurrentHP( float NewHP );

	/** Is it respawn-able after it gets destroyed */
	UPROPERTY(EditAnywhere, Category = "Setting")
	bool IsRespawnable;

	/** How long it would take to disappear in seconds after it gets destroyed */
	UPROPERTY(EditAnywhere, Category = "Setting", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float DissolveDelayTime;

	/** hp property of destructible. Considered to be once shot for Tank and other to multiple */
	UPROPERTY(EditAnywhere, Category = "Setting", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float HP;

	/** Min radial damage property of destructible. Considered to be once shot for Tank and other to multiple */
	UPROPERTY(EditAnywhere, Category = "Setting", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MinRadialDamage;

	/** Max radial damage property of destructible. Considered to be once shot for Tank and other to multiple */
	UPROPERTY(EditAnywhere, Category = "Setting", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MaxRadialDamage;

	/** Min Radial Damage Radius property of destructible. */
	UPROPERTY(EditAnywhere, Category = "Setting", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MinRadialDamageRadius;

	/** Max Radial Damage Radius property of destructible.  */
	UPROPERTY(EditAnywhere, Category = "Setting", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MaxRadialDamageRadius;

	/** Radius in unit, how far it will affect the actors to damage them. (e.g. Match this value with MaxRadialDamageRadius and MinRadialDamageRadius for good gradient damage range) */
	UPROPERTY(EditAnywhere, Category = "Setting", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float CheckRadius;

	/** Checking this box will allow this destrucble's destruct to damage destructible within check radius */
	UPROPERTY(EditAnywhere, Category = "Setting")
	bool bDamageByDestructible;

	/** How long it would take to activate secondary destruction in seconds (e.g. Usually for chain destruction of destructibles) */
	UPROPERTY(EditAnywhere, Category = "Setting", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ChainDestructionDelayTime;

	/** Armor Type */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setting")
	TEnumAsByte<EArmorType::Type> ArmorType;

	/** Indicates that it can be destroyed */
	bool IsAvailable;

	/** To be called at ShooterProjectile side when OnCollision() */
	void AttemptDestruction(class AShooterProjectile* Projectile, const FHitResult & SweepResult, float Damage);

	/** Do Damage */
	void DoDamage(const float Damage);

	/** to be called to check that HP is empty or not. */
	bool IsHPEmpty();


#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostLoad() override;
#endif
	// virtual void Tick(float DeltaSeconds) override;
	virtual void PostInitProperties() override;
	virtual void PostInitializeComponents() override;
	virtual void PostUnregisterAllComponents() override;

	void Reset();

private:
	// Indicates that is is ready for respawn
	bool IsReadyToRespawn;

	// initial time how long it will take to get dissolved
	float DissolveTime; 

	/** How long it would take to disappear in seconds after it gets destroyed */
	float DestructionLifeTime;

	/** Current HP of actor */
	float CurrentHP;

	float CheckDurationForRespawn;

	FTimerHandle RespawnConditionCheckHandle;
	void CheckRespawnCondition();

	FTimerHandle RespawnTimeHandle;
	void RespawnDestructible();

	FTimerHandle DissolveTimeHandle;
	void HandleDissolve();

	FTimerHandle DisableSkeletalMeshTickHandle;
	void DisableSkeletalMeshTick();

	void DoDamageToNearbyDestructible(AShooterDestructible* Instigator, AController* LastDamagingController);

	void DoDamagePawns(AShooterDestructible* Instigator);

	void RunDestruction(class AShooterProjectile* Projectile, const FHitResult & SweepResult);

	void TimedAttemptDestruction();

	void PlayDestroyEffects(class AShooterGameState * GameState);
	void PlayDestroySound(class AShooterGameState * GameState);

	bool IsOverlapped();

	void DestroyOn();
	void DestroyOff();

	void InitDestructibleMesh();
	void SetDestructibleState(const EDestructableState::Type State);

	/* To animate dissolve of an object
	@ float MinTime   : Minimum time interval between function stacks
	@ float MaxTime   : Maximum time interval between function stacks
	@ uint32 MaxCount : Maximum count of function stacks
	*/
	void AnimateDissolve(const float MinTime, const float MaxTime, int32 MaxCount);
};

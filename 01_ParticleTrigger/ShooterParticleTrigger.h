// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/Actor.h"
#include "ShooterParticleTrigger.generated.h"

UCLASS()
class SHOOTERGAME_API AShooterParticleTrigger : public AActor
{
	GENERATED_UCLASS_BODY()
	
public:	

	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
	// Called every frame
	virtual void Tick( float DeltaSeconds ) override;

	void Run();

	virtual void PostInitializeComponents() override;

	FTimerHandle ParticleTriggerTimerHandle;
	FTimerHandle SoundTriggerTimerHandle;
	FTimerHandle TrailTriggerTimerHandle;
	FTimerHandle DistChecktimerHandle;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	/** visible indicator - radius to check if any pawn is nearby */
	USphereComponent* CheckRadiusComponent;
#endif

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	// A UBillboardComponent to hold Icon sprite
	UBillboardComponent* SpriteComponentUIView;
#endif // WITH_EDITORONLY_DATA

	UPROPERTY()
	USceneComponent* SceneComp;

	UPROPERTY()
	UStaticMeshComponent* TrailHeadMeshComponent;

	/* Will be reset and ready for another particle trigger in this second. E.g 5 will be 'Trigger this particle after 5second after it has been triggered. */
	UPROPERTY(EditAnywhere, Category = "Setting", meta = (ClampMin = "5.0", UIMin = "5.0"))
	float ResetTime;

	/* How far it will detect player to be triggered in unit*/
	UPROPERTY(EditAnywhere, Category = "Setting", meta = (ClampMin = "100.0", UIMin = "100.0"))
	float CheckRadius;

	/* How long trail will live*/
	UPROPERTY(EditAnywhere, Category = "Setting - FX")
	bool bEnableFX;

	/* Particle position. For particle, if its local-space, it will respect source's rotation as well. */
	UPROPERTY(EditAnywhere, Category = "Setting - FX")
	ATargetPoint* ParticlesPosition;

	/* Particles can be added multiple */
	UPROPERTY(EditAnywhere, Category = "Setting - FX")
	TArray<FEffectsElement> Particles;

	/* How long trail will live*/
	UPROPERTY(EditAnywhere, Category = "Setting - Sound")
	bool bEnableSound;

	/* Sound position */
	UPROPERTY(EditAnywhere, Category = "Setting - Sound")
	ATargetPoint* SoundPosition;

	/* Sound that will be triggered */
	UPROPERTY(EditAnywhere, Category = "Setting - Sound")
	USoundCue* Sound;

	/* How long trail will live*/
	UPROPERTY(EditAnywhere, Category = "Setting - Trail")
	bool bEnableTrail;

	/* How long trail will take to get to end position from start position - speed*/
	UPROPERTY(EditAnywhere, Category = "Setting - Trail", meta = (ClampMin = "1.0", UIMin = "1.0"))
		float TrailAnimationDuration;	

	/* Object e.g. bullet's start position */
	UPROPERTY(EditAnywhere, Category = "Setting - Trail")
	ATargetPoint* TrailStartPosition;

	/* Object e.g. bullet's end position */
	UPROPERTY(EditAnywhere, Category = "Setting - Trail")
	ATargetPoint* TrailEndPosition;

	/* Object e.g. bullet's mesh */
	UPROPERTY(EditAnywhere, Category = "Setting - Trail")
	UStaticMesh* TrailHeadMesh;

	/* Uniform size scale for trail head mesh */
	UPROPERTY(EditAnywhere, Category = "Setting - Trail", meta = (ClampMin = "0.000001", UIMin = "0.000001"))
	float MeshUniformSize;

	/* Align the headmesh toward the end trail point */
	UPROPERTY(EditAnywhere, Category = "Setting - Trail", meta = (ClampMin = "1.0", UIMin = "1.0"))
	bool bAlignHeadMeshToTrail;

	/* trail mesh rotation */
	UPROPERTY(EditAnywhere, Category = "Setting - Trail")
	FRotator TrailMeshRotationOffset;

	/* Particle that will be attached to the trail head mesh*/
	UPROPERTY(EditAnywhere, Category = "Setting - Trail")
	FEffectsElement TrailParticle;

protected:
	void PlayParticles();
	void PlaySound();
	void PlayTrail();

	void AnimateTrail();
	void SetupTrail();

	void ResetParticles();
	void ResetSound();
	void ResetTrail();

	bool IsPlayerWithinDistance();
	void InitTrailParticle();

private:
	FVector TrailOriginalLocation;
	FVector TrailShootDir;
	FVector TrailShootDirN;

	float AnimationStartTime;
	float AnimationEndTime;

	float CheckDistTime;
	bool bIsParticleAvailable;
	bool bIsSoundAvailable;

	bool bIsTrailAvailable;
	bool bIsTrailAnimate;
};

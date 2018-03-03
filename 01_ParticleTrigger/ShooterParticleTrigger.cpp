// Fill out your copyright notice in the Description page of Project Settings.

#include "ShooterGame.h"
#include "ShooterSound.h"
#include "ShooterParticleTrigger.h"

AShooterParticleTrigger::AShooterParticleTrigger(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	bIsParticleAvailable = true;
	bIsSoundAvailable = true;
	bIsTrailAvailable = true;

	CheckRadius = 300.0f;

	CheckDistTime = 500.0f;

	ResetTime = 5.0f;

	MeshUniformSize = 1.0f;

	TrailMeshRotationOffset = FRotator::ZeroRotator;

	SceneComp = ObjectInitializer.CreateDefaultSubobject<USceneComponent>(this, TEXT("SceneComp"));
	RootComponent = SceneComp;

	TrailHeadMeshComponent = ObjectInitializer.CreateDefaultSubobject<UStaticMeshComponent>(this, TEXT("TrailHeadMeshComponent"));
	TrailHeadMeshComponent->SetVisibility(false);
	TrailHeadMeshComponent->SetCastShadow(false);
	TrailHeadMeshComponent->bCastDynamicShadow = false;

#if WITH_EDITOR
	// Define capsule component attributes
	CheckRadiusComponent = ObjectInitializer.CreateDefaultSubobject<USphereComponent>(this, TEXT("CheckRadiusSphere"));
	CheckRadiusComponent->ShapeColor = FColor(10, 138, 255, 200);
	CheckRadiusComponent->bDrawOnlyIfSelected = true;
	CheckRadiusComponent->InitSphereRadius(CheckRadius);
	CheckRadiusComponent->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	CheckRadiusComponent->bShouldCollideWhenPlacing = false;
	CheckRadiusComponent->bShouldUpdatePhysicsVolume = false;
	CheckRadiusComponent->Mobility = EComponentMobility::Static;
#endif

#if WITH_EDITORONLY_DATA
	// Structure to hold one-time initialization
	struct FConstructorStaticsUIVienIcon
	{
		// A helper class object we use to find target UTexture2D object in resource package
		ConstructorHelpers::FObjectFinderOptional<UTexture2D> UIViewDefaultObject;

		// Icon sprite category name
		FName ID_UIVIEWIcon;

		// Icon sprite display name
		FText NAME_UIVIEWIcon;

		FConstructorStaticsUIVienIcon()
			// Use helper class object to find the texture
			// "/Engine/EditorResources/S_Note" is resource path
			: UIViewDefaultObject(TEXT("/Game/UI/GameModeHelper/ParticleTriggerIcon"))
			, ID_UIVIEWIcon(TEXT("UIViewIcon"))
			, NAME_UIVIEWIcon(NSLOCTEXT("SpriteCategory", "UIViewIcon", "UIViewIcon"))
		{
		}
	};
	static FConstructorStaticsUIVienIcon ConstructorStaticsUIViewIcon;

	SpriteComponentUIView = ObjectInitializer.CreateEditorOnlyDefaultSubobject<UBillboardComponent>(this, TEXT("ShooterUIViewIcon"));
	if (SpriteComponentUIView)
	{
		SpriteComponentUIView->Sprite = ConstructorStaticsUIViewIcon.UIViewDefaultObject.Get();		// Get the sprite texture from helper class object
		SpriteComponentUIView->SpriteInfo.Category = ConstructorStaticsUIViewIcon.ID_UIVIEWIcon;		// Assign sprite category name
		SpriteComponentUIView->SpriteInfo.DisplayName = ConstructorStaticsUIViewIcon.NAME_UIVIEWIcon;	// Assign sprite display name
		SpriteComponentUIView->Mobility = EComponentMobility::Movable;
		SpriteComponentUIView->bHiddenInGame = true;
		SpriteComponentUIView->RelativeScale3D = FVector(1.75f, 1.75f, 1.75f);
		SpriteComponentUIView->bAbsoluteScale = true;
		SpriteComponentUIView->SetupAttachment(SceneComp);
		SpriteComponentUIView->bIsScreenSizeScaled = true;
		SpriteComponentUIView->SetVisibility(true);
	}
#endif // WITH_EDITORONLY_DATA
}

#if WITH_EDITOR
void AShooterParticleTrigger::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Dissolving time to be always smaller than DestructionLifeTime
	CheckRadiusComponent->SetSphereRadius(CheckRadius);

	CheckRadiusComponent->SetWorldLocation(GetActorLocation());
	CheckRadiusComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
}
#endif // END of EDITOR_ONLY

// Called when the game starts or when spawned
void AShooterParticleTrigger::BeginPlay()
{
	Super::BeginPlay();

	SetupTrail();

	GetWorld()->GetTimerManager().SetTimer(DistChecktimerHandle, this, &AShooterParticleTrigger::Run, 1.0f, true);
}

// Called every frame
void AShooterParticleTrigger::Tick( float DeltaTime )
{
	Super::Tick( DeltaTime );

	AnimateTrail();
}

void AShooterParticleTrigger::PostInitializeComponents()
{
	Super::PostInitializeComponents();
}

void AShooterParticleTrigger::SetupTrail()
{
	if (TrailEndPosition && TrailStartPosition){

		TrailShootDir = TrailEndPosition->GetActorLocation() - TrailStartPosition->GetActorLocation();

		FLookAtMatrix AimMatrix(TrailStartPosition->GetActorLocation(), TrailEndPosition->GetActorLocation(), FVector(0.0f, 0.0f, 1.0f));

		FRotator LookAtRot = AimMatrix.Rotator();
		if (bAlignHeadMeshToTrail){
			TrailHeadMeshComponent->AddRelativeRotation(LookAtRot);
		}

		// Add Mesh Offset
		TrailHeadMeshComponent->AddRelativeRotation(TrailMeshRotationOffset);

	}
	else {

		TrailShootDir = FVector(0.0, 0.0, 0.0);

	}

	if (TrailHeadMeshComponent && TrailHeadMesh){
		TrailHeadMeshComponent->SetStaticMesh(TrailHeadMesh);
		TrailHeadMeshComponent->SetWorldScale3D(FVector(MeshUniformSize, MeshUniformSize, MeshUniformSize));
	}

	if (TrailStartPosition)
		TrailOriginalLocation = TrailStartPosition->GetActorLocation();
}

void AShooterParticleTrigger::PlayParticles()
{
	if (!bEnableFX)
		return;

	if (!bIsParticleAvailable)
		return;

	bIsParticleAvailable = false;

	AShooterGameState* GameState = Cast<AShooterGameState>(GetWorld()->GameState);
	check(GameState);

	for (int32 i = 0; i < Particles.Num(); ++i)
	{
		if (!(Particles[i].ParticleSystem))
		{
			return;
		}

		AShooterEmitter* Emitter = GameState->AllocateAndActivateEmitter(Particles[i].ParticleSystem, Particles[i].LifeTime);

		check(Emitter);

		FVector EffectsLocation = ParticlesPosition ? ParticlesPosition->GetActorLocation() : GetActorLocation();

		// for local based rotation of particle system
		FRotator EffectsRotation = ParticlesPosition->GetActorRotation();

		Emitter->TeleportTo(EffectsLocation, EffectsRotation);

		GetWorld()->GetTimerManager().SetTimer(ParticleTriggerTimerHandle, this, &AShooterParticleTrigger::ResetParticles, ResetTime, false);
	}
}

void AShooterParticleTrigger::PlaySound()
{
	if (!bEnableSound)
		return;

	if (!bIsSoundAvailable)
		return;

	bIsSoundAvailable = false;

	AShooterGameState* GameState = Cast<AShooterGameState>(GetWorld()->GameState);
	check(GameState);

	if (!Sound)
	{
		return;
	}

	FVector SoundLocation = SoundPosition ? SoundPosition->GetActorLocation() : GetActorLocation();

	bool bIs1PSound = false;
	bool bLooping = false;
	bool bDelay = false;
	bool bSpatialized = true;
	AShooterSound* ShooterSound = GameState->AllocateSound(Sound, GetOwner(), bIs1PSound, bLooping, bDelay, bSpatialized, SoundLocation);
	//check(ShooterSound);

	GetWorld()->GetTimerManager().SetTimer(SoundTriggerTimerHandle, this, &AShooterParticleTrigger::ResetSound, ResetTime, false);
}

void AShooterParticleTrigger::PlayTrail() // start animating the trail
{
	if (!bEnableTrail)
		return;

	if (!bIsTrailAvailable)
		return;

	AnimationStartTime = GetWorld()->TimeSeconds;
	AnimationEndTime = AnimationStartTime + TrailAnimationDuration;

	bIsTrailAvailable = false;

	TrailHeadMeshComponent->SetVisibility(true);
	TrailHeadMeshComponent->SetWorldLocation(TrailOriginalLocation);

	InitTrailParticle();

	bIsTrailAnimate = true;

	GetWorld()->GetTimerManager().SetTimer(ParticleTriggerTimerHandle, this, &AShooterParticleTrigger::ResetTrail, TrailAnimationDuration, false);
}

void AShooterParticleTrigger::InitTrailParticle()
{
	if (!TrailParticle.ParticleSystem)
		return;

	AShooterGameState* GameState = Cast<AShooterGameState>(GetWorld()->GameState);
	check(GameState);

	AShooterEmitter* Emitter = GameState->AllocateAndActivateEmitter(TrailParticle.ParticleSystem, TrailAnimationDuration);

	check(Emitter);

	Emitter->SetActorScale3D(FVector(TrailParticle.Scale, TrailParticle.Scale, TrailParticle.Scale));
	Emitter->AttachToComponent(TrailHeadMeshComponent, FAttachmentTransformRules::KeepRelativeTransform);
	Emitter->TeleportTo(TrailOriginalLocation, FRotator::ZeroRotator);
}

void AShooterParticleTrigger::AnimateTrail()
{
	if (!bIsTrailAnimate)
		return;

	float CurrentDeltaTime = GetWorld()->TimeSeconds - AnimationStartTime;
	float AnimationStatus = FMath::Clamp(CurrentDeltaTime / TrailAnimationDuration, 0.0f, 1.0f);

	//float LocationBias = FMath::Lerp(0.0f, 1.0f, AnimationStatus);

	float LocationBias = FMath::InterpEaseOut(0.0f, 1.0f, AnimationStatus, 1.2f);

	// move head mesh from start to end within given time
	FVector CurrLocation = TrailOriginalLocation + (TrailShootDir * LocationBias);
	TrailHeadMeshComponent->SetWorldLocation(CurrLocation);
}

void AShooterParticleTrigger::ResetParticles()
{
	bIsParticleAvailable = true;
}

void AShooterParticleTrigger::ResetSound()
{
	bIsSoundAvailable = true;
}

void AShooterParticleTrigger::ResetTrail()
{
	bIsTrailAvailable = true;
	bIsTrailAnimate   = false;

	TrailHeadMeshComponent->SetVisibility(false);

	// reset location of the head mesh comp
	TrailHeadMeshComponent->SetWorldLocation(TrailStartPosition->GetActorLocation());
}

bool AShooterParticleTrigger::IsPlayerWithinDistance()
{
	for (FConstPawnIterator It = GetWorld()->GetPawnIterator(); It; ++It)
	{
		AShooterCharacter* TestPawn = Cast<AShooterCharacter>(*It);
		if (TestPawn)
		{
			const FVector CheckLocationCenter = GetActorLocation();

			float DistSq = (CheckLocationCenter - TestPawn->GetActorLocation()).SizeSquared();
			float TestRadiusSq = CheckRadius * CheckRadius;

			if (DistSq < TestRadiusSq)
			{
				return true;
			}
		}
	}
	return false;
}

void AShooterParticleTrigger::Run()
{
	bool bIsPlayerWithinDistance = IsPlayerWithinDistance();

	if (bIsPlayerWithinDistance)
	{
		if (bEnableFX)
			PlayParticles();

		if (bEnableSound)
			PlaySound();

		if (bEnableTrail)
			PlayTrail();
	}
}

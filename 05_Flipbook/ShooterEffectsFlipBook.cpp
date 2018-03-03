// Fill out your copyright notice in the Description page of Project Settings.

#include "ShooterGame.h"
#include "ShooterEffectsFlipBook.h"
#include "FlipBookData.h"

DEFINE_LOG_CATEGORY_STATIC(ShooterFlipBookLog, Log, All);

// Sets default values
AShooterEffectsFlipBook::AShooterEffectsFlipBook(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	StaticMeshComp = ObjectInitializer.CreateDefaultSubobject<UStaticMeshComponent>(this, TEXT("FlipBookMeshComp"));
	//StaticMeshComp->SetCollisionProfileName(TEXT("OverlapAll"));

	StaticMeshComp->SetHiddenInGame(true);
	StaticMeshComp->SetVisibility(false);
	StaticMeshComp->SetCollisionObjectType(ECC_Visibility);
	StaticMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StaticMeshComp->SetCollisionResponseToAllChannels(ECR_Ignore);
	StaticMeshComp->SetCastShadow(false);

	RootComponent = StaticMeshComp;
	bMeshLoad = false;
	IsAvailable = true;
	RandRot = 0.0f;

#if WITH_EDITORONLY_DATA
	// Structure to hold one-time initialization
	struct FConstructorStaticsFlipbook
	{
		// A helper class object we use to find target UTexture2D object in resource package
		ConstructorHelpers::FObjectFinderOptional<UTexture2D> FlipbookTextureObject;

		// Icon sprite category name
		FName ID_FlipbookIcon;

		// Icon sprite display name
		FText NAME_FlipbookIcon;

		FConstructorStaticsFlipbook()
			// Use helper class object to find the texture
			// "/Engine/EditorResources/S_Note" is resource path
			: FlipbookTextureObject(TEXT("/Game/UI/GameModeHelper/FlipbookIcon"))
			, ID_FlipbookIcon(TEXT("FlipbookIcon"))
			, NAME_FlipbookIcon(NSLOCTEXT("SpriteCategory", "FlipbookIcon", "FlipbookIcon"))
		{
		}
	};
	static FConstructorStaticsFlipbook ConstructorStaticsFlipbook;

	SpriteHelperInEditor = ObjectInitializer.CreateEditorOnlyDefaultSubobject<UBillboardComponent>(this, TEXT("FlipbookSprite"));
	if (SpriteHelperInEditor)
	{
		SpriteHelperInEditor->Sprite = ConstructorStaticsFlipbook.FlipbookTextureObject.Get();		// Get the sprite texture from helper class object
		SpriteHelperInEditor->SpriteInfo.Category = ConstructorStaticsFlipbook.ID_FlipbookIcon;		// Assign sprite category name
		SpriteHelperInEditor->SpriteInfo.DisplayName = ConstructorStaticsFlipbook.NAME_FlipbookIcon;	// Assign sprite display name
		SpriteHelperInEditor->Mobility = EComponentMobility::Movable;
		SpriteHelperInEditor->RelativeScale3D = FVector(0.75f, 0.75f, 0.75f);
		SpriteHelperInEditor->bHiddenInGame = true;
		SpriteHelperInEditor->bAbsoluteScale = true;
		SpriteHelperInEditor->SetupAttachment(RootComponent);
		SpriteHelperInEditor->bIsScreenSizeScaled = true;
	}
#endif // WITH_EDITORONLY_DATA
}

void AShooterEffectsFlipBook::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	SetReplicates(false);
	GetWorld()->RemoveNetworkActor(this);

	// only enable initial tick for environmental flipbook fx i.e. torch
	SetActorTickEnabled(bCustomFlipbook);
}

void AShooterEffectsFlipBook::Activate(FEffectsFlipBook* FlipbookElement)
{
	if (bCustomFlipbook){
		IsAvailable = false;
		CurrentFlipbookOption = &FlipbookOptionLocal; // usually used for env
	}
	else{
		CurrentFlipbookOption = FlipbookElement;
	}

	PrepareMesh();
	Show();
}

void AShooterEffectsFlipBook::PrepareMesh()
{
	if (!CurrentFlipbookOption->FlipBookData)
		return;

	FBD = Cast<AFlipBookData>(CurrentFlipbookOption->FlipBookData->GetDefaultObject());

	if (FBD && FBD->MeshArray.Num() > 0){
		StaticMeshes = FBD->MeshArray;
		bMeshLoad = true;

		/* set additional rotation offset to the billboard oriented flip book (e.g. , align to user camera + random rot)*/
		Loop = CurrentFlipbookOption->Loop;

		StaticMeshComp->SetWorldScale3D(FVector(CurrentFlipbookOption->Scale, CurrentFlipbookOption->Scale, CurrentFlipbookOption->Scale));
		StaticMeshComp->AddRelativeLocation(CurrentFlipbookOption->LocationOffset);

		RandRot = FMath::FRandRange(-20.0f, 20.0f);

		if (FBD->FlipBookFPS == EFlippBookSpeed::FPS_16)
			UpdateThreshold = 0.0625; //16
		else if (FBD->FlipBookFPS == EFlippBookSpeed::FPS_24)
			UpdateThreshold = 0.041666666; //24
		else if (FBD->FlipBookFPS == EFlippBookSpeed::FPS_30)
			UpdateThreshold = 0.033333333; //30
		else if (FBD->FlipBookFPS == EFlippBookSpeed::FPS_60)
			UpdateThreshold = 0.016666666; //60
		else if (FBD->FlipBookFPS == EFlippBookSpeed::FPS_8)
			UpdateThreshold = 0.125; //8
		else if (FBD->FlipBookFPS == EFlippBookSpeed::FPS_48)
			UpdateThreshold = 0.0208; //48
		else if (FBD->FlipBookFPS == EFlippBookSpeed::FPS_52)
			UpdateThreshold = 0.0192; //52
		else if (FBD->FlipBookFPS == EFlippBookSpeed::FPS_38)
			UpdateThreshold = 0.026315789473; //38
		else if (FBD->FlipBookFPS == EFlippBookSpeed::FPS_72)
			UpdateThreshold = 0.01388888; //72
		else if (FBD->FlipBookFPS == EFlippBookSpeed::FPS_86)
			UpdateThreshold = 0.0116279068; //86
		else if (FBD->FlipBookFPS == EFlippBookSpeed::FPS_90)
			UpdateThreshold = 0.011111; //90
		else if (FBD->FlipBookFPS == EFlippBookSpeed::FPS_120)
			UpdateThreshold = 0.008333; //120

		CurrentMeshFrame = 0;
		if (bCustomFlipbook)
			CurrentMeshFrame = FMath::RandHelper(StaticMeshes.Num() - 1);

		StaticMeshComp->SetStaticMesh(StaticMeshes[CurrentMeshFrame]);

		SpawnTime = GetWorld()->TimeSeconds;

		/* calculate total lifetime as */
		LifeTime = UpdateThreshold * FBD->MeshArray.Num(); //CurrentFlipbookOption->LifeTime;

		EndTime = LifeTime + SpawnTime;
	}
	else {
		EndTime = 0.0;
	}

	// use tint material
	/*
	OriginalMaterialInstance = StaticMeshComp->GetMaterial(0);
	if (FBD->bUseTint)
		StaticMeshComp->SetMaterial(0, FBD->TintMaterial);
		*/
}

void AShooterEffectsFlipBook::Show()
{
	SetActorHiddenInGame(false);
	StaticMeshComp->SetVisibility(true);
	StaticMeshComp->SetHiddenInGame(false);
}

void AShooterEffectsFlipBook::Hide()
{
	SetActorHiddenInGame(true);
	StaticMeshComp->SetVisibility(false);
	StaticMeshComp->SetHiddenInGame(true);
}

// Called when the game starts or when spawned
void AShooterEffectsFlipBook::BeginPlay()
{
	Super::BeginPlay();

	/* Custom flipbook activate usually for environment stuff */
	if (bCustomFlipbook){
		Activate(&FlipbookOptionLocal);
	}
}

void AShooterEffectsFlipBook::Billboard(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World) return;

	APlayerController* PlayerController = GEngine->GetFirstLocalPlayerController(GetWorld());
	if (PlayerController){
		FRotator CtrlRot = PlayerController->GetControlRotation();

		if (GetWorld()->TimeSince(StaticMeshComp->LastRenderTime) <= 0.05f)
		{
			/* Set main dir to user camera */
			StaticMeshComp->SetWorldRotation(CtrlRot);

			/* set additional rotation offset to the billboard oriented flip book (e.g. , align to user camera)*/
			if (CurrentFlipbookOption->bUseCustomRotationOffset)
				StaticMeshComp->AddRelativeRotation(CurrentFlipbookOption->RotationOffset);
			else
				StaticMeshComp->AddRelativeRotation(FBD->RotationOffset);

			/* Rotation offset for randomness is shape */
			FRotator RandRotator = FRotator(0.0f, RandRot, 0.0f);
			StaticMeshComp->AddLocalRotation(RandRotator);
		}
	}
}

void AShooterEffectsFlipBook::Animate(float DeltaTime)
{
	if (!bMeshLoad)
		return;

	check(StaticMeshes.IsValidIndex(0));

	if (!Loop && CurrentMeshFrame >= StaticMeshes.Num()){
		Hide();
		return;
	}

	if (!Sec)
		Sec = 0.0f;

	if (CurrentMeshFrame < StaticMeshes.Num()) /* Regular Animation Process */
	{
		Sec += DeltaTime;

		if (Sec >= UpdateThreshold){
			UStaticMesh* CurrentMesh = StaticMeshes[CurrentMeshFrame];
			
			/* consider case where user put wrong timeframe */
			if (!CurrentMesh){
#if !UE_BUILD_SHIPPING
				UE_LOG(ShooterFlipBookLog, Warning, TEXT("Loading mesh failed while animation is playing. Frame : %s"), *FString::FromInt(CurrentMeshFrame));
#endif
				CurrentMeshFrame++;
				Sec = 0.0f;
				return;
			}

			if (GetWorld()->TimeSince(StaticMeshComp->LastRenderTime) <= 0.05f)
				StaticMeshComp->SetStaticMesh(CurrentMesh);

			CurrentMeshFrame++;
			Sec = 0.0f;
		}
	}
	else if (Loop) /* Animation Loop */
	{
		CurrentMeshFrame = 0;
	}
	else /* Animation End */
	{
		Hide();
	}
}

// Called every frame
void AShooterEffectsFlipBook::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	/* tick will only tick when its not available which means its activated! */
	if (IsAvailable && !bCustomFlipbook)
		return;

	/* custom flipbooks are the ones that exist in level as environment */
	if (!IsAvailable && EndTime == 0.0f && !bCustomFlipbook) {
		DeallocateFromPool();
		return;
	}
		
	float CurrentTime = GetWorld()->TimeSeconds;
	if (CurrentTime > EndTime && !bCustomFlipbook){
		DeallocateFromPool();
		return;
	}

	Animate(DeltaTime);
	Billboard(DeltaTime);
}

void AShooterEffectsFlipBook::AllocateFromPool(FEffectsFlipBook* FlipbookElement, AActor* Owner)
{
	IsAvailable = false;
	SetActorTickEnabled(true);

	/* if there is owner available, its attached*/
	IsAttached = (!Owner == false); 

	if (IsAttached)
	{
		/* EAttachLocation::SnapToTarget? */
		AttachToActor(Owner, FAttachmentTransformRules::KeepRelativeTransform, FlipbookElement->Bone);
		SetOwner(Owner);
	}

	//Activate(FlipbookElement);
}

void AShooterEffectsFlipBook::ResetFlipbook()
{
	IsAvailable = true;
	SetActorTickEnabled(false);

	bMeshLoad = false;
	EndTime = 0.0f;
	IsAttached = false;
	SpawnTime = 0.0f;
	Loop = false;
	Sec = 0.0f;
	//SetOwner(NULL);

	TeleportTo(FVector(1000000.0f, 1000000.0f, 100000.0f), FRotator::ZeroRotator, false, true);
	StaticMeshes.Empty();
	StaticMeshComp->StaticMesh = NULL;
	//StaticMeshComp->SetMaterial(0, OriginalMaterialInstance);
}

/*
* Deallocation from pool means here is that,
* it will make the AShooterEmitter instance's member variable 'IsAvailable' to be true,
* and make it's particle system to be deactivated until it will be assigned when "allocated" again.
*/
void AShooterEffectsFlipBook::DeallocateFromPool()
{
	DetachRootComponentFromParent();
	ResetFlipbook();
	Hide();
}

void AShooterEffectsFlipBook::FellOutOfWorld(const class UDamageType& dmgType)
{
#if !UE_BUILD_SHIPPING
	// * operator is used to return TCHAR
	float FlipBookCurrentAge = GetWorld()->TimeSeconds - SpawnTime;
	UE_LOG(ShooterFlipBookLog, Warning, TEXT("FLIPBOOK FeelOutWorld :: Getting deallocated..\nCurrentAge:%.2f\nLocation:%s"), FlipBookCurrentAge, *GetActorLocation().ToCompactString());
#endif

	// don't do killz with any emitters in pool. we manage them ourselves
	DeallocateFromPool();
}

void AShooterEffectsFlipBook::OutsideWorldBounds()
{
#if !UE_BUILD_SHIPPING
	float FlipBookCurrentAge = GetWorld()->TimeSeconds - SpawnTime;
	UE_LOG(ShooterFlipBookLog, Warning, TEXT("FLIPBOOK OutsideWorldBounds :: Getting deallocated..\nCurrentAge:%.2f\nLocation:%s"), FlipBookCurrentAge, *GetActorLocation().ToCompactString());
#endif

	// don't do destroy() with any emitters in pool. we manage them ourselves
	DeallocateFromPool();
}

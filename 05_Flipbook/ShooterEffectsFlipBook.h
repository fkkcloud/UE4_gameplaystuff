// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/Actor.h"
#include "ShooterEffectsFlipBook.generated.h"

UCLASS()
class SHOOTERGAME_API AShooterEffectsFlipBook : public AActor
{
	GENERATED_UCLASS_BODY()
	
public:	
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
	// Called every frame
	virtual void Tick( float DeltaSeconds ) override;

	virtual void PostInitializeComponents() override;

	// Activate with template FlipbookElement,
	// if the value is NULL, it will use the template FlipbookElement which is saved
	// within the actor instance. Usually used for environment fx.
	void Activate(FEffectsFlipBook* FlipbookElement = NULL);

	void Show();
	void Hide();

	// Reset Flipbook so it deallocate and avaialable in pool
	void ResetFlipbook();  
	
	// Pool management
	void AllocateFromPool(FEffectsFlipBook* Flipbook, AActor* InOwner = NULL); /* Owner is option , if there is no owner option available, its no attached flipbook*/
	void DeallocateFromPool();

	/** called when the actor falls out of the world 'safely' (below KillZ and such) */
	virtual void FellOutOfWorld(const class UDamageType& dmgType) override;

	virtual void OutsideWorldBounds() override; 

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	// A UBillboardComponent to hold Icon sprite
	UBillboardComponent* SpriteHelperInEditor;
#endif // WITH_EDITORONLY_DATA

	/** Setup for flipbook (e.g. environment geos) */
	UPROPERTY(EditAnywhere, Category = "Setting", BlueprintReadWrite)
	FEffectsFlipBook FlipbookOptionLocal;

	/* Optimization */
	UPROPERTY(EditAnywhere, Category = "Setting")
	FDrawDistance DrawDist;

	/** Customized Flipbook. This will allow to use custom flipbook template (usually for environmnet fx) */
	UPROPERTY(EditAnywhere, Category = "Setting")
	bool bCustomFlipbook;

	class UStaticMeshComponent* StaticMeshComp;

	class UMaterialInterface* OriginalMaterialInstance;

	/* actual spawned time of this pool gets initialized with a new geo path*/
	float SpawnTime; 
	
	/* when it will end animating, and deallocate itself */
	float LifeTime;
	float EndTime; 

	float DrawDistance;

	/* is available in flipbook pool : look into ShooterGameState.h and .cpp for more detail*/
	bool IsAvailable; 

	/* Is this attached */
	bool IsAttached; 
	
	bool Loop;

private:

	/* start animation */
	void Animate(float DeltaTime); 

	/* always head toward 1st person camera */
	void Billboard(float DeltaTime); 

	/* get mesh loaded when game begin */
	void PrepareMesh(); 

	FEffectsFlipBook* CurrentFlipbookOption;

	TArray<UStaticMesh*> StaticMeshes;

	AFlipBookData* FBD;

	/* for animation speed*/
	float UpdateThreshold; 

	/* current activated random rotation */
	float RandRot;

	/* animation sec check */
	float Sec;

	/* current playing mesh frame for animation */
	int CurrentMeshFrame;

	/* if mesh is not loaded, dont play the animation */
	bool bMeshLoad; 
};

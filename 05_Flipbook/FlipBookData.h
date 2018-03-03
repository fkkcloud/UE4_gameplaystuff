// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/Actor.h"
#include "FlipBookData.generated.h"

UCLASS()
class SHOOTERGAME_API AFlipBookData : public AActor
{
	GENERATED_UCLASS_BODY()
	
public:	
	/** Speed for the flipbook animation */
	UPROPERTY(EditAnywhere, Category = "Flipbook Mesh")
	EFlippBookSpeed FlipBookFPS;

	UPROPERTY(EditAnywhere, Category = "Flipbook Mesh")
	TArray< UStaticMesh* > MeshArray;

	UPROPERTY(EditAnywhere, Category = "Flipbook Mesh")
	FRotator RotationOffset;

	UPROPERTY(EditAnywhere, Category = "Flipbook Mesh")
	bool bUseTint;

	UPROPERTY(EditAnywhere, Category = "Flipbook Mesh")
	class UMaterialInstance* TintMaterial;
};

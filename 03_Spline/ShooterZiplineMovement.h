/*
ShooterZiplineMovement

Description
AShooterZiplineMovment class derived from AShooterSplineMovement,
specific spline shape and movement to fit Zipline behavior along with the spline.
Spline movement is linear.
*/

#pragma once

#include "Movement/ShooterSplineMovement.h"
#include "ShooterZiplineMovement.generated.h"

UCLASS()
class SHOOTERGAME_API AShooterZiplineMovement : public AShooterSplineMovement
{
	GENERATED_UCLASS_BODY()

	/* Offset from the zipline start point in Z-Axis */
	UPROPERTY(EditAnywhere, Category = "Zipline")
	float ZipLineStartOffset;

	/* Offset from the zipline arrival in Z-Axis */
	UPROPERTY(EditAnywhere, Category = "Zipline")
	float ZipLineArrialOffset;

	/* Offset of zipline mesh from the zipline spline in Z-Axis */
	UPROPERTY(EditAnywhere, Category = "Zipline")
	float ZipLineMeshOffset;

	/* Offset of zipline mesh from the zipline spline in Z-Axis */
	UPROPERTY(EditAnywhere, Category = "Zipline")
	FVector ZipLineIndicatorPosition;

	/*
	// Mesh to be used as magnet to grab the character that is moving along with the spline
	UPROPERTY(EditAnywhere, Category = "Zipline")
	UStaticMesh* ZipLineMesh;

	// Mesh to be used as magnet to notify that zipline is ready
	UPROPERTY(EditAnywhere, Category = "Zipline")
	UStaticMesh* ZipLineIndicatorMesh;
	*/

	UPROPERTY(EditAnywhere, Category = "Zipline")
	FRotator ZipLineMeshRotation;

	/* Mesh to be used as magnet to grab the character that is moving along with the spline */
	UPROPERTY(VisibleAnywhere, Category = "Zipline")
	UStaticMeshComponent*  ZipLineIndicator;

	UPROPERTY(VisibleAnywhere, Category = "Zipline")
	USkeletalMeshComponent* ZipLineMeshComp2;

	UPROPERTY(EditAnywhere, Category = "Zipline")
	FRotator ZipLineIndicatorRotation;

	UPROPERTY(EditDefaultsOnly, Category = "Zipline")
	USoundCue* ZipLineSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zipline")
	TArray<USplineMeshComponent*> SplineMeshes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zipline")
	float Spacing;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zipline")
	float TangentLength;

	UPROPERTY()
		UMaterialInstanceDynamic* mat;

	AShooterSound* ZipSound;

	virtual void FinishSplineMovement(AShooterCharacter* InCharacter, EMovementMode CharacterMovementMode) override;

	virtual void PostInitProperties() override;

	// BlueprintCallable function override
	virtual void MoveCharacterAlongSpline(AShooterCharacter* InCharacter) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	virtual void OnConstruction(const FTransform& Transform) override;

	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, Category = "Zipline")
		UStaticMesh* myMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Zipline")
		UMaterialInterface* myMaterial;

protected:
	virtual void MovementImplementation(float DeltaSeconds) override;

	virtual void SetSpline() override;

	virtual void CalculateOffset(const AShooterCharacter* InCharacter, FVector & InVector) override;
	UFUNCTION(NetMulticast, Reliable)
	void MulticastSplineMovement(AShooterCharacter* InCharacter);
	virtual void ResetZiplineFeatures();

	void PlayZipLineSound(AShooterCharacter* InCharacter);
	void StopZipLineSound(AShooterCharacter* InCharacter);

	void InitZiplineHelpers();

	void LoadMaterials();

	class AShooterSound* ZiplineVOSound;
	UFUNCTION()
	void ClearSound(AShooterSound* Sound);
};

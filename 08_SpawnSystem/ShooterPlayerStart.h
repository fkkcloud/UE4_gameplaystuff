#pragma once

#include "GameFramework/PlayerStart.h"
#include "ShooterPlayerStart.generated.h"

UCLASS()
class SHOOTERGAME_API AShooterPlayerStart : public APlayerStart
{
	GENERATED_UCLASS_BODY()

	/** parameter - radius to check if any pawn is nearby */
	UPROPERTY(EditAnywhere, Category = "SpawnControl")
	float CheckRadius;

#if WITH_EDITOR
	/** visible indicator - radius to check if any pawn is nearby */
	USphereComponent* CheckRadiusComponent;
#endif

	/**
	* parameter - timestep to check if any pawn is nearby
	* 1.0f - every approx 1 sec
	* 0.5f - every approx 0.5 sec
	*/
	// UPROPERTY(EditAnywhere, Category = "SpawnControl")
	float CheckTimeStep;

	void OnPlayerStartAddedToGameMode();

	///** To update debug every tick. */
	//virtual void ReceiveTick(float DeltaSeconds) override;

	/** Update for spawn score */
	virtual void UpdateScore();

	/** Getter for the current spawning score. */
	inline float GetSpawnScore() const
	{
		return SpawnScore;
	}

	/** Notify the PlayerStart actor to */
	void Spawned();

	/**
	* When it spawns it will update neayby spawn points to affect their spawn points
	* Meant to be called in game mode.
	* @param PlayerStarts - TArray of playerstart that is near by the spawn point to be affected
	*/
	void AffectNearSpawnPoints(TArray<APlayerStart*> & PlayerStarts);

	/** Manually adding to spawn point */
	inline void AddToSpawnScore(const float Value)
	{
		SpawnScore += Value;
		SpawnScore = FMath::Clamp(SpawnScore, SCORE_SPAWN_MIN, SCORE_SPAWN_MAX);
	}

	inline void SetSpawnScore(const float Value)
	{
		SpawnScore = FMath::Clamp(Value, SCORE_SPAWN_MIN, SCORE_SPAWN_MAX);
	}

	bool operator<(const AShooterPlayerStart& rhs) const;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostLoad() override;
#endif

#if WITH_RELOAD_STUDIOS
	// Re-enable AddPlayerStart/RemovePlayerStart functionality that was removed by Epic's GameMode.h
	virtual void PostInitializeComponents();
	virtual void PostUnregisterAllComponents();
#endif // #if WITH_RELOAD_STUDIOS

protected:
	/**
	* Calculate for spawn score,
	*	  1. distance check for XY plane.
	*     2. height check for Z Axis.
	* @ TestPawn - Pawn to update the spawn score
	* @ Return - Did it find any near by pawn to calculate
	*/
	bool IsSpawnPointNearBy(ACharacter* TestPawn);

	/**
	* Calculate for spawn score,
	*     1. check if it is looking towards the spawn point. - dot product
	*     2. check if it succeeds for ray - casting. - UNavigationSystem::NavigationRayCast  - TEMP
	* @ TestPawn - Pawn to update the spawn score
	* @ Return - Did it actually looked at the spawn point with given FOV
	*/
	bool IsSpawnPointVisible(ACharacter* TestPawn);

	/**
	* Calculate for spawn score,
	*     1. check if all the pawn is near by the spawn point
	2. If none are near by, regen the spawn point
	* @ TestPawn - Pawn to update the spawn score
	* @ Return - Was there any neayby character
	*/
	virtual bool IsRegenable();

	/** Calculate the score for recently spawned points */
	void CalculateRecentSpawnedState();

	/** Store for spawn score */
	float SpawnScore;

	/** Store the weight of how recent the spawn happened for this Spawn Point. */
	float RecentSpawnWeight;

	/** Heuristic values for spawn logic. */
	static const float SCORE_DOTPRODUCT_MULT;
	static const float SCORE_SEARCH_RANGE;
	static const float SCORE_REGEN_RATE;
	static const float SCORE_RECENT_SPAWN_MAX;
	static const float SCORE_RECENT_SPAWN_DECREASE;
	static const float SCORE_SPAWN_MIN;
	static const float SCORE_SPAWN_MAX;
	static const float SCORE_NEARBY_DIST;
	static const float SCORE_FOV;

private:
	FTimerHandle UpdateScoreHandle;

// Debug code - only for none-shipping
public:
#if !UE_BUILD_SHIPPING
	/**
	* Debug code
	* Record if there was any player walked through.
	* If there was players within 4 seconds, it will decrease spawn score.
	*/
	void Record();

	//// Materials for ScoreTag
	//class UMaterialInstanceConstant* ScoreTagMaterial;
	//class UMaterialInstanceConstant* ScoreHitTagMaterial;
	//
	//UPROPERTY(VisibleAnywhere, Category = "Debug")
	//TSubobjectPtr<UTextRenderComponent> TextRenderComponent;

	bool bRayHit; // for debug dot product

	/**
	* Show spawn score above the spawn point
	* Color changes to red when its ray casting failed.
	*/
	//void SetScoreTag(const FString& Name);
#endif
};

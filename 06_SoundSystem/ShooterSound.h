#pragma once
#include "ShooterTypes.h"
#include "ShooterSound.generated.h"

/**
 * 
 */
UCLASS()
class AShooterSound : public AActor
{
	GENERATED_UCLASS_BODY()

	// bLooping sounds need to be explicitly Deallocated by owner
	void DeActivate();
	static void DumpActiveSounds();
	static void PlayCharacterSpecificSoundAtLocation1P(UObject* WorldContextObject, FShooterSounds inSoundStruct, FVector inLocation, EFaction::Type inFaction, float inVolumeMultiplier = 1.f, float inPitchMultiplier = 1.f, float inStartTime = 0.f, class USoundAttenuation* inAttenuationSettings = NULL);
	static void PlayCharacterSpecificSoundAtLocation3P(UObject* WorldContextObject, FShooterSounds inSoundStruct, FVector inLocation, EFaction::Type inFaction, float inVolumeMultiplier = 1.f, float inPitchMultiplier = 1.f, float inStartTime = 0.f, class USoundAttenuation* inAttenuationSettings = NULL);

	void FadeOutAndDeallocate(float inFadeDuration);

	//void virtual ReceiveTick(float DeltaSeconds) override;
	void Tick_Internal(float DeltaSeconds);

	// play from where it should play
	bool	Play(float PlayTime);

	bool	Stop();

	void    RequestPlay();

	UFUNCTION(BlueprintCallable, Category = "ShooterSound")
	float GetPlaybackTime();

	// Begin AActor interface.
#if WITH_EDITOR
	virtual void CheckForErrors() override;
	virtual bool GetReferencedContentObjects(TArray<UObject*>& Objects) const override;
#endif
	virtual void PostLoad() override;
	virtual void PostRegisterAllComponents() override;

	virtual void Reset();
	// End AActor interface.

	/** Audio component that handles sound playing */
	UPROPERTY(Category = Sound, VisibleAnywhere, BlueprintReadOnly, meta = (ExposeFunctionCategories = "Sound,Audio,Audio|Components|Audio"))
	UAudioComponent* AudioComponent;

	float	SpawnTime;
	float	LifeTime;

	ESoundType SoundType;

	UPROPERTY(Category = Sound, VisibleAnywhere)
	bool    bMustPlay;

	bool	bIsLooping;
	bool	bIsBeingUsed; // activated but not playing state
	bool	bIsPlayingSound; // actually playing the sound state
	bool	bCanPlay;
	bool	HasOwner;

	bool	Activate(USoundCue* Cue, bool b2DSound, bool bLooping, bool bDelay);
	bool    ActivateWithLocation(USoundCue* Cue, bool b2DSound, bool bLooping, bool bDelay, FVector Location);

	// delegate for clearing referenced shooter sound actors
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FClearSound, AShooterSound*, SoundName);
	FClearSound OnSoundDeallocate;

private:

	bool bPendingDeallocate;
	float FadeStartTime;
	float FadeDuration;

	USoundAttenuation* DefaultAttenuation;
};

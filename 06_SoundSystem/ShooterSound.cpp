#include "ShooterGame.h"
#include "ShooterSound.h"
#include "MessageLog.h"
#include "UObjectToken.h"
#include "MapErrors.h"

//#define LOCTEXT_NAMESPACE "AmbientSound"

AShooterSound::AShooterSound(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AudioComponent = ObjectInitializer.CreateDefaultSubobject<UAudioComponent>(this, TEXT("AudioComponent0"));

	AudioComponent->bAutoActivate				 = false;
	AudioComponent->bStopWhenOwnerDestroyed		 = true;
	AudioComponent->bShouldRemainActiveIfDropped = false;
	AudioComponent->Mobility					 = EComponentMobility::Movable;

	RootComponent = AudioComponent;

	DefaultAttenuation = ObjectInitializer.CreateDefaultSubobject<USoundAttenuation>(this, TEXT("AudioAttenuation0"));


	FAttenuationSettings NewAttenuationSettings;
	DefaultAttenuation->Attenuation = NewAttenuationSettings;
	DefaultAttenuation->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
	AudioComponent->AttenuationSettings = DefaultAttenuation;

	bReplicates   = false;
	bHidden		  = true;
	bCanBeDamaged = false;

	bIsBeingUsed    = false;
	bIsPlayingSound = false;
}

static FString GetSoundDescription(AShooterSound *sound)
{
	check(sound);

	FString OwnerName = sound->GetOwner() ? sound->GetOwner()->GetFullName() : TEXT("nullptr");
	FString CueName(TEXT("None"));

	USoundBase* soundBase = sound->AudioComponent ? sound->AudioComponent->Sound : nullptr;

	if (soundBase)
	{
		CueName = soundBase->GetFullName();
	}

	return FString::Printf(TEXT("Used: %d '%s', Looping: %d, SpawnTime: %.2f, HasOwner: %d, Owner: '%s'"), (int)sound->bIsBeingUsed, *CueName, (int)sound->bIsLooping, sound->SpawnTime, (int)sound->HasOwner, *OwnerName);
}

/*static*/

void AShooterSound::DumpActiveSounds()
{
	check(GWorld);

	AShooterGameState* GameState = Cast<AShooterGameState>(GWorld->GameState);

	if (!GameState)
	{
		UE_LOG(LogShooter, Log, TEXT("Invalid game state to dump sounds."));
		return;
	}

	UE_LOG(LogShooter, Log, TEXT("Dumping sound pool at time: %f"), GWorld->GetTimeSeconds());

	// dump all allocated sounds
	for (int soundIndex = 0; soundIndex < GameState->SoundPool.Num(); ++soundIndex)
	{
		AShooterSound* sound = GameState->SoundPool[soundIndex];

		check(sound);

		UE_LOG(LogShooter, Log, TEXT("%d: %s"), soundIndex + 1, *GetSoundDescription(sound));
	}
}

void AShooterSound::PlayCharacterSpecificSoundAtLocation1P(UObject* WorldContextObject, FShooterSounds inSoundStruct, FVector inLocation, EFaction::Type inFaction, float inVolumeMultiplier, float inPitchMultiplier, float inStartTime, class USoundAttenuation* inAttenuationSettings)
{
	USoundCue* Sound = inSoundStruct.Sound1P;

	if (Sound == NULL)
	{
		return;
	}

	UWorld* ThisWorld = GEngine->GetWorldFromContextObject(WorldContextObject);
	if (ThisWorld == nullptr)
	{
		return;
	}

	AShooterCharacter* SC = Cast<AShooterCharacter>(WorldContextObject);

	AShooterGameState* GameState = Cast<AShooterGameState>(ThisWorld->GameState);
	if (GameState) {
		bool bIs1PSound = true;
		bool bIsLooping = false;
		bool bDelay = false;
		bool bSpatialized = true;
		GameState->AllocateSound(Sound, SC, bIs1PSound, bIsLooping, bDelay, bSpatialized, inLocation);
	}
}

void AShooterSound::PlayCharacterSpecificSoundAtLocation3P(UObject* WorldContextObject, FShooterSounds inSoundStruct, FVector inLocation, EFaction::Type inFaction, float inVolumeMultiplier, float inPitchMultiplier, float inStartTime, class USoundAttenuation* inAttenuationSettings)
{
	USoundCue* Sound = NULL;

	Sound = inSoundStruct.Sound3P;

	if (Sound == NULL)
	{
		return;
	}

	UWorld* ThisWorld = GEngine->GetWorldFromContextObject(WorldContextObject);
	if (ThisWorld == nullptr)
	{
		return;
	}

	AShooterCharacter* SC = Cast<AShooterCharacter>(WorldContextObject);

	AShooterGameState* GameState = Cast<AShooterGameState>(ThisWorld->GameState);
	if (GameState) {
		bool bIs1PSound = false;
		bool bIsLooping = false;
		bool bDelay = false;
		bool bSpatialized = true;
		GameState->AllocateSound(Sound, SC, bIs1PSound, bIsLooping, bDelay, bSpatialized, inLocation);
	}
}

bool AShooterSound::Activate(USoundCue* Cue, bool bIs1PSound, bool bLooping, bool bDelay)
{
	if (!Cue)
		DeActivate();

	bIsBeingUsed = true;

	// when activated it is not played yet but waiting for the game state tick to call this to play this.
	bIsPlayingSound = false;

	bCanPlay = !bDelay;

	AudioComponent->SetSound(Cue); // setting sound of the cue

	if (Cue->bOverrideAttenuation) {
		AudioComponent->AttenuationSettings->Attenuation = *(Cue->GetAttenuationSettingsToApply());
	}
	else {
		AudioComponent->AttenuationSettings = DefaultAttenuation;
	}
		
	AudioComponent->SetPitchMultiplier(Cue->PitchMultiplier);
	AudioComponent->SetVolumeMultiplier(Cue->VolumeMultiplier);

	SpawnTime  = GetWorld()->GetTimeSeconds();
	LifeTime   = Cue->Duration;
	bIsLooping = bLooping;

	// if its not for local player but the sound owner is not local player, dont play it!
	// if there is problem with server not playing any 1P sound, consider removing/adjust below code.
	AShooterCharacter* SC = Cast<AShooterCharacter>(GetOwner());
	UWorld* World = GetWorld();
	if (bIs1PSound && SC && World) {
		
		// TODO : have to double check that if server --kill--> client is not playing sound in the real multiplayer session
		bool bIsAvailableFor1PSound = ( (SC->Role >= ROLE_AutonomousProxy) || UShooterStatics::IsLocallyControlled(SC, World) );
		if (!bIsAvailableFor1PSound) {
			DeActivate();
			return false;
		}

	}

	// set to be 2d sound
	AudioComponent->bAllowSpatialization = false;
	
	SetActorHiddenInGame(false);

	OnSoundDeallocate.Clear();
	//OnSoundDeallocate.RemoveAll(this);

	return true;
}

bool AShooterSound::ActivateWithLocation(USoundCue* Cue, bool bIs1PSound, bool bLooping, bool bDelay, FVector Location)
{
	bool isSoundActivated = Activate(Cue, bIs1PSound, bLooping, bDelay);

	if (!isSoundActivated)
		return false;

	// set to be 3d sound
	AudioComponent->SetWorldLocation(Location);
	AudioComponent->bAllowSpatialization = true;
	
	if (AudioComponent->AttenuationSettings)
		AudioComponent->AttenuationSettings->Attenuation.bSpatialize = AudioComponent->bAllowSpatialization;

	return true;
}

void AShooterSound::DeActivate()
{
	// Delegate to null out externally referenced pointer to ShooterSound
	if (OnSoundDeallocate.IsBound())
	{
		OnSoundDeallocate.Broadcast(this);
		OnSoundDeallocate.Clear();
		//OnSoundDeallocate.RemoveAll(this);
	}

	Reset();
}

void AShooterSound::Reset()
{
	SetActorRelativeLocation(FVector::ZeroVector, false, nullptr, ETeleportType::TeleportPhysics);
	SetActorLocation(FVector(0.0f, 0.0f, 10000.0f), false, nullptr, ETeleportType::TeleportPhysics);
	DetachRootComponentFromParent();
	Stop();

	AudioComponent->SetVolumeMultiplier(1.f);
	AudioComponent->SetPitchMultiplier(1.f);
	
	FAttenuationSettings NewAttenuationSettings;
	DefaultAttenuation->Attenuation = NewAttenuationSettings;
	DefaultAttenuation->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
	AudioComponent->AttenuationSettings = DefaultAttenuation;

	AudioComponent->SetSound(NULL);
	
	AudioComponent->SetWorldLocation(FVector(10000.0f, 10000.0f, 10000.0f)); // reset the audio component 
	AudioComponent->bAllowSpatialization = false;

	//SetActorTickEnabled(false);
	SetActorHiddenInGame(true);

	HasOwner = false;
	SetOwner(NULL); // reset owner

	bIsBeingUsed = false; // this value let the gamestate to check if it has to be removed this from PlayingSounds and VOSounds array of gamestate.

	bCanPlay = true;
	bIsLooping = false;

	SpawnTime = 0.0f;
	LifeTime = 0.0f;
	
	bPendingDeallocate = false;

	AShooterGameState* GameState = Cast<AShooterGameState>(GetWorld()->GameState);
	if (GameState)
	{
		// for sound queue - PlayingSound array
		bIsPlayingSound = false;

		// remove from Playing sounds array
		GameState->PlayingSounds.Remove(this);

		// remove from Playing VO sounds array
		GameState->VOSounds.Remove(this);
	}
}

void AShooterSound::FadeOutAndDeallocate(float inFadeDuration)
{
	if(!bPendingDeallocate)
	{
		bPendingDeallocate = true;
		FadeStartTime = GetWorld()->TimeSeconds;
		FadeDuration = inFadeDuration;
	}
}

void AShooterSound::RequestPlay()
{
	check(bIsBeingUsed); // when its requested to be played, it HAS TO BE in using condition.
	bCanPlay = true;
}

bool AShooterSound::Play(float StartTime)
{
	if (!AudioComponent)
		return false;

	AudioComponent->Activate(true);
	AudioComponent->Play(StartTime);

	/* This was causing looping sound to be played forever in clients.. and also possible problem for double sounds..!! 
	const bool bIsInGameWorld = GetWorld()->IsGameWorld();
	*/

	/*
	if (GEngine && GEngine->UseSound() && GetWorld()->bAllowAudioPlayback && GetWorld()->GetNetMode() != NM_DedicatedServer)
	{
		FActiveSound NewActiveSound;
		NewActiveSound.World = GetWorld();
		NewActiveSound.Sound = AudioComponent->Sound;

		NewActiveSound.VolumeMultiplier = AudioComponent->VolumeMultiplier;
		NewActiveSound.PitchMultiplier = AudioComponent->PitchMultiplier;

		NewActiveSound.RequestedStartTime = FMath::Max(0.f, StartTime);

		NewActiveSound.bLocationDefined = true;
		NewActiveSound.Transform.SetTranslation(AudioComponent->GetComponentLocation());

		NewActiveSound.bIsUISound = false; // !bIsInGameWorld;
		NewActiveSound.bHandleSubtitles = true;
		NewActiveSound.SubtitlePriority = 10000.f;

		const FAttenuationSettings* AttenuationSettingsToApply = (AudioComponent->AttenuationSettings ? &AudioComponent->AttenuationSettings->Attenuation : AudioComponent->GetAttenuationSettingsToApply());

		NewActiveSound.bHasAttenuationSettings = (GetWorld() && AttenuationSettingsToApply);
		if (NewActiveSound.bHasAttenuationSettings)
		{
			NewActiveSound.AttenuationSettings = *AttenuationSettingsToApply;
		}

		// TODO - Audio Threading. This call would be a task call to dispatch to the audio thread
		GEngine->GetMainAudioDevice()->AddNewActiveSound(NewActiveSound);
	}
	*/

	return true;
}

bool AShooterSound::Stop()
{
	if (!bIsBeingUsed)
		return false;

	if (!AudioComponent)
		return false;

	AudioComponent->Deactivate();
	AudioComponent->Stop();

	return true;
}

float AShooterSound::GetPlaybackTime()
{
	if (FAudioDevice* AudioDevice = AudioComponent->GetAudioDevice())
	{
		FActiveSound* ActiveSound = AudioDevice->FindActiveSound(AudioComponent);
		if (ActiveSound)
		{
			return ActiveSound->PlaybackTime;
		}
	}

	return 0.f;
}

void AShooterSound::Tick_Internal(float DeltaSeconds)
{
	AShooterGameState* GameState = Cast<AShooterGameState>(GetWorld()->GameState);

	if (HasOwner)
	{
		if (!GetOwner())
		{
			UE_LOG(LogShooter, Log, TEXT("Force deallocate no owner sound: %s"), *GetSoundDescription(this));
			GameState->DeAllocateSound(this);
			return;
		}
		else
		{
			AShooterCharacter* MyOwner = Cast<AShooterCharacter>(GetOwner());

			if (!MyOwner || !MyOwner->IsAlive() || !MyOwner->IsActiveAndFullyReplicated)
			{
				UE_LOG(LogShooter, Log, TEXT("Force deallocate invalid owner sound: %s"), *GetSoundDescription(this));
				GameState->DeAllocateSound(this);
				return;
			}
		}
	}

	if (bPendingDeallocate)
	{
		float elapsed = FMath::Max(GetWorld()->TimeSeconds - FadeStartTime, 0.f);

		if (elapsed > FadeDuration)
		{
			GameState->DeAllocateSound(this);
			return;
		}
		else
		{
			float alpha = FMath::Lerp(1.f, 0.f, elapsed / FadeDuration);
			AudioComponent->SetVolumeMultiplier(alpha);
		}
	}

	//// looping sounds need to be deallocated by owner but just in case, check.
	if (bIsLooping)
	{
		if (!bIsBeingUsed)
		{
			//UE_LOG(LogShooter, Log, TEXT("Force deallocate looping sound: %s"), *GetSoundDescription(this));
			GameState->DeAllocateSound(this);
			return;
		}
	}
	

	// life time check to see if it has to be deallocated
	// if its resetted sound then its LifeTime is 0.0f which is sound we dont want to delegate
	if (LifeTime > 0.0f && GetWorld()->TimeSeconds - SpawnTime > LifeTime)
	{
		GameState->DeAllocateSound(this);
		return;
	}
}

#if WITH_EDITOR
void AShooterSound::CheckForErrors()
{ 
	Super::CheckForErrors();
	/*
	if (!AudioComponent.IsValid())
	{
		FMessageLog("MapCheck").Warning()
			->AddToken(FUObjectToken::Create(this))
			->AddToken(FTextToken::Create(LOCTEXT("MapCheck_Message_AudioComponentNull", "Ambient sound actor has NULL AudioComponent property - please delete")))
			->AddToken(FMapErrorToken::Create(FMapErrors::AudioComponentNull));
	}
	else if (AudioComponent->Sound == NULL)
	{
		FMessageLog("MapCheck").Warning()
			->AddToken(FUObjectToken::Create(this))
			->AddToken(FTextToken::Create(LOCTEXT("MapCheck_Message_SoundCueNull", "Ambient sound actor has NULL Sound Cue property")))
			->AddToken(FMapErrorToken::Create(FMapErrors::SoundCueNull));
	}
	*/
}

bool AShooterSound::GetReferencedContentObjects(TArray<UObject*>& Objects) const 
{ 
	if (AudioComponent->Sound)
	{
		Objects.Add(AudioComponent->Sound);
	}
	return true;
}
#endif

void AShooterSound::PostLoad()
{
	Super::PostLoad();
}

void AShooterSound::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();

#if WITH_EDITORONLY_DATA
	if (AudioComponent && bHiddenEdLevel)
	{
		AudioComponent->Stop();
	}
#endif // WITH_EDITORONLY_DATA
}
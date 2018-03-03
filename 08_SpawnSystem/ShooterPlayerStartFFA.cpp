// Fill out your copyright notice in the Description page of Project Settings.

#include "ShooterGame.h"
#include "ShooterPlayerStartFFA.h"

AShooterPlayerStartFFA::AShooterPlayerStartFFA(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

#if WITH_EDITORONLY_DATA
	// Structure to hold one-time initialization
	struct FConstructorStaticsFFA
	{
		// A helper class object we use to find target UTexture2D object in resource package
		ConstructorHelpers::FObjectFinderOptional<UTexture2D> FFATextureObject;

		// Icon sprite category name
		FName ID_FFAIcon;

		// Icon sprite display name
		FText NAME_FFAIcon;

		FConstructorStaticsFFA()
			// Use helper class object to find the texture
			// "/Engine/EditorResources/S_Note" is resource path
			: FFATextureObject(TEXT("/Game/UI/GameModeHelper/FFAIcon"))
			, ID_FFAIcon(TEXT("FFAIcon"))
			, NAME_FFAIcon(NSLOCTEXT("SpriteCategory", "FFAIcon", "FFAIcon"))
		{
		}
	};
	static FConstructorStaticsFFA ConstructorStaticsFFA;


	SpriteComponentFFA = ObjectInitializer.CreateEditorOnlyDefaultSubobject<UBillboardComponent>(this, TEXT("FFASprite"));
	if (SpriteComponentFFA)
	{
		SpriteComponentFFA->Sprite = ConstructorStaticsFFA.FFATextureObject.Get();		// Get the sprite texture from helper class object
		SpriteComponentFFA->SpriteInfo.Category = ConstructorStaticsFFA.ID_FFAIcon;		// Assign sprite category name
		SpriteComponentFFA->SpriteInfo.DisplayName = ConstructorStaticsFFA.NAME_FFAIcon;	// Assign sprite display name
		SpriteComponentFFA->Mobility = EComponentMobility::Static;
		SpriteComponentFFA->RelativeScale3D = FVector(0.75f, 0.75f, 0.75f);
		SpriteComponentFFA->bHiddenInGame = true;
		SpriteComponentFFA->bAbsoluteScale = true;
		SpriteComponentFFA->SetupAttachment(GetCapsuleComponent());
		SpriteComponentFFA->bIsScreenSizeScaled = true;
	}
#endif // WITH_EDITORONLY_DATA
}

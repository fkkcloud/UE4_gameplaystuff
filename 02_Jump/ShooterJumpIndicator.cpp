// Fill out your copyright notice in the Description page of Project Settings.

#include "ShooterGame.h"
#include "ShooterJumpIndicator.h"
#include "Movement/ShooterJumpMovement.h"

class TJumpPadRenderingProxy : public FPrimitiveSceneProxy
{
private:
	FVector JumpPadLocation;
	FVector JumpPadTarget;
	FVector JumpVelocity;
	float	JumpTime;
	float	GravityZ;
	AShooterJumpMovement* JumpPad;

public:
	TJumpPadRenderingProxy(const UPrimitiveComponent* InComponent) : FPrimitiveSceneProxy(InComponent)
	{
		JumpPad = Cast<AShooterJumpMovement>(InComponent->GetOwner());

		if (JumpPad != NULL)
		{
			JumpPadLocation = InComponent->GetOwner()->GetActorLocation();

			JumpTime = JumpPad->JumpHeight;
			GravityZ = JumpPad->GetWorld()->GetGravityZ();

			if (JumpPad->GetJumpstate() == EJumpState::Destination1)
			{
				if (JumpPad->LaunchDestinationActor)
				{
					JumpPadTarget = JumpPad->LaunchDestinationActor->GetActorLocation();
					JumpVelocity = JumpPad->CalculateJumpVelocity(JumpPad, JumpPad->LaunchDestinationActor);
				}
				else
				{
					JumpPadTarget = FVector(100.0f, 0.0f, 0.0f);
				}
			}
			else if (JumpPad->GetJumpstate() == EJumpState::Destination2)
			{
				if (JumpPad->LaunchDestinationActor2)
				{
					JumpPadTarget = JumpPad->LaunchDestinationActor2->GetActorLocation();
					JumpVelocity = JumpPad->CalculateJumpVelocity(JumpPad, JumpPad->LaunchDestinationActor2);
				}
				else
				{
					JumpPadTarget = FVector(100.0f, 0.0f, 0.0f);
				}
			}
		}
	}

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
	{
		// This function will not be called if its owning component is bHiddenInGame = true or Visibility = false.

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			if (VisibilityMap & (1 << ViewIndex))
			{
				FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);

				static const float LINE_THICKNESS = 20;
				static const int32 NUM_DRAW_LINES = 16;

				FVector Start = JumpPadLocation;
				float TimeTick = JumpTime / NUM_DRAW_LINES;

				for (int32 i = 1; i <= NUM_DRAW_LINES; i++)
				{
					//Find the position in the Trajectory
					float TimeElapsed = TimeTick * i;
					FVector End = JumpPadLocation + (JumpVelocity * TimeElapsed);
					End.Z -= (-GravityZ * FMath::Pow(TimeElapsed, 2)) / 2;

					FColor LineClr = FColor(220.0f, 180.0f, 5.0f);

					//Draw and swap line ends
					PDI->DrawLine(Start, End, LineClr, 0, LINE_THICKNESS);
					Start = End;
				}
			}
		}
	}


	virtual uint32 GetMemoryFootprint(void) const
	{
		return(sizeof(*this));
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const
	{
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = IsShown(View) && (IsSelected() || View->Family->EngineShowFlags.Navigation);
		Result.bDynamicRelevance = true;
		Result.bNormalTranslucencyRelevance = IsShown(View);
		Result.bShadowRelevance = IsShadowCast(View);
		Result.bEditorPrimitiveRelevance = UseEditorCompositing(View);
		return Result;
	}
};

UShooterJumpIndicator::UShooterJumpIndicator(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Mobility = EComponentMobility::Stationary;

	BodyInstance.SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

	AlwaysLoadOnClient = false;
	AlwaysLoadOnServer = false;
	bHiddenInGame = false;
	bGenerateOverlapEvents = false;
}

FBoxSphereBounds UShooterJumpIndicator::CalcBounds(const FTransform & LocalToWorld) const
{
	float gravity = UPhysicsSettings::Get()->DefaultGravityZ;

	// N.B. GetWorld is NULL when exiting PIE
	if (GetWorld() && IsInGameThread())
	{
		gravity = GetWorld()->GetGravityZ();
	}

	FBox Bounds(0);
	AShooterJumpMovement* JumpPad = Cast<AShooterJumpMovement>(GetOwner());

	if (JumpPad != NULL)
	{
		FVector JumpPadLocation = JumpPad->GetActorLocation();

		FVector JumpPadTarget;
		FVector JumpVelocity;
		if (JumpPad->GetJumpstate() == EJumpState::Destination1)
		{
			if (JumpPad->LaunchDestinationActor)
			{
				JumpPadTarget = JumpPad->LaunchDestinationActor->GetActorLocation();
				JumpVelocity = JumpPad->CalculateJumpVelocity(JumpPad, JumpPad->LaunchDestinationActor);
			}
			else
			{
				JumpPadTarget = FVector(100.0f, 0.0f, 0.0f);
			}
		}
		else if (JumpPad->GetJumpstate() == EJumpState::Destination2)
		{
			if (JumpPad->LaunchDestinationActor2)
			{
				JumpPadTarget = JumpPad->LaunchDestinationActor2->GetActorLocation();
				JumpVelocity = JumpPad->CalculateJumpVelocity(JumpPad, JumpPad->LaunchDestinationActor2);
			}
			else
			{
				JumpPadTarget = FVector(100.0f, 0.0f, 0.0f);
			}
		}

		float JumpTime = JumpPad->JumpHeight;
		float GravityZ = -gravity;

		Bounds += JumpPadLocation;
		Bounds += JumpPadTarget;
		// Bounds += JumpPad->ActorToWorld().TransformPosition(JumpPadTarget);

		//Guard divide by zero potential with gravity
		if (gravity != 0.0f)
		{
			//If the apex of the jump is within the Pad and destination add to the bounds
			float ApexTime = JumpVelocity.Z / GravityZ;
			if (ApexTime > 0.0f && ApexTime < JumpTime)
			{
				FVector Apex = JumpPadLocation + (JumpVelocity * ApexTime);
				Apex.Z -= (GravityZ * FMath::Pow(ApexTime, 2)) / 2;
				Bounds += Apex;
			}
		}
	}
	return FBoxSphereBounds(Bounds);
}

FPrimitiveSceneProxy* UShooterJumpIndicator::CreateSceneProxy()
{
	return new TJumpPadRenderingProxy(this);
}

// Fill out your copyright notice in the Description page of Project Settings.

#include "ShooterGame.h"
#include "ShooterJumpVisualizier.h"
#include "ShooterJumpMovement.h"

UShooterJumpVisualizier::UShooterJumpVisualizier(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetMobility(EComponentMobility::Static);

	SourceComponent = ObjectInitializer.CreateDefaultSubobject<UStaticMeshComponent>(this, TEXT("SourceComp"));
	SourceComponent->SetVisibility(false, true);
	SourceComponent->SetHiddenInGame(true, true);
	SourceComponent->SetupAttachment(this);

	DestinationVisualizer = ObjectInitializer.CreateDefaultSubobject<UStaticMeshComponent>(this, TEXT("DestinationVizComp"));
	DestinationVisualizer->SetVisibility(false, true);
	DestinationVisualizer->SetupAttachment(this);

	//SourceComponent->SetHiddenInGame(true);
}

void UShooterJumpVisualizier::InitializeParms()
{
	JumpPad = Cast<AShooterJumpMovement>(GetOwner());

	if (JumpPad != NULL)
	{
		JumpPadLocation = JumpPad->GetActorLocation();
		if (JumpPad->GetJumpstate() == EJumpState::Destination1)
		{
			if (JumpPad->LaunchDestinationActor)
			{
				JumpVelocity = JumpPad->CalculateJumpVelocity(JumpPad, JumpPad->LaunchDestinationActor);
				JumpPadTarget = JumpPad->LaunchDestinationActor->GetActorLocation();
				Count = JumpPad->VisualizationStepSize;
			}
			else
			{
				JumpPadTarget = FVector(100.0f, 0.0f, 0.0f);
			}
		}
		else
		{
			if (JumpPad->LaunchDestinationActor2)
			{
				JumpVelocity = JumpPad->CalculateJumpVelocity(JumpPad, JumpPad->LaunchDestinationActor2);
				JumpPadTarget = JumpPad->LaunchDestinationActor2->GetActorLocation();
				Count = JumpPad->VisualizationStepSize2;
			}
			else
			{
				JumpPadTarget = FVector(100.0f, 0.0f, 0.0f);
			}
		}
		JumpTime = JumpPad->JumpHeight;
		GravityZ = JumpPad->GetWorld()->GetGravityZ();

	}

	if (VizMeshes.Num() == 0)
	{
		const int32 PoolCount = 80;

		for (int32 Index = 0; Index < PoolCount; Index++)
		{
			UStaticMeshComponent* NewComponent = DuplicateObject<UStaticMeshComponent>(SourceComponent, this);
			NewComponent->RegisterComponent();
			NewComponent->SetVisibility(true, true);
			NewComponent->SetHiddenInGame(true, true);
			NewComponent->BodyInstance.SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

			VizMeshes.Add(NewComponent);
		}
	}
}

void UShooterJumpVisualizier::Draw()
{
	if (!SourceComponent->StaticMesh && Count < 2)
	{
		return;
	}

	FVector Start = JumpPadLocation;
	FVector RotStart = JumpPadLocation;
	float TimeTick = JumpTime / Count;
	
	for (int32 i = 1; i < Count; ++i)
	{
		//Find the position in the Trajectory
		float TimeElapsed = TimeTick * i;
		FVector End = JumpPadLocation + (JumpVelocity * TimeElapsed);
		End.Z -= (-GravityZ * FMath::Pow(TimeElapsed, 2)) / 2;

		//Find the position in the for rotation
		float RotTimeElapsed = TimeTick * (i+2);
		FVector RotEnd = JumpPadLocation + (JumpVelocity * RotTimeElapsed);
		RotEnd.Z -= (-GravityZ * FMath::Pow(RotTimeElapsed, 2)) / 2;

		//Draw and swap line ends
		 
		if(VizMeshes.Num() >= i)
		{
			UStaticMeshComponent* NewComponent = VizMeshes[i-1];
			NewComponent->SetWorldLocation(End);
			NewComponent->SetVisibility(true, true);
			NewComponent->SetHiddenInGame(true, true);
			NewComponent->BodyInstance.SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

			FRotator AdjustRotator;
			FVector TangentDir2D = (RotEnd - RotStart).GetSafeNormal();
			FVector TangentDir = FVector(TangentDir2D.X, TangentDir2D.Y, TangentDir2D.Z);

			AdjustRotator = FRotator(NewComponent->GetComponentRotation().Pitch + TangentDir.Rotation().Pitch,
				NewComponent->GetComponentRotation().Yaw + TangentDir.Rotation().Yaw,
				NewComponent->GetComponentRotation().Roll + TangentDir.Rotation().Roll); //TangentDir.Rotation().Yaw
			NewComponent->SetWorldRotation(AdjustRotator);

			Start = End;
			RotStart = RotEnd;
		}
		else
		{
			UE_LOG(LogShooter, Warning, TEXT("Draw: Allocating another Static Mesh for ShooterJumpVisualizer. Consider increasing the pool count for ShooterJumpVisualizer. Count needed is %d"), Count);

			UStaticMeshComponent* NewComponent = DuplicateObject<UStaticMeshComponent>(SourceComponent, this);
			NewComponent->RegisterComponent();
			NewComponent->SetWorldLocation(End);
			NewComponent->SetVisibility(true, true);
			NewComponent->SetHiddenInGame(true, true);
			NewComponent->BodyInstance.SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

			FRotator AdjustRotator;
			FVector TangentDir2D = (RotEnd - RotStart).GetSafeNormal();
			FVector TangentDir = FVector(TangentDir2D.X, TangentDir2D.Y, TangentDir2D.Z);

			AdjustRotator = FRotator(NewComponent->GetComponentRotation().Pitch + TangentDir.Rotation().Pitch,
				NewComponent->GetComponentRotation().Yaw + TangentDir.Rotation().Yaw,
				NewComponent->GetComponentRotation().Roll + TangentDir.Rotation().Roll); //TangentDir.Rotation().Yaw
			NewComponent->SetWorldRotation(AdjustRotator);

			Start = End;
			RotStart = RotEnd;
			VizMeshes.Add(NewComponent);
		}
	}

	//DestinationVisualizer->SetWorldLocation(JumpPadTarget);
	DestinationVisualizer->SetVisibility(true, true);
	DestinationVisualizer->SetHiddenInGame(true, true);
	DestinationVisualizer->BodyInstance.SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
}

void UShooterJumpVisualizier::SetHiddenInGameAll(bool Value)
{
	for (int i = 0; i < Count-1; ++i)
	{
		if (VizMeshes[i] && VizMeshes[i]->StaticMesh)
		{
			VizMeshes[i]->SetHiddenInGame(Value, true);
		}
	}

	if (DestinationVisualizer && DestinationVisualizer->StaticMesh)
	{
		DestinationVisualizer->SetHiddenInGame(Value, true);
	}
}

void UShooterJumpVisualizier::SetHiddenInGameIndicators(bool Value)
{
	for (int i = 0; i < VizMeshes.Num(); ++i)
	{
		if (VizMeshes[i] && VizMeshes[i]->StaticMesh)
		{
			VizMeshes[i]->SetHiddenInGame(Value, true);
		}
	}
}

void UShooterJumpVisualizier::SetHiddenInGameDestination(bool Value)
{
	if (DestinationVisualizer && DestinationVisualizer->StaticMesh)
	{
		DestinationVisualizer->SetHiddenInGame(Value, true);
	}
}

void UShooterJumpVisualizier::AdjustAlpha(float NewAlpha)
{
	return;
	// TODO :  It was creating dynamic instances and did not delete it. --> Have to be fixeD!
	//FName ParameterName = FName("Opacity");

	//for (int i = 0; i < VizMeshes.Num(); ++i)
	//{
	//	if (VizMeshes[i] && VizMeshes[i]->StaticMesh)
	//	{
	//		UMaterialInstanceDynamic* MeshMID = VizMeshes[i]->CreateDynamicMaterialInstance(0, VizMeshes[i]->StaticGetMesh()->GetMaterial(0));
	//		MeshMID->GetScalarParameterValue(ParameterName, NewAlpha);
	//	}
	//}
}

void UShooterJumpVisualizier::SetDestinationVisualizerLoc(const FVector & Location)
{
	if (DestinationVisualizer && DestinationVisualizer->StaticMesh)
	{
		FVector NewLoc = Location;
		DestinationVisualizer->SetWorldLocation(NewLoc);
	}
}

void UShooterJumpVisualizier::SetIndicationVisualizerScale(const FVector & Scale3D)
{
	for (int i = 0; i < VizMeshes.Num(); ++i)
	{
		if (VizMeshes[i] && VizMeshes[i]->StaticMesh)
		{
			VizMeshes[i]->SetWorldScale3D(Scale3D);
		}
	}
}

void UShooterJumpVisualizier::SetDestinationVisualizerScale(const FVector & Scale3D)
{
	if (DestinationVisualizer && DestinationVisualizer->StaticMesh)
	{
		DestinationVisualizer->SetWorldScale3D(Scale3D);
	}
}

void UShooterJumpVisualizier::SetIndicationVisualizerRotater(const FRotator & Rotate3D)
{
	for (int i = 0; i < VizMeshes.Num(); ++i)
	{
		if (VizMeshes[i] && VizMeshes[i]->StaticMesh)
		{
			VizMeshes[i]->SetWorldRotation(VizMeshes[i]->GetComponentRotation() + Rotate3D);
		}
	}
}

void UShooterJumpVisualizier::SetDestinationVisualizerRotator(const FRotator & Rotate3D)
{
	if (DestinationVisualizer && DestinationVisualizer->StaticMesh)
	{
		DestinationVisualizer->SetWorldRotation(Rotate3D);
	}
}

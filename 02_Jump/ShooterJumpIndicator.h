// This class is meant to be only be used for Editor purpose

#pragma once

#include "Components/PrimitiveComponent.h"
#include "ShooterJumpIndicator.generated.h"

UCLASS()
class SHOOTERGAME_API UShooterJumpIndicator : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()

	UStaticMesh* IndicatorMesh;

	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;

	virtual bool ShouldRecreateProxyOnUpdateTransform() const override { return true; }

	virtual FBoxSphereBounds CalcBounds(const FTransform &LocalToWorld) const override;
};

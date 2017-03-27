#pragma once

#include "FancyBuildingActor.generated.h"

/** An actor that renders a fancy building component */
UCLASS(hidecategories = (Physics)) // Physics category in detail panel is hidden. Our component/Actor is not simulated !
class STREETMAPRUNTIME_API AFancyBuildingActor : public AActor
{
	GENERATED_UCLASS_BODY()

		/**  Component that represents the building */
		UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FancyBuilding")
		class UFancyBuildingComponent* FancyBuildingComponent;

public:
	FORCEINLINE class UFancyBuildingComponent* GetFancyBuildingComponent() { return FancyBuildingComponent; }
};

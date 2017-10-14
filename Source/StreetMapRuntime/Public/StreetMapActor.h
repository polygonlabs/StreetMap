// Copyright 2017 Mike Fricker. All Rights Reserved.

#pragma once

#include "Components/StaticMeshComponent.h"
#include "StreetMapActor.generated.h"

/** An actor that renders a street map mesh component */
UCLASS(HideCategories = (Input, Actor, Rendering, Collision, Lighting, Tags, Activation, Cooking, Mobile, Physics))
class STREETMAPRUNTIME_API AStreetMapActor : public AActor
{
	GENERATED_UCLASS_BODY()

	/**  Component that represents a section of street map roads and buildings */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StreetMap")
		class UStreetMapComponent* StreetMapComponent;

public: 
	FORCEINLINE class UStreetMapComponent* GetStreetMapComponent() { return StreetMapComponent; }
};
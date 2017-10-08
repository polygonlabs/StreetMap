// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "StreetMapRuntime.h"
#include "StreetMapActor.h"
#include "StreetMapComponent.h"

AStreetMapActor::AStreetMapActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	StreetMapComponent = CreateDefaultSubobject<UStreetMapComponent>(TEXT("StreetMapComp"));
	RootComponent = StreetMapComponent;
}


ATrafficSign::ATrafficSign(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	TrafficSignMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SignComp"));
	RootComponent = TrafficSignMesh;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereVisualAsset(TEXT("/Game/StarterContent/Shapes/Shape_Plane.Shape_Plane"));
	if (SphereVisualAsset.Succeeded())
	{
		TrafficSignMesh->SetStaticMesh(SphereVisualAsset.Object);
	}
}
// Fill out your copyright notice in the Description page of Project Settings.

#include "StreetMapRuntime.h"
#include "GuardRail.h"


AGuardRail::AGuardRail(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> MeshAsset(TEXT("StaticMesh'/Game/StarterContent/Shapes/Shape_Cube.Shape_Cube'"));
	if (MeshAsset.Succeeded())
	{
		Mesh = MeshAsset.Object;
	}
}
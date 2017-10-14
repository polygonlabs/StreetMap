// Fill out your copyright notice in the Description page of Project Settings.

#include "StreetMapRuntime.h"
#include "PowerGeneratorWind.h"


APowerGeneratorWind::APowerGeneratorWind(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> MeshAsset(TEXT("StaticMesh'/StreetMap/Meshes/WindTurbine/WindTurbine.WindTurbine'"));
	if (MeshAsset.Succeeded())
	{
		MeshComponent->SetStaticMesh(MeshAsset.Object);
	}
}
// Fill out your copyright notice in the Description page of Project Settings.

#include "StreetMapRuntime.h"
#include "TrafficSign.h"


ATrafficSign::ATrafficSign(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> MeshAsset(TEXT("StaticMesh'/StreetMap/Meshes/Signs/Ortsschild.Ortsschild'"));
	if (MeshAsset.Succeeded())
	{
		MeshComponent->SetStaticMesh(MeshAsset.Object);
	}
}
// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PowerGeneratorWind.generated.h"

/**
 * 
 */
UCLASS()
class STREETMAPRUNTIME_API APowerGeneratorWind : public AActor
{
	GENERATED_UCLASS_BODY()

public:
	/**  Component that represents the geometry in the world */
	UPROPERTY(EditAnywhere)
		UStaticMeshComponent* MeshComponent;
};

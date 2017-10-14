// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Map.h"
#include "GameFramework/Actor.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "OsmBaseTypes.generated.h"


/**
 * 
 */
UCLASS(HideCategories = (Input, Actor, Rendering))
class STREETMAPRUNTIME_API AOsmNode : public AActor
{
	GENERATED_UCLASS_BODY()

public:
	/**  OSM Id */
	UPROPERTY()
	int64 Id;

	/**  OSM tags */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
		TMap<FName, FName> Info;

	/**  OSM tags */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		TMap<FName, FName> OsmTags;

	/**  Component that represents the geometry in the world */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		UStaticMeshComponent* MeshComponent;
};


/**
*
*/
UCLASS(HideCategories=(Input, Actor, Rendering))
class STREETMAPRUNTIME_API AOsmWay : public AActor
{
	GENERATED_UCLASS_BODY()

public:
	/**  OSM tags */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
		TMap<FName, FName> Info;

	/**  OSM tags */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		TMap<FName, FName> OsmTags;

	/**  Component that represents the geometry in the world */
	UPROPERTY()
		USplineComponent* WaySpline;

	/** Mesh that has been assigned to this way */
	UPROPERTY(EditAnywhere)
		UStaticMesh* Mesh;

public:
	/** Construction script that gets fired when changes to this actor are made in editor */
	virtual void OnConstruction(const FTransform & Transform) override;

private:
	/** Internal book keeping for the spline meshes we created along the spline */
	TArray<USplineMeshComponent*> SplineMeshComponents;
};

// Copyright 2017 Mike Fricker. All Rights Reserved.
#pragma once

#include "StreetMap.h"
#include "Components/MeshComponent.h"
#include "Interfaces/Interface_CollisionDataProvider.h"
#include "StreetMapSceneProxy.h"
#include "StreetMapComponent.generated.h"



class UBodySetup;

/**
 * Component that represents a section of street map roads and buildings
 */
UCLASS(meta = (BlueprintSpawnableComponent), hidecategories = (Physics))
class STREETMAPRUNTIME_API UStreetMapComponent : public UMeshComponent, public IInterface_CollisionDataProvider
{
	GENERATED_BODY()

public:

	/** UStreetMapComponent constructor */
	UStreetMapComponent(const class FObjectInitializer& ObjectInitializer);

	/** @return Gets the street map object associated with this component */
	UFUNCTION(BlueprintCallable, Category = "StreetMap")
		UStreetMap* GetStreetMap()
	{
		return StreetMap;
	}

	/** Returns StreetMap asset object name  */
	FString GetStreetMapAssetName() const;

	/** Returns true if we have valid cached mesh data from our assigned street map asset */
	bool HasValidMesh() const
	{
		return (StreetVertices.Num() != 0 && StreetIndices.Num() != 0) ||
			(MajorRoadVertices.Num() != 0 && MajorRoadIndices.Num() != 0) ||
			(HighwayVertices.Num() != 0 && HighwayIndices.Num() != 0) ||
			(BuildingVertices.Num() != 0 && BuildingIndices.Num() != 0);
	}

	/** Returns Cached raw mesh vertices */
	TArray< struct FStreetMapVertex > GetRawMeshVertices(EVertexType type) const
	{
		switch (type) {
		case EVertexType::VBuilding:
			return BuildingVertices;
		case EVertexType::VMajorRoad:
			return MajorRoadVertices;
		case EVertexType::VHighway:
			return HighwayVertices;
		default:
			return StreetVertices;
		}
	}

	/** Returns Cached raw mesh triangle indices */
	TArray< uint32 > GetRawMeshIndices(EVertexType type) const
	{
		switch (type) {
		case EVertexType::VBuilding:
			return BuildingIndices;
		case EVertexType::VMajorRoad:
			return MajorRoadIndices;
		case EVertexType::VHighway:
			return HighwayIndices;
		default:
			return StreetIndices;
		}
	}

	/**
	* Returns StreetMap Default Material if a valid one is found in plugin's content folder.
	* Otherwise , it returns the default surface 3d material.
	*/
	UMaterialInterface* GetDefaultMaterial() const
	{
		return StreetMapDefaultMaterial != nullptr ? StreetMapDefaultMaterial : UMaterial::GetDefaultMaterial(MD_Surface);
	}

	/** Returns true, if the input PropertyName correspond to a collision property. */
	bool IsCollisionProperty(const FName& PropertyName) const
	{
		return PropertyName == TEXT("bGenerateCollision") || PropertyName == TEXT("bAllowDoubleSidedGeometry");
	}

	/**
	*	Returns sub-meshes count.
	*	In this case we are creating one single mesh section, so it will return 1.
	*	If cached mesh data are not valid , it will return 0.
	*/
	int32 GetNumMeshSections() const
	{
		return HasValidMesh() ? 1 : 0;
	}

	/**
	 * Assigns a street map asset to this component.  Render state will be updated immediately.
	 *
	 * @param NewStreetMap The street map to use
	 *
	 * @param bRebuildMesh : Rebuilds map mesh based on the new map asset
	 *
	 * @return Sets the street map object
	 */
	UFUNCTION(BlueprintCallable, Category = "StreetMap")
		void SetStreetMap(UStreetMap* NewStreetMap, bool bClearPreviousMeshIfAny = false, bool bRebuildMesh = false);



	//** Begin Interface_CollisionDataProvider Interface */
	virtual bool GetPhysicsTriMeshData(struct FTriMeshCollisionData* CollisionData, bool InUseAllTriData) override;
	virtual bool ContainsPhysicsTriMeshData(bool InUseAllTriData) const override;
	virtual bool WantsNegXTriMesh() override;
	//** End Interface_CollisionDataProvider Interface *//

protected:

	/**
	* Ensures the body setup is initialized/configured and updates it if needed.
	* @param bForceCreation : Force new BodySetup creation even if a valid one already exists.
	*/
	void CreateBodySetupIfNeeded(bool bForceCreation = false);

	/** Marks collision data as dirty, and re-create on instance if necessary */
	void GenerateCollision();

	/** Wipes out and invalidate collision data. */
	void ClearCollision();

public:

	// UPrimitiveComponent interface
	virtual  UBodySetup* GetBodySetup() override;
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual int32 GetNumMaterials() const override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	/** Wipes out our cached mesh data. Designed to be called on demand.*/
	void InvalidateMesh();

	/** Rebuilds the graphics and physics mesh representation if we don't have one right now.  Designed to be called on demand. */
	void BuildMesh();

	void BuildRoadMesh(EStreetMapRoadType Type);

	void ColorRoadMesh(FLinearColor val, TArray<FStreetMapVertex>& Vertices);
	void ColorRoadMesh(FLinearColor val, TArray<FStreetMapVertex>& Vertices, int64 ID);

	UFUNCTION(BlueprintCallable, Category = "StreetMap")
		void ChangeStreetThickness(float val, EStreetMapRoadType type);

	UFUNCTION(BlueprintCallable, Category = "StreetMap")
		void ChangeStreetColor(FLinearColor val, EStreetMapRoadType type);

	UFUNCTION(BlueprintCallable, Category = "StreetMap")
		void ChangeStreetColorByID(FLinearColor val, EStreetMapRoadType type, int64 ID);

protected:

	/** Giving a default material to the mesh if no valid material is already assigned or materials array is empty. */
	void AssignDefaultMaterialIfNeeded();

	/** Updating navoctree entry for this component , if need/possible. */
	void UpdateNavigationIfNeeded();

	/** Generates a cached mesh from raw street map data */
	void GenerateMesh();

	/** Adds a 2D line to the raw mesh */
	void AddThick2DLine(const FVector2D Start, const FVector2D End, const float Z, const float Thickness, const FColor& StartColor, const FColor& EndColor, FBox& MeshBoundingBox, TArray<FStreetMapVertex>& Vertices, TArray<uint32>& Indices, int64 ID = -1, FString TMC = "");

	/** Adds 3D triangles to the raw mesh */
	void AddTriangles(const TArray<FVector>& Points, const TArray<int32>& PointIndices, const FVector& ForwardVector, const FVector& UpVector, const FColor& Color, FBox& MeshBoundingBox, TArray<FStreetMapVertex>& Vertices, TArray<uint32>& Indices);

	/** Generate a quad for a road segment */
	void AddQuad(const FVector2D Start, const FVector2D End, const float Thickness, const FColor& StartColor, const FColor& EndColor, FBox& MeshBoundingBox, TArray<FStreetMapQuad>& Quads, int64 ID = -1, FString TMC = "");
protected:

	/** The street map we're representing. */
	UPROPERTY(EditAnywhere, Category = "StreetMap")
		UStreetMap* StreetMap;
	
	UPROPERTY(EditAnywhere, Category = "StreetMap")
		FStreetMapMeshBuildSettings MeshBuildSettings;

	UPROPERTY(EditAnywhere, Category = "StreetMap")
		FStreetMapCollisionSettings CollisionSettings;

	UPROPERTY(EditAnywhere, Category = "Landscape")
		FStreetMapLandscapeBuildSettings LandscapeSettings;

	UPROPERTY(EditAnywhere, Category = "Railway")
		FStreetMapRailwayBuildSettings RailwaySettings;

	UPROPERTY(EditAnywhere, Category = "Roads")
		FStreetMapRoadBuildSettings RoadSettings;

	UPROPERTY(EditAnywhere, Category = "Splines")
		FStreetMapSplineBuildSettings SplineSettings;

	//** Physics data for mesh collision. */
	UPROPERTY(Transient)
		UBodySetup* StreetMapBodySetup;

	friend class FStreetMapComponentDetails;

protected:
	//
	// Cached mesh representation
	//

	/** Cached raw mesh vertices */
	UPROPERTY()
		TArray< struct FStreetMapVertex > StreetVertices;
	UPROPERTY()
		TArray< struct FStreetMapVertex > MajorRoadVertices;
	UPROPERTY()
		TArray< struct FStreetMapVertex > HighwayVertices;
	UPROPERTY()
		TArray< struct FStreetMapVertex > BuildingVertices;

	/** Cached raw mesh triangle indices */
	UPROPERTY()
		TArray< uint32 > StreetIndices;
	UPROPERTY()
		TArray< uint32 > MajorRoadIndices;
	UPROPERTY()
		TArray< uint32 > HighwayIndices;
	UPROPERTY()
		TArray< uint32 > BuildingIndices;

	/** Cached bounding box */
	UPROPERTY()
		FBoxSphereBounds CachedLocalBounds;

	/** Cached StreetMap DefaultMaterial */
	UPROPERTY()
		UMaterialInterface* StreetMapDefaultMaterial;
};

// Copyright 2017 Mike Fricker. All Rights Reserved.
// Copyright 2017 Mike Fricker. All Rights Reserved.
#pragma once

#include "Runtime/Engine/Public/PrimitiveSceneProxy.h"
#include "Runtime/Engine/Public/LocalVertexFactory.h"
#include "Runtime/Engine/Public/DynamicMeshBuilder.h"
#include "StreetMapSceneProxy.generated.h"

/**	A single vertex on a street map mesh */
USTRUCT()
struct FStreetMapVertex
{

	GENERATED_USTRUCT_BODY()

	/** Location of the vertex in local space */
	UPROPERTY()
		FVector Position;

	/** Texture coordinate */
	UPROPERTY()
		FVector2D TextureCoordinate;

	/** Texture coordinate */
	UPROPERTY()
		FVector2D TextureCoordinate2;

	/** Texture coordinate */
	UPROPERTY()
		FVector2D TextureCoordinate3;

	/** Texture coordinate */
	UPROPERTY()
		FVector2D TextureCoordinate4;

	/** Tangent vector X */
	UPROPERTY()
		FVector TangentX;

	/** Tangent vector Z (normal) */
	UPROPERTY()
		FVector TangentZ;

	/** Color */
	UPROPERTY()
		FColor Color;

	/** Link ID of road */
	UPROPERTY()
		int64 LinkId;

	/** Link Direction of road */
	UPROPERTY()
		FString LinkDir;

	/** TMC of road */
	UPROPERTY()
		FName TMC;

	/** Speed limit of road (mph) */
	UPROPERTY()
		int SpeedLimit;

	/** Is flagged as trace */
	UPROPERTY()
		uint32 IsTrace:1;

	/** Default constructor, leaves everything uninitialized */
	FStreetMapVertex()
	{
	}

	/** Construct with a supplied position and tangents for the vertex */
	FStreetMapVertex(const FVector InitLocation, const FVector2D InitTextureCoordinate, const FVector InitTangentX, const FVector InitTangentZ, const FColor InitColor)
		: Position(InitLocation),
		TextureCoordinate(InitTextureCoordinate),
		TextureCoordinate2(FVector2D::ZeroVector),
		TextureCoordinate3(FVector2D::ZeroVector),
		TextureCoordinate4(FVector2D::ZeroVector),
		TangentX(InitTangentX),
		TangentZ(InitTangentZ),
		Color(InitColor),
		IsTrace(false)
	{
	}
};


/** Scene proxy for rendering a section of a street map mesh on the rendering thread */
class FStreetMapSceneProxy : public FPrimitiveSceneProxy
{

public:

	/** Construct this scene proxy */
	FStreetMapSceneProxy(const class UStreetMapComponent* InComponent);

	/**
	* Init this street map mesh scene proxy for the specified component (32-bit indices)
	*
	* @param	InComponent			The street map mesh component to initialize this with
	* @param	Vertices			The vertices for this street map mesh
	* @param	Indices				The vertex indices for this street map mesh
	*/
	void Init(const UStreetMapComponent* InComponent, EVertexType Type, const TArray< FStreetMapVertex >& Vertices, const TArray< uint32 >& Indices);

	/** Destructor that cleans up our rendering data */
	virtual ~FStreetMapSceneProxy();

	SIZE_T GetTypeHash() const override;

protected:

	/** Initializes this scene proxy's vertex buffer, index buffer and vertex factory (on the render thread.) */
	void InitResources(FStaticMeshVertexBuffers& VertexBuffer, FDynamicMeshIndexBuffer32& IndexBuffer, FLocalVertexFactory& VertexFactory);

	/** Makes a MeshBatch for rendering.  Called every time the mesh is drawn */
	void MakeMeshBatch(struct FMeshBatch& Mesh, class FMeshElementCollector& Collector, class FMaterialRenderProxy* WireframeMaterialRenderProxyOrNull, const FStaticMeshVertexBuffers& VertexBuffer, const FDynamicMeshIndexBuffer32& IndexBuffer, const FLocalVertexFactory& VertexFactory, bool bDrawCollision = false) const;

	/** Checks to see if this mesh must be drawn during the dynamic pass.  Note that even when this returns false, we may still
	have other (debug) geometry to render as dynamic */
	bool MustDrawMeshDynamically(const class FSceneView& View) const;

	/** Returns true , if in a collision view */
	bool IsInCollisionView(const FEngineShowFlags& EngineShowFlags) const;

	// FPrimitiveSceneProxy interface
	//virtual void DrawStaticElements(class FStaticPrimitiveDrawInterface* PDI) override;
	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, class FMeshElementCollector& Collector) const override;
	void AddDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, class FMeshElementCollector& Collector, const FStaticMeshVertexBuffers& VertexBuffer, const FDynamicMeshIndexBuffer32& IndexBuffer, const FLocalVertexFactory& VertexFactory) const;
	virtual uint32 GetMemoryFootprint(void) const override;
	virtual FPrimitiveViewRelevance GetViewRelevance(const class FSceneView* View) const override;
	virtual bool CanBeOccluded() const override;
	
protected:

	/** Contains all of the vertices in our street map mesh */
	FStaticMeshVertexBuffers StreetVertexBuffer;
	FStaticMeshVertexBuffers MajorRoadVertexBuffer;
	FStaticMeshVertexBuffers HighwayVertexBuffer;
	FStaticMeshVertexBuffers BuildingVertexBuffer;

	/** All of the vertex indices32 in our street map mesh */
	FDynamicMeshIndexBuffer32 StreetIndexBuffer32;
	FDynamicMeshIndexBuffer32 MajorRoadIndexBuffer32;
	FDynamicMeshIndexBuffer32 HighwayIndexBuffer32;
	FDynamicMeshIndexBuffer32 BuildingIndexBuffer32;

	FLocalVertexFactory StreetVertexFactory;
	FLocalVertexFactory MajorRoadVertexFactory;
	FLocalVertexFactory HighwayVertexFactory;
	FLocalVertexFactory BuildingVertexFactory;

	/** Cached material relevance */
	FMaterialRelevance MaterialRelevance;

	/** The material we'll use to render this street map mesh */
	class UMaterialInterface* MaterialInterface;

	const UStreetMapComponent* StreetMapComp;

	// The Collision Response of the component being proxied
	FCollisionResponseContainer CollisionResponse;
};
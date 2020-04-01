// Copyright 2017 Mike Fricker. All Rights Reserved.

#include "StreetMapSceneProxy.h"
#include "StreetMapRuntime.h"
#include "StreetMapComponent.h"
#include "Runtime/Engine/Public/SceneManagement.h"
#include "Runtime/Renderer/Public/MeshPassProcessor.h"
#include "Runtime/Renderer/Public/PrimitiveSceneInfo.h"

FStreetMapSceneProxy::FStreetMapSceneProxy(const UStreetMapComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent),
	BuildingVertexFactory(GetScene().GetFeatureLevel(), "FStreetMapSceneProxy"),
	StreetVertexFactory(GetScene().GetFeatureLevel(), "FStreetMapSceneProxy"),
	MajorRoadVertexFactory(GetScene().GetFeatureLevel(), "FStreetMapSceneProxy"),
	HighwayVertexFactory(GetScene().GetFeatureLevel(), "FStreetMapSceneProxy"),
	StreetMapComp(InComponent),
	CollisionResponse(InComponent->GetCollisionResponseToChannels())
{

}

void FStreetMapSceneProxy::Init(const UStreetMapComponent* InComponent, EVertexType Type, const TArray< FStreetMapVertex >& Vertices, const TArray< uint32 >& Indices)
{
	// Copy index buffer
	switch (Type) {
	case EVertexType::VBuilding:
		BuildingIndexBuffer32.Indices = Indices;
		break;
	case EVertexType::VStreet:
		StreetIndexBuffer32.Indices = Indices;
		break;
	case EVertexType::VMajorRoad:
		MajorRoadIndexBuffer32.Indices = Indices;
		break;
	case EVertexType::VHighway:
		HighwayIndexBuffer32.Indices = Indices;
		break;
	}

	if (Indices.Num() == 0) return;
	
	MaterialInterface = nullptr;
	this->MaterialRelevance = InComponent->GetMaterialRelevance(GetScene().GetFeatureLevel());

	const int32 NumVerts = Vertices.Num();

	TArray<FDynamicMeshVertex> DynamicVertices;
	DynamicVertices.SetNumUninitialized(NumVerts);

	for (int VertIdx = 0; VertIdx < NumVerts; VertIdx++)
	{
		const FStreetMapVertex& StreetMapVert = Vertices[VertIdx];
		FDynamicMeshVertex& Vert = DynamicVertices[VertIdx];
		Vert.Position = StreetMapVert.Position;
		Vert.Color = StreetMapVert.Color;
		Vert.TextureCoordinate[0] = StreetMapVert.TextureCoordinate;
		Vert.TextureCoordinate[1] = StreetMapVert.TextureCoordinate2;
		Vert.TextureCoordinate[2] = StreetMapVert.TextureCoordinate3;
		Vert.TextureCoordinate[3] = StreetMapVert.TextureCoordinate4;
		Vert.TangentX = StreetMapVert.TangentX;
		Vert.TangentZ = StreetMapVert.TangentZ;
	}

	switch (Type) {
	case EVertexType::VBuilding:
		BuildingVertexBuffer.InitFromDynamicVertex(&BuildingVertexFactory, DynamicVertices, 3);
		
		InitResources(BuildingVertexBuffer, BuildingIndexBuffer32, BuildingVertexFactory);
		break;
	case EVertexType::VStreet:
		StreetVertexBuffer.InitFromDynamicVertex(&StreetVertexFactory, DynamicVertices, 3);

		InitResources(StreetVertexBuffer, StreetIndexBuffer32, StreetVertexFactory);
		break;
	case EVertexType::VMajorRoad:
		MajorRoadVertexBuffer.InitFromDynamicVertex(&MajorRoadVertexFactory, DynamicVertices, 3);

		InitResources(MajorRoadVertexBuffer, MajorRoadIndexBuffer32, MajorRoadVertexFactory);
		break;
	case EVertexType::VHighway:
		HighwayVertexBuffer.InitFromDynamicVertex(&HighwayVertexFactory, DynamicVertices, 3);
		
		InitResources(HighwayVertexBuffer, HighwayIndexBuffer32, HighwayVertexFactory);
		break;
	}
	
	// Set a material
	{
		if (InComponent->GetNumMaterials() > 0)
		{
			MaterialInterface = InComponent->GetMaterial(0);
		}

		// Use the default material if we don't have one set
		if (MaterialInterface == nullptr)
		{
			MaterialInterface = UMaterial::GetDefaultMaterial(MD_Surface);
		}
	}
}

FStreetMapSceneProxy::~FStreetMapSceneProxy()
{
	StreetVertexBuffer.PositionVertexBuffer.ReleaseResource();
	StreetVertexBuffer.StaticMeshVertexBuffer.ReleaseResource();
	StreetVertexBuffer.ColorVertexBuffer.ReleaseResource();

	MajorRoadVertexBuffer.PositionVertexBuffer.ReleaseResource();
	MajorRoadVertexBuffer.StaticMeshVertexBuffer.ReleaseResource();
	MajorRoadVertexBuffer.ColorVertexBuffer.ReleaseResource();

	HighwayVertexBuffer.PositionVertexBuffer.ReleaseResource();
	HighwayVertexBuffer.StaticMeshVertexBuffer.ReleaseResource();
	HighwayVertexBuffer.ColorVertexBuffer.ReleaseResource();

	BuildingVertexBuffer.PositionVertexBuffer.ReleaseResource();
	BuildingVertexBuffer.StaticMeshVertexBuffer.ReleaseResource();
	BuildingVertexBuffer.ColorVertexBuffer.ReleaseResource();

	StreetIndexBuffer32.ReleaseResource();
	MajorRoadIndexBuffer32.ReleaseResource();
	HighwayIndexBuffer32.ReleaseResource();
	BuildingIndexBuffer32.ReleaseResource();

	StreetVertexFactory.ReleaseResource();
	MajorRoadVertexFactory.ReleaseResource();
	HighwayVertexFactory.ReleaseResource();
	BuildingVertexFactory.ReleaseResource();
}

SIZE_T FStreetMapSceneProxy::GetTypeHash() const
{
	static size_t UniquePointer;
	return reinterpret_cast<size_t>(&UniquePointer);
}


void FStreetMapSceneProxy::InitResources(FStaticMeshVertexBuffers& VertexBuffer, FDynamicMeshIndexBuffer32& IndexBuffer32, FLocalVertexFactory& VertexFactory)
{
	// Start initializing our vertex buffer, index buffer, and vertex factory.  This will be kicked off on the render thread.
	BeginInitResource(&VertexBuffer.PositionVertexBuffer);
	BeginInitResource(&VertexBuffer.StaticMeshVertexBuffer);
	BeginInitResource(&VertexBuffer.ColorVertexBuffer);
	BeginInitResource(&IndexBuffer32);
	BeginInitResource(&VertexFactory);
}


bool FStreetMapSceneProxy::MustDrawMeshDynamically( const FSceneView& View ) const
{
	//return ( AllowDebugViewmodes() && View.Family->EngineShowFlags.Wireframe ) || IsSelected();
	return true;
}


bool FStreetMapSceneProxy::IsInCollisionView(const FEngineShowFlags& EngineShowFlags) const
{
	return  EngineShowFlags.CollisionVisibility || EngineShowFlags.CollisionPawn;
}

FPrimitiveViewRelevance FStreetMapSceneProxy::GetViewRelevance( const FSceneView* View ) const
{
	FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = IsShown(View);
	Result.bShadowRelevance = IsShadowCast(View);
	
	const bool bAlwaysHasDynamicData = false;

	// Only draw dynamically if we're drawing in wireframe or we're selected in the editor
	Result.bDynamicRelevance = MustDrawMeshDynamically( *View ) || bAlwaysHasDynamicData;
	Result.bStaticRelevance = !MustDrawMeshDynamically( *View );
	
	MaterialRelevance.SetPrimitiveViewRelevance(Result);
	return Result;
}


bool FStreetMapSceneProxy::CanBeOccluded() const
{
	return !MaterialRelevance.bDisableDepthTest;
}


void FStreetMapSceneProxy::MakeMeshBatch(FMeshBatch& Mesh, class FMeshElementCollector& Collector, FMaterialRenderProxy* WireframeMaterialRenderProxyOrNull, const FStaticMeshVertexBuffers& VertexBuffer, const FDynamicMeshIndexBuffer32& IndexBuffer32, const FLocalVertexFactory& VertexFactory, bool bDrawCollision) const
{
	FMaterialRenderProxy* MaterialProxy = NULL;
	if( WireframeMaterialRenderProxyOrNull != nullptr )
	{
		MaterialProxy = WireframeMaterialRenderProxyOrNull;
	}
	else
	{
		if (bDrawCollision)
		{
			MaterialProxy = new FColoredMaterialRenderProxy(GEngine->ShadedLevelColorationUnlitMaterial->GetRenderProxy(), FLinearColor::Blue);
		}
		else if (MaterialProxy == nullptr)
		{
			MaterialProxy = StreetMapComp->GetMaterial(0)->GetRenderProxy();
		}
	}
	
	FMeshBatchElement& BatchElement = Mesh.Elements[0];
	BatchElement.IndexBuffer = &IndexBuffer32;
	Mesh.bWireframe = WireframeMaterialRenderProxyOrNull != nullptr;
	Mesh.VertexFactory = &VertexFactory;
	Mesh.MaterialRenderProxy = MaterialProxy;
	Mesh.CastShadow = true;
	//	BatchElement.PrimitiveUniformBufferResource = &GetUniformBuffer();
	//	BatchElement.PrimitiveUniformBuffer = CreatePrimitiveUniformBufferImmediate(GetLocalToWorld(), GetBounds(), GetLocalBounds(), true, UseEditorDepthTest());

	FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer = Collector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();
	DynamicPrimitiveUniformBuffer.Set(GetLocalToWorld(), GetLocalToWorld(), GetBounds(), GetLocalBounds(), true, false, DrawsVelocity(), false);
	BatchElement.PrimitiveUniformBufferResource = &DynamicPrimitiveUniformBuffer.UniformBuffer;
	BatchElement.FirstIndex = 0;
	const int IndexCount = IndexBuffer32.Indices.Num();
	BatchElement.NumPrimitives = IndexCount / 3;
	BatchElement.MinVertexIndex = 0;
	BatchElement.MaxVertexIndex = VertexBuffer.PositionVertexBuffer.GetNumVertices() - 1;
	Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
	Mesh.Type = PT_TriangleList;
	Mesh.DepthPriorityGroup = SDPG_World;
}
//
//void FStreetMapSceneProxy::DrawStaticElements( FStaticPrimitiveDrawInterface* PDI )
//{
//	const int IndexCount = IndexBuffer32.Indices.Num();
//	if (VertexBuffer.PositionVertexBuffer.GetNumVertices() > 0 && IndexCount > 0)
//	{
//		const float ScreenSize = 1.0f;
//
//		FMeshBatch MeshBatch;
//		MakeMeshBatch( MeshBatch, nullptr);
//		PDI->DrawMesh( MeshBatch, ScreenSize );
//	}
//}


void FStreetMapSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, class FMeshElementCollector& Collector) const
{
	this->AddDynamicMeshElements(Views, ViewFamily, VisibilityMap, Collector, StreetVertexBuffer, StreetIndexBuffer32, StreetVertexFactory);
	this->AddDynamicMeshElements(Views, ViewFamily, VisibilityMap, Collector, MajorRoadVertexBuffer, MajorRoadIndexBuffer32, MajorRoadVertexFactory);
	this->AddDynamicMeshElements(Views, ViewFamily, VisibilityMap, Collector, HighwayVertexBuffer, HighwayIndexBuffer32, HighwayVertexFactory);
	this->AddDynamicMeshElements(Views, ViewFamily, VisibilityMap, Collector, BuildingVertexBuffer, BuildingIndexBuffer32, BuildingVertexFactory);
}

void FStreetMapSceneProxy::AddDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, class FMeshElementCollector& Collector, const FStaticMeshVertexBuffers& VertexBuffer, const FDynamicMeshIndexBuffer32& IndexBuffer32, const FLocalVertexFactory& VertexFactory) const
{
	const int IndexCount = IndexBuffer32.Indices.Num();
	if (VertexBuffer.PositionVertexBuffer.GetNumVertices() > 0 && IndexCount > 0)
	{
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
		{
			const FSceneView& View = *Views[ViewIndex];

			const bool bIsWireframe = AllowDebugViewmodes() && View.Family->EngineShowFlags.Wireframe;

			FColoredMaterialRenderProxy* WireframeMaterialRenderProxy = GEngine->WireframeMaterial && bIsWireframe ? new FColoredMaterialRenderProxy(GEngine->WireframeMaterial->GetRenderProxy(), FLinearColor(0, 0.5f, 1.f)) : NULL;


			if (MustDrawMeshDynamically(View))
			{
				const bool bInCollisionView = IsInCollisionView(ViewFamily.EngineShowFlags);
				const bool bCanDrawCollision = bInCollisionView && IsCollisionEnabled();

				if (!IsCollisionEnabled() && bInCollisionView)
				{
					continue;
				}

				// Draw the mesh!
				FMeshBatch& MeshBatch = Collector.AllocateMesh();
				MakeMeshBatch(MeshBatch, Collector, WireframeMaterialRenderProxy, VertexBuffer, IndexBuffer32, VertexFactory, bCanDrawCollision);
				Collector.AddMesh(ViewIndex, MeshBatch);
			}
		}
	}
}


uint32 FStreetMapSceneProxy::GetMemoryFootprint( void ) const
{ 
	return sizeof( *this ) + GetAllocatedSize();
}

// Copyright 2017 Mike Fricker. All Rights Reserved.

#include "Public/StreetMapComponent.h"
#include "StreetMapRuntime.h"
#include "StreetMapSceneProxy.h"
#include "Runtime/Engine/Classes/Engine/StaticMesh.h"
#include "Runtime/Engine/Public/StaticMeshResources.h"
#include "Engine/Polys.h"
#include "GenericPlatform/GenericPlatformMath.h"
#include "PolygonTools.h"
#include "Async.h"
#include "Spatial/GeometrySet3.h"
#include "RayTypes.h"


#include <algorithm>

#include "PhysicsEngine/BodySetup.h"

#if WITH_EDITOR
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "LandscapeLayerInfoObject.h"
#include "Public\StreetMapComponent.h"
#endif //WITH_EDITOR

// #define BAKE_THICKNESS

UStreetMapComponent::UStreetMapComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer),
	StreetMap(nullptr),
	CachedLocalBounds(FBox(ForceInitToZero))
{
	// We make sure our mesh collision profile name is set to NoCollisionProfileName at initialization. 
	// Because we don't have collision data yet!
	SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

	// We don't currently need to be ticked.  This can be overridden in a derived class though.
	PrimaryComponentTick.bCanEverTick = false;
	this->bAutoActivate = false;	// NOTE: Components instantiated through C++ are not automatically active, so they'll only tick once and then go to sleep!

	// We don't currently need InitializeComponent() to be called on us.  This can be overridden in a
	// derived class though.
	bWantsInitializeComponent = false;

	// Turn on shadows.  It looks better.
	CastShadow = true;

	// Our mesh is too complicated to be a useful occluder.
	bUseAsOccluder = false;

	// Our mesh can influence navigation.
	bCanEverAffectNavigation = true;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DefaultMaterialAsset(TEXT("/StreetMap/StreetMapDefaultInstanceMaterial"));
	StreetMapDefaultMaterial = DefaultMaterialAsset.Object;

	mFlowData.Empty();
	mTraces.Empty();

	mTMC2RoadIndex.Empty();
	mLink2RoadIndex.Empty();

#if WITH_EDITOR
	if (GEngine)
	{
		static ConstructorHelpers::FObjectFinder<UMaterialInterface> DefaultLandscapeMaterialAsset(TEXT("/StreetMap/LandscapeDefaultMaterial"));
		LandscapeSettings.Material = DefaultLandscapeMaterialAsset.Object;

		TArray<FName> LayerNames = ALandscapeProxy::GetLayersFromMaterial(LandscapeSettings.Material);
		LandscapeSettings.Layers.Reset(LayerNames.Num());
		for (int32 i = 0; i < LayerNames.Num(); i++)
		{
			const FName& LayerName = LayerNames[i];

			const FString LayerInfoAssetPath = TEXT("/StreetMap/Landscape_") + LayerName.ToString() + TEXT("_DefaultLayerInfo");
			ConstructorHelpers::FObjectFinder<ULandscapeLayerInfoObject> DefaultLandscapeLayerInfoAsset(*LayerInfoAssetPath);

			FLandscapeImportLayerInfo NewImportLayer;
			NewImportLayer.LayerName = LayerName;
			NewImportLayer.LayerInfo = DefaultLandscapeLayerInfoAsset.Object;

			LandscapeSettings.Layers.Add(MoveTemp(NewImportLayer));
		}
	}
#endif
}

void UStreetMapComponent::IndexStreetMap()
{
	if (StreetMap != nullptr) {
		auto Roads = StreetMap->GetRoads();
		int RoadIndex = 0;
		int PointIndex = 0;

		mHighwayGeometrySet3.Reset();
		mHighwayRoadIndex2PointIndices.Reset();
		mHighwayPointIndex2RoadIndex.Reset();

		mMajorGeometrySet3.Reset();
		mMajorRoadIndex2PointIndices.Reset();
		mMajorPointIndex2RoadIndex.Reset();

		mStreetGeometrySet3.Reset();
		mStreetRoadIndex2PointIndices.Reset();
		mStreetPointIndex2RoadIndex.Reset();

		mTMC2RoadIndex.Reset();
		mLink2RoadIndex.Reset();

		for (auto& Road : Roads)
		{
			TArray<int> PointIndices;
			double PositionZ = 0;

			int LocalPointIndex = 0;
			FPoly Poly = FPoly();

			FGeometrySet3* GeometrySet3;
			TMap<int, TArray<int>>* RoadIndex2PointIndices;
			TMap<int, int>* PointIndex2RoadIndex;

			switch (Road.RoadType) {
			case EStreetMapRoadType::Highway:
				PositionZ = MeshBuildSettings.HighwayOffsetZ;
				GeometrySet3 = &mHighwayGeometrySet3;
				RoadIndex2PointIndices = &mHighwayRoadIndex2PointIndices;
				PointIndex2RoadIndex = &mHighwayPointIndex2RoadIndex;

				break;
			case EStreetMapRoadType::MajorRoad:
				PositionZ = MeshBuildSettings.MajorRoadOffsetZ;
				GeometrySet3 = &mMajorGeometrySet3;
				RoadIndex2PointIndices = &mMajorRoadIndex2PointIndices;
				PointIndex2RoadIndex = &mMajorPointIndex2RoadIndex;

				break;
			default:
				PositionZ = MeshBuildSettings.StreetOffsetZ;
				GeometrySet3 = &mStreetGeometrySet3;
				RoadIndex2PointIndices = &mStreetRoadIndex2PointIndices;
				PointIndex2RoadIndex = &mStreetPointIndex2RoadIndex;

				break;
			}


			// add road points to geometry set
			for (auto& Point : Road.RoadPoints) {
				FVector Vector = FVector(Point.X, Point.Y, PositionZ);

				// build poly
				Poly.InsertVertex(LocalPointIndex, Vector);

				// add point to geometry set
				GeometrySet3->AddPoint(PointIndex, Vector);

				// map point index to road index
				PointIndices.Add(PointIndex);
				PointIndex2RoadIndex->Add(PointIndex, RoadIndex);

				LocalPointIndex++;
				PointIndex++;
			}

			// add midpoint
			FVector MidPoint = Poly.GetMidPoint();
			GeometrySet3->AddPoint(PointIndex, MidPoint);
			PointIndices.Add(PointIndex);
			PointIndex2RoadIndex->Add(PointIndex, RoadIndex);
			PointIndex++;

			//// add 1/4 and 3/4 points
			if (Poly.Vertices.Num() >= 2) {
				FPoly PolyA = FPoly();
				PolyA.InsertVertex(0, Poly.Vertices[0]);
				PolyA.InsertVertex(1, MidPoint);
				GeometrySet3->AddPoint(PointIndex, PolyA.GetMidPoint());
				PointIndices.Add(PointIndex);
				PointIndex2RoadIndex->Add(PointIndex, RoadIndex);
				PointIndex++;

				FPoly PolyB = FPoly();
				PolyB.InsertVertex(0, MidPoint);
				PolyB.InsertVertex(1, Poly.Vertices[Poly.Vertices.Num() - 1]);
				GeometrySet3->AddPoint(PointIndex, PolyB.GetMidPoint());
				PointIndices.Add(PointIndex);
				PointIndex2RoadIndex->Add(PointIndex, RoadIndex);
				PointIndex++;
			}

			// map road index to point indices
			RoadIndex2PointIndices->Add(RoadIndex, PointIndices);

			// map TMC and link to road index
			mTMC2RoadIndex.Add(Road.TMC, RoadIndex);
			mLink2RoadIndex.Add(Road.Link, RoadIndex);

			RoadIndex++;
		}

		IndexVertices(mHighwayLink2Vertices, mHighwayTmcs2Vertices, HighwayVertices, mHighwayGeometryVertSet);
		IndexVertices(mMajorLink2Vertices, mMajorTmcs2Vertices, MajorRoadVertices, mMajorGeometryVertSet);
		IndexVertices(mStreetLink2Vertices, mStreetTmcs2Vertices, StreetVertices, mStreetGeometryVertSet);
	}
}

void UStreetMapComponent::IndexVertices(
	TMap<FStreetMapLink,
	TArray<int>>&LinkMap,
	TMap<FName, TArray<int>>& TmcMap,
	TArray<FStreetMapVertex>& Vertices,
	FGeometrySet3& GeometrySet
) {
	GeometrySet.Reset();
	LinkMap.Reset();
	TmcMap.Reset();

	int NumVertices = Vertices.Num();

	for (int i = 0; i < NumVertices; i++) {
		auto* Vertex = &Vertices[i];

		auto Link = FStreetMapLink(Vertex->LinkId, Vertex->LinkDir);
		if (LinkMap.Contains(Link)) {
			LinkMap[Link].Add(i);
		}
		else {
			auto VertexIndices = TArray<int>({ i });
			LinkMap.Add(Link, VertexIndices);
		}

		if (Vertex->TMC != "None") {
			if (TmcMap.Contains(Vertex->TMC)) {
				TmcMap[Vertex->TMC].Add(i);
			}
			else {
				auto VertexIndices = TArray<int>({ i });
				TmcMap.Add(Vertex->TMC, VertexIndices);
			}
		}

		GeometrySet.AddPoint(i, Vertex->Position);
	}
}

FPrimitiveSceneProxy* UStreetMapComponent::CreateSceneProxy()
{
	FStreetMapSceneProxy* StreetMapSceneProxy = nullptr;

	if (HasValidMesh())
	{
		StreetMapSceneProxy = new FStreetMapSceneProxy(this);
		StreetMapSceneProxy->Init(this, EVertexType::VBuilding, BuildingVertices, BuildingIndices);
		StreetMapSceneProxy->Init(this, EVertexType::VStreet, StreetVertices, StreetIndices);
		StreetMapSceneProxy->Init(this, EVertexType::VMajorRoad, MajorRoadVertices, MajorRoadIndices);
		StreetMapSceneProxy->Init(this, EVertexType::VHighway, HighwayVertices, HighwayIndices);

		IndexStreetMap();
	}

	return StreetMapSceneProxy;
}


int32 UStreetMapComponent::GetNumMaterials() const
{
	// NOTE: This is a bit of a weird thing about Unreal that we need to deal with when defining a component that
	// can have materials assigned.  UPrimitiveComponent::GetNumMaterials() will return 0, so we need to override it 
	// to return the number of overridden materials, which are the actual materials assigned to the component.
	return HasValidMesh() ? GetNumMeshSections() : GetNumOverrideMaterials();
}


void UStreetMapComponent::SetStreetMap(class UStreetMap* NewStreetMap, bool bClearPreviousMeshIfAny /*= false*/, bool bRebuildMesh /*= false */)
{
	if (StreetMap != NewStreetMap)
	{
		StreetMap = NewStreetMap;

		if (bClearPreviousMeshIfAny)
			InvalidateMesh();

		if (bRebuildMesh)
			BuildMesh();
	}
}


bool UStreetMapComponent::GetPhysicsTriMeshData(struct FTriMeshCollisionData* CollisionData, bool InUseAllTriData)
{
	if (!CollisionSettings.bGenerateCollision || !HasValidMesh())
	{
		return false;
	}

	// Copy vertices data
	const int32 NumVertices = BuildingVertices.Num();
	CollisionData->Vertices.Empty();
	CollisionData->Vertices.AddUninitialized(NumVertices);

	for (int32 VertexIndex = 0; VertexIndex < NumVertices; VertexIndex++)
	{
		CollisionData->Vertices[VertexIndex] = BuildingVertices[VertexIndex].Position;
	}

	// Copy indices data
	const int32 NumTriangles = BuildingIndices.Num() / 3;
	FTriIndices TempTriangle;
	for (int32 TriangleIndex = 0; TriangleIndex < NumTriangles * 3; TriangleIndex += 3)
	{

		TempTriangle.v0 = BuildingIndices[TriangleIndex + 0];
		TempTriangle.v1 = BuildingIndices[TriangleIndex + 1];
		TempTriangle.v2 = BuildingIndices[TriangleIndex + 2];


		CollisionData->Indices.Add(TempTriangle);
		CollisionData->MaterialIndices.Add(0);
	}

	CollisionData->bFlipNormals = true;
	CollisionData->bDeformableMesh = true;

	return HasValidMesh();
}


bool UStreetMapComponent::ContainsPhysicsTriMeshData(bool InUseAllTriData) const
{
	return HasValidMesh() && CollisionSettings.bGenerateCollision;
}


bool UStreetMapComponent::WantsNegXTriMesh()
{
	return false;
}


void UStreetMapComponent::CreateBodySetupIfNeeded(bool bForceCreation /*= false*/)
{
	if (StreetMapBodySetup == nullptr || bForceCreation == true)
	{
		// Creating new BodySetup Object.
		StreetMapBodySetup = NewObject<UBodySetup>(this);
		StreetMapBodySetup->BodySetupGuid = FGuid::NewGuid();
		StreetMapBodySetup->bDoubleSidedGeometry = CollisionSettings.bAllowDoubleSidedGeometry;

		// shapes per poly shape for collision (Not working in simulation mode).
		StreetMapBodySetup->CollisionTraceFlag = CTF_UseComplexAsSimple;
	}
}


void UStreetMapComponent::GenerateCollision()
{
	if (!CollisionSettings.bGenerateCollision || !HasValidMesh())
	{
		return;
	}

	// create a new body setup
	CreateBodySetupIfNeeded(true);


	if (GetCollisionProfileName() == UCollisionProfile::NoCollision_ProfileName)
	{
		SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	}

	// Rebuild the body setup
	StreetMapBodySetup->InvalidatePhysicsData();
	StreetMapBodySetup->CreatePhysicsMeshes();

	UpdateNavigationIfNeeded();
}


void UStreetMapComponent::ClearCollision()
{

	if (StreetMapBodySetup != nullptr)
	{
		StreetMapBodySetup->InvalidatePhysicsData();
		StreetMapBodySetup = nullptr;
	}

	if (GetCollisionProfileName() != UCollisionProfile::NoCollision_ProfileName)
	{
		SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	}

	UpdateNavigationIfNeeded();
}

class UBodySetup* UStreetMapComponent::GetBodySetup()
{
	if (CollisionSettings.bGenerateCollision == true)
	{
		// checking if we have a valid body setup. 
		// A new one is created only if a valid body setup is not found.
		CreateBodySetupIfNeeded();
		return StreetMapBodySetup;
	}

	if (StreetMapBodySetup != nullptr) StreetMapBodySetup = nullptr;

	return nullptr;
}

void UStreetMapComponent::GenerateMesh()
{
	/////////////////////////////////////////////////////////
	// Visual tweakables for generated Street Map mesh
	//
	const float StreetOffsetZ = MeshBuildSettings.StreetOffsetZ;
	const float MajorRoadOffsetZ = MeshBuildSettings.MajorRoadOffsetZ;
	const float HighwayOffsetZ = MeshBuildSettings.HighwayOffsetZ;

	const bool bWantSmoothStreets = MeshBuildSettings.bWantSmoothStreets;
	const bool bWantConnectStreets = MeshBuildSettings.bWantConnectStreets;
	const bool bWant3DBuildings = MeshBuildSettings.bWant3DBuildings;

	const float BuildingLevelFloorFactor = MeshBuildSettings.BuildingLevelFloorFactor;
	const bool bWantLitBuildings = MeshBuildSettings.bWantLitBuildings;
	const bool bWantBuildingBorderOnGround = !bWant3DBuildings;

	const float StreetThickness = MeshBuildSettings.StreetThickness;
	const FColor StreetColor = MeshBuildSettings.StreetColor.ToFColor(false);

	const float MajorRoadThickness = MeshBuildSettings.MajorRoadThickness;
	const FColor MajorRoadColor = MeshBuildSettings.MajorRoadColor.ToFColor(false);

	const float HighwayThickness = MeshBuildSettings.HighwayThickness;
	const FColor HighwayColor = MeshBuildSettings.HighwayColor.ToFColor(false);

	const float BuildingBorderThickness = MeshBuildSettings.BuildingBorderThickness;
	FLinearColor BuildingBorderLinearColor = MeshBuildSettings.BuildingBorderLinearColor;
	const float BuildingBorderZ = MeshBuildSettings.BuildingBorderZ;
	const FColor BuildingBorderColor(BuildingBorderLinearColor.ToFColor(false));
	const FColor BuildingFillColor(FLinearColor(BuildingBorderLinearColor * 0.33f).CopyWithNewOpacity(1.0f).ToFColor(false));

	const FColor LowFlowColor = MeshBuildSettings.LowFlowColor.ToFColor(false);
	const FColor MedFlowColor = MeshBuildSettings.MedFlowColor.ToFColor(false);
	const FColor HighFlowColor = MeshBuildSettings.HighFlowColor.ToFColor(false);
	const EColorMode ColorMode = MeshBuildSettings.ColorMode;
	/////////////////////////////////////////////////////////

	CachedLocalBounds = FBox(ForceInitToZero);

	BuildingVertices.Reset();
	BuildingIndices.Reset();
	StreetVertices.Reset();
	StreetIndices.Reset();
	MajorRoadVertices.Reset();
	MajorRoadIndices.Reset();
	HighwayVertices.Reset();
	HighwayIndices.Reset();

	const float MaxThickness = std::max(std::max(StreetThickness, MajorRoadThickness), HighwayThickness);

	if (StreetMap != nullptr)
	{
		FBox MeshBoundingBox;
		MeshBoundingBox.Init();

		auto& Roads = StreetMap->GetRoads();
		const auto& Nodes = StreetMap->GetNodes();
		const auto& Buildings = StreetMap->GetBuildings();

		for (auto& Road : Roads)
		{
			float RoadThickness = HighwayThickness;
			EVertexType VertexType = EVertexType::VHighway;
			float RoadZ = HighwayOffsetZ;

			TArray<FStreetMapVertex>* Vertices = nullptr;
			TArray<uint32>* Indices = nullptr;

			const FName TMC = Road.TMC;
			const FStreetMapLink Link = Road.Link;

			FColor RoadColor;
			float Speed, SpeedLimit, SpeedRatio;
			bool bUseDefaultColor = !GetSpeedAndColorFromData(&Road, Speed, SpeedLimit, SpeedRatio, RoadColor);

			switch (Road.RoadType)
			{
			case EStreetMapRoadType::Highway:
				RoadThickness = HighwayThickness;
				if (bUseDefaultColor) {
					RoadColor = HighFlowColor;
				}
				RoadZ = HighwayOffsetZ;
				VertexType = EVertexType::VHighway;
				Vertices = &HighwayVertices;
				Indices = &HighwayIndices;
				break;

			case EStreetMapRoadType::MajorRoad:
				RoadThickness = MajorRoadThickness;
				if (bUseDefaultColor) {
					RoadColor = HighFlowColor;
				}
				RoadZ = MajorRoadOffsetZ;
				VertexType = EVertexType::VMajorRoad;
				Vertices = &MajorRoadVertices;
				Indices = &MajorRoadIndices;
				break;

			case EStreetMapRoadType::Street:
			case EStreetMapRoadType::Other:
			case EStreetMapRoadType::Bridge:
				RoadThickness = StreetThickness;
				if (bUseDefaultColor) {
					RoadColor = HighFlowColor;
				}
				RoadZ = StreetOffsetZ;
				VertexType = EVertexType::VStreet;
				Vertices = &StreetVertices;
				Indices = &StreetIndices;
				break;

			default:
				check(0);
				break;
			}

			if (Vertices && Indices)
			{
				auto newWay = bWantSmoothStreets && Road.RoadPoints.Num() >= 2;
				float VAccumulation = 0.f;
				if (newWay)
				{
					if (bWantConnectStreets)
					{
						CheckRoadSmoothQuadList(Road,
							true,
							RoadZ,
							RoadThickness,
							MaxThickness,
							RoadColor,
							RoadColor,
							VAccumulation,
							MeshBoundingBox,
							Vertices,
							Indices,
							VertexType,
							Road.Link.LinkId,
							Road.Link.LinkDir,
							Road.TMC,
							Road.SpeedLimit,
							SpeedRatio,
							static_cast<float>(Road.RoadType)
						);
					}
					else
					{
						StartSmoothQuadList(Road.RoadPoints[0],
							Road.RoadPoints[1],
							RoadZ,
							RoadThickness,
							MaxThickness,
							RoadColor,
							RoadColor,
							VAccumulation,
							MeshBoundingBox,
							Vertices,
							Indices,
							VertexType,
							Road.Link.LinkId,
							Road.Link.LinkDir,
							Road.TMC,
							Road.SpeedLimit,
							SpeedRatio,
							static_cast<float>(Road.RoadType)
						);
					}
					int32 PointIndex = 0;
					bool even = true;
					for (; PointIndex < Road.RoadPoints.Num() - 2; ++PointIndex)
					{
						even = PointIndex % 2 == 0;
						AddSmoothQuad(
							Road.RoadPoints[PointIndex],
							Road.RoadPoints[PointIndex + 1],
							Road.RoadPoints[PointIndex + 2],
							RoadZ,
							RoadThickness,
							MaxThickness,
							RoadColor,
							RoadColor,
							VAccumulation,
							MeshBoundingBox,
							Vertices,
							Indices,
							VertexType,
							Road.Link.LinkId,
							Road.Link.LinkDir,
							Road.TMC,
							Road.SpeedLimit,
							SpeedRatio,
							static_cast<float>(Road.RoadType)
						);
					}
					if (bWantConnectStreets)
					{
						CheckRoadSmoothQuadList(Road,
							false,
							RoadZ,
							RoadThickness,
							MaxThickness,
							RoadColor,
							RoadColor,
							VAccumulation,
							MeshBoundingBox,
							Vertices,
							Indices,
							VertexType,
							Road.Link.LinkId,
							Road.Link.LinkDir,
							Road.TMC,
							Road.SpeedLimit,
							SpeedRatio,
							static_cast<float>(Road.RoadType)
						);
					}
					else
					{
						EndSmoothQuadList(Road.RoadPoints[PointIndex],
							Road.RoadPoints[PointIndex + 1],
							RoadZ,
							RoadThickness,
							MaxThickness,
							RoadColor,
							RoadColor,
							VAccumulation,
							MeshBoundingBox,
							Vertices,
							Indices,
							VertexType,
							Road.Link.LinkId,
							Road.Link.LinkDir,
							Road.TMC,
							Road.SpeedLimit,
							SpeedRatio,
							static_cast<float>(Road.RoadType)
						);
					}
				}
				else
				{
					for (int32 PointIndex = 0; PointIndex < Road.RoadPoints.Num() - 1; ++PointIndex)
					{
						AddThick2DLine(
							Road.RoadPoints[PointIndex],
							Road.RoadPoints[PointIndex + 1],
							RoadZ,
							RoadThickness,
							MaxThickness,
							RoadColor,
							RoadColor,
							MeshBoundingBox,
							Vertices,
							Indices,
							VertexType,
							Road.Link.LinkId,
							Road.Link.LinkDir,
							Road.TMC,
							Road.SpeedLimit,
							SpeedRatio,
							static_cast<float>(Road.RoadType)
						);
					}
				}
			}
		}

		TArray< int32 > TempIndices;
		TArray< int32 > TriangulatedVertexIndices;
		TArray< FVector > TempPoints;
		for (int32 BuildingIndex = 0; BuildingIndex < Buildings.Num(); ++BuildingIndex)
		{
			const auto& Building = Buildings[BuildingIndex];

			// Building mesh (or filled area, if the building has no height)

			// Triangulate this building
			// @todo: Performance: Triangulating lots of building polygons is quite slow.  We could easily do this 
			//        as part of the import process and store tessellated geometry instead of doing this at load time.
			bool WindsClockwise;
			if (FPolygonTools::TriangulatePolygon(Building.BuildingPoints, TempIndices, /* Out */ TriangulatedVertexIndices, /* Out */ WindsClockwise))
			{
				// @todo: Performance: We could preprocess the building shapes so that the points always wind
				//        in a consistent direction, so we can skip determining the winding above.

				const int32 FirstTopVertexIndex = this->BuildingVertices.Num();

				// calculate fill Z for buildings
				// either use the defined height or extrapolate from building level count
				float BuildingFillZ = MeshBuildSettings.BuildDefaultZ;
				if (bWant3DBuildings) {
					if (Building.Height > 0) {
						BuildingFillZ = Building.Height;
					}
					else if (Building.BuildingLevels > 0) {
						BuildingFillZ = (float)Building.BuildingLevels * BuildingLevelFloorFactor;
					}
				}

				// Top of building
				{
					TempPoints.SetNum(Building.BuildingPoints.Num(), false);
					for (int32 PointIndex = 0; PointIndex < Building.BuildingPoints.Num(); ++PointIndex)
					{
						TempPoints[PointIndex] = FVector(Building.BuildingPoints[(Building.BuildingPoints.Num() - PointIndex) - 1], BuildingFillZ);
					}
					AddTriangles(TempPoints, TriangulatedVertexIndices, FVector::ForwardVector, FVector::UpVector, BuildingFillColor, MeshBoundingBox, BuildingVertices, BuildingIndices);
				}

				if (bWant3DBuildings && (Building.Height > KINDA_SMALL_NUMBER || Building.BuildingLevels > 0 || MeshBuildSettings.BuildDefaultZ > 0.0))
				{
					// NOTE: Lit buildings can't share vertices beyond quads (all quads have their own face normals), so this uses a lot more geometry!
					if (bWantLitBuildings)
					{
						// Create edges for the walls of the 3D buildings
						for (int32 LeftPointIndex = 0; LeftPointIndex < Building.BuildingPoints.Num(); ++LeftPointIndex)
						{
							const int32 RightPointIndex = (LeftPointIndex + 1) % Building.BuildingPoints.Num();

							TempPoints.SetNum(4, false);

							const int32 TopLeftVertexIndex = 0;
							TempPoints[TopLeftVertexIndex] = FVector(Building.BuildingPoints[WindsClockwise ? RightPointIndex : LeftPointIndex], BuildingFillZ);

							const int32 TopRightVertexIndex = 1;
							TempPoints[TopRightVertexIndex] = FVector(Building.BuildingPoints[WindsClockwise ? LeftPointIndex : RightPointIndex], BuildingFillZ);

							const int32 BottomRightVertexIndex = 2;
							TempPoints[BottomRightVertexIndex] = FVector(Building.BuildingPoints[WindsClockwise ? LeftPointIndex : RightPointIndex], 0.0f);

							const int32 BottomLeftVertexIndex = 3;
							TempPoints[BottomLeftVertexIndex] = FVector(Building.BuildingPoints[WindsClockwise ? RightPointIndex : LeftPointIndex], 0.0f);


							TempIndices.SetNum(6, false);

							TempIndices[0] = BottomLeftVertexIndex;
							TempIndices[1] = TopLeftVertexIndex;
							TempIndices[2] = BottomRightVertexIndex;

							TempIndices[3] = BottomRightVertexIndex;
							TempIndices[4] = TopLeftVertexIndex;
							TempIndices[5] = TopRightVertexIndex;

							const FVector FaceNormal = FVector::CrossProduct((TempPoints[0] - TempPoints[2]).GetSafeNormal(), (TempPoints[0] - TempPoints[1]).GetSafeNormal());
							const FVector ForwardVector = FVector::UpVector;
							const FVector UpVector = FaceNormal;
							AddTriangles(TempPoints, TempIndices, ForwardVector, UpVector, BuildingFillColor, MeshBoundingBox, BuildingVertices, BuildingIndices);
						}
					}
					else
					{
						// Create vertices for the bottom
						const int32 FirstBottomVertexIndex = this->BuildingVertices.Num();
						for (int32 PointIndex = 0; PointIndex < Building.BuildingPoints.Num(); ++PointIndex)
						{
							const FVector2D Point = Building.BuildingPoints[PointIndex];

							FStreetMapVertex& NewVertex = *new(this->BuildingVertices)FStreetMapVertex();
							NewVertex.Position = FVector(Point, 0.0f);
							NewVertex.TextureCoordinate = FVector2D(0.0f, 0.0f);	// NOTE: We're not using texture coordinates for anything yet
							NewVertex.TextureCoordinate2 = FVector2D(0.0f, 0.0f);
							NewVertex.TextureCoordinate3 = FVector2D(0.0f, 1.0f); // Thicknesses
							NewVertex.TextureCoordinate4 = FVector2D(0.0f, 0.0f);
							NewVertex.TextureCoordinate5 = FVector2D(0.0f, 0.0f);
							NewVertex.TangentX = FVector::ForwardVector;	 // NOTE: Tangents aren't important for these unlit buildings
							NewVertex.TangentZ = FVector::UpVector;
							NewVertex.Color = BuildingFillColor;

							MeshBoundingBox += NewVertex.Position;
						}

						// Create edges for the walls of the 3D buildings
						for (int32 LeftPointIndex = 0; LeftPointIndex < Building.BuildingPoints.Num(); ++LeftPointIndex)
						{
							const int32 RightPointIndex = (LeftPointIndex + 1) % Building.BuildingPoints.Num();

							const int32 BottomLeftVertexIndex = FirstBottomVertexIndex + LeftPointIndex;
							const int32 BottomRightVertexIndex = FirstBottomVertexIndex + RightPointIndex;
							const int32 TopRightVertexIndex = FirstTopVertexIndex + RightPointIndex;
							const int32 TopLeftVertexIndex = FirstTopVertexIndex + LeftPointIndex;

							this->BuildingIndices.Add(BottomLeftVertexIndex);
							this->BuildingIndices.Add(TopLeftVertexIndex);
							this->BuildingIndices.Add(BottomRightVertexIndex);

							this->BuildingIndices.Add(BottomRightVertexIndex);
							this->BuildingIndices.Add(TopLeftVertexIndex);
							this->BuildingIndices.Add(TopRightVertexIndex);
						}
					}
				}
			}
			else
			{
				// @todo: Triangulation failed for some reason, possibly due to degenerate polygons.  We can
				//        probably improve the algorithm to avoid this happening.
			}

			// Building border
			if (bWantBuildingBorderOnGround)
			{
				for (int32 PointIndex = 0; PointIndex < Building.BuildingPoints.Num(); ++PointIndex)
				{
					AddThick2DLine(
						Building.BuildingPoints[PointIndex],
						Building.BuildingPoints[(PointIndex + 1) % Building.BuildingPoints.Num()],
						BuildingBorderZ,
						BuildingBorderThickness,		// Thickness
						BuildingBorderThickness,
						BuildingBorderColor,
						BuildingBorderColor,
						MeshBoundingBox,
						&BuildingVertices,
						&BuildingIndices,
						EVertexType::VBuilding
					);
				}
			}
		}

		CachedLocalBounds = MeshBoundingBox;
	}
}

void UStreetMapComponent::BuildRoadMesh(FColor HighFlowColor, FColor MedFlowColor, FColor LowFlowColor) {
	/////////////////////////////////////////////////////////
	// Visual tweakables for generated Street Map mesh
	//
	const float StreetOffsetZ = MeshBuildSettings.StreetOffsetZ;
	const float MajorRoadOffsetZ = MeshBuildSettings.MajorRoadOffsetZ;
	const float HighwayOffsetZ = MeshBuildSettings.HighwayOffsetZ;

	const bool bWantSmoothStreets = MeshBuildSettings.bWantSmoothStreets;
	const bool bWantConnectStreets = MeshBuildSettings.bWantConnectStreets;

	const float StreetThickness = MeshBuildSettings.StreetThickness;
	const FColor StreetColor = MeshBuildSettings.StreetColor.ToFColor(false);

	const float MajorRoadThickness = MeshBuildSettings.MajorRoadThickness;
	const FColor MajorRoadColor = MeshBuildSettings.MajorRoadColor.ToFColor(false);

	const float HighwayThickness = MeshBuildSettings.HighwayThickness;
	const FColor HighwayColor = MeshBuildSettings.HighwayColor.ToFColor(false);

	/////////////////////////////////////////////////////////

	CachedLocalBounds = FBox(ForceInitToZero);

	StreetVertices.Reset();
	StreetIndices.Reset();
	MajorRoadVertices.Reset();
	MajorRoadIndices.Reset();
	HighwayVertices.Reset();
	HighwayIndices.Reset();

	const float MaxThickness = std::max(std::max(StreetThickness, MajorRoadThickness), HighwayThickness);

	if (StreetMap != nullptr)
	{
		FBox MeshBoundingBox;
		MeshBoundingBox.Init();

		auto& Roads = StreetMap->GetRoads();

		for (auto& Road : Roads)
		{
			float RoadThickness = HighwayThickness;
			EVertexType VertexType = EVertexType::VHighway;
			float RoadZ = HighwayOffsetZ;

			TArray<FStreetMapVertex>* Vertices = nullptr;
			TArray<uint32>* Indices = nullptr;

			const FName TMC = Road.TMC;
			const FStreetMapLink Link = Road.Link;

			FColor RoadColor;
			float Speed, SpeedLimit, SpeedRatio;
			bool bUseDefaultColor = !GetSpeedAndColorFromData(&Road, Speed, SpeedLimit, SpeedRatio, RoadColor, HighFlowColor, MedFlowColor, LowFlowColor);

			switch (Road.RoadType)
			{
			case EStreetMapRoadType::Highway:
				RoadThickness = HighwayThickness;
				if (bUseDefaultColor) {
					RoadColor = HighFlowColor;
				}
				RoadZ = HighwayOffsetZ;
				VertexType = EVertexType::VHighway;
				Vertices = &HighwayVertices;
				Indices = &HighwayIndices;
				break;

			case EStreetMapRoadType::MajorRoad:
				RoadThickness = MajorRoadThickness;
				if (bUseDefaultColor) {
					RoadColor = HighFlowColor;
				}
				RoadZ = MajorRoadOffsetZ;
				VertexType = EVertexType::VMajorRoad;
				Vertices = &MajorRoadVertices;
				Indices = &MajorRoadIndices;
				break;

			case EStreetMapRoadType::Street:
			case EStreetMapRoadType::Other:
			case EStreetMapRoadType::Bridge:
				RoadThickness = StreetThickness;
				if (bUseDefaultColor) {
					RoadColor = HighFlowColor;
				}
				RoadZ = StreetOffsetZ;
				VertexType = EVertexType::VStreet;
				Vertices = &StreetVertices;
				Indices = &StreetIndices;
				break;

			default:
				check(0);
				break;
			}

			if (SpeedRatio > HighSpeedRatio) {
				RoadZ += 0.0;
			}
			else if (SpeedRatio > MedSpeedRatio) {
				RoadZ += 0.001;
			}
			else {
				RoadZ += 0.002;
			}

			if (Vertices && Indices)
			{
				auto newWay = bWantSmoothStreets && Road.RoadPoints.Num() >= 2;
				float VAccumulation = 0.f;
				if (newWay)
				{
					if (bWantConnectStreets)
					{
						CheckRoadSmoothQuadList(Road,
							true,
							RoadZ,
							RoadThickness,
							MaxThickness,
							RoadColor,
							RoadColor,
							VAccumulation,
							MeshBoundingBox,
							Vertices,
							Indices,
							VertexType,
							Road.Link.LinkId,
							Road.Link.LinkDir,
							Road.TMC,
							Road.SpeedLimit,
							SpeedRatio,
							static_cast<float>(Road.RoadType)
						);
					}
					else
					{
						StartSmoothQuadList(Road.RoadPoints[0],
							Road.RoadPoints[1],
							RoadZ,
							RoadThickness,
							MaxThickness,
							RoadColor,
							RoadColor,
							VAccumulation,
							MeshBoundingBox,
							Vertices,
							Indices,
							VertexType,
							Road.Link.LinkId,
							Road.Link.LinkDir,
							Road.TMC,
							Road.SpeedLimit,
							SpeedRatio,
							static_cast<float>(Road.RoadType)
						);
					}
					int32 PointIndex = 0;
					bool even = true;
					for (; PointIndex < Road.RoadPoints.Num() - 2; ++PointIndex)
					{
						even = PointIndex % 2 == 0;
						AddSmoothQuad(
							Road.RoadPoints[PointIndex],
							Road.RoadPoints[PointIndex + 1],
							Road.RoadPoints[PointIndex + 2],
							RoadZ,
							RoadThickness,
							MaxThickness,
							RoadColor,
							RoadColor,
							VAccumulation,
							MeshBoundingBox,
							Vertices,
							Indices,
							VertexType,
							Road.Link.LinkId,
							Road.Link.LinkDir,
							Road.TMC,
							Road.SpeedLimit,
							SpeedRatio,
							static_cast<float>(Road.RoadType)
						);
					}
					if (bWantConnectStreets)
					{
						CheckRoadSmoothQuadList(Road,
							false,
							RoadZ,
							RoadThickness,
							MaxThickness,
							RoadColor,
							RoadColor,
							VAccumulation,
							MeshBoundingBox,
							Vertices,
							Indices,
							VertexType,
							Road.Link.LinkId,
							Road.Link.LinkDir,
							Road.TMC,
							Road.SpeedLimit,
							SpeedRatio,
							static_cast<float>(Road.RoadType)
						);
					}
					else
					{
						EndSmoothQuadList(Road.RoadPoints[PointIndex],
							Road.RoadPoints[PointIndex + 1],
							RoadZ,
							RoadThickness,
							MaxThickness,
							RoadColor,
							RoadColor,
							VAccumulation,
							MeshBoundingBox,
							Vertices,
							Indices,
							VertexType,
							Road.Link.LinkId,
							Road.Link.LinkDir,
							Road.TMC,
							Road.SpeedLimit,
							SpeedRatio,
							static_cast<float>(Road.RoadType)
						);
					}
				}
				else
				{
					for (int32 PointIndex = 0; PointIndex < Road.RoadPoints.Num() - 1; ++PointIndex)
					{
						AddThick2DLine(
							Road.RoadPoints[PointIndex],
							Road.RoadPoints[PointIndex + 1],
							RoadZ,
							RoadThickness,
							MaxThickness,
							RoadColor,
							RoadColor,
							MeshBoundingBox,
							Vertices,
							Indices,
							VertexType,
							Road.Link.LinkId,
							Road.Link.LinkDir,
							Road.TMC,
							Road.SpeedLimit,
							SpeedRatio,
							static_cast<float>(Road.RoadType)
						);
					}
				}
			}
		}

		CachedLocalBounds = MeshBoundingBox;

		if (HasValidMesh())
		{
			// We have a new bounding box
			UpdateBounds();
		}
		else
		{
			// No mesh was generated
		}

		GenerateCollision();

		// Mark our render state dirty so that CreateSceneProxy can refresh it on demand
		MarkRenderStateDirty();

		AssignDefaultMaterialIfNeeded();

		Modify();
	}
}

void UStreetMapComponent::BuildRoadMesh(EStreetMapRoadType RoadType, FColor HighFlowColor, FColor MedFlowColor, FColor LowFlowColor)
{
	/////////////////////////////////////////////////////////
	// Visual tweakables for generated Street Map mesh
	//
	const float StreetOffsetZ = MeshBuildSettings.StreetOffsetZ;
	const float MajorRoadOffsetZ = MeshBuildSettings.MajorRoadOffsetZ;
	const float HighwayOffsetZ = MeshBuildSettings.HighwayOffsetZ;

	const float StreetThickness = MeshBuildSettings.StreetThickness;
	const FColor StreetColor = MeshBuildSettings.StreetColor.ToFColor(false);

	const float MajorRoadThickness = MeshBuildSettings.MajorRoadThickness;
	const FColor MajorRoadColor = MeshBuildSettings.MajorRoadColor.ToFColor(false);

	const float HighwayThickness = MeshBuildSettings.HighwayThickness;
	const FColor HighwayColor = MeshBuildSettings.HighwayColor.ToFColor(false);

	const bool bWantSmoothStreets = MeshBuildSettings.bWantSmoothStreets;
	const bool bWantConnectStreets = MeshBuildSettings.bWantConnectStreets;

	/////////////////////////////////////////////////////////
	const float MaxThickness = std::max(std::max(StreetThickness, MajorRoadThickness), HighwayThickness);

	CachedLocalBounds = FBox(ForceInitToZero);

	switch (RoadType) {
	case EStreetMapRoadType::Highway:
		HighwayIndices.Reset();
		break;
	case EStreetMapRoadType::MajorRoad:
		MajorRoadIndices.Reset();
		break;
	case EStreetMapRoadType::Street:
		StreetIndices.Reset();
		break;
	}

	if (StreetMap != nullptr)
	{
		FBox MeshBoundingBox;
		MeshBoundingBox.Init();

		auto& Roads = StreetMap->GetRoads();

		for (auto& Road : Roads)
		{
			if (Road.RoadType == RoadType) {
				float RoadThickness = HighwayThickness;
				FColor RoadColor = HighFlowColor;
				float RoadZ = HighwayOffsetZ;
				EVertexType VertexType = EVertexType::VHighway;
				TArray<FStreetMapVertex>* Vertices = nullptr;
				TArray<uint32>* Indices = nullptr;

				const FName TMC = Road.TMC;
				const FStreetMapLink Link = Road.Link;

				float Speed, SpeedLimit, SpeedRatio;
				bool bUseDefaultColor = !GetSpeedAndColorFromData(&Road, Speed, SpeedLimit, SpeedRatio, RoadColor);

				switch (Road.RoadType)
				{
				case EStreetMapRoadType::Highway:
					RoadThickness = HighwayThickness;
					if (bUseDefaultColor) {
						RoadColor = HighFlowColor;
					}
					RoadZ = HighwayOffsetZ;
					VertexType = EVertexType::VHighway;
					Vertices = &HighwayVertices;
					Indices = &HighwayIndices;
					break;

				case EStreetMapRoadType::MajorRoad:
					RoadThickness = MajorRoadThickness;
					if (bUseDefaultColor) {
						RoadColor = HighFlowColor;
					}
					RoadZ = MajorRoadOffsetZ;
					VertexType = EVertexType::VMajorRoad;
					Vertices = &MajorRoadVertices;
					Indices = &MajorRoadIndices;
					break;

				case EStreetMapRoadType::Street:
				case EStreetMapRoadType::Other:
				case EStreetMapRoadType::Bridge:
					RoadThickness = StreetThickness;
					if (bUseDefaultColor) {
						RoadColor = HighFlowColor;
					}
					RoadZ = StreetOffsetZ;
					VertexType = EVertexType::VStreet;
					Vertices = &StreetVertices;
					Indices = &StreetIndices;
					break;

				default:
					check(0);
					break;
				}

				if (SpeedRatio > HighSpeedRatio) {
					RoadZ += 0.0;
				}
				else if (SpeedRatio > MedSpeedRatio) {
					RoadZ += 0.001;
				}
				else {
					RoadZ += 0.002;
				}

				if (Vertices && Indices)
				{
					auto newWay = bWantSmoothStreets && Road.RoadPoints.Num() >= 2;
					float VAccumulation = 0.f;
					if (newWay)
					{
						if (bWantConnectStreets)
						{
							CheckRoadSmoothQuadList(Road,
								true,
								RoadZ,
								RoadThickness,
								MaxThickness,
								RoadColor,
								RoadColor,
								VAccumulation,
								MeshBoundingBox,
								Vertices,
								Indices,
								VertexType,
								Road.Link.LinkId,
								Road.Link.LinkDir,
								Road.TMC,
								Road.SpeedLimit,
								SpeedRatio,
								static_cast<float>(Road.RoadType)
							);
						}
						else
						{
							StartSmoothQuadList(Road.RoadPoints[0],
								Road.RoadPoints[1],
								RoadZ,
								RoadThickness,
								MaxThickness,
								RoadColor,
								RoadColor,
								VAccumulation,
								MeshBoundingBox,
								Vertices,
								Indices,
								VertexType,
								Road.Link.LinkId,
								Road.Link.LinkDir,
								Road.TMC,
								Road.SpeedLimit,
								SpeedRatio,
								static_cast<float>(Road.RoadType)
							);
						}
						int32 PointIndex = 0;
						bool even = true;
						for (; PointIndex < Road.RoadPoints.Num() - 2; ++PointIndex)
						{
							even = PointIndex % 2 == 0;
							AddSmoothQuad(
								Road.RoadPoints[PointIndex],
								Road.RoadPoints[PointIndex + 1],
								Road.RoadPoints[PointIndex + 2],
								RoadZ,
								RoadThickness,
								MaxThickness,
								RoadColor,
								RoadColor,
								VAccumulation,
								MeshBoundingBox,
								Vertices,
								Indices,
								VertexType,
								Road.Link.LinkId,
								Road.Link.LinkDir,
								Road.TMC,
								Road.SpeedLimit,
								SpeedRatio,
								static_cast<float>(Road.RoadType)
							);
						}
						if (bWantConnectStreets)
						{
							CheckRoadSmoothQuadList(Road,
								false,
								RoadZ,
								RoadThickness,
								MaxThickness,
								RoadColor,
								RoadColor,
								VAccumulation,
								MeshBoundingBox,
								Vertices,
								Indices,
								VertexType,
								Road.Link.LinkId,
								Road.Link.LinkDir,
								Road.TMC,
								Road.SpeedLimit,
								SpeedRatio,
								static_cast<float>(Road.RoadType)
							);
						}
						else
						{
							EndSmoothQuadList(Road.RoadPoints[PointIndex],
								Road.RoadPoints[PointIndex + 1],
								RoadZ,
								RoadThickness,
								MaxThickness,
								RoadColor,
								RoadColor,
								VAccumulation,
								MeshBoundingBox,
								Vertices,
								Indices,
								VertexType,
								Road.Link.LinkId,
								Road.Link.LinkDir,
								Road.TMC,
								Road.SpeedLimit,
								SpeedRatio,
								static_cast<float>(Road.RoadType)
							);
						}
					}
					else
					{
						for (int32 PointIndex = 0; PointIndex < Road.RoadPoints.Num() - 1; ++PointIndex)
						{
							AddThick2DLine(
								Road.RoadPoints[PointIndex],
								Road.RoadPoints[PointIndex + 1],
								RoadZ,
								RoadThickness,
								MaxThickness,
								RoadColor,
								RoadColor,
								MeshBoundingBox,
								Vertices,
								Indices,
								VertexType,
								Road.Link.LinkId,
								Road.Link.LinkDir,
								Road.TMC,
								Road.SpeedLimit,
								SpeedRatio,
								static_cast<float>(Road.RoadType)
							);
						}
					}
				}
			}
		}

		CachedLocalBounds = MeshBoundingBox;

		if (HasValidMesh())
		{
			// We have a new bounding box
			UpdateBounds();
		}
		else
		{
			// No mesh was generated
		}

		GenerateCollision();

		// Mark our render state dirty so that CreateSceneProxy can refresh it on demand
		MarkRenderStateDirty();

		AssignDefaultMaterialIfNeeded();

		Modify();
	}
}

TArray<FVector> UStreetMapComponent::GetRoadVertices(const FStreetMapRoad& Road) {
	TArray<FVector> Vertices;
	TArray<FStreetMapVertex>* StreetMapVertices;
	TMap<FStreetMapLink, TArray<int>>* LinkMap;

	switch (Road.RoadType) {
	case EStreetMapRoadType::MajorRoad:
		StreetMapVertices = &MajorRoadVertices;
		LinkMap = &mMajorLink2Vertices;
		break;
	case EStreetMapRoadType::Highway:
		StreetMapVertices = &HighwayVertices;
		LinkMap = &mHighwayLink2Vertices;
		break;
	default:
		StreetMapVertices = &StreetVertices;
		LinkMap = &mStreetLink2Vertices;
		break;
	}

	if ((*LinkMap).Contains(Road.Link)) {
		TArray<int> VertexIndices = (*LinkMap)[Road.Link];

		for (int VertexIndex : VertexIndices) {
			Vertices.Add((*StreetMapVertices)[VertexIndex].Position);
		}
	}

	return Vertices;
}

FStreetMapRoad UStreetMapComponent::GetClosestRoad(
	FVector Origin,
	FVector Direction,
	FStreetMapRoad& NearestHighway,
	float& NearestHighwayDistance,
	FStreetMapRoad& NearestMajorRoad,
	float& NearestMajorRoadDistance,
	FStreetMapRoad& NearestStreet,
	float& NearestStreetDistance,
	EStreetMapRoadType MaxRoadType = EStreetMapRoadType::Highway
) {
	auto Roads = StreetMap->GetRoads();
	FStreetMapRoad NearestRoad;
	double ClosestDistance = DBL_MAX;
	FRay3d Ray3d;
	Ray3d.Origin = Origin;
	Ray3d.Direction = Direction;

	FGeometrySet3::FNearest Nearest;
	float MaxDistance = 250.0f;
	switch (MaxRoadType)
	{
	case EStreetMapRoadType::Highway:
		MaxDistance = 10000.0f;
		break;
	case EStreetMapRoadType::MajorRoad:
		MaxDistance = 2500.0f;
		break;
	default:
		MaxDistance = 1000.0f;
		break;
	}

	TFunction <bool(const FVector3d&, const FVector3d&)> PointWithinToleranceTest = [&](const FVector3d& RayPoint, const FVector3d& Point) {
		return RayPoint.DistanceSquared(Point) < MaxDistance;
	};

	//if (mHighwayGeometrySet3.FindNearestPointToRay(Ray3d, Nearest, PointWithinToleranceTest))
	//{
	//	int PointIndex = Nearest.ID;
	//	if (mHighwayPointIndex2RoadIndex.Contains(PointIndex)) {
	//		int RoadIndex = mHighwayPointIndex2RoadIndex[PointIndex];
	//		if (RoadIndex >= 0 && RoadIndex < Roads.Num()) {
	//			NearestHighway = Roads[RoadIndex];
	//			ClosestDistance = Nearest.NearestRayPoint.DistanceSquared(Nearest.NearestGeoPoint);
	//			NearestRoad = NearestHighway;
	//		}
	//	}
	//}

	//if (mMajorGeometrySet3.FindNearestPointToRay(Ray3d, Nearest, PointWithinToleranceTest))
	//{
	//	int PointIndex = Nearest.ID;
	//	if (mMajorPointIndex2RoadIndex.Contains(PointIndex)) {
	//		int RoadIndex = mMajorPointIndex2RoadIndex[PointIndex];
	//		if (RoadIndex >= 0 && RoadIndex < Roads.Num()) {
	//			NearestMajorRoad = Roads[RoadIndex];
	//			double Distance = Nearest.NearestRayPoint.DistanceSquared(Nearest.NearestGeoPoint);
	//			Distance *= 1.5; // make larger roads easier to pick by multiplying distance
	//			if (Distance < ClosestDistance) {
	//				ClosestDistance = Distance;
	//				NearestRoad = NearestMajorRoad;
	//			}
	//		}
	//	}
	//}

	//if (mStreetGeometrySet3.FindNearestPointToRay(Ray3d, Nearest, PointWithinToleranceTest))
	//{
	//	int PointIndex = Nearest.ID;
	//	if (mStreetPointIndex2RoadIndex.Contains(PointIndex)) {
	//		int RoadIndex = mStreetPointIndex2RoadIndex[PointIndex];
	//		if (RoadIndex >= 0 && RoadIndex < Roads.Num()) {
	//			NearestStreet = Roads[RoadIndex];
	//			double Distance = Nearest.NearestRayPoint.DistanceSquared(Nearest.NearestGeoPoint);
	//			Distance *= 2.0; // make larger roads easier to pick by multiplying distance
	//			if (Distance < ClosestDistance) {
	//				ClosestDistance = Distance;
	//				NearestRoad = NearestStreet;
	//			}
	//		}
	//	}
	//}

	if (mHighwayGeometryVertSet.FindNearestPointToRay(Ray3d, Nearest, PointWithinToleranceTest))
	{
		int VertIndex = Nearest.ID;
		auto Vertex = HighwayVertices[VertIndex];
		auto Link = FStreetMapLink(Vertex.LinkId, Vertex.LinkDir);

		if (mLink2RoadIndex.Contains(Link))
		{
			int RoadIndex = mLink2RoadIndex[Link];
			if (RoadIndex >= 0 && RoadIndex < Roads.Num()) {
				NearestHighway = Roads[RoadIndex];
				NearestHighwayDistance = Nearest.NearestRayPoint.DistanceSquared(Nearest.NearestGeoPoint);
				ClosestDistance = NearestHighwayDistance;
				NearestRoad = NearestHighway;
			}
		}
	}

	if (mMajorGeometryVertSet.FindNearestPointToRay(Ray3d, Nearest, PointWithinToleranceTest))
	{
		int VertIndex = Nearest.ID;
		auto Vertex = MajorRoadVertices[VertIndex];
		auto Link = FStreetMapLink(Vertex.LinkId, Vertex.LinkDir);

		if (mLink2RoadIndex.Contains(Link))
		{
			int RoadIndex = mLink2RoadIndex[Link];
			if (RoadIndex >= 0 && RoadIndex < Roads.Num()) {
				NearestMajorRoad = Roads[RoadIndex];
				NearestMajorRoadDistance = Nearest.NearestRayPoint.DistanceSquared(Nearest.NearestGeoPoint);
				if (NearestMajorRoadDistance < ClosestDistance && MaxRoadType != EStreetMapRoadType::Highway) {
					ClosestDistance = NearestMajorRoadDistance;
					NearestRoad = NearestMajorRoad;
				}
			}
		}
	}

	if (mStreetGeometryVertSet.FindNearestPointToRay(Ray3d, Nearest, PointWithinToleranceTest))
	{
		int VertIndex = Nearest.ID;
		auto Vertex = StreetVertices[VertIndex];
		auto Link = FStreetMapLink(Vertex.LinkId, Vertex.LinkDir);

		if (mLink2RoadIndex.Contains(Link))
		{
			int RoadIndex = mLink2RoadIndex[Link];
			if (RoadIndex >= 0 && RoadIndex < Roads.Num()) {
				NearestStreet = Roads[RoadIndex];
				NearestStreetDistance = Nearest.NearestRayPoint.DistanceSquared(Nearest.NearestGeoPoint);
				if (NearestStreetDistance < ClosestDistance && MaxRoadType == EStreetMapRoadType::Street) {
					ClosestDistance = NearestStreetDistance;
					NearestRoad = NearestStreet;
				}
			}
		}
	}

	UE_LOG(LogStreetMap, Log, TEXT("Clicked Road: %s"), *NearestRoad.RoadName);

	return NearestRoad;
}

bool UStreetMapComponent::GetSpeedAndColorFromData(const FStreetMapRoad* Road, float& Speed, float& SpeedLimit, float& SpeedRatio, FColor& Color, FColor HighFlowColor, FColor MedFlowColor, FColor LowFlowColor) {
	Speed = Road->SpeedLimit;
	SpeedLimit = Road->SpeedLimit;
	SpeedRatio = 1.0f;

	FName TMC = Road->TMC;
	bool bFound = false;

	switch (MeshBuildSettings.ColorMode) {
	case EColorMode::Default:
		break;
	case EColorMode::Flow:
		if (mFlowData.Contains(TMC)) {
			Speed = mFlowData[TMC];
			bFound = true;
		}
		break;
	case EColorMode::Predictive0:
		if (mPredictiveData.Contains(TMC)) {
			Speed = mPredictiveData[TMC].S0;
			bFound = true;
		}
		break;
	case EColorMode::Predictive15:
		if (mPredictiveData.Contains(TMC)) {
			Speed = mPredictiveData[TMC].S15;
			bFound = true;
		}
		break;
	case EColorMode::Predictive30:
		if (mPredictiveData.Contains(TMC)) {
			Speed = mPredictiveData[TMC].S30;
			bFound = true;
		}
		break;
	case EColorMode::Predictive45:
		if (mPredictiveData.Contains(TMC)) {
			Speed = mPredictiveData[TMC].S45;
			bFound = true;
		}
		break;
	}

	SpeedRatio = FGenericPlatformMath::Min(Speed / Road->SpeedLimit, 1.0f);

	if (SpeedRatio > HighSpeedRatio) {
		Color = HighFlowColor;
	}
	else if (SpeedRatio > MedSpeedRatio) {
		Color = MedFlowColor;
	}
	else {
		Color = LowFlowColor;
	}

	return bFound;
}

bool UStreetMapComponent::GetSpeedAndColorFromData(const FStreetMapRoad* Road, float& Speed, float& SpeedLimit, float& SpeedRatio, FColor& Color) {
	const FColor LowFlowColor = MeshBuildSettings.LowFlowColor.ToFColor(false);
	const FColor MedFlowColor = MeshBuildSettings.MedFlowColor.ToFColor(false);
	const FColor HighFlowColor = MeshBuildSettings.HighFlowColor.ToFColor(false);

	return GetSpeedAndColorFromData(Road, Speed, SpeedLimit, SpeedRatio, Color, HighFlowColor, MedFlowColor, LowFlowColor);
}

bool UStreetMapComponent::GetSpeedAndColorFromData(FName TMC, float SpeedLimit, float& Speed, float& SpeedRatio, FColor& Color, FColor HighFlowColor, FColor MedFlowColor, FColor LowFlowColor) {
	FStreetMapRoad Road;
	Road.TMC = TMC;
	Road.SpeedLimit = SpeedLimit;
	float SpeedLimitDummy;

	return GetSpeedAndColorFromData(&Road, Speed, SpeedLimitDummy, SpeedRatio, Color, HighFlowColor, MedFlowColor, LowFlowColor);
}

bool UStreetMapComponent::GetSpeedAndColorFromData(FName TMC, float SpeedLimit, float& Speed, float& SpeedRatio, FColor& Color) {
	const FColor LowFlowColor = MeshBuildSettings.LowFlowColor.ToFColor(false);
	const FColor MedFlowColor = MeshBuildSettings.MedFlowColor.ToFColor(false);
	const FColor HighFlowColor = MeshBuildSettings.HighFlowColor.ToFColor(false);

	return GetSpeedAndColorFromData(TMC, SpeedLimit, Speed, SpeedRatio, Color, HighFlowColor, MedFlowColor, LowFlowColor);
}

void UStreetMapComponent::ColorRoadMesh(FLinearColor val, TArray<FStreetMapVertex>& Vertices, bool IsTrace, float ZOffset)
{
	int NumVertices = Vertices.Num();

	for (int i = 0; i < NumVertices; i++) {
		Vertices[i].Color = val.ToFColor(false);
		Vertices[i].IsTrace = IsTrace;
		if (ZOffset != 0.0f) {
			Vertices[i].Position.Z = ZOffset;
		}
	}

	// Mark our render state dirty so that CreateSceneProxy can refresh it on demand
	MarkRenderStateDirty();

	Modify();
}

void UStreetMapComponent::ColorRoadMesh(FLinearColor val, FStreetMapLink Link, bool IsTrace, float ZOffset)
{
	auto& Roads = StreetMap->GetRoads();

	if (!mLink2RoadIndex.Contains(Link)) return;

	int RoadIndex = mLink2RoadIndex[Link];
	auto Road = Roads[RoadIndex];
	TMap<FStreetMapLink, TArray<int>>* LinkMap;
	TArray<FStreetMapVertex>* Vertices;

	switch (Road.RoadType) {
	case EStreetMapRoadType::Highway:
		LinkMap = &mHighwayLink2Vertices;
		Vertices = &HighwayVertices;
		break;
	case EStreetMapRoadType::MajorRoad:
		LinkMap = &mMajorLink2Vertices;
		Vertices = &MajorRoadVertices;
		break;
	default:
		LinkMap = &mStreetLink2Vertices;
		Vertices = &StreetVertices;
		break;
	}

	for (int VertexIndex : (*LinkMap)[Link]) {
		(*Vertices)[VertexIndex].Color = val.ToFColor(false);
		(*Vertices)[VertexIndex].IsTrace = IsTrace;
		if (ZOffset != 0.0f) {
			(*Vertices)[VertexIndex].Position.Z = ZOffset;
		}
	}

	// Mark our render state dirty so that CreateSceneProxy can refresh it on demand
	MarkRenderStateDirty();

	//Modify();
}

void UStreetMapComponent::ColorRoadMesh(FLinearColor val, TArray<FStreetMapLink> Links, bool IsTrace, float ZOffset)
{
	auto& Roads = StreetMap->GetRoads();

	for (auto& Link : Links) {
		if (mLink2RoadIndex.Contains(Link)) {
			int RoadIndex = mLink2RoadIndex[Link];
			auto Road = Roads[RoadIndex];
			TMap<FStreetMapLink, TArray<int>>* LinkMap;
			TArray<FStreetMapVertex>* Vertices;

			switch (Road.RoadType) {
			case EStreetMapRoadType::Highway:
				LinkMap = &mHighwayLink2Vertices;
				Vertices = &HighwayVertices;
				break;
			case EStreetMapRoadType::MajorRoad:
				LinkMap = &mMajorLink2Vertices;
				Vertices = &MajorRoadVertices;
				break;
			default:
				LinkMap = &mStreetLink2Vertices;
				Vertices = &StreetVertices;
				break;
			}

			auto LinkVerts = (*LinkMap)[Link];
			for (int VertexIndex : LinkVerts) {
				(*Vertices)[VertexIndex].Color = val.ToFColor(false);
				(*Vertices)[VertexIndex].IsTrace = IsTrace;
				if (ZOffset != 0.0f) {
					(*Vertices)[VertexIndex].Position.Z = ZOffset;
				}
			}
		}
	}

	// Mark our render state dirty so that CreateSceneProxy can refresh it on demand
	MarkRenderStateDirty();

	//Modify();
}

void UStreetMapComponent::ColorRoadMesh(FLinearColor val, FName TMC, bool IsTrace, float ZOffset)
{
	auto& Roads = StreetMap->GetRoads();

	if (!mTMC2RoadIndex.Contains(TMC)) return;

	int RoadIndex = mTMC2RoadIndex[TMC];
	auto Road = Roads[RoadIndex];
	TMap<FName, TArray<int>>* TmcMap;
	TArray<FStreetMapVertex>* Vertices;

	switch (Road.RoadType) {
	case EStreetMapRoadType::Highway:
		TmcMap = &mHighwayTmcs2Vertices;
		Vertices = &HighwayVertices;
		break;
	case EStreetMapRoadType::MajorRoad:
		TmcMap = &mMajorTmcs2Vertices;
		Vertices = &MajorRoadVertices;
		break;
	default:
		TmcMap = &mStreetTmcs2Vertices;
		Vertices = &StreetVertices;
		break;
	}

	for (int VertexIndex : (*TmcMap)[TMC]) {
		(*Vertices)[VertexIndex].Color = val.ToFColor(false);
		(*Vertices)[VertexIndex].IsTrace = IsTrace;
		if (ZOffset != 0.0f) {
			(*Vertices)[VertexIndex].Position.Z = ZOffset;
		}
	}

	// Mark our render state dirty so that CreateSceneProxy can refresh it on demand
	MarkRenderStateDirty();

	//Modify();
}

void UStreetMapComponent::ColorRoadMesh(FLinearColor val, TArray<FName> TMCs, bool IsTrace, float ZOffset)
{
	auto& Roads = StreetMap->GetRoads();

	for (auto& TMC : TMCs) {
		if (mTMC2RoadIndex.Contains(TMC)) {
			int RoadIndex = mTMC2RoadIndex[TMC];
			auto Road = Roads[RoadIndex];
			TMap<FName, TArray<int>>* TmcMap;
			TArray<FStreetMapVertex>* Vertices;

			switch (Road.RoadType) {
			case EStreetMapRoadType::Highway:
				TmcMap = &mHighwayTmcs2Vertices;
				Vertices = &HighwayVertices;
				break;
			case EStreetMapRoadType::MajorRoad:
				TmcMap = &mMajorTmcs2Vertices;
				Vertices = &MajorRoadVertices;
				break;
			default:
				TmcMap = &mStreetTmcs2Vertices;
				Vertices = &StreetVertices;
				break;
			}

			for (int VertexIndex : (*TmcMap)[TMC]) {
				(*Vertices)[VertexIndex].Color = val.ToFColor(false);
				(*Vertices)[VertexIndex].IsTrace = IsTrace;
				if (ZOffset != 0.0f) {
					(*Vertices)[VertexIndex].Position.Z = ZOffset;
				}
			}
		}
	}

	// Mark our render state dirty so that CreateSceneProxy can refresh it on demand
	MarkRenderStateDirty();

	//Modify();
}

void UStreetMapComponent::ColorRoadMeshFromData(TArray<FStreetMapVertex> & Vertices, FColor DefaultColor, FColor LowFlowColor, FColor MedFlowColor, FColor HighFlowColor, bool OverwriteTrace, float ZOffset) {
	int NumVertices = Vertices.Num();
	for (int i = 0; i < NumVertices; i++) {
		auto* Vertex = &Vertices[i];
		if (!Vertex->IsTrace || OverwriteTrace) {
			const FName TMC = Vertex->TMC;

			FColor RoadColor;
			float Speed, SpeedRatio;
			bool bUseDefaultColor = !GetSpeedAndColorFromData(TMC, Vertex->SpeedLimit, Speed, SpeedRatio, RoadColor, HighFlowColor, MedFlowColor, LowFlowColor);
			if (bUseDefaultColor) {
				RoadColor = DefaultColor;
			}

			Vertex->Color = RoadColor;

			auto Direction = Vertex->TextureCoordinate4.Y;
			Vertex->TextureCoordinate4 = FVector2D(SpeedRatio * 100, Direction);

			if (ZOffset != 0.0f) {
				Vertex->Position.Z = ZOffset;
			}

			if (SpeedRatio > HighSpeedRatio) {
				Vertex->Position.Z += 0.0;
			}
			else if (SpeedRatio > MedSpeedRatio) {
				Vertex->Position.Z += 0.001;
			}
			else {
				Vertex->Position.Z += 0.002;
			}
		}
	}
}

void UStreetMapComponent::ColorRoadMeshFromData(TArray<FStreetMapVertex>& Vertices, FLinearColor DefaultColor, bool OverwriteTrace, float ZOffset) {
	const FColor LowFlowColor = MeshBuildSettings.LowFlowColor.ToFColor(false);
	const FColor MedFlowColor = MeshBuildSettings.MedFlowColor.ToFColor(false);
	const FColor HighFlowColor = MeshBuildSettings.HighFlowColor.ToFColor(false);

	ColorRoadMeshFromData(
		Vertices,
		DefaultColor.ToFColor(false),
		LowFlowColor,
		MedFlowColor,
		HighFlowColor,
		OverwriteTrace,
		ZOffset
	);
}

void UStreetMapComponent::ColorRoadMeshFromData(TArray<FStreetMapVertex>& Vertices, FLinearColor DefaultColor, FLinearColor LowFlowColor, FLinearColor MedFlowColor, FLinearColor HighFlowColor, bool OverwriteTrace, float ZOffset) {
	ColorRoadMeshFromData(
		Vertices,
		DefaultColor.ToFColor(false),
		LowFlowColor.ToFColor(false),
		MedFlowColor.ToFColor(false),
		HighFlowColor.ToFColor(false),
		OverwriteTrace,
		ZOffset
	);
}

void UStreetMapComponent::ColorRoadMeshFromData(TArray<FStreetMapLink> Links, FColor DefaultColor, FColor LowFlowColor, FColor MedFlowColor, FColor HighFlowColor, bool OverwriteTrace, float ZOffset) {
	auto& Roads = StreetMap->GetRoads();

	for (auto& Link : Links) {
		if (!mLink2RoadIndex.Contains(Link)) return;

		int RoadIndex = mLink2RoadIndex[Link];
		auto Road = Roads[RoadIndex];
		TMap<FStreetMapLink, TArray<int>>* LinkMap;
		TArray<FStreetMapVertex>* Vertices;

		switch (Road.RoadType) {
		case EStreetMapRoadType::Highway:
			LinkMap = &mHighwayLink2Vertices;
			Vertices = &HighwayVertices;
			break;
		case EStreetMapRoadType::MajorRoad:
			LinkMap = &mMajorLink2Vertices;
			Vertices = &MajorRoadVertices;
			break;
		default:
			LinkMap = &mStreetLink2Vertices;
			Vertices = &StreetVertices;
			break;
		}

		for (int VertexIndex : (*LinkMap)[Link]) {
			const FName TMC = (*Vertices)[VertexIndex].TMC;
			FColor RoadColor;
			float Speed, SpeedRatio;
			bool bUseDefaultColor = !GetSpeedAndColorFromData(TMC, (*Vertices)[VertexIndex].SpeedLimit, Speed, SpeedRatio, RoadColor);
			if (bUseDefaultColor) {
				RoadColor = DefaultColor;
			}

			(*Vertices)[VertexIndex].Color = RoadColor;

			auto Direction = (*Vertices)[VertexIndex].TextureCoordinate4.Y;
			(*Vertices)[VertexIndex].TextureCoordinate4 = FVector2D(SpeedRatio * 100, Direction);

			(*Vertices)[VertexIndex].TextureCoordinate4 = FVector2D(SpeedRatio * 100, 0.0f);
			if (ZOffset != 0.0f) {
				(*Vertices)[VertexIndex].Position.Z = ZOffset;
			}

			if (SpeedRatio > HighSpeedRatio) {
				(*Vertices)[VertexIndex].Position.Z += 0.0;
			}
			else if (SpeedRatio > MedSpeedRatio) {
				(*Vertices)[VertexIndex].Position.Z += 0.001;
			}
			else {
				(*Vertices)[VertexIndex].Position.Z += 0.002;
			}
		}
	}

	// Mark our render state dirty so that CreateSceneProxy can refresh it on demand
	MarkRenderStateDirty();

	//Modify();
}

void UStreetMapComponent::ColorRoadMeshFromData(TArray<FStreetMapLink> Links, FLinearColor DefaultColor, bool OverwriteTrace, float ZOffset) {
	const FColor LowFlowColor = MeshBuildSettings.LowFlowColor.ToFColor(false);
	const FColor MedFlowColor = MeshBuildSettings.MedFlowColor.ToFColor(false);
	const FColor HighFlowColor = MeshBuildSettings.HighFlowColor.ToFColor(false);

	ColorRoadMeshFromData(
		Links,
		DefaultColor.ToFColor(false),
		LowFlowColor,
		MedFlowColor,
		HighFlowColor,
		OverwriteTrace,
		ZOffset
	);
}

void UStreetMapComponent::ColorRoadMeshFromData(TArray<FStreetMapLink> Links, FLinearColor DefaultColor, FLinearColor LowFlowColor, FLinearColor MedFlowColor, FLinearColor HighFlowColor, bool OverwriteTrace, float ZOffset) {
	ColorRoadMeshFromData(
		Links,
		DefaultColor.ToFColor(false),
		LowFlowColor.ToFColor(false),
		MedFlowColor.ToFColor(false),
		HighFlowColor.ToFColor(false),
		OverwriteTrace,
		ZOffset
	);
}

void UStreetMapComponent::RefreshStreetColors()
{
	//const FColor StreetColor = MeshBuildSettings.StreetColor.ToFColor(false);
	//const FColor MajorRoadColor = MeshBuildSettings.MajorRoadColor.ToFColor(false);
	//const FColor HighwayColor = MeshBuildSettings.HighwayColor.ToFColor(false);
	//const FColor HighFlowColor = MeshBuildSettings.HighFlowColor.ToFColor(false);
	//const EColorMode ColorMode = MeshBuildSettings.ColorMode;

	//switch (ColorMode) {
	//case EColorMode::Flow:
	//case EColorMode::Predictive0:
	//case EColorMode::Predictive15:
	//case EColorMode::Predictive30:
	//case EColorMode::Predictive45:
	//	ColorRoadMeshFromData(HighwayVertices, HighFlowColor);
	//	ColorRoadMeshFromData(MajorRoadVertices, HighFlowColor);
	//	ColorRoadMeshFromData(StreetVertices, HighFlowColor);
	//	break;
	//default:
	//	ColorRoadMesh(HighwayColor, HighwayVertices);
	//	ColorRoadMesh(MajorRoadColor, MajorRoadVertices);
	//	ColorRoadMesh(StreetColor, StreetVertices);
	//}

	//if (HasValidMesh())
	//{
	//	// We have a new bounding box
	//	UpdateBounds();
	//}
	//else
	//{
	//	// No mesh was generated
	//}

	//GenerateCollision();

	//// Mark our render state dirty so that CreateSceneProxy can refresh it on demand
	//MarkRenderStateDirty();

	//AssignDefaultMaterialIfNeeded();

	//Modify();

	const FColor LowFlowColor = MeshBuildSettings.LowFlowColor.ToFColor(false);
	const FColor MedFlowColor = MeshBuildSettings.MedFlowColor.ToFColor(false);
	const FColor HighFlowColor = MeshBuildSettings.HighFlowColor.ToFColor(false);

	BuildRoadMesh(HighFlowColor, MedFlowColor, LowFlowColor);
}

void UStreetMapComponent::OverrideFlowColors(FLinearColor LowFlowColor, FLinearColor MedFlowColor, FLinearColor HighFlowColor)
{
	//const FColor StreetColor = MeshBuildSettings.StreetColor.ToFColor(false);
	//const FColor MajorRoadColor = MeshBuildSettings.MajorRoadColor.ToFColor(false);
	//const FColor HighwayColor = MeshBuildSettings.HighwayColor.ToFColor(false);
	//const EColorMode ColorMode = MeshBuildSettings.ColorMode;

	//switch (ColorMode) {
	//case EColorMode::Flow:
	//case EColorMode::Predictive0:
	//case EColorMode::Predictive15:
	//case EColorMode::Predictive30:
	//case EColorMode::Predictive45:
	//	ColorRoadMeshFromData(HighwayVertices, HighFlowColor, LowFlowColor, MedFlowColor, HighFlowColor);
	//	ColorRoadMeshFromData(MajorRoadVertices, HighFlowColor, LowFlowColor, MedFlowColor, HighFlowColor);
	//	ColorRoadMeshFromData(StreetVertices, HighFlowColor, LowFlowColor, MedFlowColor, HighFlowColor);
	//	break;
	//}

	//GenerateCollision();

	//// Mark our render state dirty so that CreateSceneProxy can refresh it on demand
	//MarkRenderStateDirty();

	//AssignDefaultMaterialIfNeeded();

	//Modify();

	BuildRoadMesh(HighFlowColor.ToFColor(false), MedFlowColor.ToFColor(false), LowFlowColor.ToFColor(false));
}

TArray<int64> UStreetMapComponent::CalculateRouteNodes(int64 start, int64 target)
{
	return ComputeRouteNodes(start, target);
}

TArray<int64> UStreetMapComponent::ComputeRouteNodes(int64 start, int64 target)
{
	TArray<int64> openList;
	TArray<int64> closedList;

	int64 startNode = start;
	int64 targetNode = target;

	TMap<int64, float> g;
	TMap<int64, float> f;

	TMap<int64, int64> pred;

	auto Roads = StreetMap->GetRoads();
	auto Nodes = StreetMap->GetNodes();

	auto getNeighbours = [&](int64 index) -> TArray<int64>
	{
		TArray<int64> neighbours;
		if (index < 0 || index >= Nodes.Num())
			return neighbours;

		// possible optimization
		// only use start/end indices as neighbours (but have to check if target node lies within this road)
		for (const auto& ref : Nodes[index].RoadRefs)
		{
			for (auto& idx : Roads[ref.RoadIndex].NodeIndices)
			{
				neighbours.Push(idx);
			}
		}

		return neighbours;
	};

	auto distance = [&](int64 node1, int64 node2) -> float
	{
		auto dist = Nodes[node1].Location - Nodes[node2].Location;
		return dist.Size();
	};

	auto heuristic = [&](int64 node) -> float
	{
		if (node == targetNode)
		{
			return 0.0f;
		}

		return distance(node, targetNode);
	};

	auto expandNode = [&](int64 node)
	{
		TArray<int64> neighbours = getNeighbours(node);
		for (auto successor : neighbours)
		{
			if (successor < 0)
			{
				continue;
			}
			if (closedList.Contains(successor))
			{
				continue;
			}

			float fDistance = distance(node, successor);
			if (successor == targetNode)
			{
				fDistance = 0.0f;
			}

			float tentative_g = g[node] + fDistance;

			if (!openList.Contains(successor))
			{
				openList.Push(successor);
				g.Emplace(successor, FLT_MAX);
				f.Emplace(successor, 0.0f);
			}

			if (tentative_g >= g[successor])
			{
				continue;
			}

			pred.Emplace(successor, node);
			g.Emplace(successor, tentative_g);
			f.Emplace(successor, tentative_g + heuristic(successor));
		}
	};

	auto removeMinOpen = [&]() -> int
	{
		int64 min = openList[0];
		float f_ = f[min];
		for (auto n : openList)
		{
			if (f[n] < f_)
			{
				f_ = f[n];
				min = n;
			}
		}
		openList.Remove(min);
		return min;
	};

	auto reconstructPath = [&]() -> TArray<int64>
	{
		TArray<int64> path;
		int64 current = targetNode;
		while (current != startNode)
		{
			path.Add(current);
			current = pred[current];
		}

		return path;
	};

	bool found = false;
	int64 nodesVisisted = 0;
	g.Emplace(startNode, 0);
	f.Emplace(startNode, heuristic(startNode));
	openList.Add(startNode);
	while (openList.Num() > 0)
	{
		int64 currentNode = removeMinOpen();

		if (currentNode == targetNode)
		{
			found = true;
			break;
		}

		expandNode(currentNode);
		++nodesVisisted;

		closedList.Push(currentNode);
	}

	if (found)
	{
		// reconstruct path
		return reconstructPath();
	}
	else
	{
		return TArray<int64>();
	}
}

TArray<FStreetMapLink>
UStreetMapComponent::CalculateRoute(int64 start, int64 target, bool keepRoadType)
{
	return ComputeRoute(start, target, keepRoadType);
}

TArray<FStreetMapLink>
UStreetMapComponent::ComputeRoute(int64 start, int64 target, bool keepRoadType)
{
	auto Roads = StreetMap->GetRoads();
	auto Nodes = StreetMap->GetNodes();

	TArray<int32> openList;
	TArray<int32> closedList;

	auto startRoad = Roads.IndexOfByPredicate([&](const FStreetMapRoad& r)
	{
		return r.Link.LinkId == start;
	});

	auto targetRoad = Roads.IndexOfByPredicate([&](const FStreetMapRoad& r)
	{
		return r.Link.LinkId == target;
	});


	TMap<int32, float> g;
	TMap<int32, float> f;

	TMap<int32, int32> pred;

	auto targetMidIdx = Roads[targetRoad].RoadPoints.Num() >> 1;
	FVector2D targetMid = Roads[targetRoad].RoadPoints[targetMidIdx];

	auto getNeighbours = [&](int32 index) -> TArray<int32>
	{
		TArray<int32> neighbours;
		if (index < 0 || index >= Roads.Num())
			return neighbours;

		auto roadStart = Roads[index].NodeIndices[0];
		auto roadEnd = Roads[index].NodeIndices.Last();

		for (auto& road : Nodes[roadStart].RoadRefs)
		{
			if (road.RoadIndex != index
				&& (!keepRoadType || (Roads[road.RoadIndex].RoadType == Roads[index].RoadType) ))
			{
				neighbours.Push(road.RoadIndex);
			}
		}

		for (auto& road : Nodes[roadEnd].RoadRefs)
		{
			if (road.RoadIndex != index
				&& (!keepRoadType || (Roads[road.RoadIndex].RoadType == Roads[index].RoadType)))
			{
				neighbours.Push(road.RoadIndex);
			}
		}

		return neighbours;
	};

	auto distance = [&](int32 road1, int32 road2) -> float
	{
		auto mid1 = Roads[road1].RoadPoints.Num() >> 1;
		auto mid2 = Roads[road2].RoadPoints.Num() >> 1;

		auto dist = Roads[road2].RoadPoints[mid2] - Roads[road1].RoadPoints[mid1];

		return dist.Size();
	};

	auto heuristic = [&](int32 road) -> float
	{
		if (road == targetRoad)
		{
			return 0.0f;
		}

		// Possible improvements
		// - Take the direction of the road (LastPoint - First Point) and the sine
		//   between it and the vector (target - mid) to scale the heuristic
		//   -> Does the street point in the cirection of the target
		// - Flat scale for slower roads

		auto mid = Roads[road].RoadPoints.Num() >> 1;
		auto dist = targetMid - Roads[road].RoadPoints[mid];

		return dist.Size();
	};

	auto expandNode = [&](int32 road)
	{
		TArray<int32> neighbours = getNeighbours(road);
		for (auto successor : neighbours)
		{
			if (successor < 0)
			{
				continue;
			}
			if (closedList.Contains(successor))
			{
				continue;
			}

			float fDistance = distance(road, successor);
			if (successor == targetRoad)
			{
				fDistance = 0.0f;
			}

			float tentative_g = g[road] + fDistance;

			if (!openList.Contains(successor))
			{
				openList.Push(successor);
				g.Emplace(successor, FLT_MAX);
				f.Emplace(successor, 0.0f);
			}

			if (tentative_g >= g[successor])
			{
				continue;
			}

			pred.Emplace(successor, road);
			g.Emplace(successor, tentative_g);
			f.Emplace(successor, tentative_g + heuristic(successor));
		}
	};

	auto removeMinOpen = [&]() -> int
	{
		int32 min = openList[0];
		float f_ = f[min];
		for (auto n : openList)
		{
			if (f[n] < f_)
			{
				f_ = f[n];
				min = n;
			}
		}
		openList.Remove(min);
		return min;
	};

	auto reconstructPath = [&]() -> TArray<FStreetMapLink>
	{
		TArray<FStreetMapLink> path;
		int32 current = targetRoad;
		while (current != startRoad)
		{
			path.Add({ Roads[current].Link.LinkId, Roads[current].Link.LinkDir });
			current = pred[current];
		}

		return path;
	};

	bool found = false;
	int32 nodesVisisted = 0;
	g.Emplace(startRoad, 0);
	f.Emplace(startRoad, heuristic(startRoad));
	openList.Add(startRoad);
	while (openList.Num() > 0)
	{
		int32 currentNode = removeMinOpen();

		if (currentNode == targetRoad)
		{
			found = true;
			break;
		}

		expandNode(currentNode);
		++nodesVisisted;

		closedList.Push(currentNode);
	}

	if (found)
	{
		// reconstruct path
		return reconstructPath();
	}
	else
	{
		return TArray<FStreetMapLink>();
	}
}

void UStreetMapComponent::ChangeStreetThickness(float val, EStreetMapRoadType type)
{
	const FColor LowFlowColor = MeshBuildSettings.LowFlowColor.ToFColor(false);
	const FColor MedFlowColor = MeshBuildSettings.MedFlowColor.ToFColor(false);
	const FColor HighFlowColor = MeshBuildSettings.HighFlowColor.ToFColor(false);

	switch (type)
	{
	case EStreetMapRoadType::Highway:
		MeshBuildSettings.HighwayThickness = val;
		break;
	case EStreetMapRoadType::MajorRoad:
		MeshBuildSettings.MajorRoadThickness = val;
		break;
	case EStreetMapRoadType::Street:
		MeshBuildSettings.StreetThickness = val;
		break;
	default:
		break;
	}

	BuildRoadMesh(type, HighFlowColor, MedFlowColor, LowFlowColor);
}

void UStreetMapComponent::ChangeStreetColor(FLinearColor val, EStreetMapRoadType type)
{
	switch (type)
	{
	case EStreetMapRoadType::Highway:
		MeshBuildSettings.HighwayColor = val;
		ColorRoadMesh(val, HighwayVertices);
		break;
	case EStreetMapRoadType::MajorRoad:
		MeshBuildSettings.MajorRoadColor = val;
		ColorRoadMesh(val, MajorRoadVertices);
		break;
	case EStreetMapRoadType::Street:
		MeshBuildSettings.StreetColor = val;
		ColorRoadMesh(val, StreetVertices);
		break;
	default:
		break;
	}
}

void UStreetMapComponent::ChangeStreetColorByLink(FLinearColor val, FStreetMapLink Link)
{
	ColorRoadMesh(val, Link);
}

void UStreetMapComponent::ChangeStreetColorByLinks(FLinearColor val, TArray<FStreetMapLink> Links)
{
	ColorRoadMesh(val, Links);
}

void UStreetMapComponent::ChangeStreetColorByTMC(FLinearColor val, EStreetMapRoadType type, FName TMC)
{
	switch (type)
	{
	case EStreetMapRoadType::Highway:
		ColorRoadMesh(val, TMC);
		break;
	case EStreetMapRoadType::MajorRoad:
		ColorRoadMesh(val, TMC);
		break;
	case EStreetMapRoadType::Street:
		ColorRoadMesh(val, TMC);
		break;

	default:
		break;
	}
}

void UStreetMapComponent::ChangeStreetColorByTMCs(FLinearColor val, EStreetMapRoadType type, TArray<FName> TMCs)
{
	ColorRoadMesh(val, TMCs);
}

#if WITH_EDITOR
void UStreetMapComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	bool bNeedRefreshCustomizationModule = false;

	// Check to see if the "StreetMap" property changed.
	if (PropertyChangedEvent.Property != nullptr)
	{
		const FName PropertyName(PropertyChangedEvent.Property->GetFName());
		if (PropertyName == GET_MEMBER_NAME_CHECKED(UStreetMapComponent, StreetMap))
		{
			bNeedRefreshCustomizationModule = true;
		}
		else if (IsCollisionProperty(PropertyName)) // For some unknown reason , GET_MEMBER_NAME_CHECKED(UStreetMapComponent, CollisionSettings) is not working ??? "TO CHECK LATER"
		{
			if (CollisionSettings.bGenerateCollision == true)
			{
				GenerateCollision();
			}
			else
			{
				ClearCollision();
			}
			bNeedRefreshCustomizationModule = true;
		}
	}

	if (bNeedRefreshCustomizationModule)
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.NotifyCustomizationModuleChanged();
	}

	// Call the parent implementation of this function
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif	// WITH_EDITOR

void UStreetMapComponent::BuildMesh()
{
	// Wipes out our cached mesh data. Maybe unnecessary in case GenerateMesh is clearing cached mesh data and creating a new SceneProxy  !
	InvalidateMesh();

	GenerateMesh();

	if (HasValidMesh())
	{
		// We have a new bounding box
		UpdateBounds();
	}
	else
	{
		// No mesh was generated
	}

	GenerateCollision();

	// Mark our render state dirty so that CreateSceneProxy can refresh it on demand
	MarkRenderStateDirty();

	AssignDefaultMaterialIfNeeded();

	Modify();
}

void UStreetMapComponent::AssignDefaultMaterialIfNeeded()
{
	if (this->GetNumMaterials() == 0 || this->GetMaterial(0) == nullptr)
	{
		if (!HasValidMesh() || GetDefaultMaterial() == nullptr)
			return;

		this->SetMaterial(0, GetDefaultMaterial());
	}
}

void UStreetMapComponent::UpdateNavigationIfNeeded()
{
	if (bCanEverAffectNavigation || bNavigationRelevant)
	{
		FNavigationSystem::UpdateComponentData(*this);
	}
}

void UStreetMapComponent::InvalidateMesh()
{
	BuildingVertices.Reset();
	BuildingIndices.Reset();
	StreetVertices.Reset();
	StreetIndices.Reset();
	MajorRoadVertices.Reset();
	MajorRoadIndices.Reset();
	HighwayVertices.Reset();
	HighwayIndices.Reset();

	CachedLocalBounds = FBoxSphereBounds(FBox(ForceInitToZero));
	ClearCollision();
	// Mark our render state dirty so that CreateSceneProxy can refresh it on demand
	MarkRenderStateDirty();
	Modify();
}

FBoxSphereBounds UStreetMapComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	if (HasValidMesh())
	{
		FBoxSphereBounds WorldSpaceBounds = CachedLocalBounds.TransformBy(LocalToWorld);
		WorldSpaceBounds.BoxExtent *= BoundsScale;
		WorldSpaceBounds.SphereRadius *= BoundsScale;
		return WorldSpaceBounds;
	}
	else
	{
		return FBoxSphereBounds(LocalToWorld.GetLocation(), FVector::ZeroVector, 0.0f);
	}
}

void UStreetMapComponent::AddThick2DLine(
	const FVector2D Start,
	const FVector2D End,
	const float Z,
	const float Thickness,
	const float MaxThickness,
	const FColor& StartColor,
	const FColor& EndColor,
	FBox& MeshBoundingBox,
	TArray<FStreetMapVertex>* Vertices,
	TArray<uint32>* Indices,
	EVertexType VertexType,
	int64 LinkId,
	FString LinkDir,
	FName TMC,
	int SpeedLimit,
	float SpeedRatio,
	float RoadTypeFloat
)
{
	const float HalfThickness = Thickness * 0.5f;

	const float Distance = (End - Start).Size();
	const float XRatio = Distance / Thickness;
	const FVector2D LineDirection = (End - Start).GetSafeNormal();
	const FVector2D RightVector(-LineDirection.Y, LineDirection.X);
	const bool IsForward = LinkDir.Compare(TEXT("T"), ESearchCase::IgnoreCase) == 0;

	const int32 BottomLeftVertexIndex = Vertices->Num();
	FStreetMapVertex& BottomLeftVertex = *new(*Vertices)FStreetMapVertex();
	BottomLeftVertex.LinkId = LinkId;
	BottomLeftVertex.LinkDir = LinkDir;
	BottomLeftVertex.TMC = TMC;
	BottomLeftVertex.SpeedLimit = SpeedLimit;
	switch (VertexType)
	{
	case EVertexType::VStreet:
	case EVertexType::VMajorRoad:
	case EVertexType::VHighway:
		if (IsForward)
		{
			BottomLeftVertex.Position = FVector(Start, Z);
			BottomLeftVertex.TextureCoordinate = FVector2D(0.0f, 0.0f);
		}
		else
		{
			BottomLeftVertex.Position = FVector(Start - RightVector * HalfThickness, Z);
			BottomLeftVertex.TextureCoordinate = FVector2D(0.0f, XRatio);
		}
		break;
	default:
		BottomLeftVertex.Position = FVector(Start - RightVector * HalfThickness, Z);
		BottomLeftVertex.TextureCoordinate = FVector2D(0.0f, 0.0f);
		break;
	}
	BottomLeftVertex.TextureCoordinate2 = FVector2D(-RightVector.X, -RightVector.Y);
	BottomLeftVertex.TextureCoordinate3 = FVector2D(HalfThickness, MaxThickness);
	BottomLeftVertex.TextureCoordinate4 = FVector2D(SpeedRatio * 100, 0);
	BottomLeftVertex.TextureCoordinate5 = FVector2D(RoadTypeFloat, 0);
	BottomLeftVertex.TangentX = FVector(LineDirection, 0.0f);
	BottomLeftVertex.TangentZ = FVector::UpVector;
	BottomLeftVertex.Color = StartColor;
	MeshBoundingBox += BottomLeftVertex.Position;

	const int32 BottomRightVertexIndex = Vertices->Num();
	FStreetMapVertex& BottomRightVertex = *new(*Vertices)FStreetMapVertex();
	BottomRightVertex.LinkId = LinkId;
	BottomRightVertex.LinkDir = LinkDir;
	BottomRightVertex.TMC = TMC;
	BottomRightVertex.SpeedLimit = SpeedLimit;
	switch (VertexType)
	{
	case EVertexType::VStreet:
	case EVertexType::VMajorRoad:
	case EVertexType::VHighway:
		if (IsForward)
		{
			BottomRightVertex.Position = FVector(Start + RightVector * HalfThickness, Z);
			BottomRightVertex.TextureCoordinate = FVector2D(0.5f, 0.0f);
		}
		else
		{
			BottomRightVertex.Position = FVector(Start, Z);
			BottomRightVertex.TextureCoordinate = FVector2D(0.5f, XRatio);
		}
		break;
	default:
		BottomRightVertex.Position = FVector(Start + RightVector * HalfThickness, Z);
		BottomRightVertex.TextureCoordinate = FVector2D(1.0f, 0.0f);
		break;
	}
	BottomRightVertex.TextureCoordinate2 = FVector2D(RightVector.X, RightVector.Y);
	BottomRightVertex.TextureCoordinate3 = FVector2D(HalfThickness, MaxThickness);
	BottomRightVertex.TextureCoordinate4 = FVector2D(SpeedRatio * 100, 0);
	BottomRightVertex.TextureCoordinate5 = FVector2D(RoadTypeFloat, 0);
	BottomRightVertex.TangentX = FVector(LineDirection, 0.0f);
	BottomRightVertex.TangentZ = FVector::UpVector;
	BottomRightVertex.Color = StartColor;
	MeshBoundingBox += BottomRightVertex.Position;

	const int32 TopRightVertexIndex = Vertices->Num();
	FStreetMapVertex& TopRightVertex = *new(*Vertices)FStreetMapVertex();
	TopRightVertex.LinkId = LinkId;
	TopRightVertex.LinkDir = LinkDir;
	TopRightVertex.TMC = TMC;
	TopRightVertex.SpeedLimit = SpeedLimit;
	switch (VertexType)
	{
	case EVertexType::VStreet:
	case EVertexType::VMajorRoad:
	case EVertexType::VHighway:
		if (IsForward)
		{
			TopRightVertex.Position = FVector(End + RightVector * HalfThickness, Z);
			TopRightVertex.TextureCoordinate = FVector2D(0.5f, XRatio);
		}
		else
		{
			TopRightVertex.Position = FVector(End, Z);
			TopRightVertex.TextureCoordinate = FVector2D(0.5f, 0.0f);
		}
		break;
	default:
		TopRightVertex.Position = FVector(End + RightVector * HalfThickness, Z);
		TopRightVertex.TextureCoordinate = FVector2D(1.0f, XRatio);
		break;
	}
	TopRightVertex.TextureCoordinate2 = FVector2D(RightVector.X, RightVector.Y);
	TopRightVertex.TextureCoordinate3 = FVector2D(HalfThickness, MaxThickness);
	TopRightVertex.TextureCoordinate4 = FVector2D(SpeedRatio * 100, 0);
	TopRightVertex.TextureCoordinate5 = FVector2D(RoadTypeFloat, 0);
	TopRightVertex.TangentX = FVector(LineDirection, 0.0f);
	TopRightVertex.TangentZ = FVector::UpVector;
	TopRightVertex.Color = EndColor;
	MeshBoundingBox += TopRightVertex.Position;

	const int32 TopLeftVertexIndex = Vertices->Num();
	FStreetMapVertex& TopLeftVertex = *new(*Vertices)FStreetMapVertex();
	TopLeftVertex.LinkId = LinkId;
	TopLeftVertex.LinkDir = LinkDir;
	TopLeftVertex.TMC = TMC;
	TopLeftVertex.SpeedLimit = SpeedLimit;
	switch (VertexType)
	{
	case EVertexType::VStreet:
	case EVertexType::VMajorRoad:
	case EVertexType::VHighway:
		if (IsForward)
		{
			TopLeftVertex.Position = FVector(End, Z);
			TopLeftVertex.TextureCoordinate = FVector2D(0.0f, XRatio);
		}
		else
		{
			TopLeftVertex.Position = FVector(End - RightVector * HalfThickness, Z);
			TopLeftVertex.TextureCoordinate = FVector2D(0.0f, 0.0f);
		}
		break;
	default:
		TopLeftVertex.Position = FVector(End - RightVector * HalfThickness, Z);
		TopLeftVertex.TextureCoordinate = FVector2D(0.0f, XRatio);
		break;
	}
	TopLeftVertex.TextureCoordinate2 = FVector2D(-RightVector.X, -RightVector.Y);
	TopLeftVertex.TextureCoordinate3 = FVector2D(HalfThickness, MaxThickness);
	TopLeftVertex.TextureCoordinate4 = FVector2D(SpeedRatio * 100, 0);
	TopLeftVertex.TextureCoordinate5 = FVector2D(RoadTypeFloat, 0);
	TopLeftVertex.TangentX = FVector(LineDirection, 0.0f);
	TopLeftVertex.TangentZ = FVector::UpVector;
	TopLeftVertex.Color = EndColor;
	MeshBoundingBox += TopLeftVertex.Position;

	Indices->Add(BottomLeftVertexIndex);
	Indices->Add(BottomRightVertexIndex);
	Indices->Add(TopRightVertexIndex);

	Indices->Add(BottomLeftVertexIndex);
	Indices->Add(TopRightVertexIndex);
	Indices->Add(TopLeftVertexIndex);
};


void UStreetMapComponent::AddTriangles(const TArray<FVector>& Points, const TArray<int32>& PointIndices, const FVector& ForwardVector, const FVector& UpVector, const FColor& Color, FBox& MeshBoundingBox, TArray<FStreetMapVertex>& Vertices, TArray<uint32>& Indices)
{
	const int32 FirstVertexIndex = Vertices.Num();

	for (FVector Point : Points)
	{
		FStreetMapVertex& NewVertex = *new(Vertices)FStreetMapVertex();
		NewVertex.Position = Point;
		NewVertex.TextureCoordinate = FVector2D(0.0f, 0.0f);	// NOTE: We're not using texture coordinates for anything yet
		NewVertex.TextureCoordinate2 = FVector2D(0.0f, 0.0f);
		NewVertex.TextureCoordinate3 = FVector2D(0.0f, 1.0f); // Thicknesses
		NewVertex.TextureCoordinate4 = FVector2D(0.0f, 0.0f);
		NewVertex.TextureCoordinate5 = FVector2D(0.0f, 0.0f);
		NewVertex.TangentX = ForwardVector;
		NewVertex.TangentZ = UpVector;
		NewVertex.Color = Color;

		MeshBoundingBox += NewVertex.Position;
	}

	for (int32 PointIndex : PointIndices)
	{
		Indices.Add(FirstVertexIndex + PointIndex);
	}
};

void UStreetMapComponent::findConnectedRoad(
	const FStreetMapRoad& Road
	, int32 RoadCheckIndex
	, const bool Start
	, FString LinkDir
	, int32& ChosenRoadIndex
	, bool& fromBack)
{
	const auto& Roads = StreetMap->GetRoads();
	const auto& Nodes = StreetMap->GetNodes();

	auto& Node = Nodes[Road.NodeIndices[RoadCheckIndex]];
	auto RoadIndex = Road.GetRoadIndex(*StreetMap);
	int RefCount = Node.RoadRefs.Num();

	int actualRefCount = 0;
	// check the number of roads we can connect to
	for (auto& OtherRoadNode : Node.RoadRefs)
	{
		if (OtherRoadNode.RoadIndex != RoadIndex)
		{
			auto& OtherRoad = Roads[OtherRoadNode.RoadIndex];
			if (OtherRoad.RoadType == Road.RoadType && OtherRoadNode.RoadIndex != RoadIndex)
			{
				++actualRefCount;
			}
		}
	}

	if (RefCount > 1)
	{
		auto RoadInNodeIndex = Node.RoadRefs.IndexOfByKey(RoadIndex);
		float CosAlpha = MeshBuildSettings.fThresholdConnectStreets;

		for (auto& OtherRoadNode : Node.RoadRefs)
		{
			if (OtherRoadNode.RoadIndex != RoadIndex)
			{
				bool forward = LinkDir.Compare(TEXT("T"), ESearchCase::IgnoreCase) == 0;
				bool backward = LinkDir.Compare(TEXT("F"), ESearchCase::IgnoreCase) == 0;

				auto& OtherRoad = Roads[OtherRoadNode.RoadIndex];

				bool forwardOther = OtherRoad.Link.LinkDir.Compare(TEXT("T"), ESearchCase::IgnoreCase) == 0;
				bool backwardOther = OtherRoad.Link.LinkDir.Compare(TEXT("F"), ESearchCase::IgnoreCase) == 0;

				if (OtherRoad.RoadType != Road.RoadType
					|| Road.NodeIndices[RoadCheckIndex] == INDEX_NONE
					|| (forward != forwardOther || backward != backwardOther)
					)
				{
					continue;
				}

				if (INDEX_NONE != OtherRoad.NodeIndices[0] && OtherRoad.NodeIndices[0] == Road.NodeIndices[RoadCheckIndex])
				{
					fromBack = false;
				}
				else if (INDEX_NONE != OtherRoad.NodeIndices.Last() && OtherRoad.NodeIndices.Last() == Road.NodeIndices[RoadCheckIndex])
				{
					fromBack = true;
				}
				else
				{
					continue;
				}

				{
					// check angle between the 2 road segments
					const FVector2D* Prev = nullptr, *Mid = nullptr, *Next = nullptr;
					if (Start)
					{
						Prev = fromBack ? &OtherRoad.RoadPoints.Last(1) : &OtherRoad.RoadPoints[1];
						Mid = &Road.RoadPoints[0];
						Next = &Road.RoadPoints[1];
					}
					else
					{
						Prev = &Road.RoadPoints.Last(1);
						Mid = &Road.RoadPoints.Last();
						Next = fromBack ? &OtherRoad.RoadPoints.Last(1) : &OtherRoad.RoadPoints[1];
					}

					if (Prev && Mid && Next)
					{
						auto direction1 = (*Prev - *Mid).GetSafeNormal();
						auto direction2 = (*Next - *Mid).GetSafeNormal();

						auto cosAlpha = FVector2D::DotProduct(direction1, direction2);

						if (cosAlpha < -CosAlpha || actualRefCount == 1)
						{
							CosAlpha = -cosAlpha;
							ChosenRoadIndex = OtherRoad.GetRoadIndex(*StreetMap);
						}
					}
				}
			}
		}
	}
}

void UStreetMapComponent::CheckRoadSmoothQuadList(
	FStreetMapRoad& Road
	, const bool Start
	, const float Z
	, const float Thickness
	, const float MaxThickness
	, const FColor& StartColor
	, const FColor& EndColor
	, float& VAccumulation
	, FBox& MeshBoundingBox
	, TArray<FStreetMapVertex>* Vertices
	, TArray<uint32>* Indices
	, EVertexType VertexType
	, int64 LinkId
	, FString LinkDir
	, FName TMC
	, int SpeedLimit
	, float SpeedRatio
	, float RoadTypeFloat
)
{
	if (!StreetMap)
	{
		return;
	}
	auto& Roads = StreetMap->GetRoads();
	const auto& Nodes = StreetMap->GetNodes();

	int32 RoadCheckIndex = 0;
	if (!Start)
	{
		RoadCheckIndex = Road.NodeIndices.Num() - 1;
	}


	// PARAMS
	int32 ChosenRoadIndex = -1;
	bool fromBack = false;
	// PARAMS end


	// START
	if (Road.NodeIndices[RoadCheckIndex] < Nodes.Num())
	{
		findConnectedRoad(Road
			, RoadCheckIndex
			, Start
			, LinkDir
			, ChosenRoadIndex
			, fromBack
		);


	}
	// END


	if (ChosenRoadIndex >= 0)
	{
		auto& OtherRoad = Roads[ChosenRoadIndex];
		if (Start)
		{
			//if (!Road.lengthComputed) // not yet computed
			//{
			//	Road.ComputeUVspan(0.f, Thickness);
			//}
			//if (!OtherRoad.lengthComputed)
			//{
			//	OtherRoad.ComputeUVspanFromBack(Road.textureVStart.X, Thickness);
			//}
			//VAccumulation = Road.textureVStart.X;

			StartSmoothQuadList(fromBack ? OtherRoad.RoadPoints.Last(1) : OtherRoad.RoadPoints[1],
				Road.RoadPoints[0],
				Road.RoadPoints[1],
				Z,
				Thickness,
				MaxThickness,
				StartColor,
				EndColor,
				VAccumulation,
				MeshBoundingBox,
				Vertices,
				Indices,
				VertexType,
				LinkId,
				LinkDir,
				TMC,
				SpeedLimit,
				SpeedRatio,
				RoadTypeFloat
			);
		}
		else
		{
			EndSmoothQuadList(Road.RoadPoints.Last(1),
				Road.RoadPoints.Last(),
				fromBack ? OtherRoad.RoadPoints.Last(1) : OtherRoad.RoadPoints[1],
				Z,
				Thickness,
				MaxThickness,
				StartColor,
				EndColor,
				VAccumulation,
				MeshBoundingBox,
				Vertices,
				Indices,
				VertexType,
				LinkId,
				LinkDir,
				TMC,
				SpeedLimit,
				SpeedRatio,
				RoadTypeFloat
			);
		}
		return;
	}

	// fall through to default
	if (Start)
	{
		//if (!Road.lengthComputed) // not yet computed
		//{
		//	Road.ComputeUVspan(0.f, Thickness);
		//}
		//VAccumulation = Road.textureVStart.X;

		StartSmoothQuadList(Road.RoadPoints[0],
			Road.RoadPoints[1],
			Z,
			Thickness,
			MaxThickness,
			StartColor,
			EndColor,
			VAccumulation,
			MeshBoundingBox,
			Vertices,
			Indices,
			VertexType,
			LinkId,
			LinkDir,
			TMC,
			SpeedLimit,
			SpeedRatio,
			RoadTypeFloat
		);
	}
	else
	{
		EndSmoothQuadList(Road.RoadPoints.Last(1),
			Road.RoadPoints.Last(),
			Z,
			Thickness,
			MaxThickness,
			StartColor,
			EndColor,
			VAccumulation,
			MeshBoundingBox,
			Vertices,
			Indices,
			VertexType,
			LinkId,
			LinkDir,
			TMC,
			SpeedLimit,
			SpeedRatio,
			RoadTypeFloat
		);
	}
}

void startSmoothVertices(const FVector2D Start
	, const FVector2D RightVector
	, const FVector2D Tangent
	, const float Z
	, const float HalfThickness
	, const float MaxThickness
	, const float XRatio
	, const FColor& StartColor
	, const FColor& EndColor
	, FStreetMapMeshBuildSettings MeshBuildSettings
	, FBox& MeshBoundingBox
	, TArray<FStreetMapVertex>* Vertices
	, TArray<uint32>* Indices
	, EVertexType VertexType
	, int64 LinkId
	, FString LinkDir
	, FName TMC
	, int SpeedLimit
	, float SpeedRatio
	, float RoadTypeFloat
)
{
	const float QuarterThickness = HalfThickness * .5f;
	const bool IsForward = LinkDir.Compare(TEXT("T"), ESearchCase::IgnoreCase) == 0;
	const bool IsBackward = LinkDir.Compare(TEXT("F"), ESearchCase::IgnoreCase) == 0;

	const int32 BottomLeftVertexIndex = Vertices->Num();
	FStreetMapVertex& BottomLeftVertex = *new(*Vertices)FStreetMapVertex();
	BottomLeftVertex.LinkId = LinkId;
	BottomLeftVertex.LinkDir = LinkDir;
	BottomLeftVertex.TMC = TMC;
	BottomLeftVertex.SpeedLimit = SpeedLimit;
	switch (VertexType)
	{
	case EVertexType::VStreet:
	case EVertexType::VMajorRoad:
	case EVertexType::VHighway:
		if (IsForward)
		{
			BottomLeftVertex.TextureCoordinate = FVector2D(0.0f, XRatio);
			BottomLeftVertex.TextureCoordinate4 = FVector2D(SpeedRatio * 100, -1.f);
			break;
		}
		else if (IsBackward)
		{
			BottomLeftVertex.TextureCoordinate = FVector2D(0.0f, -XRatio);
			BottomLeftVertex.TextureCoordinate4 = FVector2D(SpeedRatio * 100, 1.f);
			break;
		}
		else
		{
			// fall through if neither
		}
	default:
		BottomLeftVertex.TextureCoordinate = FVector2D(0.0f, XRatio);
		BottomLeftVertex.TextureCoordinate4 = FVector2D(SpeedRatio * 100, 0);
		break;
	}

	BottomLeftVertex.Position = FVector(Start, Z);
	BottomLeftVertex.TextureCoordinate2 = FVector2D(-RightVector.X, -RightVector.Y);
	BottomLeftVertex.TextureCoordinate3 = FVector2D(HalfThickness, MaxThickness);
	BottomLeftVertex.TextureCoordinate5 = FVector2D(RoadTypeFloat, 0.f);
	BottomLeftVertex.TangentX = FVector(Tangent, 0.0f);
	BottomLeftVertex.TangentZ = FVector::UpVector;
	BottomLeftVertex.Color = StartColor;
	// BottomLeftVertex.Color = FColor(0, 0, 255);
	MeshBoundingBox += BottomLeftVertex.Position;

	const int32 BottomRightVertexIndex = Vertices->Num();
	FStreetMapVertex& BottomRightVertex = *new(*Vertices)FStreetMapVertex();
	BottomRightVertex.LinkId = LinkId;
	BottomRightVertex.LinkDir = LinkDir;
	BottomRightVertex.TMC = TMC;
	BottomRightVertex.SpeedLimit = SpeedLimit;
	switch (VertexType)
	{
	case EVertexType::VStreet:
	case EVertexType::VMajorRoad:
	case EVertexType::VHighway:
		if (IsForward)
		{
			BottomRightVertex.TextureCoordinate = FVector2D(1.0f, XRatio);
			BottomRightVertex.TextureCoordinate4 = FVector2D(SpeedRatio * 100, 1.f);
			break;
		}
		else if (IsBackward)
		{
			BottomRightVertex.TextureCoordinate = FVector2D(1.0f, -XRatio);
			BottomRightVertex.TextureCoordinate4 = FVector2D(SpeedRatio * 100, -1.f);
			break;
		}
		else
		{
			// fall through if neither
		}
	default:
		BottomRightVertex.TextureCoordinate = FVector2D(1.0f, XRatio);
		BottomRightVertex.TextureCoordinate4 = FVector2D(SpeedRatio * 100, 0.f);
		break;
	}
	BottomRightVertex.Position = FVector(Start, Z);
	BottomRightVertex.TextureCoordinate2 = FVector2D(RightVector.X, RightVector.Y);
	BottomRightVertex.TextureCoordinate3 = FVector2D(HalfThickness, MaxThickness);
	BottomRightVertex.TextureCoordinate5 = FVector2D(RoadTypeFloat, 0.f);
	BottomRightVertex.TangentX = FVector(Tangent, 0.0f);
	BottomRightVertex.TangentZ = FVector::UpVector;
	BottomRightVertex.Color = StartColor;
	// BottomRightVertex.Color = FColor(0, 0, 255);
	MeshBoundingBox += BottomRightVertex.Position;

	Indices->Add(BottomLeftVertexIndex);
	Indices->Add(BottomRightVertexIndex);
}

void UStreetMapComponent::StartSmoothQuadList(const FVector2D& Prev
	, const FVector2D Start
	, const FVector2D& Mid
	, const float Z
	, const float Thickness
	, const float MaxThickness
	, const FColor& StartColor
	, const FColor& EndColor
	, float& VAccumulation
	, FBox& MeshBoundingBox
	, TArray<FStreetMapVertex>* Vertices
	, TArray<uint32>* Indices
	, EVertexType VertexType
	, int64 LinkId
	, FString LinkDir
	, FName TMC
	, int SpeedLimit
	, float SpeedRatio
	, float RoadTypeFloat
)
{
	const float HalfThickness = Thickness * 0.5f;
	const float Distance = (Mid - Start).Size();
	const float XRatio = VAccumulation;
	// VAccumulation = XRatio;

	const FVector2D LineDirection1 = (Start - Prev).GetSafeNormal();
	const FVector2D LineDirection2 = (Mid - Start).GetSafeNormal();

	auto alteredLineDirection = LineDirection1 + LineDirection2;
	alteredLineDirection.Normalize();

	const FVector2D RightVector(-alteredLineDirection.Y, alteredLineDirection.X);

	startSmoothVertices(Start
		, RightVector
		, alteredLineDirection
		, Z
		, HalfThickness
		, MaxThickness
		, XRatio
		, StartColor
		, EndColor
		, MeshBuildSettings
		, MeshBoundingBox
		, Vertices
		, Indices
		, VertexType
		, LinkId
		, LinkDir
		, TMC
		, SpeedLimit
		, SpeedRatio
		, RoadTypeFloat
	);
}

void UStreetMapComponent::StartSmoothQuadList(const FVector2D& Start
	, const FVector2D& Mid
	, const float Z
	, const float Thickness
	, const float MaxThickness
	, const FColor& StartColor
	, const FColor& EndColor
	, float& VAccumulation
	, FBox& MeshBoundingBox
	, TArray<FStreetMapVertex>* Vertices
	, TArray<uint32>* Indices
	, EVertexType VertexType
	, int64 LinkId
	, FString LinkDir
	, FName TMC
	, int SpeedLimit
	, float SpeedRatio
	, float RoadTypeFloat
)
{
	const float HalfThickness = Thickness * 0.5f;
	const float Distance = (Mid - Start).Size();
	const float XRatio = VAccumulation;

	const FVector2D LineDirection1 = (Mid - Start).GetSafeNormal();
	const FVector2D RightVector(-LineDirection1.Y, LineDirection1.X);

	startSmoothVertices(Start
		, RightVector
		, LineDirection1
		, Z
		, HalfThickness
		, MaxThickness
		, XRatio
		, StartColor
		, EndColor
		, MeshBuildSettings
		, MeshBoundingBox
		, Vertices
		, Indices
		, VertexType
		, LinkId
		, LinkDir
		, TMC
		, SpeedLimit
		, SpeedRatio
		, RoadTypeFloat
	);
}


/** Generate a quad for a road segment */
void UStreetMapComponent::AddSmoothQuad(const FVector2D& Start
	, const FVector2D& Mid
	, const FVector2D& End
	, const float Z
	, const float Thickness
	, const float MaxThickness
	, const FColor& StartColor
	, const FColor& EndColor
	, float& VAccumulation
	, FBox& MeshBoundingBox
	, TArray<FStreetMapVertex>* Vertices
	, TArray<uint32>* Indices
	, EVertexType VertexType
	, int64 LinkId
	, FString LinkDir
	, FName TMC
	, int SpeedLimit
	, float SpeedRatio
	, float RoadTypeFloat
)
{
	const float HalfThickness = Thickness * 0.5f;
	const float QuarterThickness = Thickness * 0.25f;
	const float Distance = (Mid - Start).Size();
	const float XRatio = (Distance / Thickness);
	const bool IsForward = LinkDir.Compare(TEXT("T"), ESearchCase::IgnoreCase) == 0;
	const bool IsBackward = LinkDir.Compare(TEXT("F"), ESearchCase::IgnoreCase) == 0;

	const FVector2D LineDirection1 = (Mid - Start).GetSafeNormal();
	const FVector2D LineDirection2 = (End - Mid).GetSafeNormal();

	auto alteredLineDirection = LineDirection1 + LineDirection2;
	alteredLineDirection.Normalize();
	const FVector2D RightVector(-alteredLineDirection.Y, alteredLineDirection.X);

	const int32 MidLeftVertexIndex = Vertices->Num();
	FStreetMapVertex& MidLeftVertex = *new(*Vertices)FStreetMapVertex();
	MidLeftVertex.LinkId = LinkId;
	MidLeftVertex.LinkDir = LinkDir;
	MidLeftVertex.TMC = TMC;
	MidLeftVertex.SpeedLimit = SpeedLimit;
	switch (VertexType)
	{
	case EVertexType::VStreet:
	case EVertexType::VMajorRoad:
	case EVertexType::VHighway:
		if (IsForward)
		{
			MidLeftVertex.TextureCoordinate = FVector2D(0.0f, VAccumulation + XRatio);
			MidLeftVertex.TextureCoordinate4 = FVector2D(SpeedRatio * 100, -1.f);
			break;
		}
		else if (IsBackward)
		{
			MidLeftVertex.TextureCoordinate = FVector2D(0.0f, -VAccumulation - XRatio);
			MidLeftVertex.TextureCoordinate4 = FVector2D(SpeedRatio * 100, 1.f);
			break;
		}
		else
		{
			// fall through if neither
		}
	default:
		MidLeftVertex.TextureCoordinate = FVector2D(0.0f, VAccumulation + XRatio);
		MidLeftVertex.TextureCoordinate4 = FVector2D(SpeedRatio * 100, 0.f);
		break;
	}
	MidLeftVertex.Position = FVector(Mid, Z);
	MidLeftVertex.TextureCoordinate2 = FVector2D(-RightVector.X, -RightVector.Y);
	MidLeftVertex.TextureCoordinate3 = FVector2D(HalfThickness, MaxThickness);
	MidLeftVertex.TextureCoordinate5 = FVector2D(RoadTypeFloat, 0.f);
	MidLeftVertex.TangentX = FVector(alteredLineDirection, 0.0f);
	MidLeftVertex.TangentZ = FVector::UpVector;
	MidLeftVertex.Color = StartColor;
	MeshBoundingBox += MidLeftVertex.Position;

	const int32 MidRightVertexIndex = Vertices->Num();
	FStreetMapVertex& MidRightVertex = *new(*Vertices)FStreetMapVertex();
	MidRightVertex.LinkId = LinkId;
	MidRightVertex.LinkDir = LinkDir;
	MidRightVertex.TMC = TMC;
	MidRightVertex.SpeedLimit = SpeedLimit;
	switch (VertexType)
	{
	case EVertexType::VStreet:
	case EVertexType::VMajorRoad:
	case EVertexType::VHighway:
		if (IsForward)
		{
			MidRightVertex.TextureCoordinate = FVector2D(1.0f, VAccumulation + XRatio);
			MidRightVertex.TextureCoordinate4 = FVector2D(SpeedRatio * 100, 1.f);
			break;
		}
		else if (IsBackward)
		{
			MidRightVertex.TextureCoordinate = FVector2D(1.0f, -VAccumulation - XRatio);
			MidRightVertex.TextureCoordinate4 = FVector2D(SpeedRatio * 100, -1.f);
			break;
		}
		else
		{
			// fall through if neither
		}
	default:
		MidRightVertex.TextureCoordinate = FVector2D(1.0f, VAccumulation + XRatio);
		MidRightVertex.TextureCoordinate4 = FVector2D(SpeedRatio * 100, 0.f);
		break;
	}
	MidRightVertex.Position = FVector(Mid, Z);
	MidRightVertex.TextureCoordinate2 = FVector2D(RightVector.X, RightVector.Y);
	MidRightVertex.TextureCoordinate3 = FVector2D(HalfThickness, MaxThickness);
	MidRightVertex.TextureCoordinate5 = FVector2D(RoadTypeFloat, 0.f);
	MidRightVertex.TangentX = FVector(alteredLineDirection, 0.0f);
	MidRightVertex.TangentZ = FVector::UpVector;
	MidRightVertex.Color = StartColor;
	MeshBoundingBox += MidRightVertex.Position;

	auto numIdx = Indices->Num();
	auto BottomRightVertexIndex = (*Indices)[numIdx - 1];
	auto BottomLeftVertexIndex = (*Indices)[numIdx - 2];

	VAccumulation += XRatio;
	// Finish Last trinagle
	Indices->Add(MidRightVertexIndex);

	Indices->Add(BottomLeftVertexIndex);
	Indices->Add(MidRightVertexIndex);
	Indices->Add(MidLeftVertexIndex);

	// Start of next triangle
	Indices->Add(MidLeftVertexIndex);
	Indices->Add(MidRightVertexIndex);

}

void endSmoothVertices(const FVector2D End
	, const FVector2D RightVector
	, const FVector2D Tangent
	, const float Z
	, const float HalfThickness
	, const float MaxThickness
	, const float XRatio
	, const FColor& StartColor
	, const FColor& EndColor
	, FStreetMapMeshBuildSettings MeshBuildSettings
	, FBox& MeshBoundingBox
	, TArray<FStreetMapVertex>* Vertices
	, TArray<uint32>* Indices
	, EVertexType VertexType
	, int64 LinkId
	, FString LinkDir
	, FName TMC
	, int SpeedLimit
	, float SpeedRatio
	, float RoadTypeFloat
)
{
	const float QuarterThickness = HalfThickness * .5f;
	//const float Distance = (End - Start).Size();
	//const float XRatio = Distance / Thickness;
	const bool IsForward = LinkDir.Compare(TEXT("T"), ESearchCase::IgnoreCase) == 0;
	const bool IsBackward = LinkDir.Compare(TEXT("F"), ESearchCase::IgnoreCase) == 0;
	const int32 TopLeftVertexIndex = Vertices->Num();
	FStreetMapVertex& TopLeftVertex = *new(*Vertices)FStreetMapVertex();
	TopLeftVertex.LinkId = LinkId;
	TopLeftVertex.LinkDir = LinkDir;
	TopLeftVertex.TMC = TMC;
	TopLeftVertex.SpeedLimit = SpeedLimit;
	switch (VertexType)
	{
	case EVertexType::VStreet:
	case EVertexType::VMajorRoad:
	case EVertexType::VHighway:
		if (IsForward)
		{
			TopLeftVertex.TextureCoordinate = FVector2D(0.0f, XRatio);
			TopLeftVertex.TextureCoordinate4 = FVector2D(SpeedRatio * 100, -1.f);
			break;
		}
		else if (IsBackward)
		{
			TopLeftVertex.TextureCoordinate = FVector2D(0.0f, -XRatio);
			TopLeftVertex.TextureCoordinate4 = FVector2D(SpeedRatio * 100, 1.f);
			break;
		}
		else
		{
			// fall through if neither
		}
	default:
		TopLeftVertex.TextureCoordinate = FVector2D(0.0f, XRatio);
		TopLeftVertex.TextureCoordinate4 = FVector2D(SpeedRatio * 100, 0.f);
		break;
	}
	TopLeftVertex.Position = FVector(End, Z);
	TopLeftVertex.TextureCoordinate2 = FVector2D(-RightVector.X, -RightVector.Y);
	TopLeftVertex.TextureCoordinate3 = FVector2D(HalfThickness, MaxThickness);
	TopLeftVertex.TextureCoordinate5 = FVector2D(RoadTypeFloat, 0.f);
	TopLeftVertex.TangentX = FVector(Tangent, 0.0f);
	TopLeftVertex.TangentZ = FVector::UpVector;
	TopLeftVertex.Color = EndColor;
	// TopLeftVertex.Color = FColor(255, 0, 0);
	MeshBoundingBox += TopLeftVertex.Position;

	const int32 TopRightVertexIndex = Vertices->Num();
	FStreetMapVertex& TopRightVertex = *new(*Vertices)FStreetMapVertex();
	TopRightVertex.LinkId = LinkId;
	TopRightVertex.LinkDir = LinkDir;
	TopRightVertex.TMC = TMC;
	TopRightVertex.SpeedLimit = SpeedLimit;
	switch (VertexType)
	{
	case EVertexType::VStreet:
	case EVertexType::VMajorRoad:
	case EVertexType::VHighway:
		if (IsForward)
		{
			TopRightVertex.TextureCoordinate = FVector2D(1.0f, XRatio);
			TopRightVertex.TextureCoordinate4 = FVector2D(SpeedRatio * 100, 1.f);
			break;
		}
		else if (IsBackward)
		{
			TopRightVertex.TextureCoordinate = FVector2D(1.0f, -XRatio);
			TopRightVertex.TextureCoordinate4 = FVector2D(SpeedRatio * 100, -1.f);
			break;
		}
		else
		{
			// fall through if neither
		}
	default:
		TopRightVertex.TextureCoordinate = FVector2D(1.0f, XRatio);
		TopRightVertex.TextureCoordinate4 = FVector2D(SpeedRatio * 100, 0.f);
		break;
	}
	TopRightVertex.Position = FVector(End, Z);
	TopRightVertex.TextureCoordinate2 = FVector2D(RightVector.X, RightVector.Y);
	TopRightVertex.TextureCoordinate3 = FVector2D(HalfThickness, MaxThickness);
	TopRightVertex.TextureCoordinate5 = FVector2D(RoadTypeFloat, 0.f);
	TopRightVertex.TangentX = FVector(Tangent, 0.0f);
	TopRightVertex.TangentZ = FVector::UpVector;
	TopRightVertex.Color = EndColor;
	// TopRightVertex.Color = FColor(255, 0, 0);
	MeshBoundingBox += TopRightVertex.Position;

	auto numIdx = Indices->Num();

	auto MidRightVertexIndex = (*Indices)[numIdx - 1];
	auto MidLeftVertexIndex = (*Indices)[numIdx - 2];

	Indices->Add(TopRightVertexIndex);

	Indices->Add(MidLeftVertexIndex);
	Indices->Add(TopRightVertexIndex);
	Indices->Add(TopLeftVertexIndex);
}

void UStreetMapComponent::EndSmoothQuadList(const FVector2D& Mid
	, const FVector2D& End
	, const float Z
	, const float Thickness
	, const float MaxThickness
	, const FColor& StartColor
	, const FColor& EndColor
	, float& VAccumulation
	, FBox& MeshBoundingBox
	, TArray<FStreetMapVertex>* Vertices
	, TArray<uint32>* Indices
	, EVertexType VertexType
	, int64 LinkId
	, FString LinkDir
	, FName TMC
	, int SpeedLimit
	, float SpeedRatio
	, float RoadTypeFloat
)
{
	const float HalfThickness = Thickness * 0.5f;
	const float Distance = (End - Mid).Size();
	const float XRatio = (Distance / Thickness) + VAccumulation;
	VAccumulation = XRatio;

	const FVector2D LineDirection1 = (End - Mid).GetSafeNormal();
	const FVector2D RightVector(-LineDirection1.Y, LineDirection1.X);

	endSmoothVertices(End
		, RightVector
		, LineDirection1
		, Z
		, HalfThickness
		, MaxThickness
		, XRatio
		, StartColor
		, EndColor
		, MeshBuildSettings
		, MeshBoundingBox
		, Vertices
		, Indices
		, VertexType
		, LinkId
		, LinkDir
		, TMC
		, SpeedLimit
		, SpeedRatio
		, RoadTypeFloat
	);
}

void UStreetMapComponent::EndSmoothQuadList(const FVector2D& Start
	, const FVector2D& Mid
	, const FVector2D& End
	, const float Z
	, const float Thickness
	, const float MaxThickness
	, const FColor& StartColor
	, const FColor& EndColor
	, float& VAccumulation
	, FBox& MeshBoundingBox
	, TArray<FStreetMapVertex>* Vertices
	, TArray<uint32>* Indices
	, EVertexType VertexType
	, int64 LinkId
	, FString LinkDir
	, FName TMC
	, int SpeedLimit
	, float SpeedRatio
	, float RoadTypeFloat
)
{
	const float HalfThickness = Thickness * 0.5f;
	const float Distance = (Mid - Start).Size();
	const float XRatio = (Distance / Thickness) + VAccumulation;
	VAccumulation = XRatio;

	const FVector2D LineDirection1 = (Mid - Start).GetSafeNormal();
	const FVector2D LineDirection2 = (End - Mid).GetSafeNormal();

	auto alteredLineDirection = LineDirection1 + LineDirection2;
	alteredLineDirection.Normalize();
	const FVector2D RightVector(-alteredLineDirection.Y, alteredLineDirection.X);

	endSmoothVertices(Mid
		, RightVector
		, alteredLineDirection
		, Z
		, HalfThickness
		, MaxThickness
		, XRatio
		, StartColor
		, EndColor
		, MeshBuildSettings
		, MeshBoundingBox
		, Vertices
		, Indices
		, VertexType
		, LinkId
		, LinkDir
		, TMC
		, SpeedLimit
		, SpeedRatio
		, RoadTypeFloat
	);
}

FString UStreetMapComponent::GetStreetMapAssetName() const
{
	return StreetMap != nullptr ? StreetMap->GetName() : FString(TEXT("NONE"));
}

void UStreetMapComponent::AddOrUpdateFlowData(FName TMC, float Speed)
{
	if (!mFlowData.Contains(TMC)) {
		mFlowData.Add(TMC, Speed);
	}
	else {
		mFlowData[TMC] = Speed;
	}
}

void UStreetMapComponent::DeleteFlowData(FName TMC)
{
	mFlowData.Remove(TMC);
}

void UStreetMapComponent::ClearFlowData()
{
	mFlowData.Empty();
}

void UStreetMapComponent::AddOrUpdatePredictiveData(FName TMC, float S0, float S15, float S30, float S45)
{
	FPredictiveData Data;
	Data.S0 = S0;
	Data.S15 = S15;
	Data.S30 = S30;
	Data.S45 = S45;

	if (!mPredictiveData.Contains(TMC)) {
		mPredictiveData.Add(TMC, Data);
	}
	else {
		mPredictiveData[TMC] = Data;
	}
}

void UStreetMapComponent::DeletePredictiveData(FName TMC)
{
	mPredictiveData.Remove(TMC);
}

void UStreetMapComponent::ClearPredictiveData()
{
	mPredictiveData.Empty();
}

FGuid UStreetMapComponent::AddTrace(FStreetMapTrace Trace)
{
	FGuid NewGuid = FGuid::NewGuid();

	Trace.GUID = NewGuid;
	mTraces.Add(NewGuid, Trace);

	this->ColorRoadMesh(Trace.Color, Trace.Links, true, 1.0f);

	return NewGuid;
}

bool UStreetMapComponent::ShowTrace(FGuid GUID)
{
	if (mTraces.Contains(GUID)) {
		auto Trace = mTraces[GUID];

		this->ColorRoadMesh(Trace.Color, Trace.Links, true, 1.0f);

		return true;
	}
	else {
		return false;
	}
}

bool UStreetMapComponent::HideTrace(FGuid GUID, FColor LowFlowColor = FColor::Transparent, FColor MedFlowColor = FColor::Transparent, FColor HighFlowColor = FColor::Transparent)
{
	const FColor StreetColor = MeshBuildSettings.StreetColor.ToFColor(false);
	const FColor MajorRoadColor = MeshBuildSettings.MajorRoadColor.ToFColor(false);
	const FColor HighwayColor = MeshBuildSettings.HighwayColor.ToFColor(false);
	const float StreetOffsetZ = MeshBuildSettings.StreetOffsetZ;
	const float MajorRoadOffsetZ = MeshBuildSettings.MajorRoadOffsetZ;
	const float HighwayOffsetZ = MeshBuildSettings.HighwayOffsetZ;
	const EColorMode ColorMode = MeshBuildSettings.ColorMode;

	if (LowFlowColor == FColor::Transparent) {
		LowFlowColor = MeshBuildSettings.LowFlowColor.ToFColor(false);
	}
	if (MedFlowColor == FColor::Transparent) {
		MedFlowColor = MeshBuildSettings.MedFlowColor.ToFColor(false);
	}
	if (HighFlowColor == FColor::Transparent) {
		HighFlowColor = MeshBuildSettings.HighFlowColor.ToFColor(false);
	}

	if (!mTraces.Contains(GUID)) return false;

	auto Trace = mTraces[GUID];

	switch (ColorMode) {
	case EColorMode::Flow:
	case EColorMode::Predictive0:
	case EColorMode::Predictive15:
	case EColorMode::Predictive30:
	case EColorMode::Predictive45:
		this->ColorRoadMeshFromData(Trace.Links, HighwayColor, LowFlowColor, MedFlowColor, HighFlowColor, true);
		break;
	default:
		this->ColorRoadMesh(HighwayColor, Trace.Links);
		break;
	}

	return true;
}

bool UStreetMapComponent::GetTraceDetails(TArray<FStreetMapLink> Links, float& OutAvgSpeed, float& OutDistance, float& OutTravelTime, float& OutIdealTravelTime)
{
	if (!StreetMap) return false;
	//if (!mTraces.Contains(GUID)) return false;

	const auto& Roads = StreetMap->GetRoads();
	//const auto Trace = mTraces[GUID];
	float IdealTotalTimeMin = 0;
	float TotalTimeMin = 0;

	for (const auto& TraceLink : Links)
	{
		if (mLink2RoadIndex.Contains(TraceLink))
		{
			const auto RoadIndex = mLink2RoadIndex[TraceLink];
			const auto TMC = Roads[RoadIndex].TMC;
			auto SpeedMpm = Roads[RoadIndex].SpeedLimit / 60.0f;
			if (mFlowData.Contains(TMC)) {
				SpeedMpm = mFlowData[TMC] / 60.0f;
			}
			const auto SpeedLimitMpm = Roads[RoadIndex].SpeedLimit / 60.0f;
			const auto Distance = Roads[RoadIndex].Distance;
			const auto TimeMin = Distance / SpeedMpm;
			const auto IdealTimeMin = Distance / SpeedLimitMpm;

			OutDistance += Distance;
			TotalTimeMin += TimeMin;
			IdealTotalTimeMin += IdealTimeMin;
		}
	}

	OutTravelTime = TotalTimeMin;
	OutIdealTravelTime = IdealTotalTimeMin;
	OutAvgSpeed = OutDistance / (TotalTimeMin / 60.0f);

	return true;
}

bool UStreetMapComponent::DeleteTrace(FGuid GUID, FColor LowFlowColor = FColor::Transparent, FColor MedFlowColor = FColor::Transparent, FColor HighFlowColor = FColor::Transparent)
{
	const FColor StreetColor = MeshBuildSettings.StreetColor.ToFColor(false);
	const FColor MajorRoadColor = MeshBuildSettings.MajorRoadColor.ToFColor(false);
	const FColor HighwayColor = MeshBuildSettings.HighwayColor.ToFColor(false);
	const float StreetOffsetZ = MeshBuildSettings.StreetOffsetZ;
	const float MajorRoadOffsetZ = MeshBuildSettings.MajorRoadOffsetZ;
	const float HighwayOffsetZ = MeshBuildSettings.HighwayOffsetZ;
	const EColorMode ColorMode = MeshBuildSettings.ColorMode;

	if (LowFlowColor == FColor::Transparent) {
		LowFlowColor = MeshBuildSettings.LowFlowColor.ToFColor(false);
	}
	if (MedFlowColor == FColor::Transparent) {
		MedFlowColor = MeshBuildSettings.MedFlowColor.ToFColor(false);
	}
	if (HighFlowColor == FColor::Transparent) {
		HighFlowColor = MeshBuildSettings.HighFlowColor.ToFColor(false);
	}

	if (!mTraces.Contains(GUID)) return false;

	auto Trace = mTraces[GUID];

	switch (ColorMode) {
	case EColorMode::Flow:
	case EColorMode::Predictive0:
	case EColorMode::Predictive15:
	case EColorMode::Predictive30:
	case EColorMode::Predictive45:
		this->ColorRoadMeshFromData(Trace.Links, HighwayColor, LowFlowColor, MedFlowColor, HighFlowColor, true);
		break;
	default:
		this->ColorRoadMesh(HighwayColor, Trace.Links);
		break;
	}

	mTraces.Remove(GUID);

	return true;
}

bool UStreetMapComponent::GetSpeed(FStreetMapLink Link, float& OutSpeed, float& OutSpeedLimit, float& OutSpeedRatio)
{
	const EColorMode ColorMode = MeshBuildSettings.ColorMode;
	const auto& Roads = StreetMap->GetRoads();
	const int64 LinkId = Link.LinkId;
	const FString LinkDir = Link.LinkDir;
	FColor Color;

	bool bFound = false;

	auto RoadPtr = Roads.FindByPredicate([LinkId, LinkDir](const FStreetMapRoad& Road)
	{
		return Road.Link.LinkId == LinkId && Road.Link.LinkDir == LinkDir;
	});

	if (RoadPtr == nullptr)
	{
		OutSpeed = 25;
		OutSpeedLimit = 25;
		OutSpeedRatio = 1.0f;

		return false;
	}

	switch (ColorMode) {
	case EColorMode::Flow:
	case EColorMode::Predictive0:
	case EColorMode::Predictive15:
	case EColorMode::Predictive30:
	case EColorMode::Predictive45:
		bFound = GetSpeedAndColorFromData(RoadPtr, OutSpeed, OutSpeedLimit, OutSpeedRatio, Color);
		break;
	}

	if (!bFound)
	{
		OutSpeed = RoadPtr->SpeedLimit;
		OutSpeedLimit = RoadPtr->SpeedLimit;
		OutSpeedRatio = 1.0f;

		return false;
	}

	return true;
}

EColorMode UStreetMapComponent::GetColorMode() {
	return MeshBuildSettings.ColorMode.GetValue();
}

void UStreetMapComponent::SetColorMode(EColorMode colorMode) {
	MeshBuildSettings.ColorMode = colorMode;
}

TArray<FStreetMapRoad> UStreetMapComponent::GetRoads(const TArray<FStreetMapLink>& Links)
{
	TArray<FStreetMapRoad> LinkRoads;

	const auto& Roads = StreetMap->GetRoads();

	for (auto& Link : Links) {
		if (mLink2RoadIndex.Contains(Link)) {
			int RoadIndex = mLink2RoadIndex[Link];
			LinkRoads.Add(Roads[RoadIndex]);
		}
	}

	return LinkRoads;
}

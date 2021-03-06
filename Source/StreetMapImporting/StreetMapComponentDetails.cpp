// Copyright 2017 Mike Fricker. All Rights Reserved.

#include "StreetMapImporting.h"

#include "StreetMapComponentDetails.h"

#include "SlateBasics.h"
#include "RawMesh.h"
#include "PropertyEditorModule.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "PropertyCustomizationHelpers.h"
#include "IDetailsView.h"
#include "IDetailCustomization.h"
#include "AssetRegistryModule.h"
#include "Dialogs/DlgPickAssetPath.h"
#include "IDetailCustomization.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Misc/AssertionMacros.h"


#include "StreetMapComponent.h"
#include "GISUtils/Elevation.h"
#include "StreetMapRailway.h"
#include "StreetMapRoad.h"
#include "StreetMapSplineTools.h"

#define LOCTEXT_NAMESPACE "StreetMapComponentDetails"


FStreetMapComponentDetails::FStreetMapComponentDetails() :
	SelectedStreetMapComponent(nullptr),
	LastDetailBuilderPtr(nullptr)
{

}

TSharedRef<IDetailCustomization> FStreetMapComponentDetails::MakeInstance()
{
	return MakeShareable(new FStreetMapComponentDetails());
}

void FStreetMapComponentDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	LastDetailBuilderPtr = &DetailBuilder;

	TArray <TWeakObjectPtr<UObject>> SelectedObjects = DetailBuilder.GetDetailsView()->GetSelectedObjects();

	for (const TWeakObjectPtr<UObject>& Object : SelectedObjects)
	{
		UStreetMapComponent* TempStreetMapComp = Cast<UStreetMapComponent>(Object.Get());
		if (TempStreetMapComp != nullptr && !TempStreetMapComp->IsTemplate())
		{
			SelectedStreetMapComponent = TempStreetMapComp;
			break;
		}
	}


	if (SelectedStreetMapComponent == nullptr)
	{
		TArray<TWeakObjectPtr<AActor>> SelectedActors = DetailBuilder.GetDetailsView()->GetSelectedActors();

		for (const TWeakObjectPtr<UObject>& Object : SelectedObjects)
		{
			AActor* TempActor = Cast<AActor>(Object.Get());
			if (TempActor != nullptr && !TempActor->IsTemplate())
			{
				UStreetMapComponent* TempStreetMapComp = TempActor->FindComponentByClass<UStreetMapComponent>();
				if (TempStreetMapComp != nullptr && !TempStreetMapComp->IsTemplate())
				{
					SelectedStreetMapComponent = TempStreetMapComp;
					break;
				}
				break;
			}
		}
	}



	if (SelectedStreetMapComponent == nullptr)
	{
		return;
	}


	IDetailCategoryBuilder& StreetMapCategory = DetailBuilder.EditCategory("StreetMap", FText::GetEmpty(), ECategoryPriority::Important);
	StreetMapCategory.InitiallyCollapsed(false);


	const bool bCanRebuildMesh = HasValidMapObject();
	const bool bCanClearMesh = HasValidMeshData();
	const bool bCanCreateMeshAsset = HasValidMeshData();

	TSharedPtr< SHorizontalBox > TempHorizontalBox;

	StreetMapCategory.AddCustomRow(FText::GetEmpty(), false)
		[
			SAssignNew(TempHorizontalBox, SHorizontalBox)
			+ SHorizontalBox::Slot()
		[
			SNew(SButton)
			.ToolTipText(LOCTEXT("GenerateMesh_Tooltip", "Generate a cached mesh from raw street map data."))
		.OnClicked(this, &FStreetMapComponentDetails::OnBuildMeshClicked)
		.IsEnabled(bCanRebuildMesh)
		.HAlign(HAlign_Center)
		[
			SNew(STextBlock)
			.Text(bCanClearMesh ? LOCTEXT("RebuildMesh", "Rebuild Mesh") : LOCTEXT("BuildMesh", "Build Mesh"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		]
		];


	TempHorizontalBox->AddSlot()
		[
			SNew(SButton)
			.ToolTipText(LOCTEXT("ClearMesh_Tooltip", "Clear current mesh data , in case we have a valid mesh "))
		.OnClicked(this, &FStreetMapComponentDetails::OnClearMeshClicked)
		.IsEnabled(bCanClearMesh)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ClearMesh", "Clear Mesh"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		];


	StreetMapCategory.AddCustomRow(FText::GetEmpty(), false)
		[
			SAssignNew(TempHorizontalBox, SHorizontalBox)
			+ SHorizontalBox::Slot()
		[
			SNew(SButton)
			.ToolTipText(LOCTEXT("CreateStaticMeshAsset_Tooltip", "Create a new Static Mesh Asset from selected StreetMapComponent."))
		.OnClicked(this, &FStreetMapComponentDetails::OnCreateStaticMeshAssetClicked)
		.IsEnabled(bCanCreateMeshAsset)
		.HAlign(HAlign_Center)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("CreateStaticMeshAsset", "Create Static Mesh Asset"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		]
		];

	if (bCanCreateMeshAsset)
	{
		const int32 NumVertices = SelectedStreetMapComponent->GetRawMeshVertices(EVertexType::VBuilding).Num() + SelectedStreetMapComponent->GetRawMeshVertices(EVertexType::VHighway).Num() + SelectedStreetMapComponent->GetRawMeshVertices(EVertexType::VMajorRoad).Num() + SelectedStreetMapComponent->GetRawMeshVertices(EVertexType::VStreet).Num();
		const FString NumVerticesToString = TEXT("Vertex Count : ") + FString::FromInt(NumVertices);

		const int32 NumTriangles = (SelectedStreetMapComponent->GetRawMeshIndices(EVertexType::VBuilding).Num() + SelectedStreetMapComponent->GetRawMeshIndices(EVertexType::VHighway).Num() + SelectedStreetMapComponent->GetRawMeshIndices(EVertexType::VMajorRoad).Num() + SelectedStreetMapComponent->GetRawMeshIndices(EVertexType::VStreet).Num()) / 3;
		const FString NumTrianglesToString = TEXT("Triangle Count : ") + FString::FromInt(NumTriangles);

		const bool bCollisionEnabled = SelectedStreetMapComponent->IsCollisionEnabled();
		const FString CollisionStatusToString = bCollisionEnabled ? TEXT("Collision : ON") : TEXT("Collision : OFF");

		StreetMapCategory.AddCustomRow(FText::GetEmpty(), true)
			[
				SAssignNew(TempHorizontalBox, SHorizontalBox)
				+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Font(FSlateFontInfo("Verdana", 8))
			.Text(FText::FromString(NumVerticesToString))
			]
		+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Font(FSlateFontInfo("Verdana", 8))
			.Text(FText::FromString(NumTrianglesToString))
			]
		+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Font(FSlateFontInfo("Verdana", 8))
			.Text(FText::FromString(CollisionStatusToString))
			]
			];
	}

	// Landscape settings
	{
		IDetailCategoryBuilder& LandscapeCategory = DetailBuilder.EditCategory("Landscape", FText::GetEmpty(), ECategoryPriority::Important);
		LandscapeCategory.InitiallyCollapsed(false);

		LandscapeCategory.AddCustomRow(FText::GetEmpty(), false)
			[
				SAssignNew(TempHorizontalBox, SHorizontalBox)
				+ SHorizontalBox::Slot()
			[
				SNew(SButton)
				.ToolTipText(LOCTEXT("BuildLandscape_Tooltip", "Download elevation model and build a Landscape beneath the OpenStreetMap."))
			.OnClicked(this, &FStreetMapComponentDetails::OnBuildLandscapeClicked)
			.IsEnabled(this, &FStreetMapComponentDetails::BuildLandscapeIsEnabled)
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("BuildLandscape", "Build Landscape"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
			]
			]
			];

		TSharedRef<IPropertyHandle> PropertyHandle_Material = DetailBuilder.GetProperty("LandscapeSettings.Material");
		PropertyHandle_Material->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FStreetMapComponentDetails::RefreshLandscapeLayersList));

		RefreshLandscapeLayersList();
	}

	// Railway settings
	{
		IDetailCategoryBuilder& RailwayCategory = DetailBuilder.EditCategory("Railway", FText::GetEmpty(), ECategoryPriority::Important);
		RailwayCategory.InitiallyCollapsed(false);

		RailwayCategory.AddCustomRow(FText::GetEmpty(), false)
			[
				SAssignNew(TempHorizontalBox, SHorizontalBox)
				+ SHorizontalBox::Slot()
			[
				SNew(SButton)
				.ToolTipText(LOCTEXT("BuildRailway_Tooltip", "Generate railways onto the Landscape based on the OpenStreetMap data."))
			.OnClicked(this, &FStreetMapComponentDetails::OnBuildRailwayClicked)
			.IsEnabled(this, &FStreetMapComponentDetails::BuildRailwayIsEnabled)
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("BuildRailway", "Build Railway"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
			]
			]
			];
	}

	// Roads settings
	{
		IDetailCategoryBuilder& RoadCategory = DetailBuilder.EditCategory("Roads", FText::GetEmpty(), ECategoryPriority::Important);
		RoadCategory.InitiallyCollapsed(false);

		RoadCategory.AddCustomRow(FText::GetEmpty(), false)
			[
				SAssignNew(TempHorizontalBox, SHorizontalBox)
				+ SHorizontalBox::Slot()
			[
				SNew(SButton)
				.ToolTipText(LOCTEXT("BuildRoads_Tooltip", "Generate roads onto the Landscape based on the OpenStreetMap data."))
			.OnClicked(this, &FStreetMapComponentDetails::OnBuildRoadsClicked)
			.IsEnabled(this, &FStreetMapComponentDetails::BuildRoadsIsEnabled)
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("BuildRoads", "Build Roads"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
			]
			]
			];
	}

	// Spline settings
	{
		IDetailCategoryBuilder& RailwayCategory = DetailBuilder.EditCategory("Splines", FText::GetEmpty(), ECategoryPriority::Important);
		RailwayCategory.InitiallyCollapsed(false);

		RailwayCategory.AddCustomRow(FText::GetEmpty(), false)
			[
				SAssignNew(TempHorizontalBox, SHorizontalBox)
				+ SHorizontalBox::Slot()
			[
				SNew(SButton)
				.ToolTipText(LOCTEXT("BuildSplines_Tooltip", "Generate Spline Actors along the shortest path connected Landscape Splines."))
			.OnClicked(this, &FStreetMapComponentDetails::OnBuildSplinesClicked)
			.IsEnabled(this, &FStreetMapComponentDetails::BuildSplinesIsEnabled)
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("BuildSplines", "Build Splines"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
			]
			]
			];
	}
}

bool FStreetMapComponentDetails::HasValidMeshData() const
{
	return SelectedStreetMapComponent != nullptr && SelectedStreetMapComponent->HasValidMesh();
}


bool FStreetMapComponentDetails::HasValidMapObject() const
{
	return SelectedStreetMapComponent != nullptr && SelectedStreetMapComponent->GetStreetMap() != nullptr;
}

FReply FStreetMapComponentDetails::OnCreateStaticMeshAssetClicked()
{
	if (SelectedStreetMapComponent != nullptr)
	{
		FString NewNameSuggestion = SelectedStreetMapComponent->GetStreetMapAssetName();
		FString PackageName = FString(TEXT("/Game/Meshes/")) + NewNameSuggestion;
		FString Name;
		FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
		AssetToolsModule.Get().CreateUniqueAssetName(PackageName, TEXT(""), PackageName, Name);

		TSharedPtr<SDlgPickAssetPath> PickAssetPathWidget =
			SNew(SDlgPickAssetPath)
			.Title(LOCTEXT("ConvertToStaticMeshPickName", "Choose New StaticMesh Location"))
			.DefaultAssetPath(FText::FromString(PackageName));

		if (PickAssetPathWidget->ShowModal() == EAppReturnType::Ok)
		{
			// Get the full name of where we want to create the physics asset.
			FString UserPackageName = PickAssetPathWidget->GetFullAssetPath().ToString();
			FName MeshName(*FPackageName::GetLongPackageAssetName(UserPackageName));

			// Check if the user inputed a valid asset name, if they did not, give it the generated default name
			if (MeshName == NAME_None)
			{
				// Use the defaults that were already generated.
				UserPackageName = PackageName;
				MeshName = *Name;
			}


			const TArray<FStreetMapVertex > BuildingRawMeshVertices = SelectedStreetMapComponent->GetRawMeshVertices(EVertexType::VBuilding);
			const TArray< uint32 > BuildingRawMeshIndices = SelectedStreetMapComponent->GetRawMeshIndices(EVertexType::VBuilding);

			this->CreateStaticMeshAsset(UserPackageName, MeshName, BuildingRawMeshVertices, BuildingRawMeshIndices);

			const TArray<FStreetMapVertex > StreetRawMeshVertices = SelectedStreetMapComponent->GetRawMeshVertices(EVertexType::VStreet);
			const TArray< uint32 > StreetRawMeshIndices = SelectedStreetMapComponent->GetRawMeshIndices(EVertexType::VStreet);

			this->CreateStaticMeshAsset(UserPackageName, MeshName, StreetRawMeshVertices, StreetRawMeshIndices);

			const TArray<FStreetMapVertex > HighwayRawMeshVertices = SelectedStreetMapComponent->GetRawMeshVertices(EVertexType::VHighway);
			const TArray< uint32 > HighwayRawMeshIndices = SelectedStreetMapComponent->GetRawMeshIndices(EVertexType::VHighway);

			this->CreateStaticMeshAsset(UserPackageName, MeshName, HighwayRawMeshVertices, HighwayRawMeshIndices);

			const TArray<FStreetMapVertex > MajorRoadRawMeshVertices = SelectedStreetMapComponent->GetRawMeshVertices(EVertexType::VMajorRoad);
			const TArray< uint32 > MajorRoadRawMeshIndices = SelectedStreetMapComponent->GetRawMeshIndices(EVertexType::VMajorRoad);

			this->CreateStaticMeshAsset(UserPackageName, MeshName, MajorRoadRawMeshVertices, MajorRoadRawMeshIndices);
		}
	}

	return FReply::Handled();
}

void FStreetMapComponentDetails::CreateStaticMeshAsset(FString UserPackageName, FName MeshName, TArray<FStreetMapVertex> RawMeshVertices, TArray<uint32> RawMeshIndices) {
	// Raw mesh data we are filling in
	FRawMesh RawMesh;
	// Materials to apply to new mesh
	TArray<UMaterialInterface*> MeshMaterials = SelectedStreetMapComponent->GetMaterials();

	// Copy verts
	for (int32 VertIndex = 0; VertIndex < RawMeshVertices.Num(); VertIndex++)
	{
		RawMesh.VertexPositions.Add(RawMeshVertices[VertIndex].Position);
	}

	// Copy 'wedge' info
	int32 NumIndices = RawMeshIndices.Num();
	for (int32 IndexIdx = 0; IndexIdx < NumIndices; IndexIdx++)
	{
		int32 VertexIndex = RawMeshIndices[IndexIdx];

		RawMesh.WedgeIndices.Add(VertexIndex);

		const FStreetMapVertex& StreetMapVertex = RawMeshVertices[VertexIndex];

		FVector TangentX = StreetMapVertex.TangentX;
		FVector TangentZ = StreetMapVertex.TangentZ;
		FVector TangentY = (TangentX ^ TangentZ).GetSafeNormal();

		RawMesh.WedgeTangentX.Add(TangentX);
		RawMesh.WedgeTangentY.Add(TangentY);
		RawMesh.WedgeTangentZ.Add(TangentZ);

		RawMesh.WedgeTexCoords[0].Add(StreetMapVertex.TextureCoordinate);
		RawMesh.WedgeTexCoords[1].Add(StreetMapVertex.TextureCoordinate2);
		RawMesh.WedgeTexCoords[2].Add(StreetMapVertex.TextureCoordinate3);
		RawMesh.WedgeTexCoords[3].Add(StreetMapVertex.TextureCoordinate4);
		RawMesh.WedgeTexCoords[4].Add(StreetMapVertex.TextureCoordinate5);
		RawMesh.WedgeColors.Add(StreetMapVertex.Color);
	}

	// copy face info
	int32 NumTris = NumIndices / 3;
	for (int32 TriIdx = 0; TriIdx < NumTris; TriIdx++)
	{
		RawMesh.FaceMaterialIndices.Add(0);
		RawMesh.FaceSmoothingMasks.Add(0); // Assume this is ignored as bRecomputeNormals is false
	}

	// If we got some valid data.
	if (RawMesh.VertexPositions.Num() > 3 && RawMesh.WedgeIndices.Num() > 3)
	{
		// Then find/create it.
		UPackage* Package = CreatePackage(NULL, *UserPackageName);
		check(Package);

		// Create StaticMesh object
		UStaticMesh* StaticMesh = NewObject<UStaticMesh>(Package, MeshName, RF_Public | RF_Standalone);
		StaticMesh->InitResources();

		StaticMesh->LightingGuid = FGuid::NewGuid();

		// Add source to new StaticMesh
		FStaticMeshSourceModel* SrcModel = new (StaticMesh->SourceModels) FStaticMeshSourceModel();
		SrcModel->BuildSettings.bRecomputeNormals = false;
		SrcModel->BuildSettings.bRecomputeTangents = false;
		SrcModel->BuildSettings.bRemoveDegenerates = false;
		SrcModel->BuildSettings.bUseHighPrecisionTangentBasis = false;
		SrcModel->BuildSettings.bUseFullPrecisionUVs = false;
		SrcModel->BuildSettings.bGenerateLightmapUVs = true;
		SrcModel->BuildSettings.SrcLightmapIndex = 0;
		SrcModel->BuildSettings.DstLightmapIndex = 1;
		SrcModel->RawMeshBulkData->SaveRawMesh(RawMesh);

		// Copy materials to new mesh
		for (UMaterialInterface* Material : MeshMaterials)
		{
			StaticMesh->StaticMaterials.Add(FStaticMaterial(Material));
		}

		//Set the Imported version before calling the build
		StaticMesh->ImportVersion = EImportStaticMeshVersion::LastVersion;

		// Build mesh from source
		StaticMesh->Build(/** bSilent =*/ false);
		StaticMesh->PostEditChange();

		StaticMesh->MarkPackageDirty();

		// Notify asset registry of new asset
		FAssetRegistryModule::AssetCreated(StaticMesh);


		// Display notification so users can quickly access the mesh
		if (GIsEditor)
		{
			FNotificationInfo Info(FText::Format(LOCTEXT("StreetMapMeshConverted", "Successfully Converted Mesh"), FText::FromString(StaticMesh->GetName())));
			Info.ExpireDuration = 8.0f;
			Info.bUseLargeFont = false;
			Info.Hyperlink = FSimpleDelegate::CreateLambda([=]() { FAssetEditorManager::Get().OpenEditorForAssets(TArray<UObject*>({ StaticMesh })); });
			Info.HyperlinkText = FText::Format(LOCTEXT("OpenNewAnimationHyperlink", "Open {0}"), FText::FromString(StaticMesh->GetName()));
			TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
			if (Notification.IsValid())
			{
				Notification->SetCompletionState(SNotificationItem::CS_Success);
			}
		}
	}
}

FReply FStreetMapComponentDetails::OnBuildMeshClicked()
{

	if (SelectedStreetMapComponent != nullptr)
	{
		//
		SelectedStreetMapComponent->BuildMesh();

		// regenerates details panel layouts , to take in consideration new changes.
		RefreshDetails();
	}

	return FReply::Handled();
}

FReply FStreetMapComponentDetails::OnClearMeshClicked()
{
	if (SelectedStreetMapComponent != nullptr)
	{
		//
		SelectedStreetMapComponent->InvalidateMesh();

		// regenerates details panel layouts , to take in consideration new changes.
		RefreshDetails();
	}

	return FReply::Handled();
}

void FStreetMapComponentDetails::RefreshDetails()
{
	if (LastDetailBuilderPtr != nullptr)
	{
		LastDetailBuilderPtr->ForceRefreshDetails();
	}
}

FReply FStreetMapComponentDetails::OnBuildLandscapeClicked()
{
	if (SelectedStreetMapComponent != nullptr)
	{
		BuildLandscape(SelectedStreetMapComponent, SelectedStreetMapComponent->LandscapeSettings);

		// regenerates details panel layouts, to take in consideration new changes.
		RefreshDetails();
	}

	return FReply::Handled();
}

bool FStreetMapComponentDetails::BuildLandscapeIsEnabled() const
{
	if (!SelectedStreetMapComponent || !SelectedStreetMapComponent->LandscapeSettings.Material)
	{
		return false;
	}

	for (int32 i = 0; i < SelectedStreetMapComponent->LandscapeSettings.Layers.Num(); ++i)
	{
		if (!SelectedStreetMapComponent->LandscapeSettings.Layers[i].LayerInfo)
		{
			return false;
		}
	}

	return true;
}

void FStreetMapComponentDetails::RefreshLandscapeLayersList()
{
	if (!SelectedStreetMapComponent) return;

	UMaterialInterface* Material = SelectedStreetMapComponent->LandscapeSettings.Material;
	TArray<FName> LayerNames = ALandscapeProxy::GetLayersFromMaterial(Material);

	const TArray<FLandscapeImportLayerInfo> OldLayersList = MoveTemp(SelectedStreetMapComponent->LandscapeSettings.Layers);
	const TArray<FLayerWayMapping> OldLayerWayMapping = MoveTemp(SelectedStreetMapComponent->LandscapeSettings.LayerWayMapping);
	SelectedStreetMapComponent->LandscapeSettings.Layers.Reset(LayerNames.Num());
	SelectedStreetMapComponent->LandscapeSettings.LayerWayMapping.Reset(LayerNames.Num());

	for (int32 i = 0; i < LayerNames.Num(); i++)
	{
		const FName& LayerName = LayerNames[i];

		// Find or recreate this layer.
		{
			bool bFound = false;
			FLandscapeImportLayerInfo NewImportLayer;
			for (int32 j = 0; j < OldLayersList.Num(); j++)
			{
				if (OldLayersList[j].LayerName == LayerName)
				{
					NewImportLayer = OldLayersList[j];
					bFound = true;
					break;
				}
			}
			if (!bFound)
			{
				NewImportLayer.LayerName = LayerName;
			}
			SelectedStreetMapComponent->LandscapeSettings.Layers.Add(MoveTemp(NewImportLayer));
		}

		// Find or recreate the way mapping for this layer.
		{
			bool bFound = false;
			FLayerWayMapping NewLayerWayMapping;
			for (int32 j = 0; j < OldLayerWayMapping.Num(); j++)
			{
				if (OldLayerWayMapping[j].LayerName == LayerName)
				{
					NewLayerWayMapping = OldLayerWayMapping[j];
					bFound = true;
					break;
				}
			}

			if (!bFound)
			{
				NewLayerWayMapping.LayerName = LayerName;

				if (LayerName == "Grass")
				{
					NewLayerWayMapping.Matches.Add(FWayMatch(EStreetMapMiscWayType::LandUse, TEXT("grass")));
					NewLayerWayMapping.Matches.Add(FWayMatch(EStreetMapMiscWayType::LandUse, TEXT("village_green")));
					NewLayerWayMapping.Matches.Add(FWayMatch(EStreetMapMiscWayType::LandUse, TEXT("meadow")));
					NewLayerWayMapping.Matches.Add(FWayMatch(EStreetMapMiscWayType::LandUse, TEXT("farmland")));
					NewLayerWayMapping.Matches.Add(FWayMatch(EStreetMapMiscWayType::Leisure, TEXT("park")));
					NewLayerWayMapping.Matches.Add(FWayMatch(EStreetMapMiscWayType::LandUse, TEXT("plant_nursery")));
					// might be inappropiate - in one usecase the area is marked industrial (should default to concrete) but grass fits it better
					// TODO: make LayerWayMapping better configurable in editor
					NewLayerWayMapping.Matches.Add(FWayMatch(EStreetMapMiscWayType::LandUse, TEXT("industrial")));

				}
				else if (LayerName == "Wood")
				{
					NewLayerWayMapping.Matches.Add(FWayMatch(EStreetMapMiscWayType::LandUse, TEXT("forest")));
					NewLayerWayMapping.Matches.Add(FWayMatch(EStreetMapMiscWayType::Natural, TEXT("wood")));
					NewLayerWayMapping.Matches.Add(FWayMatch(EStreetMapMiscWayType::Natural, TEXT("scrub")));
					NewLayerWayMapping.Matches.Add(FWayMatch(EStreetMapMiscWayType::Natural, TEXT("nature_reserve")));
				}
				else if (LayerName == "Water")
				{
					NewLayerWayMapping.Matches.Add(FWayMatch(EStreetMapMiscWayType::Natural, TEXT("water")));
				}
			}
			SelectedStreetMapComponent->LandscapeSettings.LayerWayMapping.Add(MoveTemp(NewLayerWayMapping));
		}
	}
}


FReply FStreetMapComponentDetails::OnBuildRailwayClicked()
{
	if (SelectedStreetMapComponent != nullptr)
	{
		BuildRailway(SelectedStreetMapComponent, SelectedStreetMapComponent->RailwaySettings);

		// regenerates details panel layouts, to take in consideration new changes.
		RefreshDetails();
	}

	return FReply::Handled();
}

bool FStreetMapComponentDetails::BuildRailwayIsEnabled() const
{
	if (!SelectedStreetMapComponent ||
		!SelectedStreetMapComponent->RailwaySettings.RailwayLineMesh ||
		!SelectedStreetMapComponent->RailwaySettings.Landscape)
	{
		return false;
	}

	return true;
}


FReply FStreetMapComponentDetails::OnBuildRoadsClicked()
{
	if (SelectedStreetMapComponent != nullptr)
	{
		BuildRoads(SelectedStreetMapComponent, SelectedStreetMapComponent->RoadSettings, SelectedStreetMapComponent->MeshBuildSettings);

		// regenerates details panel layouts, to take in consideration new changes.
		RefreshDetails();
	}

	return FReply::Handled();
}

bool FStreetMapComponentDetails::BuildRoadsIsEnabled() const
{
	if (!SelectedStreetMapComponent ||
		!SelectedStreetMapComponent->RoadSettings.RoadMesh ||
		!SelectedStreetMapComponent->RoadSettings.Landscape)
	{
		return false;
	}

	return true;
}


FReply FStreetMapComponentDetails::OnBuildSplinesClicked()
{
	if (SelectedStreetMapComponent != nullptr)
	{
		BuildSplines(
			SelectedStreetMapComponent,
			SelectedStreetMapComponent->SplineSettings,
			SelectedStreetMapComponent->RailwaySettings.Landscape);

		// regenerates details panel layouts, to take in consideration new changes.
		RefreshDetails();
	}

	return FReply::Handled();
}

bool FStreetMapComponentDetails::BuildSplinesIsEnabled() const
{
	if (!SelectedStreetMapComponent ||
		!SelectedStreetMapComponent->SplineSettings.Start ||
		!SelectedStreetMapComponent->SplineSettings.End ||
		!SelectedStreetMapComponent->RailwaySettings.Landscape ||
		!SelectedStreetMapComponent->RailwaySettings.Landscape->SplineComponent)
	{
		return false;
	}

	return true;
}

#undef LOCTEXT_NAMESPACE

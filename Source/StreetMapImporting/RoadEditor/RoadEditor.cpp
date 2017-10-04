// Copyright 2017 Max Seidenstuecker. All Rights Reserved.

#include "StreetMapImporting.h"
#include "RoadEditor.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/InputChord.h"
#include "Framework/Commands/Commands.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "EditorStyleSet.h"
#include "UnrealWidget.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "Components/SplineComponent.h"
#include "ScopedTransaction.h"
#include "ActorEditorUtils.h"



// Hit proxies are a way of collecting data about what was clicked on in the viewport.
// The first argument is the sub type and the second is the base.
IMPLEMENT_HIT_PROXY(HTargetingVisProxy, HComponentVisProxy)
IMPLEMENT_HIT_PROXY(HTargetProxy, HTargetingVisProxy)

// disabled because of warning C4005: "LOCTEXT_NAMESPACE": Makro redefinition (in Elevation.cpp)
//#define LOCTEXT_NAMESPACE "StreetMapComponentVisualizer"


// Generate a Context Menu
// generate a context menu if one of our hit proxies is right clicked.
// This is a bit more complicated but lets make it so we can right click on 
// a target and selected 'duplicate' to create a new target in the same spot.
class FStreetMapVisualizerCommands : public TCommands < FStreetMapVisualizerCommands >
{
public:
	FStreetMapVisualizerCommands() : TCommands <FStreetMapVisualizerCommands>
	(
		"StreetMapComponentVisualizer",
		LOCTEXT("StreetMapComponentVisualizer", "StreetMap Component Visualizer"),
		NAME_None,
		FEditorStyle::GetStyleSetName()
	)
	{
	}

	virtual void RegisterCommands() override
	{
		UI_COMMAND(DeleteKey, "Delete Road Element", "Delete the currently selected road.", EUserInterfaceActionType::Button, FInputChord(EKeys::Delete));
	}

public:
	/** Delete key */
	TSharedPtr<FUICommandInfo> DeleteKey;

};

FStreetMapComponentVisualizer::FStreetMapComponentVisualizer()
	: FComponentVisualizer()
	, SelectedRoadIndex(0 /* should be properly set to, jsut for testing now INDEX_NONE */)
{
	FStreetMapVisualizerCommands::Register();

	StreetMapComponentVisualizerActions = MakeShareable(new FUICommandList);
}

FStreetMapComponentVisualizer::~FStreetMapComponentVisualizer()
{
	FStreetMapVisualizerCommands::Unregister();
}


// We also need to bind our context menu commands to a function that will get called when selected
void FStreetMapComponentVisualizer::OnRegister()
{
	StreetMapComponentVisualizerActions = MakeShareable(new FUICommandList);
	
	const auto& Commands = FStreetMapVisualizerCommands::Get();
	
	StreetMapComponentVisualizerActions->MapAction(
		Commands.DeleteKey,
		FExecuteAction::CreateSP(this, &FStreetMapComponentVisualizer::OnDeleteRoad),
		FCanExecuteAction::CreateSP(this, &FStreetMapComponentVisualizer::CanDeleteRoad));
}

void FStreetMapComponentVisualizer::OnDeleteRoad()
{

}

bool FStreetMapComponentVisualizer::CanDeleteRoad() const
{
	UStreetMapComponent* StreetMapComp = GetEditedStreetMapComponent();
	return (StreetMapComp != nullptr &&
			StreetMapComp->GetStreetMap()->GetRoads()[SelectedRoadIndex].RoadPoints.Num() > 0
			);
}

// Lastly we have to generate the context menu
TSharedPtr<SWidget> FStreetMapComponentVisualizer::GenerateContextMenu() const
{
	FMenuBuilder MenuBuilder(true, StreetMapComponentVisualizerActions);
	{
		MenuBuilder.BeginSection("Target Actions");
		{
			MenuBuilder.AddMenuEntry(FStreetMapVisualizerCommands::Get().DeleteKey);
		}
		MenuBuilder.EndSection();
	}

	TSharedPtr<SWidget> MenuWidget = MenuBuilder.MakeWidget();
	return MenuWidget;
}


UStreetMapComponent* FStreetMapComponentVisualizer::GetEditedStreetMapComponent() const
{
	return Cast<UStreetMapComponent>(GetComponentFromPropertyName(StreetMapCompOwningActor.Get(), StreetMapCompPropName));
}

// Draw data in the viewport
// For this example let's just draw a line from our component to each target and a point where the target is.
void FStreetMapComponentVisualizer::DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	//cast the component into the expected component type
	if (const UStreetMapComponent* StreetMapComponent = Cast<UStreetMapComponent>(Component))
	{
		//get colors for selected and unselected targets
		//This is an editor only uproperty of our targeting component, that way we can change the colors if we can't see them against the background
		const FColor ReadOnlyColor = FColor(255, 0, 255, 255);
		const float GrabHandleSize = 30.0f;

		const FColor SelectedColor = StreetMapComponent->EditorSelectedColor;
		const FColor UnselectedColor = StreetMapComponent->EditorUnselectedColor;

		const FVector Locaction = StreetMapComponent->GetComponentLocation();

		//Iterate over each road, drawing simple HitProxies that can be selected in editor viewport
		auto NumberOfRoadsInfosToDraw = FMath::Min(StreetMapComponent->GetStreetMap()->GetRoads().Num(), 10);
		for (int i = 0; i < NumberOfRoadsInfosToDraw; i++)
		{
			FColor Color = (i == SelectedRoadIndex) ? SelectedColor : UnselectedColor;

			//Set our hit proxy
			PDI->SetHitProxy(new HTargetProxy(Component, i));
			// Just as a test, draw points slightly above the roads
			FVector2D FirstRoadPoint = StreetMapComponent->GetStreetMap()->GetRoads()[i].RoadPoints[0];
			FVector DrawPosition(FirstRoadPoint, StreetMapComponent->GetComponentLocation().Z + 100.0f);
			PDI->DrawPoint(DrawPosition, Color, GrabHandleSize, SDPG_Foreground);
			PDI->SetHitProxy(NULL);
		}
	}
}

// Receiving Clicks from viewport
bool FStreetMapComponentVisualizer::VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click)
{
	bool bEditing = false;

	if (VisProxy && VisProxy->Component.IsValid())
	{
		const UStreetMapComponent* StreetMapComp = CastChecked<const UStreetMapComponent>(VisProxy->Component.Get());

		StreetMapCompPropName = GetComponentPropertyName(StreetMapComp);
		if (StreetMapCompPropName.IsValid())
		{
			AActor* OldStreetMapCompOwningActor = StreetMapCompOwningActor.Get();
			StreetMapCompOwningActor = StreetMapComp->GetOwner();
			
			// TODO ...
		}
		bEditing = true;

		if (VisProxy->IsA(HTargetProxy::StaticGetType()))
		{
			HTargetProxy* Proxy = (HTargetProxy*)VisProxy;

			SelectedRoadIndex = Proxy->RoadIndex;
		}
	}
	else
	{
		SelectedRoadIndex = INDEX_NONE;
	}

	return bEditing;
}

// move our targets with a translation widget
bool FStreetMapComponentVisualizer::GetWidgetLocation(const FEditorViewportClient* ViewportClient, FVector& OutLocaction) const
{
	if (GetEditedStreetMapComponent()->IsValidLowLevel() && SelectedRoadIndex != INDEX_NONE)
	{
		UStreetMap* StreetMap = GetEditedStreetMapComponent()->GetStreetMap();
		FVector2D FirstRoadNode = StreetMap->GetRoads()[SelectedRoadIndex].RoadPoints[0];
		OutLocaction = FVector(FirstRoadNode, 0.0f);

		return true;
	}

	return false;
}

bool FStreetMapComponentVisualizer::GetCustomInputCoordinateSystem(const FEditorViewportClient* ViewportClient, FMatrix& OutMatrix) const
{
	if (ViewportClient->GetWidgetCoordSystemSpace() == COORD_Local || ViewportClient->GetWidgetMode() == FWidget::WM_Rotate)
	{
		UStreetMapComponent* StreetMapComp = GetEditedStreetMapComponent();
		if (StreetMapComp != nullptr)
		{
			OutMatrix = FRotationMatrix::Make(CachedRotation);
			return true;
		}
	}

	return false;
}

// This function gives us a delta translation, scale and rotation that we can use
bool FStreetMapComponentVisualizer::HandleInputDelta(FEditorViewportClient* ViewportClient,
													 FViewport* Viewport, FVector& DeltaTranslate, FRotator& DeltaRotate, FVector& DeltaScale)
{
	bool bHandled = false;

	if (GetEditedStreetMapComponent()->IsValidLowLevel() && SelectedRoadIndex != INDEX_NONE)
	{
		UStreetMap* StreetMap = GetEditedStreetMapComponent()->GetStreetMap();
		FVector2D FirstRoadNode = StreetMap->GetRoads()[SelectedRoadIndex].RoadPoints[0];
		FVector2D DeltaTranslate2D(DeltaTranslate.X, DeltaTranslate.Y);
		
		FirstRoadNode += DeltaTranslate2D;
		bHandled = true;
	}

	return bHandled;
}

// Receiving Key Input
// For example we can make it so that the delete key deletes the selected target with some callback function in our component
bool FStreetMapComponentVisualizer::HandleInputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event)
{
	bool bHandled = false;

	if (Key == EKeys::Delete)
	{
		if (GetEditedStreetMapComponent()->IsValidLowLevel() && SelectedRoadIndex != INDEX_NONE)
		{
			UStreetMap* StreetMap = GetEditedStreetMapComponent()->GetStreetMap();
			StreetMap->GetRoads()[SelectedRoadIndex].RoadPoints.RemoveAt(0);
			bHandled = true;
		}
	}
	return bHandled;
}

void FStreetMapComponentVisualizer::EndEditing()
{
	StreetMapCompOwningActor = NULL;
	StreetMapCompPropName.Clear();
}


//#undef LOCTEXT_NAMESPACE

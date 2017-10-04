#pragma once

// Copy and paste to get it to compile
// Tutorial: https://wiki.unrealengine.com/Component_Visualizers
// Other source: SplineMeshComponentVisualizer

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "HitProxies.h"
#include "ComponentVisualizer.h"
#include "StreetMapComponent.h"

// forward delcaration
class AActor;
class FEditorViewportClient;
class FPrimitiveDrawInterface;
class FSceneView;
class FUICommandList;
class FViewport;
class SWidget;
class UStreetMapComponent;
struct FViewportClick;


class FStreetMapComponentVisualizer : public FComponentVisualizer
{
public:
	FStreetMapComponentVisualizer();
	virtual ~FStreetMapComponentVisualizer();

	// Begin FComponentVisualizer interface
	virtual void OnRegister() override;
	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
	virtual bool VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click) override;
	virtual void EndEditing() override;
	virtual bool GetWidgetLocation(const FEditorViewportClient* ViewportClient, FVector& OutLocation) const override;
	virtual bool GetCustomInputCoordinateSystem(const FEditorViewportClient* ViewportClient, FMatrix& OutMatrix) const override;
	virtual bool HandleInputDelta(FEditorViewportClient* ViewportClient, FViewport* Viewport, FVector& DeltaTranslate, FRotator& DeltaRotate, FVector& DeltaScale) override;
	virtual bool HandleInputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event) override;
	virtual TSharedPtr<SWidget> GenerateContextMenu() const override;
	// End FComponentVisualizer interface

	/** Get the target component we are currently editing */
	UStreetMapComponent* GetEditedStreetMapComponent() const;

protected:
	void OnDeleteRoad();
	bool CanDeleteRoad() const;

	/** Actor that owns the currently edited StreetMapComponent */
	TWeakObjectPtr<AActor> StreetMapCompOwningActor;

	/** Name of property on the actor that references the StreetMapComp we are editing */
	FPropertyNameAndIndex StreetMapCompPropName;

	/** Cached rotation for this road */
	FQuat CachedRotation;

	/**Index of road in selected component */
	int32 CurrentlySelectedRoadId;

	/**Output log commands*/
	TSharedPtr<FUICommandList> StreetMapComponentVisualizerActions;
private:
	int32 SelectedRoadIndex;
};

/**Base class for clickable targeting editing proxies*/
struct HTargetingVisProxy : public HComponentVisProxy
{
	DECLARE_HIT_PROXY();

	HTargetingVisProxy(const UActorComponent* InComponent)
		: HComponentVisProxy(InComponent, HPP_Wireframe)
	{}
};

/**Proxy for target*/
struct HTargetProxy : public HTargetingVisProxy
{
	DECLARE_HIT_PROXY();

	HTargetProxy(const UActorComponent* InComponent, int32 InRoadIndex)
		: HTargetingVisProxy(InComponent)
		, RoadIndex(InRoadIndex)
	{}

	int32 RoadIndex;
};
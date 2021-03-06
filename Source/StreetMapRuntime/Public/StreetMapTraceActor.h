// Copyright 2017 Mike Fricker. All Rights Reserved.

#pragma once

#include "ProceduralMeshComponent.h"
#include "StreetMap.h"
#include "StreetMapTraceActor.generated.h"

/** An actor that renders a street map mesh component */
UCLASS(hidecategories = (Physics)) // Physics category in detail panel is hidden. Our component/Actor is not simulated !
class STREETMAPRUNTIME_API AStreetMapTraceActor : public AActor
{
	GENERATED_UCLASS_BODY()

	const int MAX_MESH_INDEX = 10000;
	
	TMap<FGuid, TArray<int>> mTraces;
	int mMeshIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StreetMap")
		TArray<UProceduralMeshComponent*> MeshComponents;

	UPROPERTY(BlueprintReadOnly, Category = "StreetMap")
		UProceduralMeshComponent* HighlightMeshComponent;

	UPROPERTY(EditAnywhere, Category = "StreetMap")
		UMaterialInterface* HighlightMaterial;

	UPROPERTY(EditAnywhere, Category = "StreetMap")
		UMaterialInterface* TraceMaterial;

	void PostLoad() override;

	void PostActorCreated() override;

public: 
	FORCEINLINE class TArray<UProceduralMeshComponent*> GetMeshComponents() { return MeshComponents; }

	FORCEINLINE class UProceduralMeshComponent* GetHighlightMeshComponent() { return HighlightMeshComponent; }

	FORCEINLINE class UMaterialInterface* GetHighlightMaterial() { return HighlightMaterial; }
	FORCEINLINE class UMaterialInterface* GetTraceMaterial() { return TraceMaterial; }
	
	UFUNCTION(BlueprintCallable, Category = "StreetMap")
		bool AddHighlight(
			const FStreetMapRoad& Road,
			const float Z,
			const float Thickness,
			const FLinearColor Color,
			const float SpeedRatio,
			const bool Smooth
		);

	UFUNCTION(BlueprintCallable, Category = "StreetMap")
		void DeleteHighlight();

	UFUNCTION(BlueprintCallable, Category = "StreetMap")
		FGuid AddTrace(
			const TArray<FStreetMapRoad>& Roads,
			const float Z,
			const float Thickness,
			const FLinearColor Color,
			const float SpeedRatio,
			const bool Smooth
		);

	UFUNCTION(BlueprintCallable, Category = "StreetMap")
		void DeleteTrace(FGuid Guid);

	UFUNCTION(BlueprintCallable, Category = "StreetMap")
		void ShowTrace(FGuid Guid);

	UFUNCTION(BlueprintCallable, Category = "StreetMap")
		void HideTrace(FGuid Guid);

	void AddThick2DLine(
		const FVector2D Start,
		const FVector2D End,
		const float Z,
		TArray<FVector>* Vertices,
		TArray<int32>* Indices,
		TArray<FVector2D>* UV0,
		TArray<FVector2D>* UV1,
		TArray<FVector2D>* UV2,
		TArray<FVector2D>* UV3,
		TArray<FVector>* Normals,
		TArray<FProcMeshTangent>* Tangents,
		const float Thickness,
		const float SpeedRatio,
		const bool IsForward,
		const float RoadTypeFloat
	);

	void StartSmoothQuadList(
		const FVector2D& Start,
		const FVector2D& Mid,
		const float Z,
		TArray<FVector>* Vertices,
		TArray<int32>* Indices,
		TArray<FVector2D>* UV0,
		TArray<FVector2D>* UV1,
		TArray<FVector2D>* UV2,
		TArray<FVector2D>* UV3,
		TArray<FVector>* Normals,
		TArray<FProcMeshTangent>* Tangents,
		const float Thickness,
		float& VAccumulation,
		const float SpeedRatio,
		const bool IsForward,
		const float RoadTypeFloat
	);

	void CheckRoadSmoothQuadList(
		const FVector2D& Road,
		const bool IsStart,
		const float Z,
		TArray<FVector>* Vertices,
		TArray<int32>* Indices,
		TArray<FVector2D>* UV0,
		TArray<FVector2D>* UV1,
		TArray<FVector2D>* UV2,
		TArray<FVector2D>* UV3,
		TArray<FVector>* Normals,
		TArray<FProcMeshTangent>* Tangents,
		const float Thickness,
		float& VAccumulation,
		const float SpeedRatio,
		const bool IsForward,
		const float RoadTypeFloat
	);

	void AddSmoothQuad(
		const FVector2D& Start,
		const FVector2D& Mid,
		const FVector2D& End,
		const float Z,
		TArray<FVector>* Vertices,
		TArray<int32>* Indices,
		TArray<FVector2D>* UV0,
		TArray<FVector2D>* UV1,
		TArray<FVector2D>* UV2,
		TArray<FVector2D>* UV3,
		TArray<FVector>* Normals,
		TArray<FProcMeshTangent>* Tangents,
		const float Thickness,
		float& VAccumulation,
		const float SpeedRatio,
		const bool IsForward,
		const float RoadTypeFloat
	);

	void EndSmoothQuadList(
		const FVector2D& Mid,
		const FVector2D& End,
		const float Z,
		TArray<FVector>* Vertices,
		TArray<int32>* Indices,
		TArray<FVector2D>* UV0,
		TArray<FVector2D>* UV1,
		TArray<FVector2D>* UV2,
		TArray<FVector2D>* UV3,
		TArray<FVector>* Normals,
		TArray<FProcMeshTangent>* Tangents,
		const float Thickness,
		float& VAccumulation,
		const float SpeedRatio,
		const bool IsForward,
		const float RoadTypeFloat
	);

	bool DrawHighlightRoad(
		const TArray<FVector2D>& RoadPoints,
		const FString Direction,
		const float RoadTypeFloat,
		const float Z,
		const float Thickness,
		const FLinearColor Color,
		const float SpeedRatio,
		const bool Smooth
	);

	int DrawTraceRoad(
		const TArray<FVector2D>& RoadPoints,
		const FString Direction,
		const float RoadTypeFloat,
		const float Z,
		const float Thickness,
		const FLinearColor Color,
		const float SpeedRatio,
		const bool Smooth
	);

	void GenerateMesh(
		TArray<FVector>& Vertices,
		TArray<int32>& Indices,
		TArray<FVector>& Normals,
		TArray<FVector2D>& UV0,
		TArray<FVector2D>& UV1,
		TArray<FVector2D>& UV2,
		TArray<FVector2D>& UV3,
		TArray<FProcMeshTangent>& Tangents,
		TArray<FLinearColor>& VertexColors,
		const TArray<FVector2D>& RoadPoints,
		const FString Direction,
		const float RoadTypeFloat,
		const float Z,
		const float Thickness,
		const FLinearColor Color,
		const float SpeedRatio,
		const bool Smooth
	);
};

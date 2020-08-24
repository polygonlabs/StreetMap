// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UObject/ConstructorHelpers.h"
#include "Engine/Polys.h"
#include "StreetMapTraceActor.h"

AStreetMapTraceActor::AStreetMapTraceActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{	
	if (HighlightMaterial == nullptr) {
		static ConstructorHelpers::FObjectFinder<UMaterialInterface> HighlightMaterialAsset(TEXT("/StreetMap/StreetMapHighlightMaterial"));
		HighlightMaterial = HighlightMaterialAsset.Object;
	}

	if (TraceMaterial == nullptr) {
		static ConstructorHelpers::FObjectFinder<UMaterialInterface> TraceMaterialAsset(TEXT("/StreetMap/StreetMapTraceMaterial"));
		TraceMaterial = TraceMaterialAsset.Object;
	}

	HighlightMeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>(*FString("StreetMapHighlight"));
	HighlightMeshComponent->bUseAsyncCooking = true;
	HighlightMeshComponent->SetMaterial(0, HighlightMaterial);

	for (int i = 0; i < 1000; i++)
	{
		UProceduralMeshComponent* MeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>(*(FString("StreetMapTrace") + FString::FromInt(i)));
		MeshComponent->bUseAsyncCooking = true;
		MeshComponent->SetMaterial(0, TraceMaterial);
		MeshComponents.Add(MeshComponent);
	}

	RootComponent = HighlightMeshComponent;
}

// This is called when actor is spawned (at runtime or when you drop it into the world in editor)
void AStreetMapTraceActor::PostActorCreated()
{
	Super::PostActorCreated();
}

// This is called when actor is already in level and map is opened
void AStreetMapTraceActor::PostLoad()
{
	Super::PostLoad();
}

FGuid AStreetMapTraceActor::AddTrace(
	const TArray<FStreetMapRoad>& Roads, 
	const float Z,
	const float Thickness,
	const FLinearColor Color,
	const float SpeedRatio,
	const bool Smooth
)
{
	FGuid TraceGuid = FGuid::NewGuid();
	TArray<int> MeshIndices;

	for (auto& Road : Roads)
	{
		int MeshIndex = DrawTraceRoad(
			Road.RoadPoints,
			Road.Link.LinkDir,
			static_cast<float>(Road.RoadType),
			Z,
			Thickness,
			Color,
			SpeedRatio,
			Smooth
		);
		if (MeshIndex >= 0) {
			MeshIndices.Add(MeshIndex);
		}

		mMeshIndex++;
	}

	mTraces.Add(TraceGuid, MeshIndices);

	return TraceGuid;
}

void AStreetMapTraceActor::ShowTrace(FGuid Guid)
{
	if (mTraces.Contains(Guid))
	{
		auto MeshIndices = mTraces[Guid];
		for (auto& MeshIndex : MeshIndices)
		{
			if (MeshIndex < MeshComponents.Num())
			{
				auto MeshComponent = MeshComponents[MeshIndex];
				MeshComponent->SetVisibility(true);
			}
		}
	}
}

void AStreetMapTraceActor::HideTrace(FGuid Guid)
{
	if (mTraces.Contains(Guid))
	{
		auto MeshIndices = mTraces[Guid];
		for (auto& MeshIndex : MeshIndices)
		{
			if (MeshIndex < MeshComponents.Num())
			{
				auto MeshComponent = MeshComponents[MeshIndex];
				MeshComponent->SetVisibility(false);
			}
		}
	}
}

void AStreetMapTraceActor::DeleteTrace(FGuid Guid)
{
	if (mTraces.Contains(Guid))
	{
		auto MeshIndices = mTraces[Guid];
		for (auto& MeshIndex : MeshIndices)
		{
			if (MeshIndex < MeshComponents.Num())
			{
				auto MeshComponent = MeshComponents[MeshIndex];
				MeshComponent->ClearAllMeshSections();
			}
		}
		mTraces.Remove(Guid);
	}
}

int AStreetMapTraceActor::DrawTraceRoad(
	const TArray<FVector2D>& RoadPoints, 
	const FString Direction, 
	const float RoadTypeFloat, 
	const float Z, 
	const float Thickness, 
	const FLinearColor Color, 
	const float SpeedRatio, 
	const bool Smooth = true
)
{
	if (RoadPoints.Num() < 2) return -1;

	TArray<FVector> Vertices;
	TArray<int32> Indices;
	TArray<FVector> Normals;
	TArray<FVector2D> UV0;
	TArray<FVector2D> UV1;
	TArray<FVector2D> UV2;
	TArray<FVector2D> UV3;
	TArray<FProcMeshTangent> Tangents;
	TArray<FLinearColor> VertexColors;
	
	GenerateMesh(
		Vertices,
		Indices,
		Normals,
		UV0,
		UV1,
		UV2,
		UV3,
		Tangents,
		VertexColors,
		RoadPoints,
		Direction,
		RoadTypeFloat,
		Z,
		Thickness,
		Color,
		SpeedRatio,
		Smooth
	);

	MeshComponents[mMeshIndex % 1000]->CreateMeshSection_LinearColor(0, Vertices, Indices, Normals, UV0, UV1, UV2, UV3, VertexColors, Tangents, true);

	return mMeshIndex;
}

void AStreetMapTraceActor::GenerateMesh(
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
	const bool Smooth = true
)
{
	const bool IsForward = Direction.Compare(TEXT("T"), ESearchCase::IgnoreCase) == 0;
	float VAccumulation = 0.f;

	if (Smooth)
	{
		StartSmoothQuadList(
			RoadPoints[0],
			RoadPoints[1],
			Z,
			&Vertices,
			&Indices,
			&UV0,
			&UV1,
			&UV2,
			&UV3,
			&Normals,
			&Tangents,
			Thickness,
			VAccumulation,
			SpeedRatio,
			IsForward,
			RoadTypeFloat
		);

		int32 PointIndex = 0;
		for (; PointIndex < RoadPoints.Num() - 2; ++PointIndex) {
			AddSmoothQuad(
				RoadPoints[PointIndex],
				RoadPoints[PointIndex + 1],
				RoadPoints[PointIndex + 2],
				Z,
				&Vertices,
				&Indices,
				&UV0,
				&UV1,
				&UV2,
				&UV3,
				&Normals,
				&Tangents,
				Thickness,
				VAccumulation,
				SpeedRatio,
				IsForward,
				RoadTypeFloat
			);
		}

		EndSmoothQuadList(
			RoadPoints[PointIndex],
			RoadPoints[PointIndex + 1],
			Z,
			&Vertices,
			&Indices,
			&UV0,
			&UV1,
			&UV2,
			&UV3,
			&Normals,
			&Tangents,
			Thickness,
			VAccumulation,
			SpeedRatio,
			IsForward,
			RoadTypeFloat
		);
	}
	else
	{
		for (int32 PointIndex = 0; PointIndex < RoadPoints.Num() - 1; ++PointIndex)
		{
			AddThick2DLine(
				RoadPoints[PointIndex],
				RoadPoints[PointIndex + 1],
				Z,
				&Vertices,
				&Indices,
				&UV0,
				&UV1,
				&UV2,
				&UV3,
				&Normals,
				&Tangents,
				Thickness,
				SpeedRatio,
				IsForward,
				RoadTypeFloat
			);
		}
	}

	for (int i = 0; i < Vertices.Num(); i++) {
		VertexColors.Add(Color);
	}
}

void AStreetMapTraceActor::AddThick2DLine(
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
)
{
	const float Distance = (End - Start).Size();
	const float XRatio = Distance / Thickness;
	const FVector2D LineDirection = (End - Start).GetSafeNormal();
	const FVector2D RightVector(-LineDirection.Y, LineDirection.X);	
	const float HalfThickness = Thickness * 0.5f;
	
	FVector BottomLeftVertex;
	FVector BottomRightVertex;
	FVector TopRightVertex;
	FVector TopLeftVertex;

	FVector2D BottomLeftVertexTexCoord0;
	FVector2D BottomLeftVertexTexCoord1 = FVector2D(SpeedRatio * 100, 0);
	FVector2D BottomLeftVertexTexCoord2 = FVector2D(-RightVector.X, -RightVector.Y); // direction
	FVector2D BottomLeftVertexTexCoord3 = FVector2D(RoadTypeFloat, 0.f); // road type

	FVector2D BottomRightVertexTexCoord0;
	FVector2D BottomRightVertexTexCoord1 = FVector2D(SpeedRatio * 100, 0);
	FVector2D BottomRightVertexTexCoord2 = FVector2D(RightVector.X, RightVector.Y); // direction
	FVector2D BottomRightVertexTexCoord3 = FVector2D(RoadTypeFloat, 0.f); // road type

	FVector2D TopRightVertexTexCoord0;
	FVector2D TopRightVertexTexCoord1 = FVector2D(SpeedRatio * 100, 0);
	FVector2D TopRightVertexTexCoord2 = FVector2D(RightVector.X, RightVector.Y); // direction
	FVector2D TopRightVertexTexCoord3 = FVector2D(RoadTypeFloat, 0.f); // road type

	FVector2D TopLeftVertexTexCoord0;
	FVector2D TopLeftVertexTexCoord1 = FVector2D(SpeedRatio * 100, 0);
	FVector2D TopLeftVertexTexCoord2 = FVector2D(-RightVector.X, -RightVector.Y); // direction
	FVector2D TopLeftVertexTexCoord3 = FVector2D(RoadTypeFloat, 0.f); // road type

	if (IsForward)
	{		
		BottomLeftVertex = FVector(Start, Z);
		BottomRightVertex = FVector(Start + RightVector * HalfThickness, Z);
		TopRightVertex = FVector(End + RightVector * HalfThickness, Z);
		TopLeftVertex = FVector(End, Z);
		
		BottomLeftVertexTexCoord0 = FVector2D(0.0f, 0.0f);
		BottomRightVertexTexCoord0 = FVector2D(0.5f, 0.0f);
		TopRightVertexTexCoord0 = FVector2D(0.5f, XRatio);
		TopLeftVertexTexCoord0 = FVector2D(0.0f, XRatio);

	}
	else
	{
		BottomLeftVertex = FVector(Start - RightVector * HalfThickness, Z);
		BottomRightVertex = FVector(Start, Z);
		TopRightVertex = FVector(End, Z);
		TopLeftVertex = FVector(End - RightVector * HalfThickness, Z);
		
		BottomLeftVertexTexCoord0 = FVector2D(0.0f, XRatio);
		BottomRightVertexTexCoord0 = FVector2D(0.5f, XRatio);
		TopRightVertexTexCoord0 = FVector2D(0.5f, 0.0f);
		TopLeftVertexTexCoord0 = FVector2D(0.0f, 0.0f);
	}

	const int32 BottomLeftVertexIndex = Vertices->Num();
	Vertices->Add(BottomLeftVertex);
	UV0->Add(BottomLeftVertexTexCoord0);
	UV1->Add(BottomLeftVertexTexCoord1);
	UV2->Add(BottomLeftVertexTexCoord2);
	UV3->Add(BottomLeftVertexTexCoord3);

	const int32 BottomRightVertexIndex = Vertices->Num();
	Vertices->Add(BottomRightVertex);
	UV0->Add(BottomRightVertexTexCoord0);
	UV1->Add(BottomRightVertexTexCoord1);
	UV2->Add(BottomRightVertexTexCoord2);
	UV3->Add(BottomRightVertexTexCoord3);

	const int32 TopRightVertexIndex = Vertices->Num();
	Vertices->Add(TopRightVertex);
	UV0->Add(TopRightVertexTexCoord0);
	UV1->Add(TopRightVertexTexCoord1);
	UV2->Add(TopRightVertexTexCoord2);
	UV3->Add(TopRightVertexTexCoord3);

	const int32 TopLeftVertexIndex = Vertices->Num();
	Vertices->Add(TopLeftVertex);
	UV0->Add(TopLeftVertexTexCoord0);
	UV1->Add(TopLeftVertexTexCoord1);
	UV2->Add(TopLeftVertexTexCoord2);
	UV3->Add(TopLeftVertexTexCoord3);

	Indices->Add(BottomLeftVertexIndex);
	Indices->Add(BottomRightVertexIndex);
	Indices->Add(TopRightVertexIndex);
		   
	Indices->Add(BottomLeftVertexIndex);
	Indices->Add(TopRightVertexIndex);
	Indices->Add(TopLeftVertexIndex);
};

void AStreetMapTraceActor::StartSmoothQuadList(
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
) {
	const float QuarterThickness = Thickness * .25f;
	const float Distance = (Mid - Start).Size();
	const float XRatio = VAccumulation;

	const FVector2D LineDirection1 = (Mid - Start).GetSafeNormal();
	const FVector2D RightVector(-LineDirection1.Y, LineDirection1.X);

	const FVector BottomLeftVertex = FVector(Start, Z);
	const int32 BottomLeftVertexIndex = Vertices->Num();
	FVector2D BottomLeftVertexTexCoord0;
	FVector2D BottomLeftVertexTexCoord1;
	if (IsForward)
	{
		BottomLeftVertexTexCoord0 = FVector2D(0.0f, XRatio); // scale
		BottomLeftVertexTexCoord1 = FVector2D(SpeedRatio * 100, -1.f); // texture animation speed
	}
	else
	{
		BottomLeftVertexTexCoord0 = FVector2D(0.0f, -XRatio); // scale
		BottomLeftVertexTexCoord1 = FVector2D(SpeedRatio * 100, 1.f); // texture animation speed
	}
	FVector2D BottomLeftVertexTexCoord2 = FVector2D(-RightVector.X, -RightVector.Y); // direction
	FVector2D BottomLeftVertexTexCoord3 = FVector2D(RoadTypeFloat, 0.f); // road type
	FProcMeshTangent BottomLeftVertexTangent = FProcMeshTangent(LineDirection1.X, LineDirection1.Y, 0.0f);
		
	Vertices->Add(BottomLeftVertex);
	UV0->Add(BottomLeftVertexTexCoord0);
	UV1->Add(BottomLeftVertexTexCoord1);
	UV2->Add(BottomLeftVertexTexCoord2);
	UV3->Add(BottomLeftVertexTexCoord3);
	Tangents->Add(BottomLeftVertexTangent);
	Normals->Add(FVector::UpVector);

	
	const FVector BottomRightVertex = FVector(Start, Z);
	const int32 BottomRightVertexIndex = Vertices->Num();
	FVector2D BottomRightVertexTexCoord0;
	FVector2D BottomRightVertexTexCoord1;
	if (IsForward)
	{
		BottomRightVertexTexCoord0 = FVector2D(1.0f, XRatio); // scale
		BottomRightVertexTexCoord1 = FVector2D(SpeedRatio * 100, 1.f); // texture animation speed
	}
	else
	{
		BottomRightVertexTexCoord0 = FVector2D(1.0f, -XRatio); // scale
		BottomRightVertexTexCoord1 = FVector2D(SpeedRatio * 100, -1.f); // texture animation speed
	}
	FVector2D BottomRightVertexTexCoord2 = FVector2D(RightVector.X, RightVector.Y); // direction
	FVector2D BottomRightVertexTexCoord3 = FVector2D(RoadTypeFloat, 0.f); // road type
	FProcMeshTangent BottomRightVertexTangent = FProcMeshTangent(LineDirection1.X, LineDirection1.Y, 0.0f);
		
	Vertices->Add(BottomRightVertex);
	UV0->Add(BottomRightVertexTexCoord0);
	UV1->Add(BottomRightVertexTexCoord1);
	UV2->Add(BottomRightVertexTexCoord2);
	UV3->Add(BottomRightVertexTexCoord3);
	Tangents->Add(BottomRightVertexTangent);
	Normals->Add(FVector::UpVector);

	Indices->Add(BottomLeftVertexIndex);
	Indices->Add(BottomRightVertexIndex);
};

void AStreetMapTraceActor::CheckRoadSmoothQuadList(
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
) {

}

void AStreetMapTraceActor::AddSmoothQuad(
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
) {
	const float HalfThickness = Thickness * 0.5f;
	const float QuarterThickness = Thickness * 0.25f;
	const float Distance = (Mid - Start).Size();
	const float XRatio = (Distance / Thickness);

	const FVector2D LineDirection1 = (Mid - Start).GetSafeNormal();
	const FVector2D LineDirection2 = (End - Mid).GetSafeNormal();

	auto alteredLineDirection = LineDirection1 + LineDirection2;
	alteredLineDirection.Normalize();
	const FVector2D RightVector(-alteredLineDirection.Y, alteredLineDirection.X);

	FVector MidLeftVertex = FVector(Mid, Z);
	const int32 MidLeftVertexIndex = Vertices->Num();
	FVector2D MidLeftVertexTexCoord0;
	FVector2D MidLeftVertexTexCoord1;
	if (IsForward)
	{
		MidLeftVertexTexCoord0 = FVector2D(0.0f, VAccumulation + XRatio);
		MidLeftVertexTexCoord1 = FVector2D(SpeedRatio * 100, -1.f);
	}
	else
	{
		MidLeftVertexTexCoord0 = FVector2D(0.0f, -VAccumulation - XRatio);
		MidLeftVertexTexCoord1 = FVector2D(SpeedRatio * 100, 1.f);
	}
	FVector2D MidLeftVertexTexCoord2 = FVector2D(-RightVector.X, -RightVector.Y);
	FVector2D MidLeftVertexTexCoord3 = FVector2D(RoadTypeFloat, 0.f);
	FProcMeshTangent MidLeftVertexTangent = FProcMeshTangent(alteredLineDirection.X, alteredLineDirection.Y, 0.0f);
	
	Vertices->Add(MidLeftVertex);
	UV0->Add(MidLeftVertexTexCoord0);
	UV1->Add(MidLeftVertexTexCoord1);
	UV2->Add(MidLeftVertexTexCoord2);
	UV3->Add(MidLeftVertexTexCoord3);
	Tangents->Add(MidLeftVertexTangent);
	Normals->Add(FVector::UpVector);


	FVector MidRightVertex = FVector(Mid, Z);
	const int32 MidRightVertexIndex = Vertices->Num();
	FVector2D MidRightVertexTexCoord0;
	FVector2D MidRightVertexTexCoord1;
	if (IsForward)
	{
		MidRightVertexTexCoord0 = FVector2D(1.0f, VAccumulation + XRatio);
		MidRightVertexTexCoord1 = FVector2D(SpeedRatio * 100, 1.f);
	}
	else
	{
		MidRightVertexTexCoord0 = FVector2D(1.0f, -VAccumulation - XRatio);
		MidRightVertexTexCoord1 = FVector2D(SpeedRatio * 100, -1.f);
	}
	FVector2D MidRightVertexTexCoord2 = FVector2D(RightVector.X, RightVector.Y);
	FVector2D MidRightVertexTexCoord3 = FVector2D(RoadTypeFloat, 0.f);
	FProcMeshTangent MidRightVertexTangent = FProcMeshTangent(alteredLineDirection.X, alteredLineDirection.Y, 0.0f);

	Vertices->Add(MidRightVertex);
	UV0->Add(MidRightVertexTexCoord0);
	UV1->Add(MidRightVertexTexCoord1);
	UV2->Add(MidRightVertexTexCoord2);
	UV3->Add(MidRightVertexTexCoord3);
	Tangents->Add(MidRightVertexTangent);
	Normals->Add(FVector::UpVector);


	auto numIdx = Indices->Num();
	auto BottomRightVertexIndex = (*Indices)[numIdx - 1];
	auto BottomLeftVertexIndex = (*Indices)[numIdx - 2];

	// Finish Last trinagle
	Indices->Add(MidRightVertexIndex);

	Indices->Add(BottomLeftVertexIndex);
	Indices->Add(MidRightVertexIndex);
	Indices->Add(MidLeftVertexIndex);

	// Start of next triangle
	Indices->Add(MidLeftVertexIndex);
	Indices->Add(MidRightVertexIndex);
};

void AStreetMapTraceActor::EndSmoothQuadList(
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
) {
	const float HalfThickness = Thickness * 0.5f;
	const float QuarterThickness = HalfThickness * .5f;
	const float Distance = (End - Mid).Size();
	const float XRatio = (Distance / Thickness) + VAccumulation;
	VAccumulation = XRatio;

	const FVector2D LineDirection1 = (End - Mid).GetSafeNormal();
	const FVector2D RightVector(-LineDirection1.Y, LineDirection1.X);

	FVector TopLeftVertex = FVector(End, Z);
	const int32 TopLeftVertexIndex = Vertices->Num();
	FVector2D TopLeftVertexTexCoord0;
	FVector2D TopLeftVertexTexCoord1;
	if (IsForward)
	{
		TopLeftVertexTexCoord0 = FVector2D(0.0f, XRatio);
		TopLeftVertexTexCoord1 = FVector2D(SpeedRatio * 100, -1.f);
	}
	else
	{
		TopLeftVertexTexCoord0 = FVector2D(0.0f, -XRatio);
		TopLeftVertexTexCoord1 = FVector2D(SpeedRatio * 100, 1.f);
	}
	FVector2D TopLeftVertexTexCoord2 = FVector2D(-RightVector.X, -RightVector.Y);
	FVector2D TopLeftVertexTexCoord3 = FVector2D(RoadTypeFloat, 0.f);
	FProcMeshTangent TopLeftVertexTangent = FProcMeshTangent(LineDirection1.X, LineDirection1.Y, 0.0f);
	
	Vertices->Add(TopLeftVertex);
	UV0->Add(TopLeftVertexTexCoord0);
	UV1->Add(TopLeftVertexTexCoord1);
	UV2->Add(TopLeftVertexTexCoord2);
	UV3->Add(TopLeftVertexTexCoord3);
	Tangents->Add(TopLeftVertexTangent);
	Normals->Add(FVector::UpVector);


	FVector TopRightVertex = FVector(End, Z);
	const int32 TopRightVertexIndex = Vertices->Num();
	FVector2D TopRightVertexTexCoord0;
	FVector2D TopRightVertexTexCoord1;
	if (IsForward)
	{
		TopRightVertexTexCoord0 = FVector2D(1.0f, XRatio);
		TopRightVertexTexCoord1 = FVector2D(SpeedRatio * 100, 1.f);
	}
	else
	{
		TopRightVertexTexCoord0 = FVector2D(1.0f, -XRatio);
		TopRightVertexTexCoord1 = FVector2D(SpeedRatio * 100, -1.f);
	}
	FVector2D TopRightVertexTexCoord2 = FVector2D(RightVector.X, RightVector.Y);
	FVector2D TopRightVertexTexCoord3 = FVector2D(RoadTypeFloat, 0.f);
	FProcMeshTangent TopRightVertexTangent = FProcMeshTangent(LineDirection1.X, LineDirection1.Y, 0.0f);
	
	Vertices->Add(TopRightVertex);
	UV0->Add(TopRightVertexTexCoord0);
	UV1->Add(TopRightVertexTexCoord1);
	UV2->Add(TopRightVertexTexCoord2);
	UV3->Add(TopRightVertexTexCoord3);
	Tangents->Add(TopRightVertexTangent);
	Normals->Add(FVector::UpVector);


	auto numIdx = Indices->Num();

	auto MidRightVertexIndex = (*Indices)[numIdx - 1];
	auto MidLeftVertexIndex = (*Indices)[numIdx - 2];

	Indices->Add(TopRightVertexIndex);

	Indices->Add(MidLeftVertexIndex);
	Indices->Add(TopRightVertexIndex);
	Indices->Add(TopLeftVertexIndex);
};
#pragma once

#include "CoreMinimal.h"

/**
* Creates some additional information based on the given points that form a closed polygon.
* It relies on the existence of the given vertices even after its creation.
*/
struct FPolygon2DView
{
public:

private:

	struct FEdge
	{
		FVector2D center;
		FVector2D direction;
		float	  extent;
	};

	const TArray<FVector2D>&		Vertices;
	TArray<FEdge>					Edges;

public:

	FPolygon2DView(const TArray<FVector2D>& Vertices)
		: Vertices(Vertices)
	{
		Edges.SetNumUninitialized(Vertices.Num());

		const uint32 numVertices = GetNumVertices();
		const uint32 numEdges = GetNumEdges();
		for (uint32 n = 0; n < numEdges; n++)
		{
			const FVector2D& v0 = Vertices[n];
			const FVector2D& v1 = Vertices[(n < numVertices - 1) ? n + 1 : 0];

			FEdge& edge = Edges[n];
			edge.center = (v0 + v1) * 0.5f;
			(v1 - v0).ToDirectionAndLength(edge.direction, edge.extent);
			edge.extent *= 0.5f;
		}
	}

	float ComputeSquareDistance(const FVector2D& v, bool& OutIsInside, FVector2D* OutClosestPoint = nullptr) const
	{
		OutIsInside = false;
		float minSqrDistance = TNumericLimits<float>::Max();
		FVector2D closestPointLocal;
		FVector2D* pClosestPointLocal = OutClosestPoint ? &closestPointLocal : NULL;
		for (uint32_t n = 0; n < GetNumEdges(); n++)
		{
			float sqrDistance = SquareDistanceToEdge(v, n, OutIsInside, pClosestPointLocal);
			if (sqrDistance < minSqrDistance)
			{
				minSqrDistance = sqrDistance;
				if (OutClosestPoint)
				{
					*OutClosestPoint = closestPointLocal;
				}
			}
		}

		return minSqrDistance;
	}


	uint32 GetNumEdges() const
	{
		return Edges.Num();
	}

	uint32 GetNumVertices() const
	{
		return Vertices.Num();
	}

private:

	double SquareDistanceToEdge(const FVector2D& point, uint32 edgeIndex, bool& CountAsIntersection, FVector2D* OutClosestPoint = nullptr) const
	{
		const uint32 numVertices = GetNumVertices();
		const FEdge& edge = Edges[edgeIndex];

		FVector2D diff = point - edge.center;
		float segmentParameter = FVector2D::DotProduct(edge.direction, diff);
		FVector2D segmentClosestPoint;
		if (-edge.extent < segmentParameter)
		{
			if (segmentParameter < edge.extent)
			{
				segmentClosestPoint = edge.center + edge.direction * segmentParameter;
			}
			else
			{
				// Vertex 1 of Edge
				segmentClosestPoint = Vertices[(edgeIndex < numVertices - 1) ? edgeIndex + 1 : 0];
			}
		}
		else
		{
			// Vertex 0 of Edge
			segmentClosestPoint = Vertices[edgeIndex];
		}

		diff = point - segmentClosestPoint;
		float sqrDistance = FVector2D::DotProduct(diff, diff);

		const FVector2D& v0 = Vertices[edgeIndex];
		const FVector2D& v1 = (edgeIndex < numVertices - 1) ? Vertices[edgeIndex + 1] : Vertices[0];

		if ((v0.Y > point.Y) != (v1.Y > point.Y))
		{
			CountAsIntersection ^= (point.X < ((v1.X - v0.X) * (point.Y - v0.Y) / (v1.Y - v0.Y) + v0.X));
		}

		if (OutClosestPoint)
		{
			*OutClosestPoint = segmentClosestPoint;
		}

		return sqrDistance;
	}
};

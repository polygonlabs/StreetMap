#pragma once

#include "CoreMinimal.h"
#include "PredictiveData.generated.h"

USTRUCT(Blueprintable, BlueprintType, meta = (DisplayName = "Street Map Predictive Data"))
struct STREETMAPRUNTIME_API FPredictiveData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StreetMap")
		float S0;

	UPROPERTY(BlueprintReadOnly, Category = "StreetMap")
		float S15;

	UPROPERTY(BlueprintReadOnly, Category = "StreetMap")
		float S30;

	UPROPERTY(BlueprintReadOnly, Category = "StreetMap")
		float S45;
};
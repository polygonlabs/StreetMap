#pragma once

#include "CoreMinimal.h"
#include "FlowData.generated.h"

UCLASS(Blueprintable, BlueprintType, meta = (DisplayName = "Street Map Flow Data"))
class STREETMAPRUNTIME_API UFlowData : public UObject
{

	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "StreetMap")
		FString RdsTmc;

	UPROPERTY(BlueprintReadOnly, Category = "StreetMap")
		float Speed;

	UPROPERTY(BlueprintReadOnly, Category = "StreetMap")
		uint8 EventCode;
};
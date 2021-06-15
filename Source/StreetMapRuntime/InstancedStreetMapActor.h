#pragma once

#include "CoreMinimal.h"
#include "InstancedStreetMapActor.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UStreetMap;

UCLASS()
class AInstancedStreetMapActor : public AActor
{
	GENERATED_BODY()

public:

	AInstancedStreetMapActor(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere)
	float ScaleFactor;

	/** The street map we're representing. */
	UPROPERTY(EditAnywhere, Category = "StreetMap")
	UStreetMap* StreetMap;

	UPROPERTY(VisibleAnywhere)
	UHierarchicalInstancedStaticMeshComponent* InstancedMeshComponent;

	UFUNCTION(BlueprintCallable)
	void BuildStreetMap();
};
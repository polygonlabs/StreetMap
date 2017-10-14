// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OsmBaseTypes.h"
#include "TrafficSign.generated.h"

/** Types of miscellaneous ways */
UENUM(BlueprintType)
enum ESignType
{
	CityLimit,
	OneWay,
	Stop,
	SpeedLimit,
};


/**
 * 
 */
UCLASS()
class STREETMAPRUNTIME_API ATrafficSign : public AOsmNode
{
	GENERATED_UCLASS_BODY()
	
public:
	ESignType SignType;
	FName SignText;
};

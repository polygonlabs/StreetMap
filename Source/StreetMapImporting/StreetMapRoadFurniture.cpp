#include "StreetMapImporting.h"
#include "StreetMapActor.h"
#include "StreetMapComponent.h"
#include "Elements/TrafficSign.h"
#include "Elements/PowerGeneratorWind.h"
#include "Map.h"

#include "StreetMapSplineTools.h"

#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "StreetMapImporting"

static void BuildStreetMapRoadFurniture(class UStreetMapComponent* StreetMapComponent,
	FStreetMapRoadFurnitureBuildSettings& BuildSettings)
{
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (World)
	{
		// Might have manually deleted actors still in memory, which would result in actor locations twice the distance from origin to be spawned
		// TODO: still not working as anticipated
		GEngine->ForceGarbageCollection(true);

		// Traffic Signs
		if (BuildSettings.BuildTrafficSigns)
		{
			const TArray<FStreetMapSign>& Signs = StreetMapComponent->GetStreetMap()->GetSigns();

			// Find already instanced actors and update their position or spawn new ones
			TArray<AActor*> FoundActors;
			UGameplayStatics::GetAllActorsOfClass(World, ATrafficSign::StaticClass(), FoundActors);

			for (auto Sign : Signs)
			{
				const float WorldElevation = FStreetMapSplineTools::GetLandscapeElevation(BuildSettings.Landscape, Sign.Location);
				bool bFoundInWorld = false;

				for (auto Actor : FoundActors)
				{
					if (Sign.NodeId == Actor->GetName())
					{
						bFoundInWorld = true;
						// found instance in world, so update it
						Actor->SetActorLocation(FVector(Sign.Location, WorldElevation));
						break;
					}
				}
				if (!bFoundInWorld)
				{
					FRotator Rotation(0.0f, 0.0f, 0.0f);
					FActorSpawnParameters SpawnInfo;
					SpawnInfo.Name = FName(*Sign.NodeId);
					auto NewActor = World->SpawnActor<ATrafficSign>(FVector(Sign.Location, WorldElevation), Rotation, SpawnInfo);
					if (NewActor)
					{
						NewActor->SetFolderPath("Nodes/Signs");
						NewActor->SetActorLabel(SpawnInfo.Name.ToString());

						BuildSettings.HasSpawnedTrafficSignsIntoLevel = true;
					}
				}
			}
		}
		

		// Wind Turbines
		if (BuildSettings.BuildWindTurbines)
		{
			const TArray<FStreetMapWindTurbine>& WindTurbines = StreetMapComponent->GetStreetMap()->GetWindTurbines();

			// Find already instanced actors and update their position or spawn new ones
			TArray<AActor*> FoundActors;
			UGameplayStatics::GetAllActorsOfClass(World, APowerGeneratorWind::StaticClass(), FoundActors);

			for (auto WindTurbine : WindTurbines)
			{
				const float WorldElevation = FStreetMapSplineTools::GetLandscapeElevation(BuildSettings.Landscape, WindTurbine.Location);
				bool bFoundInWorld = false;

				for (auto Actor : FoundActors)
				{
					if (WindTurbine.NodeId == Actor->GetName())
					{
						bFoundInWorld = true;
						// found instance in world, so update it
						Actor->SetActorLocation(FVector(WindTurbine.Location, WorldElevation));
						break;
					}
				}
				if (!bFoundInWorld)
				{
					FRotator Rotation(0.0f, 0.0f, 0.0f);
					FActorSpawnParameters SpawnInfo;
					SpawnInfo.Name = FName(*WindTurbine.NodeId);
					auto NewActor = World->SpawnActor<APowerGeneratorWind>(FVector(WindTurbine.Location, WorldElevation), Rotation, SpawnInfo);
					if (NewActor)
					{
						NewActor->SetFolderPath("Nodes/WindTurbines");
						NewActor->SetActorLabel(SpawnInfo.Name.ToString());

						NewActor->Id = FPlatformString::Atoi64(*WindTurbine.NodeId);
						// Copy OSM tags into a TMap for easy editing in editor
						for (auto Tag : WindTurbine.Tags)
						{
							NewActor->OsmTags.Add(Tag.Key, Tag.Value);
						}

						BuildSettings.HasSpawnedWindTurbinesIntoLevel = true;
					}
				}
			}
		}
	}
}


static void SaveStreetMapRoadFurniture(class UStreetMapComponent* StreetMapComponent,
										FStreetMapRoadFurnitureBuildSettings& BuildSettings)
{
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (World)
	{
		FSpatialReferenceSystem SRS(StreetMapComponent->GetStreetMap()->GetOriginLongitude(), StreetMapComponent->GetStreetMap()->GetOriginLatitude());

		// WindTurbines
		TArray<AActor*> FoundActors;
		UGameplayStatics::GetAllActorsOfClass(World, APowerGeneratorWind::StaticClass(), FoundActors);
		
		// Get OSM file
		UOSMFile* OSMFile = StreetMapComponent->GetStreetMap()->OSMFile;
		if (OSMFile)
		{
			// load file as it might be imported in an previous instance of the editor
			if (!OSMFile->LoadOpenStreetMapFile(OSMFile->OSMFileLocation, false, nullptr))
			{
				// TODO: log error
				return;
			}
			FXmlFile& OsmXml = OSMFile->OsmXmlFile;
			TArray<FXmlNode*> XmlNodes = OsmXml.GetRootNode()->GetChildrenNodes();
			
			for (auto FoundActor : FoundActors)
			{
				APowerGeneratorWind* WindTurbine = Cast<APowerGeneratorWind>(FoundActor);
				FXmlNode* WindTurbineXmlNode = nullptr;
				bool bFoundXmlNode = false; 
				
				if (WindTurbine == nullptr)
				{
					continue;
				}
				
				// Find node id in OSM File
				for (auto XmlNode : XmlNodes)
				{
					if (bFoundXmlNode)
					{
						break;
					}

					auto Name = XmlNode->GetTag();
					auto Attributes = XmlNode->GetAttributes();
					auto XmlNodeChildren = XmlNode->GetChildrenNodes();

					if (!Name.Compare(FString("node")))
					{
						for (auto Attribute : Attributes)
						{
							if (!Attribute.GetTag().Compare(FString("id")))
							{
								if (!Attribute.GetValue().Compare(WindTurbine->GetName()))
								{
									// ID is found
									bFoundXmlNode = true;
									WindTurbineXmlNode = XmlNode;
									break;
								}
							}
						}
					}
				}

				if (WindTurbineXmlNode)
				{
					UE_LOG(LogTemp, Display, TEXT("Writing changes to OsmNode of windturbine in OSM file"), *WindTurbine->GetName());

					// Convert to Lat / Lon
					FVector NewWorldLocation = WindTurbine->GetActorLocation();
					FVector2D NewLocation(NewWorldLocation.X / 100.0f,NewWorldLocation.Y / 100.0f);
					double NewLatitude = 0.0;
					double NewLongitude = 0.0;
					SRS.ToEPSG4326(NewLocation, NewLongitude, NewLatitude);

					// Find Lat / Lon in XmlNode and write it
					TArray<FXmlAttribute>& Attributes = WindTurbineXmlNode->GetAttributes();
					for (FXmlAttribute& Attribute : Attributes)
					{
						if (!Attribute.GetTag().Compare(FString("lat")))
						{
							// FString::SanitizeFloat() trims one too many digit important for Lat / Lon representation
							char CharBuffer[21];
							sprintf_s(CharBuffer, "%.*g", 9, NewLatitude);
							Attribute.SetValue(FString(CharBuffer));
						}
						else if (!Attribute.GetTag().Compare(FString("lon")))
						{
							char CharBuffer[21];
							sprintf_s(CharBuffer, "%.*g", 9, NewLongitude);
							Attribute.SetValue(FString(CharBuffer));
						}
					}
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("Did not find ID %s of windturbine in OSM file - is it the same file???"), *WindTurbine->GetName());
				}
			}
			if (OSMFile->SaveOpenStreetMapFile())
			{
				// reload the changed osm file
				OSMFile->LoadOpenStreetMapFile(OSMFile->OSMFileLocation, false, nullptr);
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("Failed to save osm file"));
			}
			
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to get OSM file"));
		}
	}
}


void BuildRoadFurniture(class UStreetMapComponent* StreetMapComponent, FStreetMapRoadFurnitureBuildSettings& BuildSettings)
{
	FScopedTransaction Transaction(LOCTEXT("Undo", "Creating Road Furniture"));

	BuildStreetMapRoadFurniture(StreetMapComponent, BuildSettings);
}


void SaveRoadFurniture(class UStreetMapComponent* StreetMapComponent, FStreetMapRoadFurnitureBuildSettings& BuildSettings)
{
	FScopedTransaction Transaction(LOCTEXT("Undo", "Saving Road Furniture"));

	SaveStreetMapRoadFurniture(StreetMapComponent, BuildSettings);
}

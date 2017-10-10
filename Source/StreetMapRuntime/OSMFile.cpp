// Copyright 2017 Mike Fricker. All Rights Reserved.

#include "StreetMapRuntime.h"
#include "OSMFile.h"


UOSMFile::UOSMFile()
{
}
		

UOSMFile::~UOSMFile()
{
	// Clean up time
	{
		for( auto* Way : Ways )
		{
			delete Way;
		}
		Ways.Empty();
				
		for( auto HashPair : NodeMap )
		{
			FOSMNodeInfo* NodeInfo = HashPair.Value;
			delete NodeInfo;
		}
		NodeMap.Empty();
	}
}


bool UOSMFile::LoadOpenStreetMapFile( FString& OSMFilePath, const bool bIsFilePathActuallyTextBuffer, FFeedbackContext* FeedbackContext )
{
	const bool bShowSlowTaskDialog = true;
	const bool bShowCancelButton = true;

	// TODO: make this an import option - OSM has many tags that might be interesting to display and edit, but are not needed for an UStreetMap yet
	bool ImportUnknownTags = true;

	if (OsmXmlFile.LoadFile(OSMFilePath))
	{
		auto RootNode = OsmXmlFile.GetRootNode();
		if (RootNode->GetTag().Compare(FString("osm")))
		{
			UE_LOG(LogTemp, Display, TEXT("Loaded full xml tree but it is no osm file!"));
			return false;
		}
		// Save file location for saving it later on
		OSMFileLocation = OSMFilePath;

		auto XmlNodes = RootNode->GetChildrenNodes();
		for (auto XmlNode : XmlNodes)
		{
			auto Name = XmlNode->GetTag();
			auto Attributes = XmlNode->GetAttributes();
			auto XmlNodeChildren = XmlNode->GetChildrenNodes();

			if (!Name.Compare(FString("bounds")))
			{
				for (auto Attribute : Attributes)
				{
					if (!Attribute.GetTag().Compare(FString("minlon")))
					{
						MinLongitude = FPlatformString::Atod(*Attribute.GetValue());
					}
					else if (!Attribute.GetTag().Compare(FString("minlat")))
					{
						MinLatitude = FPlatformString::Atod(*Attribute.GetValue());
					}
					else if (!Attribute.GetTag().Compare(FString("maxlat")))
					{
						MaxLatitude = FPlatformString::Atod(*Attribute.GetValue());
					}
					else if (!Attribute.GetTag().Compare(FString("maxlon")))
					{
						MaxLongitude = FPlatformString::Atod(*Attribute.GetValue());
					}
				}
			}

			if (!Name.Compare(FString("node")))
			{
				CurrentNodeInfo = new FOSMNodeInfo();
				CurrentNodeInfo->Latitude = 0.0;
				CurrentNodeInfo->Longitude = 0.0;
				// Process infos inside node tag
				for (auto Attribute : Attributes)
				{
					if (!Attribute.GetTag().Compare(FString("id")))
					{
						CurrentNodeID = FPlatformString::Atoi64(*Attribute.GetValue());
					}
					else if (!Attribute.GetTag().Compare(FString("lat")))
					{
						CurrentNodeInfo->Latitude = FPlatformString::Atod(*Attribute.GetValue());

						AverageLatitude += CurrentNodeInfo->Latitude;
					}
					else if (!Attribute.GetTag().Compare(FString("lon")))
					{
						CurrentNodeInfo->Longitude = FPlatformString::Atod(*Attribute.GetValue());

						AverageLongitude += CurrentNodeInfo->Longitude;
					}
				}
				// Process children, looking for tags
				if (ImportUnknownTags)
				{
					for (auto Child : XmlNodeChildren)
					{
						if (!Child->GetTag().Compare(FString("tag")))
						{
							auto Attributes = Child->GetAttributes();
							// assuming key, value pair
							if (Attributes.Num() == 2)
							{
								auto Key = Attributes[0];
								auto Value = Attributes[1];

								if (!Key.GetTag().Compare(FString("k")) && !Value.GetTag().Compare(FString("v")))
								{
									FOSMTag Tag;
									Tag.Key = FName::FName(*Key.GetValue());
									Tag.Value = FName::FName(*Value.GetValue());
									CurrentNodeInfo->Tags.Add(Tag);
								}
							}
						}
					}
				}
				NodeMap.Add(CurrentNodeID, CurrentNodeInfo);
			}
			else if (!Name.Compare(FString("way")))
			{
				CurrentWayInfo = new FOSMWayInfo();
				CurrentWayInfo->Name.Empty();
				CurrentWayInfo->Ref.Empty();
				CurrentWayInfo->WayType = EOSMWayType::Other;
				CurrentWayInfo->Height = 0.0;
				CurrentWayInfo->bIsOneWay = false;
				// @todo: We're currently ignoring the "visible" tag on ways, which means that roads will always
				//        be included in our data set.  It might be nice to make this an import option.
				
				// Process infos inside way tag
				for (auto Attribute : Attributes)
				{
					if (!Attribute.GetTag().Compare(FString("id")))
					{
						CurrentWayID = FPlatformString::Atoi64(*Attribute.GetValue());
						CurrentWayInfo->Id = CurrentWayID;
					}
				}
				// Process children, looking for node references and tags
				for (auto Child : XmlNodeChildren)
				{
					if (!Child->GetTag().Compare(FString("nd")))
					{
						auto Attributes = Child->GetAttributes();
						for (auto Attribute : Attributes)
						{
							if (!Attribute.GetTag().Compare(FString("ref")))
							{
								FOSMNodeInfo* ReferencedNode = NodeMap.FindRef(FPlatformString::Atoi64(*Attribute.GetValue()));
								if (ReferencedNode)
								{
									const int NewNodeIndex = CurrentWayInfo->Nodes.Num();
									CurrentWayInfo->Nodes.Add(ReferencedNode);

									// Update the node with information about the way that is referencing it
									{
										FOSMWayRef NewWayRef;
										NewWayRef.Way = CurrentWayInfo;
										NewWayRef.NodeIndex = NewNodeIndex;
										ReferencedNode->WayRefs.Add(NewWayRef);
									}
								}
							}
						}
					}
					else if (!Child->GetTag().Compare(FString("tag")))
					{
						auto Attributes = Child->GetAttributes();
						// assuming key, value pair
						if (Attributes.Num() == 2)
						{
							auto Key = Attributes[0];
							auto Value = Attributes[1];
						
							if (!Key.GetTag().Compare(FString("k")) && !Value.GetTag().Compare(FString("v")))
							{
								if (!Key.GetValue().Compare(FString("name")))
								{
									CurrentWayInfo->Name = Value.GetValue();
								}
								else if (!Key.GetValue().Compare(FString("ref")))
								{
									CurrentWayInfo->Ref = Value.GetValue();
								}
								else if (!Key.GetValue().Compare(FString("highway")))
								{
									CurrentWayInfo->WayType = EOSMWayType::Highway;
									CurrentWayInfo->Category = Value.GetValue();
								}
								else if (!Key.GetValue().Compare(FString("railway")))
								{
									CurrentWayInfo->WayType = EOSMWayType::Railway;
									CurrentWayInfo->Category = Value.GetValue();
								}
								else if (!Key.GetValue().Compare(FString("building")))
								{
									CurrentWayInfo->WayType = EOSMWayType::Building;

									if (!Value.GetValue().Compare(FString("yes")))
									{
										CurrentWayInfo->Category = Value.GetValue();
									}
								}
								else if (!Key.GetValue().Compare(FString("height")))
								{
									// Check to see if there is a space character in the height value.  For now, we're looking
									// for straight-up floating point values.
									if (!FString(Value.GetValue()).Contains(TEXT(" ")))
									{
										// Okay, no space character.  So this has got to be a floating point number.  The OSM
										// spec says that the height values are in meters.
										CurrentWayInfo->Height = FPlatformString::Atod(*Value.GetValue());
									}
									else
									{
										// Looks like the height value contains units of some sort.
										// @todo: Add support for interpreting unit strings and converting the values
									}
								}
								else if (!Key.GetValue().Compare(FString("building:levels")))
								{
									CurrentWayInfo->BuildingLevels = FPlatformString::Atoi(*Value.GetValue());
								}
								else if (!Key.GetValue().Compare(FString("oneway")))
								{
									if (!Value.GetValue().Compare(FString(("yes"))))
									{
										CurrentWayInfo->bIsOneWay = true;
									}
									else
									{
										CurrentWayInfo->bIsOneWay = false;
									}
								}
								else if (CurrentWayInfo->WayType == EOSMWayType::Other)
								{
									// if this way was not already marked as building or highway, try other types as well
									if (!Key.GetValue().Compare(FString("leisure")))
									{
										CurrentWayInfo->WayType = EOSMWayType::Leisure;
										CurrentWayInfo->Category = Value.GetValue();
									}
									else if (!Key.GetValue().Compare(FString("natural")))
									{
										CurrentWayInfo->WayType = EOSMWayType::Natural;
										CurrentWayInfo->Category = Value.GetValue();
									}
									else if (!Key.GetValue().Compare(FString("landuse")))
									{
										CurrentWayInfo->WayType = EOSMWayType::LandUse;
										CurrentWayInfo->Category = Value.GetValue();
									}
								}
							}
						}
					}
				}
				WayMap.Add(CurrentWayID, CurrentWayInfo);
				Ways.Add(CurrentWayInfo);
				CurrentWayID = 0;
				CurrentWayInfo = nullptr;
			}
			else if (!Name.Compare(FString("relation")))
			{
				CurrentRelation = new FOSMRelation();
				CurrentRelation->Type = EOSMRelationType::Other;
				// Process infos inside relation tag
				for (auto Attribute : Attributes)
				{
					if (!Attribute.GetTag().Compare(FString("id")))
					{
						CurrentRelation->Id = FPlatformString::Atoi64(*Attribute.GetValue());
					}
				}

				// Process children, looking for members and tags
				for (auto Child : XmlNodeChildren)
				{
					if (!Child->GetTag().Compare(FString("member")))
					{
						CurrentRelationMember = new FOSMRelationMember();
						CurrentRelationMember->Type = EOSMRelationMemberType::Other;
						CurrentRelationMember->Role = EOSMRelationMemberRole::Other;

						auto Attributes = Child->GetAttributes();
						for (auto Attribute : Attributes)
						{
							if (!Attribute.GetTag().Compare(FString("type")))
							{
								if (!Attribute.GetValue().Compare(FString("node")))
								{
									CurrentRelationMember->Type = EOSMRelationMemberType::Node;
								}
								else if (!Attribute.GetValue().Compare(FString("way")))
								{
									CurrentRelationMember->Type = EOSMRelationMemberType::Way;
								}
								else if (!Attribute.GetValue().Compare(FString("relation")))
								{
									CurrentRelationMember->Type = EOSMRelationMemberType::Relation;
								}
							}
							else if (!Attribute.GetTag().Compare(FString("ref")))
							{
								CurrentRelationMember->Ref = FPlatformString::Atoi64(*Attribute.GetValue()); // TODO: decide if int64 or FString is better
							}
							else if (!Attribute.GetTag().Compare(FString("role")))
							{
								if (!Attribute.GetValue().Compare(FString("outer")))
								{
									CurrentRelationMember->Role = EOSMRelationMemberRole::Outer;
								}
								else if (!Attribute.GetValue().Compare(FString("inner")))
								{
									CurrentRelationMember->Role = EOSMRelationMemberRole::Inner;
								}
							}
						}

						CurrentRelation->Members.Add(CurrentRelationMember);
					}
					else if (!Child->GetTag().Compare(FString("tag")))
					{
						auto Attributes = Child->GetAttributes();
						// assuming key, value pair
						if (Attributes.Num() == 2)
						{
							auto Key = Attributes[0];
							auto Value = Attributes[1];

							if (!Key.GetTag().Compare(FString("k")) && !Value.GetTag().Compare(FString("v")))
							{
								if (!Key.GetValue().Compare(FString("name")))
								{
									CurrentRelation->Name = Value.GetValue();
								}
								else if (!Key.GetValue().Compare(FString("ref")))
								{
									CurrentRelation->Ref = Value.GetValue();
								}
								else if (!Key.GetValue().Compare(FString("type")))
								{
									if (!Value.GetValue().Compare(FString("boundary")))
									{
										CurrentRelation->Type = EOSMRelationType::Boundary;
									}
									else if (!Value.GetValue().Compare(FString("multipolygon")))
									{
										CurrentRelation->Type = EOSMRelationType::Multipolygon;
									}
								}
								else if (ImportUnknownTags)
								{
									// all other tags don't map to our data structure, still save them
									FOSMTag Tag;
									Tag.Key = FName::FName(*Key.GetValue());
									Tag.Value = FName::FName(*Value.GetValue());
									CurrentRelation->Tags.Add(Tag);

								}
							}
						}
					}
				}
				Relations.Add(CurrentRelation);
			}
		}

		if (MinLatitude != MAX_dbl && MinLongitude != MAX_dbl && MaxLatitude != -MAX_dbl && MaxLongitude != -MAX_dbl)
		{
			AverageLatitude = (MinLatitude + MaxLatitude) / 2;
			AverageLongitude = (MinLongitude + MaxLongitude) / 2;
			SpatialReferenceSystem.SetOriginLatitude(AverageLatitude);
			SpatialReferenceSystem.SetOriginLongitude(AverageLongitude);
		}
		else if (NodeMap.Num() > 0)
		{
			AverageLatitude /= NodeMap.Num();
			AverageLongitude /= NodeMap.Num();

			SpatialReferenceSystem.SetOriginLatitude(AverageLatitude);
			SpatialReferenceSystem.SetOriginLongitude(AverageLongitude);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("OSM file does not contain any nodes nor bound tags, failed to init SpatialReferenceSystem!"));
			return false;
		}

		return true;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to load full xml tree"));
	}

	return false;
}

bool UOSMFile::SaveOpenStreetMapFile()
{
	if (OsmXmlFile.IsValid())
	{
		if (OsmXmlFile.Save(OSMFileLocation))
		{
			UE_LOG(LogTemp, Display, TEXT("Saved file successfully"));
			return true;
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to save OSM file"));
			return false;
		}
	}

	UE_LOG(LogTemp, Error, TEXT("File is not valid or not even loaded. Cannot save!"));
	return false;
}

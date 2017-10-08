#include "StreetMapImporting.h"
#include "StreetMapActor.h"
#include "StreetMapComponent.h"

#include "StreetMapSplineTools.h"

#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "StreetMapImporting"

static void BuildStreetMapRoadFurniture(class UStreetMapComponent* StreetMapComponent,
	const FStreetMapRoadFurnitureBuildSettings& BuildSettings)
{
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (World)
	{
		const TArray<FStreetMapSign>& Signs = StreetMapComponent->GetStreetMap()->GetSigns();
		for (auto Sign : Signs)
		{
			const float WorldElevation = FStreetMapSplineTools::GetLandscapeElevation(BuildSettings.Landscape, Sign.Location);
			const float ScaledWorldElevation = WorldElevation;
			const FVector2D& ScaledPointLocation = Sign.Location;

			FVector Location(ScaledPointLocation, WorldElevation);
			FRotator Rotation(0.0f, 0.0f, 0.0f);
			FActorSpawnParameters SpawnInfo;
			World->SpawnActor<ATrafficSign>(Location, Rotation, SpawnInfo);
		}
	}

}


void BuildRoadFurniture(class UStreetMapComponent* StreetMapComponent, const FStreetMapRoadFurnitureBuildSettings& BuildSettings)
{
	FScopedTransaction Transaction(LOCTEXT("Undo", "Creating Road Furniture"));

	BuildStreetMapRoadFurniture(StreetMapComponent, BuildSettings);
}

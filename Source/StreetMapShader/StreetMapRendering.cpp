#include "StreetMapRendering.h"
#include <MaterialShaderType.h>

IMPLEMENT_MATERIAL_SHADER_TYPE(, TStreetMapVS, TEXT("/Plugin/Private/StreetMapVS.usf"), TEXT("Main"), SF_Vertex);
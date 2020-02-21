#include "StreetMapRendering.h"
#include <MaterialShaderType.h>

IMPLEMENT_MATERIAL_SHADER_TYPE(, TStreetMapVS, TEXT("/Engine/Private/PositionOnlyDepthVertexShader.usf"), TEXT("Main"), SF_Vertex);
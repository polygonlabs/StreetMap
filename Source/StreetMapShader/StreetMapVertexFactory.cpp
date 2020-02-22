#include "Public/StreetMapVertexFactory.h"
#include <Renderer/Public/MeshMaterialShader.h>
#include <Rendering/ColorVertexBuffer.h>
#include <RenderCore/Public/ShaderParameterMacros.h>
#include <Runtime/Engine/Public/MeshBatch.h>
#include <Renderer/Public/MeshDrawShaderBindings.h>

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FStreetMapVertexFactoryUniformShaderParameters, "StreetVF");

TUniformBufferRef<FStreetMapVertexFactoryUniformShaderParameters> CreateStreetVFUniformBuffer(
	const FStreetMapVertexFactory* LocalVertexFactory,
	uint32 LODLightmapDataIndex,
	FColorVertexBuffer* OverrideColorVertexBuffer,
	int32 BaseVertexIndex)
{
	FStreetMapVertexFactoryUniformShaderParameters UniformParameters;

	UniformParameters.LODLightmapDataIndex = LODLightmapDataIndex;
	int32 ColorIndexMask = 0;

	if (RHISupportsManualVertexFetch(GMaxRHIShaderPlatform))
	{
		UniformParameters.VertexFetch_PositionBuffer = LocalVertexFactory->GetPositionsSRV();

		UniformParameters.VertexFetch_PackedTangentsBuffer = LocalVertexFactory->GetTangentsSRV();
		UniformParameters.VertexFetch_TexCoordBuffer = LocalVertexFactory->GetTextureCoordinatesSRV();

		if (OverrideColorVertexBuffer)
		{
			UniformParameters.VertexFetch_ColorComponentsBuffer = OverrideColorVertexBuffer->GetColorComponentsSRV();
			ColorIndexMask = OverrideColorVertexBuffer->GetNumVertices() > 1 ? ~0 : 0;
		}
		else
		{
			UniformParameters.VertexFetch_ColorComponentsBuffer = LocalVertexFactory->GetColorComponentsSRV();
			ColorIndexMask = (int32)LocalVertexFactory->GetColorIndexMask();
		}
	}
	else
	{
		UniformParameters.VertexFetch_PackedTangentsBuffer = GNullColorVertexBuffer.VertexBufferSRV;
		UniformParameters.VertexFetch_TexCoordBuffer = GNullColorVertexBuffer.VertexBufferSRV;
	}

	if (!UniformParameters.VertexFetch_ColorComponentsBuffer)
	{
		UniformParameters.VertexFetch_ColorComponentsBuffer = GNullColorVertexBuffer.VertexBufferSRV;
	}

	const int32 NumTexCoords = LocalVertexFactory->GetNumTexcoords();
	const int32 LightMapCoordinateIndex = LocalVertexFactory->GetLightMapCoordinateIndex();
	const int32 EffectiveBaseVertexIndex = RHISupportsAbsoluteVertexID(GMaxRHIShaderPlatform) ? 0 : BaseVertexIndex;
	UniformParameters.VertexFetch_Parameters = { ColorIndexMask, NumTexCoords, LightMapCoordinateIndex, EffectiveBaseVertexIndex };

	return TUniformBufferRef<FStreetMapVertexFactoryUniformShaderParameters>::CreateUniformBufferImmediate(UniformParameters, UniformBuffer_MultiFrame);
}

void FStreetMapVertexFactoryShaderParameters::Bind(const FShaderParameterMap& ParameterMap)
{
	//LODParameter.Bind(ParameterMap, TEXT("SpeedTreeLODInfo"));
	//bAnySpeedTreeParamIsBound = LODParameter.IsBound() || ParameterMap.ContainsParameterAllocation(TEXT("SpeedTreeData"));
}

void FStreetMapVertexFactoryShaderParameters::Serialize(FArchive& Ar)
{
	//Ar << bAnySpeedTreeParamIsBound;
	//Ar << LODParameter;
}


void FStreetMapVertexFactoryShaderParameters::GetElementShaderBindings(
	const FSceneInterface* Scene,
	const FSceneView* View,
	const FMeshMaterialShader* Shader,
	const EVertexInputStreamType InputStreamType,
	ERHIFeatureLevel::Type FeatureLevel,
	const FVertexFactory* VertexFactory,
	const FMeshBatchElement& BatchElement,
	FMeshDrawSingleShaderBindings& ShaderBindings,
	FVertexInputStreamArray& VertexStreams
) const
{
	// Decode VertexFactoryUserData as VertexFactoryUniformBuffer
	FRHIUniformBuffer* VertexFactoryUniformBuffer = static_cast<FRHIUniformBuffer*>(BatchElement.VertexFactoryUserData);

	GetElementShaderBindings(
		Scene,
		View,
		Shader,
		InputStreamType,
		FeatureLevel,
		VertexFactory,
		BatchElement,
		VertexFactoryUniformBuffer,
		ShaderBindings,
		VertexStreams);
}

void FStreetMapVertexFactoryShaderParameters::GetElementShaderBindings(
	const FSceneInterface* Scene,
	const FSceneView* View,
	const FMeshMaterialShader* Shader,
	const EVertexInputStreamType InputStreamType,
	ERHIFeatureLevel::Type FeatureLevel,
	const FVertexFactory* VertexFactory,
	const FMeshBatchElement& BatchElement,
	FRHIUniformBuffer* VertexFactoryUniformBuffer,
	FMeshDrawSingleShaderBindings& ShaderBindings,
	FVertexInputStreamArray& VertexStreams
) const
{
	const auto* LocalVertexFactory = static_cast<const FLocalVertexFactory*>(VertexFactory);

	if (LocalVertexFactory->SupportsManualVertexFetch(FeatureLevel) || UseGPUScene(GMaxRHIShaderPlatform, FeatureLevel))
	{
		if (!VertexFactoryUniformBuffer)
		{
			// No batch element override
			VertexFactoryUniformBuffer = LocalVertexFactory->GetUniformBuffer();
		}

		ShaderBindings.Add(Shader->GetUniformBufferParameter<FLocalVertexFactoryUniformShaderParameters>(), VertexFactoryUniformBuffer);
	}

	//@todo - allow FMeshBatch to supply vertex streams (instead of requiring that they come from the vertex factory), and this userdata hack will no longer be needed for override vertex color
	if (BatchElement.bUserDataIsColorVertexBuffer)
	{
		FColorVertexBuffer* OverrideColorVertexBuffer = (FColorVertexBuffer*)BatchElement.UserData;
		check(OverrideColorVertexBuffer);

		if (!LocalVertexFactory->SupportsManualVertexFetch(FeatureLevel))
		{
			LocalVertexFactory->GetColorOverrideStream(OverrideColorVertexBuffer, VertexStreams);
		}
	}

	//if (bAnySpeedTreeParamIsBound)
	//{
	//	QUICK_SCOPE_CYCLE_COUNTER(STAT_FLocalVertexFactoryShaderParameters_SetMesh_SpeedTree);
	//	FRHIUniformBuffer* SpeedTreeUniformBuffer = Scene ? Scene->GetSpeedTreeUniformBuffer(VertexFactory) : nullptr;
	//	if (SpeedTreeUniformBuffer == nullptr)
	//	{
	//		SpeedTreeUniformBuffer = GSpeedTreeWindNullUniformBuffer.GetUniformBufferRHI();
	//	}
	//	check(SpeedTreeUniformBuffer != nullptr);

	//	ShaderBindings.Add(Shader->GetUniformBufferParameter<FSpeedTreeUniformParameters>(), SpeedTreeUniformBuffer);

	//	if (LODParameter.IsBound())
	//	{
	//		FVector LODData(BatchElement.MinScreenSize, BatchElement.MaxScreenSize, BatchElement.MaxScreenSize - BatchElement.MinScreenSize);
	//		ShaderBindings.Add(LODParameter, LODData);
	//	}
	//}
}


void FStreetMapVertexFactory::SetData(const FStreetVertexDataType& InData)
{
	check(IsInRenderingThread());

	// The shader code makes assumptions that the color component is a FColor, performing swizzles on ES2 and Metal platforms as necessary
	// If the color is sent down as anything other than VET_Color then you'll get an undesired swizzle on those platforms
	check((InData.ColorComponent.Type == VET_None) || (InData.ColorComponent.Type == VET_Color));

	SmData = InData;
	UpdateRHI();
}

void FStreetMapVertexFactory::InitRHI()
{
	// We create different streams based on feature level
	check(HasValidFeatureLevel());

	// VertexFactory needs to be able to support max possible shader platform and feature level
	// in case if we switch feature level at runtime.
	const bool bCanUseGPUScene = UseGPUScene(GMaxRHIShaderPlatform, GMaxRHIFeatureLevel);

	// If the vertex buffer containing position is not the same vertex buffer containing the rest of the data,
	// then initialize PositionStream and PositionDeclaration.
	if (SmData.PositionComponent.VertexBuffer != SmData.TangentBasisComponents[0].VertexBuffer)
	{
		auto AddDeclaration = [this, bCanUseGPUScene](EVertexInputStreamType InputStreamType, bool bAddNormal)
		{
			FVertexDeclarationElementList StreamElements;
			StreamElements.Add(AccessStreamComponent(SmData.PositionComponent, 0, InputStreamType));

			bAddNormal = bAddNormal && SmData.TangentBasisComponents[1].VertexBuffer != NULL;
			if (bAddNormal)
			{
				StreamElements.Add(AccessStreamComponent(SmData.TangentBasisComponents[1], 2, InputStreamType));
			}

			const uint8 TypeIndex = static_cast<uint8>(InputStreamType);
			PrimitiveIdStreamIndex[TypeIndex] = -1;
			if (GetType()->SupportsPrimitiveIdStream() && bCanUseGPUScene)
			{
				// When the VF is used for rendering in normal mesh passes, this vertex buffer and offset will be overridden
				StreamElements.Add(AccessStreamComponent(FVertexStreamComponent(&GPrimitiveIdDummy, 0, 0, sizeof(uint32), VET_UInt, EVertexStreamUsage::Instancing), 1, InputStreamType));
				PrimitiveIdStreamIndex[TypeIndex] = StreamElements.Last().StreamIndex;
			}

			InitDeclaration(StreamElements, InputStreamType);
		};
		AddDeclaration(EVertexInputStreamType::PositionOnly, false);
		AddDeclaration(EVertexInputStreamType::PositionAndNormalOnly, true);
	}

	FVertexDeclarationElementList Elements;
	if (SmData.PositionComponent.VertexBuffer != NULL)
	{
		Elements.Add(AccessStreamComponent(SmData.PositionComponent, 0));
	}

	{
		const uint8 Index = static_cast<uint8>(EVertexInputStreamType::Default);
		PrimitiveIdStreamIndex[Index] = -1;
		if (GetType()->SupportsPrimitiveIdStream() && bCanUseGPUScene)
		{
			// When the VF is used for rendering in normal mesh passes, this vertex buffer and offset will be overridden
			Elements.Add(AccessStreamComponent(FVertexStreamComponent(&GPrimitiveIdDummy, 0, 0, sizeof(uint32), VET_UInt, EVertexStreamUsage::Instancing), 13));
			PrimitiveIdStreamIndex[Index] = Elements.Last().StreamIndex;
		}
	}

	// only tangent,normal are used by the stream. the binormal is derived in the shader
	uint8 TangentBasisAttributes[2] = { 1, 2 };
	for (int32 AxisIndex = 0; AxisIndex < 2; AxisIndex++)
	{
		if (SmData.TangentBasisComponents[AxisIndex].VertexBuffer != NULL)
		{
			Elements.Add(AccessStreamComponent(SmData.TangentBasisComponents[AxisIndex], TangentBasisAttributes[AxisIndex]));
		}
	}

	if (SmData.ColorComponentsSRV == nullptr)
	{
		SmData.ColorComponentsSRV = GNullColorVertexBuffer.VertexBufferSRV;
		SmData.ColorIndexMask = 0;
	}

	ColorStreamIndex = -1;
	if (SmData.ColorComponent.VertexBuffer)
	{
		Elements.Add(AccessStreamComponent(SmData.ColorComponent, 3));
		ColorStreamIndex = Elements.Last().StreamIndex;
	}
	else
	{
		//If the mesh has no color component, set the null color buffer on a new stream with a stride of 0.
		//This wastes 4 bytes of bandwidth per vertex, but prevents having to compile out twice the number of vertex factories.
		FVertexStreamComponent NullColorComponent(&GNullColorVertexBuffer, 0, 0, VET_Color, EVertexStreamUsage::ManualFetch);
		Elements.Add(AccessStreamComponent(NullColorComponent, 3));
		ColorStreamIndex = Elements.Last().StreamIndex;
	}

	if (SmData.TextureCoordinates.Num())
	{
		const int32 BaseTexCoordAttribute = 4;
		for (int32 CoordinateIndex = 0; CoordinateIndex < SmData.TextureCoordinates.Num(); CoordinateIndex++)
		{
			Elements.Add(AccessStreamComponent(
				SmData.TextureCoordinates[CoordinateIndex],
				BaseTexCoordAttribute + CoordinateIndex
			));
		}

		for (int32 CoordinateIndex = SmData.TextureCoordinates.Num(); CoordinateIndex < MAX_STATIC_TEXCOORDS / 2; CoordinateIndex++)
		{
			Elements.Add(AccessStreamComponent(
				SmData.TextureCoordinates[Data.TextureCoordinates.Num() - 1],
				BaseTexCoordAttribute + CoordinateIndex
			));
		}
	}

	if (SmData.LightMapCoordinateComponent.VertexBuffer)
	{
		Elements.Add(AccessStreamComponent(SmData.LightMapCoordinateComponent, 15));
	}
	else if (SmData.TextureCoordinates.Num())
	{
		Elements.Add(AccessStreamComponent(SmData.TextureCoordinates[0], 15));
	}

	check(Streams.Num() > 0);

	InitDeclaration(Elements);
	check(IsValidRef(GetDeclaration()));

	const int32 DefaultBaseVertexIndex = 0;
	if (RHISupportsManualVertexFetch(GMaxRHIShaderPlatform) || bCanUseGPUScene)
	{
		UniformBuffer = CreateStreetVFUniformBuffer(this, SmData.LODLightmapDataIndex, nullptr, DefaultBaseVertexIndex);
	}

	check(IsValidRef(GetDeclaration()));
}

FVertexFactoryShaderParameters* FStreetMapVertexFactory::ConstructShaderParameters(EShaderFrequency ShaderFrequency)
{
	if (ShaderFrequency == SF_Vertex)
	{
		return new FStreetMapVertexFactoryShaderParameters();
	}

#if RHI_RAYTRACING
	if (ShaderFrequency == SF_RayHitGroup)
	{
		return new FStreetMapVertexFactoryShaderParameters();
	}
	else if (ShaderFrequency == SF_Compute)
	{
		return new FStreetMapVertexFactoryShaderParameters();
	}
#endif // RHI_RAYTRACING

	return NULL;
}

IMPLEMENT_VERTEX_FACTORY_TYPE_EX(FStreetMapVertexFactory, "/Plugin/Private/StreetMapVertexFactory.ush", true, true, true, true, true, true, true);

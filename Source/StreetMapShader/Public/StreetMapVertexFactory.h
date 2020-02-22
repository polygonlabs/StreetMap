#pragma once
#include <LocalVertexFactory.h>

class FMaterial;
class FSceneView;
struct FMeshBatchElement;

/*=============================================================================
	StreetMapVertexFactory.h: Local vertex factory definitions.
=============================================================================*/

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FStreetMapVertexFactoryUniformShaderParameters, )
SHADER_PARAMETER(FIntVector4, VertexFetch_Parameters)
SHADER_PARAMETER(uint32, LODLightmapDataIndex)
SHADER_PARAMETER_SRV(Buffer<float2>, VertexFetch_TexCoordBuffer)
SHADER_PARAMETER_SRV(Buffer<float>, VertexFetch_PositionBuffer)
SHADER_PARAMETER_SRV(Buffer<float4>, VertexFetch_PackedTangentsBuffer)
SHADER_PARAMETER_SRV(Buffer<float4>, VertexFetch_ColorComponentsBuffer)
SHADER_PARAMETER_SRV(Buffer<float4>, VertexFetch_ExtrudeDirComponentsBuffer)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

extern TUniformBufferRef<FStreetMapVertexFactoryUniformShaderParameters> CreateStreetVFUniformBuffer(
	const class FStreetMapVertexFactory* VertexFactory,
	uint32 LODLightmapDataIndex,
	class FColorVertexBuffer* OverrideColorVertexBuffer,
	int32 BaseVertexIndex);


class FStreetMapVertexFactory : public FLocalVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FStreetMapVertexFactory);
public:
	FStreetMapVertexFactory(ERHIFeatureLevel::Type InFeatureLevel, const char* InDebugName)
		: FLocalVertexFactory(InFeatureLevel, InDebugName)
		, ExtrudeDirStreamIndex(-1)
	{
		bSupportsManualVertexFetch = true;
	}

	struct FStreetVertexDataType : public FDataType
	{
		/** The stream to read the vertex extrude direction from. */
		FVertexStreamComponent ExtrudeDirComponent;

	};

	/**
	 * An implementation of the interface used by TSynchronizedResource to update the resource with new data from the game thread.
	 */
	void SetData(const FStreetVertexDataType& InData);

	// FRenderResource interface.
	virtual void InitRHI() override;
	virtual void ReleaseRHI() override
	{
		UniformBuffer.SafeRelease();
		FLocalVertexFactory::ReleaseRHI();
	}

	static bool SupportsTessellationShaders() { return true; }

	static FVertexFactoryShaderParameters* ConstructShaderParameters(EShaderFrequency ShaderFrequency);

	FORCEINLINE_DEBUGGABLE void SetExtrueDirOverrideStream(FRHICommandList& RHICmdList, const FVertexBuffer* ExtrueDirVertexBuffer) const
	{
		checkf(ExtrueDirVertexBuffer->IsInitialized(), TEXT("Extrude Direction Vertex buffer was not initialized! Name %s"), *ExtrueDirVertexBuffer->GetFriendlyName());
		checkf(IsInitialized() && EnumHasAnyFlags(EVertexStreamUsage::Overridden, SmData.ExtrudeDirComponent.VertexStreamUsage) && ExtrudeDirStreamIndex > 0, TEXT("Per-mesh extrude direction with bad stream setup! Name %s"), * ExtrueDirVertexBuffer->GetFriendlyName());
		RHICmdList.SetStreamSource(ColorStreamIndex, ExtrueDirVertexBuffer->VertexBufferRHI, 0);
	}

	void GetExtrueDirOverrideStream(const FVertexBuffer* ExtrueDirVertexBuffer, FVertexInputStreamArray& VertexStreams) const
	{
		checkf(ExtrueDirVertexBuffer->IsInitialized(), TEXT("Extrude Direction Vertex buffer was not initialized! Name %s"), *ExtrueDirVertexBuffer->GetFriendlyName());
		checkf(IsInitialized() && EnumHasAnyFlags(EVertexStreamUsage::Overridden, SmData.ExtrudeDirComponent.VertexStreamUsage) && ExtrudeDirStreamIndex > 0, TEXT("Per-mesh extrude direction with bad stream setup! Name %s"), * ExtrueDirVertexBuffer->GetFriendlyName());

		VertexStreams.Add(FVertexInputStream(ExtrudeDirStreamIndex, 0, ExtrueDirVertexBuffer->VertexBufferRHI));
	}

	inline FRHIShaderResourceView* GetPositionsSRV() const
	{
		return SmData.PositionComponentSRV;
	}

	inline FRHIShaderResourceView* GetTangentsSRV() const
	{
		return SmData.TangentsSRV;
	}

	inline FRHIShaderResourceView* GetTextureCoordinatesSRV() const
	{
		return SmData.TextureCoordinatesSRV;
	}

	inline FRHIShaderResourceView* GetColorComponentsSRV() const
	{
		return SmData.ColorComponentsSRV;
	}

	inline const uint32 GetColorIndexMask() const
	{
		return SmData.ColorIndexMask;
	}

	inline const int GetLightMapCoordinateIndex() const
	{
		return SmData.LightMapCoordinateIndex;
	}

	inline const int GetNumTexcoords() const
	{
		return SmData.NumTexCoords;
	}

	FRHIUniformBuffer* GetUniformBuffer() const
	{
		return UniformBuffer.GetReference();
	}

private:
	const FStreetVertexDataType& GetData() const { return SmData; }

	FStreetVertexDataType SmData;
	TUniformBufferRef<FStreetMapVertexFactoryUniformShaderParameters> UniformBuffer;

	int32 ExtrudeDirStreamIndex;
};

/** Shader parameter class used by FLocalVertexFactory only - no derived classes. */
class FStreetMapVertexFactoryShaderParameters : public FVertexFactoryShaderParameters
{
public:
	virtual void Bind(const FShaderParameterMap& ParameterMap) override;
	virtual void Serialize(FArchive& Ar) override;

	virtual void GetElementShaderBindings(
		const FSceneInterface* Scene,
		const FSceneView* View,
		const FMeshMaterialShader* Shader,
		const EVertexInputStreamType InputStreamType,
		ERHIFeatureLevel::Type FeatureLevel,
		const FVertexFactory* VertexFactory,
		const FMeshBatchElement& BatchElement,
		FMeshDrawSingleShaderBindings& ShaderBindings,
		FVertexInputStreamArray& VertexStreams
	) const override;

	virtual void GetElementShaderBindings(
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
	) const;
};
#include <MeshMaterialShader.h>

class TStreetMapVS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(TStreetMapVS, MeshMaterial);
protected:

	TStreetMapVS() {}

	TStreetMapVS(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer) :
		FMeshMaterialShader(Initializer)
	{
		BindSceneTextureUniformBufferDependentOnShadingPath(Initializer, PassUniformBuffer);
	}

public:

	static bool ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
	{
		static const auto SupportAtmosphericFog = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.SupportAtmosphericFog"));
		static const auto SupportAllShaderPermutations = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.SupportAllShaderPermutations"));
		const bool bForceAllPermutations = SupportAllShaderPermutations && SupportAllShaderPermutations->GetValueOnAnyThread() != 0;

		const bool bProjectAllowsAtmosphericFog = !SupportAtmosphericFog || SupportAtmosphericFog->GetValueOnAnyThread() != 0 || bForceAllPermutations;

		bool bShouldCache = Super::ShouldCompilePermutation(Parameters);
		bShouldCache &= (bEnableAtmosphericFog && bProjectAllowsAtmosphericFog && IsTranslucentBlendMode(Parameters.Material->GetBlendMode())) || !bEnableAtmosphericFog;

		return bShouldCache
			&& (IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5));
	}

	//void GetShaderBindings(
	//	const FScene* Scene,
	//	ERHIFeatureLevel::Type FeatureLevel,
	//	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	//	const FMaterialRenderProxy& MaterialRenderProxy,
	//	const FMaterial& Material,
	//	const FMeshPassProcessorRenderState& DrawRenderState,
	//	const FDepthOnlyShaderElementData& ShaderElementData,
	//	FMeshDrawSingleShaderBindings& ShaderBindings) const
	//{
	//	FMeshMaterialShader::GetShaderBindings(Scene, FeatureLevel, PrimitiveSceneProxy, MaterialRenderProxy, Material, DrawRenderState, ShaderElementData, ShaderBindings);
	//}
};

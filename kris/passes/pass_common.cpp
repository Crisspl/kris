#include "pass_common.h"

namespace kris
{
	refctd<nbl::video::IGPUGraphicsPipeline> PassResources::createGfxPipeline(
		nbl::video::ILogicalDevice* device,
		const nbl::video::IGPUPipelineLayout* layout,
		const GfxMaterial::GfxShaders& shaders,
		const nbl::asset::SVertexInputParams& vtxinput)
	{
		nbl::video::IGPUShader::SSpecInfo specInfo[2] = {
			{.shader = shaders.vertex.get() },
			{.shader = shaders.pixel.get() },
		};

		nbl::asset::SBlendParams blendParams = {};
		blendParams.blendParams[0u].srcColorFactor = nbl::asset::EBF_ONE;
		blendParams.blendParams[0u].dstColorFactor = nbl::asset::EBF_ZERO;
		blendParams.blendParams[0u].colorBlendOp = nbl::asset::EBO_ADD;
		blendParams.blendParams[0u].srcAlphaFactor = nbl::asset::EBF_ONE;
		blendParams.blendParams[0u].dstAlphaFactor = nbl::asset::EBF_ZERO;
		blendParams.blendParams[0u].alphaBlendOp = nbl::asset::EBO_ADD;
		blendParams.blendParams[0u].colorWriteMask = 0xfU;

		nbl::asset::SRasterizationParams rasterParams;
		rasterParams.polygonMode = nbl::asset::EPM_FILL;
		rasterParams.faceCullingMode = nbl::asset::EFCM_NONE;
		rasterParams.depthWriteEnable = true;

		nbl::asset::SPrimitiveAssemblyParams assemblyParams;

		nbl::video::IGPUGraphicsPipeline::SCreationParams params[1] = {};
		params[0].layout = layout;
		params[0].shaders = specInfo;
		params[0].cached = {
			.vertexInput = vtxinput,
			.primitiveAssembly = assemblyParams,
			.rasterization = rasterParams,
			.blend = blendParams,
		};
		params[0].renderpass = m_renderpass.get();

		kris::refctd<nbl::video::IGPUGraphicsPipeline> gfx;

		bool result = device->createGraphicsPipelines(nullptr, params, &gfx);
		KRIS_ASSERT(result);

		return gfx;
	}
}
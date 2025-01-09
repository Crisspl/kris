#pragma once

#include "../kris_common.h"
#include "../material.h"

namespace kris
{
	refctd<nbl::video::IGPUGraphicsPipeline> createBasePassGfxPipeline(
		nbl::video::ILogicalDevice* device,
		const nbl::video::IGPURenderpass* renderpass,
		const nbl::video::IGPUPipelineLayout* layout,
		const GfxMaterial::GfxShaders& shaders,
		const nbl::asset::SVertexInputParams& vtxinput);
}
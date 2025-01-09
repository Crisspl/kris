#pragma once

#include "../kris_common.h"

namespace kris
{
	using createGfxPipeline_fptr_t = refctd<nbl::video::IGPUGraphicsPipeline>(*)(
		nbl::video::ILogicalDevice* device,
		const nbl::video::IGPURenderpass* renderpass,
		const nbl::video::IGPUPipelineLayout* layout,
		const GfxMaterial::GfxShaders& shaders,
		const nbl::asset::SVertexInputParams& vtxinput);
}
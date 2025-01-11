#pragma once

#include "../kris_common.h"
#include "../material.h"

namespace kris
{
	struct Framebuffer
	{
		refctd<ImageResource> m_colors[MaxColorBuffers];
		uint32_t m_colorCount = 0U;
		refctd<ImageResource> m_depth;

		refctd<nbl::video::IGPUFramebuffer> m_fb;
	};
	struct PassResources
	{
		Framebuffer m_fb[FramesInFlight];
		refctd<nbl::video::IGPURenderpass> m_renderpass;

		refctd<nbl::video::IGPUGraphicsPipeline> createGfxPipeline(
			nbl::video::ILogicalDevice* device,
			const nbl::video::IGPUPipelineLayout* layout,
			const GfxMaterial::GfxShaders& shaders,
			const nbl::asset::SVertexInputParams& vtxinput);
	};

	using createPassResources_fptr_t = bool(*)(PassResources* resources,
		nbl::video::ILogicalDevice* device,
		ResourceAllocator* alctr,
		ImageResource* colorimages[FramesInFlight],
		ImageResource* depthimage,
		uint32_t width, uint32_t height);
}
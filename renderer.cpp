#include "renderer.h"

namespace kris
{
    refctd<nbl::video::IGPURenderpass> Renderer::createRenderpass(nbl::video::ILogicalDevice* device, nbl::asset::E_FORMAT format, nbl::asset::E_FORMAT depthFormat)
	{
		const nbl::video::IGPURenderpass::SCreationParams::SColorAttachmentDescription colorAttachments[] = {
			{{
				{
					.format = format,
					.samples = nbl::video::IGPUImage::E_SAMPLE_COUNT_FLAGS::ESCF_1_BIT,
					.mayAlias = false
				},
			/*.loadOp = */nbl::video::IGPURenderpass::LOAD_OP::CLEAR,
			/*.storeOp = */nbl::video::IGPURenderpass::STORE_OP::STORE,
			/*.initialLayout = */nbl::video::IGPUImage::LAYOUT::UNDEFINED, // because we clear we don't care about contents
			/*.finalLayout = */ nbl::video::IGPUImage::LAYOUT::PRESENT_SRC // transition to presentation right away so we can skip a barrier
		}},
		nbl::video::IGPURenderpass::SCreationParams::ColorAttachmentsEnd
		};
		static nbl::video::IGPURenderpass::SCreationParams::SSubpassDescription subpasses[] = {
			{},
			nbl::video::IGPURenderpass::SCreationParams::SubpassesEnd
		};
		subpasses[0].colorAttachments[0] = { .render = {.attachmentIndex = 0,.layout = nbl::video::IGPUImage::LAYOUT::ATTACHMENT_OPTIMAL} };
		subpasses[0].depthStencilAttachment.render = { .attachmentIndex = 0,.layout = nbl::video::IGPUImage::LAYOUT::ATTACHMENT_OPTIMAL };

		const nbl::video::IPhysicalDevice::SImageFormatPromotionRequest req = {
		.originalFormat = depthFormat,
		.usages = {nbl::video::IGPUImage::EUF_RENDER_ATTACHMENT_BIT}
		};
		depthFormat = device->getPhysicalDevice()->promoteImageFormat(req, nbl::video::IGPUImage::TILING::OPTIMAL);

		const static nbl::video::IGPURenderpass::SCreationParams::SDepthStencilAttachmentDescription depthAttachments[] = {
			{{
				{
					.format = depthFormat,
					.samples = nbl::video::IGPUImage::ESCF_1_BIT,
					.mayAlias = false
				},
			/*.loadOp = */{nbl::video::IGPURenderpass::LOAD_OP::CLEAR},
			/*.storeOp = */{nbl::video::IGPURenderpass::STORE_OP::STORE},
			/*.initialLayout = */{nbl::video::IGPUImage::LAYOUT::UNDEFINED}, // because we clear we don't care about contents
			/*.finalLayout = */{nbl::video::IGPUImage::LAYOUT::ATTACHMENT_OPTIMAL} // transition to presentation right away so we can skip a barrier
		}},
		nbl::video::IGPURenderpass::SCreationParams::DepthStencilAttachmentsEnd
		};

		nbl::video::IGPURenderpass::SCreationParams::SSubpassDependency dependencies[] = {
			// wipe-transition of Color to ATTACHMENT_OPTIMAL
			{
				.srcSubpass = nbl::video::IGPURenderpass::SCreationParams::SSubpassDependency::External,
				.dstSubpass = 0,
				.memoryBarrier = {
				// last place where the depth can get modified in previous frame
				.srcStageMask = nbl::asset::PIPELINE_STAGE_FLAGS::LATE_FRAGMENT_TESTS_BIT,
				// only write ops, reads can't be made available
				.srcAccessMask = nbl::asset::ACCESS_FLAGS::DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				// destination needs to wait as early as possible
				.dstStageMask = nbl::asset::PIPELINE_STAGE_FLAGS::EARLY_FRAGMENT_TESTS_BIT,
				// because of depth test needing a read and a write
				.dstAccessMask = nbl::asset::ACCESS_FLAGS::DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | nbl::asset::ACCESS_FLAGS::DEPTH_STENCIL_ATTACHMENT_READ_BIT
			}
			// leave view offsets and flags default
			},
			// color from ATTACHMENT_OPTIMAL to PRESENT_SRC
			{
				.srcSubpass = 0,
				.dstSubpass = nbl::video::IGPURenderpass::SCreationParams::SSubpassDependency::External,
				.memoryBarrier = {
				// last place where the depth can get modified
				.srcStageMask = nbl::asset::PIPELINE_STAGE_FLAGS::COLOR_ATTACHMENT_OUTPUT_BIT,
				// only write ops, reads can't be made available
				.srcAccessMask = nbl::asset::ACCESS_FLAGS::COLOR_ATTACHMENT_WRITE_BIT
				// spec says nothing is needed when presentation is the destination
			}
			// leave view offsets and flags default
			},
			nbl::video::IGPURenderpass::SCreationParams::DependenciesEnd
		};

		nbl::video::IGPURenderpass::SCreationParams params;
		params.depthStencilAttachments = depthAttachments;
		params.subpasses = subpasses;
		params.colorAttachments = colorAttachments;
		params.dependencies = dependencies;

		return device->createRenderpass(params);
	}
}
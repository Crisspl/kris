#include "renderer.h"

namespace kris
{
	union SamplerState
	{
		struct
		{
			uint64_t wrapu : 3;
			uint64_t wrapv : 3;
			uint64_t wrapw : 3;
			uint64_t border : 3;
			uint64_t minfilter : 1;
			uint64_t maxfilter : 1;
			uint64_t mipmapmode : 1;
			uint64_t aniso_pow2 : 3;
			uint64_t cmp_enable : 1;
			uint64_t cmpfunc : 3;
			uint64_t lodbias_int : 2; // 0-3
			uint64_t lodbias_fract : 8; // unorm
			uint64_t minlod_f16 : 16;
			uint64_t maxlod_f16 : 16;
		} params;
		uint64_t id;
	};

	static uint64_t getSamplerHash(const nbl::video::IGPUSampler::SParams& params)
	{
		SamplerState state;
		state.params.wrapu = params.TextureWrapU;
		state.params.wrapv = params.TextureWrapV;
		state.params.wrapw = params.TextureWrapW;
		state.params.border = params.BorderColor;
		state.params.minfilter = params.MinFilter;
		state.params.maxfilter = params.MaxFilter;
		state.params.mipmapmode = params.MipmapMode;
		state.params.aniso_pow2 = params.AnisotropicFilter;
		state.params.cmp_enable = params.CompareEnable;
		state.params.cmpfunc = params.CompareFunc;
		
		const uint32_t lodbias_int = (uint32_t) nbl::core::floor(params.LodBias);
		// lodbias integer part must be positive and max 3 in order to fit on 2 bits
		KRIS_ASSERT(lodbias_int < 4U);
		state.params.lodbias_int = lodbias_int;
		state.params.lodbias_fract = (uint32_t) (255.f * nbl::core::fract(params.LodBias));
		state.params.minlod_f16 = f32tof16(params.MinLod);
		state.params.maxlod_f16 = f32tof16(params.MaxLod);

		return state.id;
	}

	refctd<nbl::video::IGPUSampler> Renderer::getSampler(const nbl::video::IGPUSampler::SParams& params)
	{
		const uint64_t hash = getSamplerHash(params);
		{
			auto* found = m_samplerCache.get(hash);
			if (!found)
			{
				return found[0];
			}
		}

		auto sampler = m_device->createSampler(params);
		m_samplerCache.insert(std::move(hash), refctd(sampler));

		return sampler;
	}

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
#pragma once

#include "kris_common.h"
#include "cmd_recorder.h"
#include "material.h"
#include "resource_allocator.h"

namespace kris
{

	class Renderer
	{
		enum : uint32_t
		{
			// Desc pool limits
			MaxSets = 400U,
			MaxCombinedImgSmplr = 1000U,
			MaxSamplers = 50U,
			MaxSampledImgs = 1000U,
			MaxStorageImgs = 1000U,
			MaxUniformTexelBufs = 0U,
			MaxStorageTexelBufs = 0U,
			MaxUniformBufs = 1000U,
			MaxStorageBufs = 1000U,
			MaxUniformBufsDyn = 0U,
			MaxStorageBufsDyn = 0U,
			MaxInputAttachments = 0U,
			MaxAccStructs = 0U,

			// Other limits
			
		};
		enum : uint64_t
		{
			FenceInitialVal = 0ULL,
		};

	public:
		ResourceMap resourceMap;

		BufferResource* getDefaultBufferResource()
		{
			return m_defaultBuffer.get();
		}
		ImageResource* getDefaultImageResource()
		{
			return m_defaultImage.get();
		}

		nbl::video::IGPUDescriptorSetLayout* getMtlDsl()
		{
			return m_mtlDsl.get();
		}

		Renderer() = default;

		void init(refctd<nbl::video::ILogicalDevice>&& dev, refctd<nbl::video::IGPURenderpass>&& renderpass,
			uint32_t qFamIx, ResourceAllocator* ra, uint32_t defResourcesMemTypeBitsConstraints) 
		{
			m_device = std::move(dev);
			m_renderpass = std::move(renderpass);
			m_fence = m_device->createSemaphore(FenceInitialVal);
			m_lifetimeTracker = std::make_unique<lifetime_tracker_t>(m_device.get());

			for (uint32_t i = 0U; i < FramesInFlight; ++i)
			{
				// cmd pool
				{
					m_cmdPool[i] = m_device->createCommandPool(qFamIx, 
						nbl::core::bitflag<nbl::video::IGPUCommandPool::CREATE_FLAGS>(nbl::video::IGPUCommandPool::CREATE_FLAGS::TRANSIENT_BIT) | nbl::video::IGPUCommandPool::CREATE_FLAGS::RESET_COMMAND_BUFFER_BIT);

					m_cmdPool[i]->createCommandBuffers(nbl::video::IGPUCommandPool::BUFFER_LEVEL::PRIMARY, { &m_cmdbufPasses[i][0],Material::NumPasses });
				}
				// desc pool
				{
					nbl::video::IDescriptorPool::SCreateInfo ci;
					ci.flags = nbl::video::IDescriptorPool::ECF_NONE;
					ci.maxSets = MaxSets;
					ci.maxDescriptorCount[(uint32_t)nbl::asset::IDescriptor::E_TYPE::ET_SAMPLER] = MaxSamplers;
					ci.maxDescriptorCount[(uint32_t)nbl::asset::IDescriptor::E_TYPE::ET_COMBINED_IMAGE_SAMPLER] = MaxCombinedImgSmplr;
					ci.maxDescriptorCount[(uint32_t)nbl::asset::IDescriptor::E_TYPE::ET_SAMPLED_IMAGE] = MaxSampledImgs;
					ci.maxDescriptorCount[(uint32_t)nbl::asset::IDescriptor::E_TYPE::ET_STORAGE_IMAGE] = MaxStorageImgs;
					ci.maxDescriptorCount[(uint32_t)nbl::asset::IDescriptor::E_TYPE::ET_UNIFORM_TEXEL_BUFFER] = MaxUniformTexelBufs;
					ci.maxDescriptorCount[(uint32_t)nbl::asset::IDescriptor::E_TYPE::ET_STORAGE_TEXEL_BUFFER] = MaxStorageTexelBufs;
					ci.maxDescriptorCount[(uint32_t)nbl::asset::IDescriptor::E_TYPE::ET_UNIFORM_BUFFER] = MaxUniformBufs;
					ci.maxDescriptorCount[(uint32_t)nbl::asset::IDescriptor::E_TYPE::ET_STORAGE_BUFFER] = MaxStorageBufs;
					ci.maxDescriptorCount[(uint32_t)nbl::asset::IDescriptor::E_TYPE::ET_UNIFORM_BUFFER_DYNAMIC] = MaxUniformBufsDyn;
					ci.maxDescriptorCount[(uint32_t)nbl::asset::IDescriptor::E_TYPE::ET_STORAGE_BUFFER_DYNAMIC] = MaxStorageBufsDyn;
					ci.maxDescriptorCount[(uint32_t)nbl::asset::IDescriptor::E_TYPE::ET_INPUT_ATTACHMENT] = MaxInputAttachments;
					ci.maxDescriptorCount[(uint32_t)nbl::asset::IDescriptor::E_TYPE::ET_ACCELERATION_STRUCTURE] = MaxAccStructs;

					m_descPool[i] = m_device->createDescriptorPool(ci);
				}
			}

			// material ds layout
			{
				nbl::video::IGPUDescriptorSetLayout::SBinding bindings[Material::BindingSlot::BindingSlotCount];
				uint32_t b_ix = 0;
				for (auto& b : bindings)
				{
					const uint32_t b_num = b_ix++;

					b.binding = b_num;
					b.count = 1;
					b.immutableSamplers = nullptr;
					b.stageFlags = nbl::core::bitflag<nbl::asset::IShader::E_SHADER_STAGE>(nbl::hlsl::ESS_VERTEX) | nbl::hlsl::ESS_FRAGMENT | nbl::hlsl::ESS_COMPUTE;
					b.createFlags = nbl::video::IGPUDescriptorSetLayout::SBinding::E_CREATE_FLAGS::ECF_NONE;
					b.type = Material::isTextureBindingSlot((Material::BindingSlot)b_num) ?
						nbl::asset::IDescriptor::E_TYPE::ET_COMBINED_IMAGE_SAMPLER :
						nbl::asset::IDescriptor::E_TYPE::ET_STORAGE_BUFFER;
				}
				m_mtlDsl = m_device->createDescriptorSetLayout({ bindings,Material::BindingSlot::BindingSlotCount });
			}

			// material ppln layout
			{
				static_assert(CommandRecorder::MaterialDescSetIndex == 0, "If MaterialDescSetIndex is not 0, this pipeline layout creation needs to be altered!");

				m_mtlPplnLayout = m_device->createPipelineLayout({}, refctd(m_mtlDsl));
			}

			//Default resources
			// default buffer:
			{
				nbl::video::IGPUBuffer::SCreationParams ci = {};
				ci.usage = nbl::video::IGPUBuffer::EUF_STORAGE_BUFFER_BIT;// TODO probably should be all of them, NONE doesnt work as all
				ci.size = 64ULL;
				m_defaultBuffer = ra->allocBuffer(m_device.get(), std::move(ci), defResourcesMemTypeBitsConstraints);
			}
			// default image:
			{
				nbl::video::IGPUImage::SCreationParams ci;
				ci.tiling = nbl::video::IGPUImage::TILING::LINEAR;
				ci.arrayLayers = 1;
				ci.mipLevels = 1;
				ci.extent = { 2,2,1 };
				ci.format = nbl::asset::EF_R8_UNORM;
				ci.type = nbl::video::IGPUImage::ET_2D;
				ci.samples = nbl::video::IGPUImage::ESCF_1_BIT;
				m_defaultImage = ra->allocImage(m_device.get(), std::move(ci), defResourcesMemTypeBitsConstraints);
			}

			resourceMap.slots[DefaultBufferResourceMapSlot] = getDefaultBufferResource();
			resourceMap.slots[DefaultImageResourceMapSlot] = getDefaultImageResource();

			// fill rmap with default resources
			for (uint32_t i = FirstUsableResourceMapSlot; i < ResourceMapSlots; ++i)
			{
				resourceMap.slots[i] = getDefaultBufferResource();
			}
		}

		DescriptorSet createDescriptorSetForMaterial()
		{
			// TODO why do we actually have 3 desc pools? Read about desc pools management
			auto* dsl = getMtlDsl();
			auto nabla_ds = m_descPool[getCurrentFrameIx()]->createDescriptorSet(refctd<nbl::video::IGPUDescriptorSetLayout>(dsl));
			KRIS_ASSERT(nabla_ds);
			auto ds = DescriptorSet(std::move(nabla_ds));

			for (uint32_t b = 0U; b < DescriptorSet::MaxBindings; ++b)
			{
				ds.m_resources[b] = Material::isTextureBindingSlot((Material::BindingSlot)b) ? 
					refctd<Resource>(getDefaultImageResource()) : 
					refctd<Resource>(getDefaultBufferResource());
			}

			return ds;
		}

		template <typename MtlType>
		refctd<MtlType> createMaterial(uint32_t passMask, uint32_t bndMask)
		{
			auto mtl = nbl::core::make_smart_refctd_ptr<MtlType>(passMask, bndMask);
			mtl->m_creatorRenderer = this;
			for (uint32_t i = 0U; i < FramesInFlight; ++i)
			{
				kris::DescriptorSet ds = createDescriptorSetForMaterial();
				mtl->m_ds3[i] = std::move(ds);
			}

			return mtl;
		}

		// After creating via this call, material still has needs pipeline(s) and bindings filled
		refctd<GfxMaterial> createGfxMaterial(uint32_t passMask, uint32_t bndMask)
		{
			return createMaterial<GfxMaterial>(passMask, bndMask);
		}
		// After creating via this call, material still has needs pipeline(s) and bindings filled
		refctd<ComputeMaterial> createComputeMaterial(uint32_t passMask, uint32_t bndMask)
		{
			return createMaterial<ComputeMaterial>(passMask, bndMask);
		}

		refctd<nbl::video::IGPUGraphicsPipeline> createGraphicsPipelineForMaterial(Material::EPass pass, const GfxMaterial::GfxShaders& shaders, const nbl::asset::SVertexInputParams& vtxinput)
		{
			KRIS_UNUSED_PARAM(pass);

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
			params[0].layout = m_mtlPplnLayout.get();
			params[0].shaders = specInfo;
			params[0].cached = {
				.vertexInput = vtxinput,
				.primitiveAssembly = assemblyParams,
				.rasterization = rasterParams,
				.blend = blendParams,
			};
			params[0].renderpass = m_renderpass.get();

			kris::refctd<nbl::video::IGPUGraphicsPipeline> gfx;

			bool result = m_device->createGraphicsPipelines(nullptr, params, &gfx);
			KRIS_ASSERT(result);

			return gfx;
		}

		refctd<nbl::video::IGPUComputePipeline> createComputePipelineForMaterial(nbl::asset::ICPUShader* cpucomp)
		{
			auto comp = m_device->createShader(cpucomp);

			nbl::video::IGPUComputePipeline::SCreationParams params;
			params.layout = m_mtlPplnLayout.get();
			params.shader.entryPoint = "main";
			params.shader.shader = comp.get();

			refctd<nbl::video::IGPUComputePipeline> pipeline;

			bool result = m_device->createComputePipelines(nullptr, { &params,1 }, &pipeline);
			KRIS_ASSERT(result);

			return pipeline;
		}

		refctd<nbl::video::IGPUShader> createShader(const nbl::asset::ICPUShader* cpushader)
		{
			return m_device->createShader(cpushader);
		}

		refctd<nbl::video::IGPURenderpass> createRenderpass(nbl::video::ILogicalDevice* device, nbl::asset::E_FORMAT format, nbl::asset::E_FORMAT depthFormat);

		CommandRecorder createCommandRecorder(Material::EPass pass)
		{
			refctd<nbl::video::IGPUCommandBuffer>& cmdbuf = m_cmdbufPasses[getCurrentFrameIx()][pass];
			
			return CommandRecorder(getCurrentFrameIx(), pass, std::move(cmdbuf));
		}

		void consumeAsPass(Material::EPass pass, CommandRecorder&& cmdrec)
		{
			KRIS_ASSERT(pass == cmdrec.pass);

			CommandRecorder::Result result;
			cmdrec.endAndObtainResources(result);

			m_cmdbufPasses[getCurrentFrameIx()][pass] = std::move(result.cmdbuf);
			nbl::video::ISemaphore::SWaitInfo wi;
			m_lifetimeTracker->latch({.semaphore = m_fence.get(), .value = m_currentFrameVal }, std::move(result.resources));
		}

		bool beginFrame()
		{
			if (m_currentFrameVal > FramesInFlight)
			{
				const nbl::video::ISemaphore::SWaitInfo wi[1] =
				{
					{
						.semaphore = m_fence.get(),
						.value = m_currentFrameVal - FramesInFlight
					}
				};
				if (m_device->blockForSemaphores(wi) != nbl::video::ISemaphore::WAIT_RESULT::SUCCESS)
					return false;
			}

			for (uint32_t p = 0U; p < Material::NumPasses; ++p)
			{
				m_cmdbufPasses[getCurrentFrameIx()][p]->reset(nbl::video::IGPUCommandBuffer::RESET_FLAGS::RELEASE_RESOURCES_BIT);
			}

			return true;
		}

		nbl::video::IQueue::SSubmitInfo::SSemaphoreInfo submit(nbl::video::IQueue* cmdq, nbl::core::bitflag<nbl::asset::PIPELINE_STAGE_FLAGS> flags)
		{
			nbl::video::IQueue::SSubmitInfo::SCommandBufferInfo cmdbufs[Material::NumPasses];
			for (uint32_t i = 0U; i < Material::NumPasses; ++i)
				cmdbufs[i].cmdbuf = m_cmdbufPasses[getCurrentFrameIx()][i].get();

			nbl::video::IQueue::SSubmitInfo submitInfos[1] = {};
			submitInfos[0].commandBuffers = cmdbufs;
			const nbl::video::IQueue::SSubmitInfo::SSemaphoreInfo signals[] = { 
				{	.semaphore = m_fence.get(), 
					.value = m_currentFrameVal,
					.stageMask = flags} };
			submitInfos[0].signalSemaphores = signals;

			cmdq->submit(submitInfos);

			return signals[0];
		}

		bool endFrame()
		{
			m_lifetimeTracker->poll();

			m_currentFrameVal++;
			return true;
		}

		// blockFor*Frame() MUST BE CALLED BEFORE endFrame()
		bool blockForCurrentFrame()
		{
			return blockForFrame(m_currentFrameVal);
		}
		bool blockForPrevFrame()
		{
			if (m_currentFrameVal <= FenceInitialVal + 1ULL)
				return true;
			return blockForFrame(m_currentFrameVal - 1ULL);
		}

		uint64_t getCurrentFrameIx() const { return m_currentFrameVal % FramesInFlight; }

	private:
		bool blockForFrame(uint64_t val)
		{
			const nbl::video::ISemaphore::SWaitInfo waitInfos[1] = { {
				.semaphore = m_fence.get(),
				.value = val
			} };
			return m_device->blockForSemaphores(waitInfos) == nbl::video::ISemaphore::WAIT_RESULT::SUCCESS;
		}

		uint64_t m_currentFrameVal = FenceInitialVal + 1ULL;

		refctd<nbl::video::ILogicalDevice> m_device;
		refctd<nbl::video::IGPURenderpass> m_renderpass;

		refctd<nbl::video::ISemaphore> m_fence;

		refctd<nbl::video::IGPUCommandPool> m_cmdPool[FramesInFlight];
		refctd<nbl::video::IDescriptorPool> m_descPool[FramesInFlight];
		using lifetime_tracker_t = nbl::video::MultiTimelineEventHandlerST<DeferredAllocDeletion, false>;
		std::unique_ptr<lifetime_tracker_t> m_lifetimeTracker;

		refctd<nbl::video::IGPUCommandBuffer> m_cmdbufPasses[FramesInFlight][Material::NumPasses];

		refctd<nbl::video::IGPUDescriptorSetLayout> m_mtlDsl;
		refctd<nbl::video::IGPUPipelineLayout> m_mtlPplnLayout;

		refctd<BufferResource> m_defaultBuffer;
		refctd<ImageResource> m_defaultImage;
	};
}
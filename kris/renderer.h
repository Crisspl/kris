#pragma once

#include "kris_common.h"
#include "cmd_recorder.h"
#include "material.h"
#include "mesh.h"
#include "resource_allocator.h"
#include "CCamera.hpp"

#include "passes/pass_common.h"
#include "passes/base_pass.h"

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
			MaxCachedSamplers = MaxSamplers,
			
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

		Renderer() : m_samplerCache(MaxCachedSamplers)
		{

		}

		void init(refctd<nbl::video::ILogicalDevice>&& dev, nbl::video::ISwapchain* sc, nbl::asset::E_FORMAT depthFormat,
			uint32_t qFamIx, ResourceAllocator* ra, uint32_t defResourcesMemTypeBitsConstraints) 
		{
			m_device = std::move(dev);

			// init pass resources
			{
				const auto& sharedParams = sc->getCreationParameters().sharedParams;
				const uint32_t w = sharedParams.width;
				const uint32_t h = sharedParams.height;

				createPassResources_fptr_t createPassResources_table[NumPasses] = { };
				createPassResources_table[BasePass] = &base_pass::createPassResources;

				refctd<ImageResource> scimages_refctd[FramesInFlight];
				ImageResource* scimages[FramesInFlight];
				KRIS_ASSERT(sc->getImageCount() == FramesInFlight);

				for (uint32_t i = 0U; i < FramesInFlight; ++i)
				{
					scimages_refctd[i] = ra->registerExternalImage(sc->createImage(i));
					scimages[i] = scimages_refctd[i].get();
				}

				// depth image
				refctd<ImageResource> depthimage;
				{
					nbl::video::IGPUImage::SCreationParams ci = {};
					ci.type = nbl::video::IGPUImage::ET_2D;
					ci.samples = nbl::video::IGPUImage::ESCF_1_BIT;
					ci.format = depthFormat;
					ci.extent = { sharedParams.width,sharedParams.height,1 };
					ci.mipLevels = 1U;
					ci.arrayLayers = 1U;
					ci.depthUsage = nbl::video::IGPUImage::EUF_RENDER_ATTACHMENT_BIT;

					depthimage = ra->allocImage(m_device.get(), std::move(ci), m_device->getPhysicalDevice()->getDeviceLocalMemoryTypeBits());
				}

				for (uint32_t pass = 0U; pass < NumPasses; ++pass)
				{
					createPassResources_table[pass](m_passResources + pass,
						m_device.get(),
						ra,
						scimages,
						depthimage.get(),
						w, h);
				}
			}

			m_fence = m_device->createSemaphore(FenceInitialVal);
			m_lifetimeTracker = std::make_unique<lifetime_tracker_t>(m_device.get());
			// cmd pool
			m_cmdPool = m_device->createCommandPool(qFamIx,
				nbl::core::bitflag<nbl::video::IGPUCommandPool::CREATE_FLAGS>(nbl::video::IGPUCommandPool::CREATE_FLAGS::TRANSIENT_BIT));

			for (uint32_t i = 0U; i < FramesInFlight; ++i)
			{
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

			// Camera resources
// cam data buffer
			{
				nbl::video::IGPUBuffer::SCreationParams ci = {};
				ci.usage = nbl::core::bitflag(nbl::video::IGPUBuffer::EUF_UNIFORM_BUFFER_BIT) |
					nbl::video::IGPUBuffer::EUF_TRANSFER_DST_BIT |
					nbl::video::IGPUBuffer::EUF_INLINE_UPDATE_VIA_CMDBUF;
				ci.size = sizeof(nbl::asset::SBasicViewParameters);
				m_camResources.camDataBuffer = ra->allocBuffer(m_device.get(), std::move(ci), m_device->getPhysicalDevice()->getDeviceLocalMemoryTypeBits());
				KRIS_ASSERT(m_camResources.camDataBuffer);
			}

			// cam resources ds layout
			{
				nbl::video::IGPUDescriptorSetLayout::SBinding b;
				{
					b.binding = 0;
					b.count = 1;
					b.immutableSamplers = nullptr;
					b.stageFlags = nbl::hlsl::ESS_VERTEX;
					b.createFlags = nbl::video::IGPUDescriptorSetLayout::SBinding::E_CREATE_FLAGS::ECF_NONE;
					b.type = nbl::asset::IDescriptor::E_TYPE::ET_UNIFORM_BUFFER;
				}
				m_camResources.camDsl = m_device->createDescriptorSetLayout({ &b, 1 });
				KRIS_ASSERT(m_camResources.camDsl);
			}

			// cam resources ds
			{
				m_camResources.camDs = m_descPool[0]->createDescriptorSet(refctd<nbl::video::IGPUDescriptorSetLayout>(m_camResources.camDsl));
				KRIS_ASSERT(m_camResources.camDs);

				nbl::video::IGPUDescriptorSet::SWriteDescriptorSet w;
				nbl::video::IGPUDescriptorSet::SDescriptorInfo info;

				info.desc = refctd<nbl::video::IGPUBuffer>(m_camResources.camDataBuffer->getBuffer());
				info.info.buffer = { .offset = 0,.size = m_camResources.camDataBuffer->getSize() };
				w = { .dstSet = m_camResources.camDs.get(), .binding = 0, .arrayElement = 0U, .count = 1U, .info = &info };

				m_device->updateDescriptorSets({ &w, 1 }, {});
			}

			// mesh ds layout
			{
				nbl::video::IGPUDescriptorSetLayout::SBinding b;
				{
					b.binding = 0;
					b.count = 1;
					b.immutableSamplers = nullptr;
					b.stageFlags = nbl::hlsl::ESS_VERTEX;
					b.createFlags = nbl::video::IGPUDescriptorSetLayout::SBinding::E_CREATE_FLAGS::ECF_NONE;
					b.type = nbl::asset::IDescriptor::E_TYPE::ET_UNIFORM_BUFFER;
				}
				m_sceneNodeDsl = m_device->createDescriptorSetLayout({ &b, 1 });
				KRIS_ASSERT(m_sceneNodeDsl);
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
					b.type = Material::getDescTypeFromBndNum((Material::BindingSlot)b_num);
				}
				m_mtlDsl = m_device->createDescriptorSetLayout({ bindings,Material::BindingSlot::BindingSlotCount });
			}

			// material ppln layout
			{
				static_assert(CameraDescSetIndex == 1U, "If CameraDescSetIndex is not 1, this pipeline layout creation needs to be altered!");
				static_assert(SceneNodeDescSetIndex == 2U, "If SceneNodeDescSetIndex is not 2, this pipeline layout creation needs to be altered!");
				static_assert(MaterialDescSetIndex == 3U, "If MaterialDescSetIndex is not 3, this pipeline layout creation needs to be altered!");

				m_mtlPplnLayout = m_device->createPipelineLayout({}, nullptr, refctd(m_camResources.camDsl), refctd(m_sceneNodeDsl), refctd(m_mtlDsl));
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

		const Framebuffer& getFramebuffer(uint32_t pass, uint32_t imgAcq)
		{
			return m_passResources[pass].m_fb[imgAcq];
		}

		MaterialDescriptorSet createDescriptorSetForMaterial()
		{
			// TODO why do we actually have 3 desc pools? Read about desc pools management
			auto* dsl = getMtlDsl();
			auto nabla_ds = m_descPool[getCurrentFrameIx()]->createDescriptorSet(refctd<nbl::video::IGPUDescriptorSetLayout>(dsl));
			KRIS_ASSERT(nabla_ds);
			auto ds = MaterialDescriptorSet(std::move(nabla_ds));

			for (uint32_t b = 0U; b < MaterialDescriptorSet::MaxBindings; ++b)
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
				kris::MaterialDescriptorSet ds = createDescriptorSetForMaterial();
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

		refctd<nbl::video::IGPUGraphicsPipeline> createGraphicsPipelineForMaterial(EPass pass, const GfxMaterial::GfxShaders& shaders, const nbl::asset::SVertexInputParams& vtxinput)
		{
			return m_passResources[pass].createGfxPipeline(m_device.get(), m_mtlPplnLayout.get(), shaders, vtxinput);
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

		refctd<nbl::video::IGPUSampler> getSampler(const nbl::video::IGPUSampler::SParams& params);

		static refctd<nbl::video::IGPURenderpass> createRenderpass(nbl::video::ILogicalDevice* device, nbl::asset::E_FORMAT format, nbl::asset::E_FORMAT depthFormat);

		// creates cmdbuf in recording state
		refctd<nbl::video::IGPUCommandBuffer> createCommandBuffer(nbl::video::IGPUCommandBuffer::USAGE usage = nbl::video::IGPUCommandBuffer::USAGE::ONE_TIME_SUBMIT_BIT)
		{
			refctd<nbl::video::IGPUCommandBuffer> cmdbuf;
			m_cmdPool->createCommandBuffers(nbl::video::IGPUCommandPool::BUFFER_LEVEL::PRIMARY, 1U, &cmdbuf);
			cmdbuf->begin(usage);
			return cmdbuf;
		}

		CommandRecorder createCommandRecorder(EPass pass = EPass::NumPasses)
		{
			refctd<nbl::video::IGPUCommandBuffer> cmdbuf = createCommandBuffer();

			if (pass != EPass::NumPasses)
			{
				cmdbuf->bindDescriptorSets(nbl::asset::EPBP_GRAPHICS, m_mtlPplnLayout.get(), CameraDescSetIndex, 1U, &m_camResources.camDs.get());
			}

			return CommandRecorder(getCurrentFrameIx(), pass, std::move(cmdbuf));
		}

		SceneNodeDescriptorSet createSceneNodeDescriptorSet()
		{
			return SceneNodeDescriptorSet(m_descPool[0]->createDescriptorSet(refctd(m_sceneNodeDsl)));
		}

		// TODO: refactor consume functions into common code
		void consumeAsTransfer(CommandRecorder&& cmdrec)
		{
			CommandRecorder::Result result;
			cmdrec.endAndObtainResources(result);

			m_cmdbuf_Transfer = std::move(result.cmdbuf);
			nbl::video::ISemaphore::SWaitInfo wi;
			m_lifetimeTracker->latch({ .semaphore = m_fence.get(), .value = m_currentFrameVal }, std::move(result.resources));
		}

		void consumeAsPass(EPass pass, CommandRecorder&& cmdrec)
		{
			KRIS_ASSERT(pass == cmdrec.pass);

			CommandRecorder::Result result;
			cmdrec.endAndObtainResources(result);

			m_cmdbuf_Passes[pass] = std::move(result.cmdbuf);
			nbl::video::ISemaphore::SWaitInfo wi;
			m_lifetimeTracker->latch({.semaphore = m_fence.get(), .value = m_currentFrameVal }, std::move(result.resources));
		}

		bool beginFrame(const Camera* cam)
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

			// setup commands
			{
				auto cmdbuf = createCommandBuffer();
				
				// bind camera ds
				{
					cmdbuf->bindDescriptorSets(nbl::asset::EPBP_GRAPHICS, m_mtlPplnLayout.get(), CameraDescSetIndex, 1U, &m_camResources.camDs.get());
				}

				// camera
				{
					nbl::asset::SBasicViewParameters camdata;

					getCamDataContents(cam, &camdata);

					nbl::asset::SBufferRange<nbl::video::IGPUBuffer> range;
					range.buffer = refctd<nbl::video::IGPUBuffer>(m_camResources.camDataBuffer->getBuffer());
					range.size = range.buffer->getSize();

					cmdbuf->updateBuffer(range, &camdata);
				}

				cmdbuf->end();
				m_cmdbuf_Setup = std::move(cmdbuf);
			}

			return true;
		}

		nbl::video::IQueue::SSubmitInfo::SSemaphoreInfo submitFrame(nbl::video::IQueue* cmdq, nbl::core::bitflag<nbl::asset::PIPELINE_STAGE_FLAGS> flags)
		{
			constexpr uint32_t NumSetupCmdbufs = 1U;
			constexpr uint32_t NumTransferCmdbufs = 1U;
			constexpr uint32_t NumCmdbufs = NumSetupCmdbufs + NumTransferCmdbufs + NumPasses;
			constexpr uint32_t NumNonPassCmdbufs = NumCmdbufs - NumPasses;

			constexpr uint32_t SetupCmdbufIx = 0U;
			constexpr uint32_t TransferCmdbufIx = 1U;

			nbl::video::IQueue::SSubmitInfo::SCommandBufferInfo cmdbufs[NumCmdbufs];
			{
				// setup
				{
					KRIS_ASSERT(m_cmdbuf_Setup);
					cmdbufs[SetupCmdbufIx].cmdbuf = m_cmdbuf_Setup.get();
				}
				// transfer
				{
					KRIS_ASSERT(m_cmdbuf_Transfer);
					cmdbufs[TransferCmdbufIx].cmdbuf = m_cmdbuf_Transfer.get();
				}
				// passes
				{
					auto* passesInfo = cmdbufs + NumNonPassCmdbufs;
					for (uint32_t i = 0U; i < NumPasses; ++i)
					{
						KRIS_ASSERT(m_cmdbuf_Passes[i]);
						passesInfo[i].cmdbuf = m_cmdbuf_Passes[i].get();
					}
				}
			}

			nbl::video::IQueue::SSubmitInfo submitInfos[1] = {};
			submitInfos[0].commandBuffers = { cmdbufs, NumCmdbufs };
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

		// blockFor[Current/Prev]Frame() MUST BE CALLED BEFORE endFrame()
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
		void getCamDataContents(const Camera* cam, nbl::asset::SBasicViewParameters* camdata)
		{
			const auto viewMatrix = cam->getViewMatrix();
			const auto viewProjectionMatrix = cam->getConcatenatedMatrix();

			nbl::core::matrix3x4SIMD modelMatrix;
			modelMatrix.setTranslation(nbl::core::vectorSIMDf(0, 0, 0, 0));
			modelMatrix.setRotation(nbl::core::quaternion(0, 0, 0));

			nbl::core::matrix3x4SIMD modelViewMatrix = nbl::core::concatenateBFollowedByA(viewMatrix, modelMatrix);
			nbl::core::matrix4SIMD modelViewProjectionMatrix = nbl::core::concatenateBFollowedByA(viewProjectionMatrix, modelMatrix);

			nbl::core::matrix3x4SIMD normalMatrix;
			modelViewMatrix.getSub3x3InverseTranspose(normalMatrix);

			nbl::asset::SBasicViewParameters& uboData = camdata[0];
			memcpy(uboData.MVP, modelViewProjectionMatrix.pointer(), sizeof(uboData.MVP));
			memcpy(uboData.MV, modelViewMatrix.pointer(), sizeof(uboData.MV));
			memcpy(uboData.NormalMat, normalMatrix.pointer(), sizeof(uboData.NormalMat));
		}
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
		PassResources m_passResources[NumPasses];

		refctd<nbl::video::ISemaphore> m_fence;

		refctd<nbl::video::IGPUCommandPool> m_cmdPool;
		refctd<nbl::video::IDescriptorPool> m_descPool[FramesInFlight];
		using lifetime_tracker_t = nbl::video::MultiTimelineEventHandlerST<DeferredAllocDeletion, false>;
		std::unique_ptr<lifetime_tracker_t> m_lifetimeTracker;

		refctd<nbl::video::IGPUCommandBuffer> m_cmdbuf_Setup;
		refctd<nbl::video::IGPUCommandBuffer> m_cmdbuf_Transfer;
		refctd<nbl::video::IGPUCommandBuffer> m_cmdbuf_Passes[NumPasses];

		// camera ds resources
		struct {
			refctd<BufferResource> camDataBuffer;
			refctd<nbl::video::IGPUDescriptorSetLayout> camDsl;
			refctd<nbl::video::IGPUDescriptorSet> camDs;
		} m_camResources;

		refctd<nbl::video::IGPUDescriptorSetLayout> m_sceneNodeDsl;

		refctd<nbl::video::IGPUDescriptorSetLayout> m_mtlDsl;
		refctd<nbl::video::IGPUPipelineLayout> m_mtlPplnLayout;

		refctd<BufferResource> m_defaultBuffer;
		refctd<ImageResource> m_defaultImage;

		nbl::core::LRUCache<uint64_t, refctd<nbl::video::IGPUSampler>> m_samplerCache;
	};
}
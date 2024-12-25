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
			MaxCombinedImgSmplr = 0U,
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

		Renderer(refctd<nbl::video::ILogicalDevice>&& dev, uint32_t qFamIx, ResourceAllocator* ra, uint32_t defResourcesMemTypeBitsConstraints) :
			m_device(std::move(dev)),
			m_fence(m_device->createSemaphore(FenceInitialVal)),
			m_lifetimeTracker(m_device.get())
		{
			for (uint32_t i = 0U; i < FramesInFlight; ++i)
			{
				// cmd pool
				{
					m_cmdPool[i] = m_device->createCommandPool(qFamIx, nbl::video::IGPUCommandPool::CREATE_FLAGS::TRANSIENT_BIT);
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

			{
				nbl::video::IGPUBuffer::SCreationParams ci = {};
				ci.usage = nbl::video::IGPUBuffer::EUF_STORAGE_BUFFER_BIT;// nbl::video::IGPUBuffer::EUF_NONE;
				ci.size = 64ULL;
				m_defaultBuffer = ra->allocBuffer(m_device.get(), std::move(ci), defResourcesMemTypeBitsConstraints);
			}
			// TODO: create default buffer

			// fill rmap with default resources
			for (uint32_t i = 0U; i < ResourceMapSlots; ++i)
			{
				resourceMap.slots[i] = getDefaultBufferResource();
			}
		}

		DescriptorSet createDescriptorSet(nbl::video::IGPUDescriptorSetLayout* layout)
		{
			auto nabla_ds = m_descPool[m_currentFrameVal & FramesInFlight]->createDescriptorSet(refctd<nbl::video::IGPUDescriptorSetLayout>(layout));
			auto ds = DescriptorSet(std::move(nabla_ds));

			for (uint32_t b = 0U; b < DescriptorSet::MaxBindings; ++b)
			{
				ds.m_resources[b] = refctd<Resource>(getDefaultBufferResource());
			}

			return ds;
		}

		CommandRecorder createCommandRecorder()
		{
			refctd<nbl::video::IGPUCommandBuffer> cmdbuf;
			m_cmdPool[m_currentFrameVal & FramesInFlight]->createCommandBuffers(nbl::video::IGPUCommandPool::BUFFER_LEVEL::PRIMARY, { &cmdbuf, 1 });
			return CommandRecorder(std::move(cmdbuf));
		}

		void consumeAsPass(Material::EPass pass, CommandRecorder&& cmdrec)
		{
			CommandRecorder::Result result;
			cmdrec.endAndObtainResources(result);

			m_cmdbufPasses[pass] = std::move(result.cmdbuf);
			nbl::video::ISemaphore::SWaitInfo wi;
			m_lifetimeTracker.latch({.semaphore = m_fence.get(), .value = m_currentFrameVal }, std::move(result.resources));
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
			return true;
		}

		void submit(nbl::video::IQueue* cmdq, nbl::core::bitflag<nbl::asset::PIPELINE_STAGE_FLAGS> flags)
		{
			nbl::video::IQueue::SSubmitInfo::SCommandBufferInfo cmdbufs[Material::NumPasses];
			for (uint32_t i = 0U; i < Material::NumPasses; ++i)
				cmdbufs[i].cmdbuf = m_cmdbufPasses[i].get();

			nbl::video::IQueue::SSubmitInfo submitInfos[1] = {};
			submitInfos[0].commandBuffers = cmdbufs;
			const nbl::video::IQueue::SSubmitInfo::SSemaphoreInfo signals[] = { 
				{	.semaphore = m_fence.get(), 
					.value = m_currentFrameVal,
					.stageMask = flags} };
			submitInfos[0].signalSemaphores = signals;

			cmdq->submit(submitInfos);
		}

		bool endFrame()
		{
			// cmdbufs are held by queue until finished execution so we can drop them here safely without CPU wait
			for (uint32_t i = 0U; i < Material::NumPasses; ++i)
			{
				m_cmdbufPasses[i] = nullptr;
			}

			m_lifetimeTracker.poll();

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

		uint64_t getCurrentFrame() const { return m_currentFrameVal; }

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

		refctd<nbl::video::ISemaphore> m_fence;

		refctd<nbl::video::IGPUCommandPool> m_cmdPool[FramesInFlight];
		refctd<nbl::video::IDescriptorPool> m_descPool[FramesInFlight];
		nbl::video::MultiTimelineEventHandlerST<DeferredAllocDeletion, false> m_lifetimeTracker;

		refctd<nbl::video::IGPUCommandBuffer> m_cmdbufPasses[Material::NumPasses];

		refctd<BufferResource> m_defaultBuffer;
		refctd<ImageResource> m_defaultImage;
	};
}
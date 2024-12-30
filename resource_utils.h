#pragma once

#include "resource_allocator.h"

namespace kris
{
    class ResourceUtils final
    {
        enum : uint32_t
        {
            StagingBufSize = 1U << 24U // 16M
        };
        enum : uint64_t
        {
            FenceInitialValue = 0ULL
        };

    public:
        ResourceUtils(nbl::video::ILogicalDevice* device, uint32_t famIx, ResourceAllocator* ra) :
            m_device(device),
            m_addrAlctrScratch(KRIS_MEM_ALLOC(addr_alctr_t::reserved_size(64U, StagingBufSize, 64U))),
            m_addrAlctr(m_addrAlctrScratch, 0U, 0U, 64U, StagingBufSize, 64U)
        {
            nbl::video::IGPUBuffer::SCreationParams ci = {};
            ci.size = StagingBufSize;
            ci.usage = nbl::video::IGPUBuffer::EUF_TRANSFER_SRC_BIT;
            m_stagingResource = ra->allocBuffer(device, std::move(ci), device->getPhysicalDevice()->getHostVisibleMemoryTypeBits());
            m_stagingResource->map(nbl::video::IDeviceMemoryAllocation::EMCAF_WRITE);

            m_cmdpool = m_device->createCommandPool(famIx, nbl::video::IGPUCommandPool::CREATE_FLAGS::TRANSIENT_BIT);
            m_cmdpool->createCommandBuffers(nbl::video::IGPUCommandPool::BUFFER_LEVEL::PRIMARY, { &m_cmdbuf, 1 });

            m_fence = m_device->createSemaphore(FenceInitialValue);
        }
        ~ResourceUtils()
        {
            KRIS_MEM_FREE(m_addrAlctrScratch);
        }

        void beginTransferPass()
        {
            m_addrAlctr.reset();
            m_cmdpool->reset();
            m_cmdbuf->begin(nbl::video::IGPUCommandBuffer::USAGE::ONE_TIME_SUBMIT_BIT);
        }

        bool uploadBufferData(BufferResource* bufferResource, size_t offset, size_t size, const void* data)
        {
            if (size > m_addrAlctr.get_total_size())
                return false;

            uint32_t srcOffset = m_addrAlctr.alloc_addr((uint32_t)size, 64U);
            if (srcOffset == m_addrAlctr.invalid_address)
                return false;
            void* srcPtr = reinterpret_cast<uint8_t*>(m_stagingResource->getMappedPtr()) + srcOffset;
            memcpy(srcPtr, data, size);

            nbl::video::IGPUCommandBuffer::SBufferCopy region;
            region.dstOffset = offset;
            region.srcOffset = srcOffset;
            region.size = size;
            m_cmdbuf->copyBuffer(m_stagingResource->getBuffer(), bufferResource->getBuffer(), 1U, &region);
        }

        nbl::video::IQueue::SSubmitInfo::SSemaphoreInfo endPassAndSubmit(nbl::video::IQueue* cmdq)
        {
            KRIS_ASSERT(m_cmdpool->getQueueFamilyIndex() == cmdq->getFamilyIndex());

            m_stagingResource->invalidate(m_device);

            m_cmdbuf->end();

            nbl::video::IQueue::SSubmitInfo::SCommandBufferInfo cmdbufs[1];
            cmdbufs[0].cmdbuf = m_cmdbuf.get();

            nbl::video::IQueue::SSubmitInfo submitInfos[1] = {};
            submitInfos[0].commandBuffers = cmdbufs;
            const nbl::video::IQueue::SSubmitInfo::SSemaphoreInfo signals[] = {
                {.semaphore = m_fence.get(),
                    .value = ++m_fenceVal,
                    .stageMask = nbl::asset::PIPELINE_STAGE_FLAGS::COPY_BIT} };
            submitInfos[0].signalSemaphores = signals;

            cmdq->submit(submitInfos);

            return signals[0];
        }

        bool blockForSubmit()
        {
            const nbl::video::ISemaphore::SWaitInfo waitInfos[1] = { {
                .semaphore = m_fence.get(),
                .value = m_fenceVal
            } };
            return m_device->blockForSemaphores(waitInfos) == nbl::video::ISemaphore::WAIT_RESULT::SUCCESS;
        }

    private:
        nbl::video::ILogicalDevice* m_device;

        refctd<BufferResource> m_stagingResource;
        using addr_alctr_t = nbl::core::GeneralpurposeAddressAllocator<uint32_t>;
        void* m_addrAlctrScratch = nullptr;
        addr_alctr_t m_addrAlctr;

        refctd<nbl::video::IGPUCommandPool> m_cmdpool;
        refctd<nbl::video::IGPUCommandBuffer> m_cmdbuf;

        refctd<nbl::video::ISemaphore> m_fence;
        uint64_t m_fenceVal = FenceInitialValue;
    };
}
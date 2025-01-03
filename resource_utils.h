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

            bufferResource->lastAccesses = nbl::asset::ACCESS_FLAGS::TRANSFER_WRITE_BIT;
            bufferResource->lastStages = nbl::asset::PIPELINE_STAGE_FLAGS::COPY_BIT;

            return true;
        }

        bool uploadImageData(ImageResource* imageResource, nbl::asset::ICPUImage* srcimg)
        {
            constexpr uint32_t MaxRegions = 16U;

            nbl::video::IGPUImage::SBufferCopy regions[MaxRegions];
            uint32_t mipcount = 0U;
            {
                auto srcregions = srcimg->getRegions();
                mipcount = srcregions.size();
                KRIS_ASSERT(mipcount <= MaxRegions);
                std::copy(srcregions.begin(), srcregions.end(), regions);
            }
            
            const size_t size = srcimg->getImageDataSizeInBytes();

            if (size > m_addrAlctr.get_total_size())
                return false;
            uint32_t srcOffset = m_addrAlctr.alloc_addr((uint32_t)size, 64U);
            if (srcOffset == m_addrAlctr.invalid_address)
                return false;

            void* srcPtr = reinterpret_cast<uint8_t*>(m_stagingResource->getMappedPtr()) + srcOffset;
            memcpy(srcPtr, srcimg->getBuffer()->getPointer(), size);

            for (uint32_t mip = 0U; mip < mipcount; ++mip)
            {
                regions[mip].bufferOffset += srcOffset;
            }

            using ibarrier_t = nbl::video::IGPUCommandBuffer::SImageMemoryBarrier<nbl::video::IGPUCommandBuffer::SOwnershipTransferBarrier>;
            ibarrier_t imgbarrier =
            {
                .barrier = {
                    .dep = {
                    // first usage doesn't need to sync against anything, so leave src default
                    .srcStageMask = nbl::asset::PIPELINE_STAGE_FLAGS::NONE,
                    .srcAccessMask = nbl::asset::ACCESS_FLAGS::NONE,
                    .dstStageMask = nbl::asset::PIPELINE_STAGE_FLAGS::NONE,
                    .dstAccessMask = nbl::asset::ACCESS_FLAGS::NONE
                } 
            },
            .image = imageResource->getImage(),
            .subresourceRange = {
                .aspectMask = nbl::video::IGPUImage::EAF_COLOR_BIT,
                // all the mips at once
                .baseMipLevel = 0,
                .levelCount = mipcount,
                // all the layers
                .baseArrayLayer = 0,
                .layerCount = 1
            },
                // first use, can transition away from undefined straight into what we want
                .oldLayout = nbl::video::IGPUImage::LAYOUT::UNDEFINED,
                .newLayout = nbl::video::IGPUImage::LAYOUT::TRANSFER_DST_OPTIMAL
            };

            m_cmdbuf->pipelineBarrier(nbl::asset::E_DEPENDENCY_FLAGS::EDF_NONE, { .memBarriers = {},.bufBarriers = {},.imgBarriers = {&imgbarrier,1} });
            
            m_cmdbuf->copyBufferToImage(m_stagingResource->getBuffer(), imageResource->getImage(), 
                nbl::video::IGPUImage::LAYOUT::TRANSFER_DST_OPTIMAL, mipcount, regions);

            imageResource->lastAccesses = nbl::asset::ACCESS_FLAGS::TRANSFER_WRITE_BIT;
            imageResource->lastStages = nbl::asset::PIPELINE_STAGE_FLAGS::COPY_BIT;
            imageResource->layout = nbl::video::IGPUImage::LAYOUT::TRANSFER_DST_OPTIMAL;
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
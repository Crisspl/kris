#pragma once

#include "resource_allocator.h"
#include "cmd_recorder.h"

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

        uint32_t allocSpace(uint32_t size)
        {
            uint32_t srcOffset = m_addrAlctr.alloc_addr(size, 64U);
            KRIS_ASSERT(srcOffset != m_addrAlctr.invalid_address);
            return srcOffset;
        }
        void* getSpacePtr(uint32_t offset)
        {
            void* ptr = reinterpret_cast<uint8_t*>(m_stagingResource->getMappedPtr()) + offset;
            return ptr;
        }

    public:
        ResourceUtils(nbl::video::ILogicalDevice* device, ResourceAllocator* ra, CommandRecorder&& cmdrec) :
            m_device(device),
            m_addrAlctrScratch(KRIS_MEM_ALLOC(addr_alctr_t::reserved_size(64U, StagingBufSize, 64U))),
            m_addrAlctr(m_addrAlctrScratch, 0U, 0U, 64U, StagingBufSize, 64U),
            m_cmdrec(std::move(cmdrec))
        {
            nbl::video::IGPUBuffer::SCreationParams ci = {};
            ci.size = StagingBufSize;
            ci.usage = nbl::video::IGPUBuffer::EUF_TRANSFER_SRC_BIT;
            m_stagingResource = ra->allocBuffer(device, std::move(ci), device->getPhysicalDevice()->getHostVisibleMemoryTypeBits());
            m_stagingResource->map(nbl::video::IDeviceMemoryAllocation::EMCAF_WRITE);
        }
        ~ResourceUtils()
        {
            KRIS_MEM_FREE(m_addrAlctrScratch);
        }

        void beginTransferPass()
        {
            m_addrAlctr.reset();
        }

        bool uploadBufferData(BufferResource* bufferResource, size_t offset, size_t size, const void* data)
        {
            uint32_t srcOffset = allocSpace((uint32_t) size);
            memcpy(getSpacePtr(srcOffset), data, size);

            nbl::video::IGPUCommandBuffer::SBufferCopy region;
            region.dstOffset = offset;
            region.srcOffset = srcOffset;
            region.size = size;

            m_cmdrec.copyBuffer(m_stagingResource.get(), bufferResource, 1U, &region);

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
            uint32_t srcOffset = allocSpace((uint32_t) size);

            memcpy(getSpacePtr(srcOffset), srcimg->getBuffer()->getPointer(), size);

            for (uint32_t mip = 0U; mip < mipcount; ++mip)
            {
                regions[mip].bufferOffset += srcOffset;
            }
            
            m_cmdrec.copyBufferToImage(m_stagingResource.get(), imageResource, mipcount, regions);

            return true;
        }

        CommandRecorder& getResult() { return m_cmdrec; }

    private:
        nbl::video::ILogicalDevice* m_device;

        refctd<BufferResource> m_stagingResource;
        using addr_alctr_t = nbl::core::GeneralpurposeAddressAllocator<uint32_t>;
        void* m_addrAlctrScratch = nullptr;
        addr_alctr_t m_addrAlctr;

        CommandRecorder m_cmdrec;
    };
}
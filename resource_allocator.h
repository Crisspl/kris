#pragma once

#include "kris_common.h"

namespace kris
{
	class MemPool
	{
		using addr_alctr_t = nbl::core::GeneralpurposeAddressAllocator<uint32_t>;

	public:
		enum : uint32_t
		{
			// block is min memory unit that pool is using for internal addressing
			BlockSizeLog2 = 6U,
			BlockSize = 1U << BlockSizeLog2,

			MaxRequestableAlignment = 512U,
			//MaxRequestableAlignmentBlocks = std::max((uint32_t) MaxRequestableAlignment >> BlockSizeLog2, 1U)
		};
		enum : size_t
		{
			InvalidOffset = ~0ULL,
		};

		explicit MemPool(nbl::video::ILogicalDevice* device, size_t size, uint32_t memTypeIx) :
			m_addrAlctr_scratch(KRIS_MEM_ALLOC(addr_alctr_t::reserved_size(MaxRequestableAlignment, nbl::core::alignUp(size, BlockSize), BlockSize))),
			m_addrAlctr(
				m_addrAlctr_scratch
				, 0U
				, 0U
				, MaxRequestableAlignment
				, nbl::core::alignUp(size, BlockSize)
				, BlockSize)
		{
			nbl::video::IDeviceMemoryAllocator::SAllocateInfo ainfo = {};
			ainfo.size = m_addrAlctr.get_total_size();
			ainfo.flags = nbl::video::IDeviceMemoryAllocation::EMAF_NONE;
			ainfo.memoryTypeIndex = memTypeIx;
			ainfo.dedication = nullptr;

			m_mem = device->allocate(ainfo);
		}

		~MemPool()
		{
			KRIS_MEM_FREE(m_addrAlctr_scratch);
		}

		nbl::video::IDeviceMemoryBacked::SMemoryBinding allocate(size_t size, uint32_t alignment)
		{
			nbl::video::IDeviceMemoryBacked::SMemoryBinding binding;
			binding.memory = nullptr;
			binding.offset = 0U;

			size_t offset = allocateOffset(size, alignment);
			if (offset == InvalidOffset)
			{
				return binding;
			}

			binding.memory = m_mem.memory.get();
			binding.offset = m_mem.offset + offset;

			return binding;
		}

		// Returns true if pool is empty and can be deleted
		bool deallocate(const nbl::video::IDeviceMemoryBacked::SMemoryBinding& binding, size_t size)
		{
			KRIS_ASSERT_MSG(binding.isValid(), "Trying to deallocate invalid memory block!");
			KRIS_ASSERT_MSG(m_mem.offset <= binding.offset, "Pool's own memory offset cannot be greater than offset of block within the memory!");

			const size_t offset = binding.offset - m_mem.offset;

			return freeOffset(offset, size);
		}

	private:
		size_t allocateOffset(size_t size, uint32_t alignment)
		{
			return (size_t)m_addrAlctr.alloc_addr((uint32_t)size, alignment);
		}
		bool freeOffset(size_t offset, size_t size)
		{
			m_addrAlctr.free_addr((uint32_t)offset, (uint32_t)size);
			return (m_addrAlctr.get_free_size() == m_addrAlctr.get_total_size());
		}

		void* m_addrAlctr_scratch;
		addr_alctr_t m_addrAlctr;
		nbl::video::IDeviceMemoryAllocator::SAllocation m_mem;
	};

	class MemHeap
	{
		enum : uint32_t
		{
			PoolSizeLog2 = 26U,
			PoolSize = 1U << PoolSizeLog2,

			MaxPlacedAllocSizeLog2 = 23U,
			MaxPlacedAllocSize = 1U << MaxPlacedAllocSizeLog2,
		};

	public:
		struct Allocation
		{
			enum : uint32_t
			{
				InvalidMemTypeIx = 32U,
			};

			nbl::video::IDeviceMemoryBacked::SMemoryBinding binding;
			MemPool* pool = nullptr;
			uint32_t memTypeIx = 32U;
			bool dedicated = false;

			bool isValid() const
			{
				return pool && binding.isValid() && memTypeIx < 32U;
			}
		};

		void setMemTypeIx(uint32_t ix)
		{
			KRIS_ASSERT_MSG(m_memTypeIx >= 32U, "MemHeap: Can't set mem type index more than once!");
			if (m_memTypeIx >= 32U)
			{
				m_memTypeIx = ix;
			}
		}

		Allocation allocate(nbl::video::ILogicalDevice* device, size_t size, uint32_t alignment, bool forceDedicated)
		{
			bool dedicated = forceDedicated | (size > MaxPlacedAllocSize);

			Allocation al;
			al.memTypeIx = m_memTypeIx;
			al.dedicated = dedicated;

			if (dedicated)
			{
				m_dedicated.push_back(KRIS_MEM_NEW MemPool(device, size, m_memTypeIx));
				auto* pool = m_dedicated.back();
				al.binding = pool->allocate(size, alignment);
				al.pool = pool;
				KRIS_ASSERT_MSG(al.binding.isValid(), "Dedicated pools should always be able to allocate!");

				return al;
			}

			for (MemPool* pool : m_pools)
			{
				nbl::video::IDeviceMemoryBacked::SMemoryBinding binding = pool->allocate(size, alignment);
				if (binding.isValid())
				{
					al.binding = binding;
					al.pool = pool;

					return al;
				}
			}

			m_pools.push_back(KRIS_MEM_NEW MemPool(device, PoolSize, m_memTypeIx));
			auto* pool = m_pools.back();
			al.binding = pool->allocate(size, alignment);
			al.pool = pool;
			KRIS_ASSERT_MSG(al.binding.isValid(), "Brand new pool should always be able to allocate!");

			return al;
		}

		void deallocate(const Allocation& al, size_t size)
		{
			if (al.pool->deallocate(al.binding, size))
			{
				auto& pools = al.dedicated ? m_dedicated : m_pools;

				auto found_it = std::find(std::begin(pools), std::end(pools), al.pool);
				KRIS_ASSERT_MSG(found_it != pools.end(), "Dedicated allocation pool not found within all the pools!");
				KRIS_MEM_SAFE_DELETE(al.pool);
				pools.erase(found_it);
			}
		}

	private:
		uint32_t m_memTypeIx = 32U;
		nbl::core::vector<MemPool*> m_pools;
		nbl::core::vector<MemPool*> m_dedicated;
	};

	class ResourceAllocator
	{
		enum : uint32_t
		{
			MaxHeaps = 32U, // Max mem types
		};

	public:
		struct Allocation : public nbl::core::IReferenceCounted, public nbl::core::Uncopyable
		{
			Allocation(ResourceAllocator* a, const MemHeap::Allocation& al, refctd<nbl::video::IBackendObject>&& res, uint32_t maxViews) :
				alctr(a),
				allocation(al),
				resource(std::move(res)),
				m_id(static_getNewId()),
				m_views(maxViews)
			{
			}
			virtual ~Allocation() = default;

			bool isValidForDeallocation() const
			{
				return alctr && allocation.isValid();
			}
			bool isValid() const
			{
				return resource && isValidForDeallocation();
			}

			virtual size_t getSize() const = 0;

			void* map(const nbl::core::bitflag<nbl::video::IDeviceMemoryAllocation::E_MAPPING_CPU_ACCESS_FLAGS> flags)
			{
				void* ptr = allocation.binding.memory->map({ allocation.binding.offset, this->getSize() }, flags);
				KRIS_ASSERT_MSG(ptr, "Failed to map!");
				return getMappedPtr();
			}
			bool unmap()
			{
				return allocation.binding.memory->unmap();
			}
			void* getMappedPtr()
			{
				void* ptr = allocation.binding.memory->getMappedPointer();
				if (!ptr)
					return nullptr;
				return reinterpret_cast<uint8_t*>(ptr) + allocation.binding.offset;
			}
			bool invalidate(nbl::video::ILogicalDevice* device)
			{
				const nbl::video::ILogicalDevice::MappedMemoryRange memoryRange(
					allocation.binding.memory,
					allocation.binding.offset,
					this->getSize());
				if (!allocation.binding.memory->getMemoryPropertyFlags().hasFlags(nbl::video::IDeviceMemoryAllocation::EMPF_HOST_COHERENT_BIT))
					return device->invalidateMappedMemoryRanges(1, &memoryRange);
				return true;
			}

			bool compareIds(Allocation* other)
			{
				return m_id == other->m_id;
			}

			ResourceAllocator* alctr;
			MemHeap::Allocation allocation;
			refctd<nbl::video::IBackendObject> resource;

		protected:
			void deallocateSelf()
			{
				KRIS_ASSERT_MSG(resource->getReferenceCount() == 1,
					"Resource %s refcount in moment of memory deallocation is >1. Deallocating resource's memory before resource itself!",
					resource->getDebugName());
				size_t size = this->getSize();
				resource = nullptr;
				alctr->deallocate(this, size);
			}

			struct View : public nbl::core::IReferenceCounted, public nbl::core::Uncopyable
			{
				using id_t = uint64_t;

				id_t id;

				View(id_t _id) : id(_id) {}
				virtual ~View() = default;
			};

			View* findView(View::id_t id)
			{
				return m_views.get(id)->get();
			}

			void addView(View::id_t id, refctd<View>&& v)
			{
				m_views.insert(std::move(id), std::move(v));
			}

		private:
			static uint64_t static_getNewId();

			uint64_t m_id;
			nbl::core::LRUCache<View::id_t, refctd<View>> m_views;
		};
		struct BufferAllocation final : public Allocation
		{
		private:
			enum : uint32_t
			{
				MaxBufferViews = 20U
			};

			union BufferViewId
			{
				static_assert(sizeof(Allocation::View::id_t) == 8ULL);
				struct
				{
					uint64_t format : 8; // nbl::asset::E_FORMAT
					uint64_t offset : 24;
					uint64_t size	: 32; 
				};
				Allocation::View::id_t id;
			};

			struct BufferView : public View
			{
				refctd<nbl::video::IGPUBufferView> bufview;

				BufferView(id_t _id, refctd<nbl::video::IGPUBufferView>&& bv) : View(_id), bufview(std::move(bv)) {}
				virtual ~BufferView() = default;
			};

		public:
			BufferAllocation(ResourceAllocator* a, refctd<nbl::video::IGPUBuffer>&& buf, const MemHeap::Allocation& al) :
				Allocation(a, al, std::move(buf), MaxBufferViews)
			{
			}
			virtual ~BufferAllocation()
			{
				deallocateSelf();
			}

			refctd<nbl::video::IGPUBufferView> getView(nbl::video::ILogicalDevice* device, nbl::asset::E_FORMAT format, uint32_t offset, uint32_t size)
			{
				BufferViewId id;
				id.id = 0ULL;
				id.format = format;
				id.offset = offset;
				id.size = size;

				refctd<nbl::video::IGPUBufferView> bufview = nullptr;
				if (View* v = findView(id.id))
				{
					bufview = static_cast<BufferView*>(v)->bufview;
				}
				if (!bufview)
				{
					nbl::asset::SBufferRange<nbl::video::IGPUBuffer> range;
					range.buffer = refctd<nbl::video::IGPUBuffer>(getBuffer());
					range.offset = offset;
					range.size = size;
					bufview = device->createBufferView(range, format);
					addView(id.id, nbl::core::make_smart_refctd_ptr<BufferView>(id.id, refctd(bufview)));
				}
				return bufview;
			}

			nbl::video::IGPUBuffer* getBuffer() const { return static_cast<nbl::video::IGPUBuffer*>(resource.get()); }
			size_t getSize() const override { return getBuffer()->getSize(); }
		};
		struct ImageAllocation final : public Allocation
		{
		private:
			enum : uint32_t
			{
				MaxImageViews = 20U
			};

			union ImageViewId
			{
				static_assert(sizeof(Allocation::View::id_t) == 8ULL);
				struct
				{
					uint64_t viewtype	: 3; // nbl::asset::IImageView::E_TYPE
					uint64_t format		: 8; // nbl::asset::E_FORMAT
					uint64_t aspect		: 4;// nbl::asset::IImage::E_ASPECT_FLAGS (but only COLOR,DEPTH,STENCIL,METADATA)
					uint64_t mipOffset	: 5;
					uint64_t mipCount	: 5;
					uint64_t layerOffset: 19;
					uint64_t layerCount	: 20;
				};
				Allocation::View::id_t id;
			};

			struct ImageView : public View
			{
				refctd<nbl::video::IGPUImageView> imgview;

				ImageView(id_t _id, refctd<nbl::video::IGPUImageView>&& iv) : View(_id), imgview(std::move(iv)) {}
				virtual ~ImageView() = default;
			};

		public:
			ImageAllocation(ResourceAllocator* a, refctd<nbl::video::IGPUImage>&& img, const MemHeap::Allocation& al) :
				Allocation(a, al, std::move(img), MaxImageViews)
			{
			}
			virtual ~ImageAllocation()
			{
				deallocateSelf();
			}

			refctd<nbl::video::IGPUImageView> getView(nbl::video::ILogicalDevice* device, 
				nbl::video::IGPUImageView::E_TYPE viewtype, 
				nbl::asset::E_FORMAT format,
				nbl::core::bitflag<nbl::asset::IImage::E_ASPECT_FLAGS> aspect,
				uint32_t mipOffset, uint32_t mipCount,
				uint32_t layerOffset, uint32_t layerCount)
			{
				ImageViewId id;
				id.id = 0ULL;
				id.viewtype = viewtype;
				id.format = format;
				id.aspect = aspect.value;
				id.mipOffset = mipOffset;
				id.mipCount = mipCount;
				id.layerOffset = layerCount;
				id.layerCount = layerCount;

				refctd<nbl::video::IGPUImageView> imgview = nullptr;
				if (View* v = findView(id.id))
				{
					imgview = static_cast<ImageView*>(v)->imgview;
				}
				if (!imgview)
				{
					nbl::video::IGPUImageView::SCreationParams ci;
					ci.format = format;
					ci.viewType = viewtype;
					ci.subresourceRange.aspectMask = aspect;
					ci.subresourceRange.baseMipLevel = mipOffset;
					ci.subresourceRange.levelCount = mipCount;
					ci.subresourceRange.baseArrayLayer = layerOffset;
					ci.subresourceRange.layerCount = layerCount;
					ci.image = refctd<nbl::video::IGPUImage>(getImage());
					imgview = device->createImageView(std::move(ci));
					addView(id.id, nbl::core::make_smart_refctd_ptr<ImageView>(id.id, refctd(imgview)));
				}
				return imgview;
			}

			nbl::video::IGPUImage* getImage() const { return static_cast<nbl::video::IGPUImage*>(resource.get()); }
		};

		ResourceAllocator()
		{
			for (uint32_t ix = 0U; ix < MaxHeaps; ++ix)
			{
				m_heaps[ix].setMemTypeIx(ix);
			}
		}

		refctd<BufferAllocation> allocBuffer(nbl::video::ILogicalDevice* device, nbl::video::IGPUBuffer::SCreationParams&& params, uint32_t memTypeBitsConstraints)
		{
			const size_t size = params.size;
			refctd<nbl::video::IGPUBuffer> buf = device->createBuffer(std::move(params));

			nbl::video::IDeviceMemoryBacked::SDeviceMemoryRequirements req = device->getMemoryRequirementsForBuffer(buf.get());
			req.memoryTypeBits &= memTypeBitsConstraints;

			const uint32_t memTypeIndex = nbl::hlsl::findLSB(req.memoryTypeBits);
			MemHeap::Allocation al = m_heaps[memTypeIndex].allocate(device, size, 1U << req.alignmentLog2, false);
			{
				nbl::video::ILogicalDevice::SBindBufferMemoryInfo info[1];
				info[0].binding = al.binding;
				info[0].buffer = buf.get();
				device->bindBufferMemory(1U, info);
			}

			return nbl::core::make_smart_refctd_ptr<BufferAllocation>(this, std::move(buf), al);
		}

		void deallocate(Allocation* al, size_t size)
		{
			KRIS_ASSERT(al != nullptr);
			KRIS_ASSERT(al->isValidForDeallocation());

			m_heaps[al->allocation.memTypeIx].deallocate(al->allocation, size);
			al->allocation.binding.memory = nullptr;
			al->allocation.pool = nullptr;
			al->allocation.memTypeIx = 32U;
		}

		std::array<MemHeap, MaxHeaps> m_heaps;
	};

	using Resource = ResourceAllocator::Allocation;
	using BufferResource = ResourceAllocator::BufferAllocation;
	using ImageResource = ResourceAllocator::ImageAllocation;
}
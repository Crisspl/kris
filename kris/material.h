#pragma once

#include "kris_common.h"
#include "resource_allocator.h"

namespace kris
{
	// fwd decl, for creatorRenderer memeber
	class Renderer;

	struct ResourceMap
	{
		struct Slot
		{
			Slot& operator=(Resource* res)
			{
				resource = refctd<Resource>(res);
				return *this;
			}

			refctd<Resource> resource;
		};

		Slot& operator[](size_t ix) { return slots[ix]; }
		const Slot& operator[](size_t ix) const { return slots[ix]; }

		Slot slots[ResourceMapSlots];
	};

	class DescriptorSet : public nbl::core::Uncopyable
	{
	public:
		virtual ~DescriptorSet() = default;

		DescriptorSet() : m_ds(nullptr) {}
		DescriptorSet(DescriptorSet&& rhs) : m_ds(std::move(rhs.m_ds)) {}
		explicit DescriptorSet(refctd<nbl::video::IGPUDescriptorSet>&& ds) : m_ds(std::move(ds))
		{
			// Note: renderer fills all m_resources with defaults in createDescriptoSet
			KRIS_ASSERT(m_ds);
		}

		DescriptorSet& operator=(DescriptorSet& rhs)
		{
			m_ds = std::move(rhs.m_ds);
			return *this;
		}

		refctd<nbl::video::IGPUDescriptorSet> m_ds;

		virtual nbl::core::SRange<const refctd<Resource>> getResources() const = 0;
	};

	template <uint32_t _MaxBindings>
	class DescriptorSetTemplate final : public DescriptorSet
	{
	public:
		enum : uint32_t
		{
			MaxBindings = _MaxBindings,
			FullBndMask = (1U << MaxBindings) - 1U,
		};

		refctd<Resource> m_resources[MaxBindings];

		DescriptorSetTemplate() = default;
		explicit DescriptorSetTemplate(refctd<nbl::video::IGPUDescriptorSet>&& ds) : DescriptorSet(std::move(ds))
		{
			// Note: renderer fills all m_resources with defaults in createDescriptoSet
		}
		DescriptorSetTemplate(DescriptorSetTemplate&& rhs) : DescriptorSet(static_cast<DescriptorSet&&>(std::move(*this)))
		{
			for (uint32_t i = 0U; i < MaxBindings; ++i)
			{
				m_resources[i] = std::move(rhs.m_resources[i]);
			}
		}
		virtual ~DescriptorSetTemplate() = default;

		DescriptorSetTemplate& operator=(DescriptorSetTemplate&& rhs)
		{
			static_cast<DescriptorSet&>(*this) = rhs;
			for (uint32_t i = 0U; i < MaxBindings; ++i)
			{
				m_resources[i] = std::move(rhs.m_resources[i]);
			}
			return *this;
		}

		nbl::core::SRange<const refctd<Resource>> getResources() const override
		{
			return nbl::core::SRange<const refctd<Resource>>(m_resources, m_resources + MaxBindings);
		}

		void update(nbl::video::ILogicalDevice* device,
			nbl::video::IGPUDescriptorSet::SWriteDescriptorSet* write, nbl::video::IGPUDescriptorSet::SDescriptorInfo* info,
			uint32_t binding, BufferResource* resource, size_t offset = 0ULL, size_t size = 0ULL)
		{
			KRIS_UNUSED_PARAM(device);

			const bool full_range = (size == Size_FullRange);

			info[0].desc = refctd<nbl::video::IGPUBuffer>(resource->getBuffer()); // bad API, too late to change, should just take raw-pointers since not consumed
			info[0].info.buffer = { .offset = full_range ? 0 : offset,.size = full_range ? resource->getSize() : size };
			write[0] = { .dstSet = m_ds.get(), .binding = binding, .arrayElement = 0U, .count = 1U, .info = info };

			m_resources[binding] = refctd<Resource>(resource);
		}
		void update(nbl::video::ILogicalDevice* device,
			nbl::video::IGPUDescriptorSet::SWriteDescriptorSet* write, nbl::video::IGPUDescriptorSet::SDescriptorInfo* info,
			uint32_t binding,
			ImageResource* resource, nbl::video::IGPUSampler* sampler,
			nbl::asset::IImage::LAYOUT layout,
			nbl::video::IGPUImageView::E_TYPE viewtype, nbl::asset::E_FORMAT format, nbl::core::bitflag<nbl::asset::IImage::E_ASPECT_FLAGS> aspect,
			uint32_t mipOffset = 0U, uint32_t mipCount = 0U,
			uint32_t layerOffset = 0U, uint32_t layerCount = 0U)
		{
			const bool full_mip_range = (mipCount == MipCount_FullRange);
			const bool full_layer_range = (layerCount == LayerCount_FullRange);

			const uint32_t actualMipCount = resource->getImage()->getCreationParameters().mipLevels;
			const uint32_t actualLayerCount = resource->getImage()->getCreationParameters().arrayLayers;

			auto view = resource->getView(device, viewtype, format, aspect,
				full_mip_range ? 0U : mipOffset, full_mip_range ? actualMipCount : mipCount,
				full_layer_range ? 0U : layerOffset, full_layer_range ? actualLayerCount : layerCount);

			info[0].desc = std::move(view);
			info[0].info.combinedImageSampler.imageLayout = layout;
			info[0].info.combinedImageSampler.sampler = refctd<nbl::video::IGPUSampler>(sampler);
			write[0] = { .dstSet = m_ds.get(),.binding = binding,.arrayElement = 0U,.count = 1U,.info = info };

			m_resources[binding] = refctd<Resource>(resource);
		}
		void update(nbl::video::ILogicalDevice* device,
			nbl::video::IGPUDescriptorSet::SWriteDescriptorSet* write, nbl::video::IGPUDescriptorSet::SDescriptorInfo* info,
			uint32_t binding, BufferResource* resource, nbl::asset::E_FORMAT format, uint32_t offset, uint32_t size)
		{
			const bool full_range = (size == Size_FullRange);

			auto view = resource->getView(device, format, full_range ? 0U : offset, full_range ? resource->getSize() : size);

			info[0].desc = std::move(view);
			info[0].info.buffer.offset = offset;
			info[0].info.buffer.size = size;
			write[0] = { .dstSet = m_ds.get(), .binding = binding, .arrayElement = 0U, .count = 1U, .info = info };

			m_resources[binding] = refctd<Resource>(resource);
		}
	};

	using MaterialDescriptorSet = DescriptorSetTemplate<16U>;

	class Material : public nbl::core::IReferenceCounted
	{
	public:
		struct ProtoBufferBarrier
		{
			BufferResource* buffer;
			nbl::core::bitflag<nbl::asset::ACCESS_FLAGS> dstaccess;
			nbl::core::bitflag<nbl::asset::PIPELINE_STAGE_FLAGS> dststages;
		};
		struct ProtoImageBarrier
		{
			ImageResource* image;
			nbl::core::bitflag<nbl::asset::ACCESS_FLAGS> dstaccess;
			nbl::core::bitflag<nbl::asset::PIPELINE_STAGE_FLAGS> dststages;
			nbl::video::IGPUImage::LAYOUT dstlayout;
		};

		enum BindingSlot : uint32_t
		{
			// b0..n - buffers
			b0 = 0,
			b1,
			b2,
			b3,
			b4,
			b5,
			b6,
			b7,
			bMAX=b7,
			// t0..n - textures
			t0,
			t1,
			t2,
			t3,
			t4,
			t5,
			t6,
			t7,
			tMAX=t7,

			BindingSlotCount,

			BufferBindingCount = bMAX - b0 + 1,
			TextureBindingCount = tMAX - t0 + 1,
		};
#define kris_bnd(bnd)		kris::Material::BindingSlot::bnd
#define kris_bndbit(bnd)	(1U << kris::Material::BindingSlot::bnd)
		static_assert(BindingSlotCount == MaterialDescriptorSet::MaxBindings);

		static bool isTextureBindingSlot(BindingSlot slot)
		{
			return (slot >= BindingSlot::t0) && (slot <= BindingSlot::tMAX);
		}
		static bool isBufferBindingSlot(BindingSlot slot)
		{
			return (slot >= BindingSlot::b0) && (slot <= BindingSlot::bMAX);
		}

		static nbl::asset::IDescriptor::E_TYPE getDescTypeFromBndNum(BindingSlot bnd)
		{
			if (bnd <= bMAX)
				return nbl::asset::IDescriptor::E_TYPE::ET_STORAGE_BUFFER;
			if (bnd <= tMAX)
				return nbl::asset::IDescriptor::E_TYPE::ET_COMBINED_IMAGE_SAMPLER;
			// TODO uniform buffers
			KRIS_ASSERT(0);
			return nbl::asset::IDescriptor::E_TYPE::ET_COUNT;
		}

		static nbl::core::bitflag<nbl::asset::ACCESS_FLAGS> getAccessFromBndNum(BindingSlot bnd)
		{
			if (bnd <= bMAX)
				return nbl::core::bitflag<nbl::asset::ACCESS_FLAGS>(nbl::asset::ACCESS_FLAGS::STORAGE_READ_BIT) | nbl::asset::ACCESS_FLAGS::STORAGE_WRITE_BIT;
			if (bnd <= tMAX)
				return nbl::core::bitflag<nbl::asset::ACCESS_FLAGS>(nbl::asset::ACCESS_FLAGS::SAMPLED_READ_BIT);
			// TODO uniform buffers
			KRIS_ASSERT(0);
			return nbl::asset::ACCESS_FLAGS::NONE;
		}

		explicit Material(uint32_t passmask, uint32_t bndmask) : m_passMask(passmask), m_bndMask(bndmask)
		{
			for (uint32_t i = 0U; i < MaterialDescriptorSet::MaxBindings; ++i)
			{
				m_bindings[i].rmapIx = isTextureBindingSlot((BindingSlot)i) ? DefaultImageResourceMapSlot : DefaultBufferResourceMapSlot;
				if (!isTextureBindingSlot((BindingSlot)i))
				{
					m_bindings[i].info.buffer.format = nbl::asset::EF_UNKNOWN;
					m_bindings[i].info.buffer.offset = 0;
					m_bindings[i].info.buffer.size = Size_FullRange;
				}
				else
				{
					// TODO sampler
					//m_bindings[i].sampler

					m_bindings[i].info.image.aspect = nbl::asset::IImage::EAF_COLOR_BIT;
					m_bindings[i].info.image.layout = nbl::video::IGPUImage::LAYOUT::GENERAL;
					m_bindings[i].info.image.viewtype = nbl::video::IGPUImageView::ET_2D;
					m_bindings[i].info.image.mipOffset = m_bindings[i].info.image.layerOffset = 0;
					m_bindings[i].info.image.mipCount = MipCount_FullRange;
					m_bindings[i].info.image.layerCount = LayerCount_FullRange;
				}
			}
		}

		~Material()
		{

		}

		BarrierCounts updateDescSet(nbl::video::ILogicalDevice* device, uint32_t frameIx, ProtoBufferBarrier* bbarriers, ProtoImageBarrier* ibarriers);

		bool livesInPass(EPass pass)
		{
			KRIS_ASSERT(m_passMask != 0U);
			return (m_passMask & (1U << pass)) != 0U;
		}

		virtual nbl::asset::E_PIPELINE_BIND_POINT getMtlType() const = 0;
		virtual nbl::core::bitflag<nbl::asset::PIPELINE_STAGE_FLAGS> getMtlShadersPipelineStageFlags() const = 0;

		Renderer* m_creatorRenderer;
		MaterialDescriptorSet m_ds3[FramesInFlight];

		struct Binding
		{
			uint32_t rmapIx = DefaultBufferResourceMapSlot;
			refctd<nbl::video::IGPUSampler> sampler = nullptr;
			union ExtraInfo
			{
				ExtraInfo()
				{
					image.viewtype = nbl::video::IGPUImageView::E_TYPE::ET_COUNT;
					image.aspect = nbl::video::IGPUImage::EAF_NONE;
					image.layout = nbl::video::IGPUImage::LAYOUT::UNDEFINED;
					image.mipOffset = image.mipCount = image.layerOffset = image.layerCount = 0;
				}
				~ExtraInfo() {}

				struct {
					size_t offset;
					size_t size;
					nbl::asset::E_FORMAT format;
				} buffer;
				struct {
					nbl::video::IGPUImageView::E_TYPE viewtype;
					nbl::core::bitflag<nbl::asset::IImage::E_ASPECT_FLAGS> aspect;
					nbl::video::IGPUImage::LAYOUT layout;
					uint32_t mipOffset;
					uint32_t mipCount;
					uint32_t layerOffset;
					uint32_t layerCount;
				} image;
			} info;
		};
		Binding m_bindings[MaterialDescriptorSet::MaxBindings];
		uint32_t m_passMask;
		uint32_t m_bndMask;
		// TODO: push constants?
	};

	class GfxMaterial : public Material
	{
	public:
		using Material::Material;

		nbl::asset::E_PIPELINE_BIND_POINT getMtlType() const override { return nbl::asset::EPBP_GRAPHICS; }

		nbl::core::bitflag<nbl::asset::PIPELINE_STAGE_FLAGS> getMtlShadersPipelineStageFlags() const override
		{
			return nbl::core::bitflag<nbl::asset::PIPELINE_STAGE_FLAGS>(nbl::asset::PIPELINE_STAGE_FLAGS::VERTEX_SHADER_BIT) |
				nbl::asset::PIPELINE_STAGE_FLAGS::FRAGMENT_SHADER_BIT;
		}

		struct GfxShaders
		{
			refctd<nbl::video::IGPUShader> vertex;
			refctd<nbl::video::IGPUShader> pixel;

			bool present() const
			{
				KRIS_ASSERT_MSG(!vertex == !pixel, "Vertex and Pixel shader must both be present or not present.")
					return vertex && pixel;
			}
		} m_gfxShaders[NumPasses];

		nbl::video::IGPUGraphicsPipeline* getGfxPipeline(EPass pass, const nbl::asset::SVertexInputParams& vtxinput);

	private:
		enum : uint32_t
		{
			PipelineCacheCapacity = 10U
		};

		struct PsoCacheEntry
		{
			nbl::asset::SVertexInputParams vtxinput;
			refctd<nbl::video::IGPUGraphicsPipeline> pso;
			uint64_t timestamp = 0ULL;
		};

		struct PsoCache
		{
			PsoCacheEntry entries[PipelineCacheCapacity];
			uint64_t counter = 0ULL;

			nbl::video::IGPUGraphicsPipeline* getPsoAt(uint32_t ix)
			{
				entries[ix].timestamp = counter++;
				return entries[ix].pso.get();
			}
			nbl::video::IGPUGraphicsPipeline* setPsoAt(uint32_t ix, nbl::asset::SVertexInputParams vtxinput, refctd<nbl::video::IGPUGraphicsPipeline>&& pso)
			{
				auto& e = entries[ix];
				e.vtxinput = vtxinput;
				e.pso = std::move(pso);
				e.timestamp = counter++;
				return e.pso.get();
			}
		} m_psoCache[NumPasses];
	};

	class ComputeMaterial : public Material
	{
	public:
		using Material::Material;

		nbl::asset::E_PIPELINE_BIND_POINT getMtlType() const override { return nbl::asset::EPBP_COMPUTE; }

		nbl::core::bitflag<nbl::asset::PIPELINE_STAGE_FLAGS> getMtlShadersPipelineStageFlags() const override
		{
			return nbl::asset::PIPELINE_STAGE_FLAGS::COMPUTE_SHADER_BIT;
		}

		refctd<nbl::video::IGPUComputePipeline> m_computePso[NumPasses];
	};
}
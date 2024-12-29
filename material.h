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
			Slot()
			{
				resource = nullptr;
				//bDirty = 0; // should it be NOT dirty on init?
				bIsBuffer = 1; // whatever
			}
			Slot& operator=(BufferResource* buffer)
			{
				resource = refctd<Resource>(buffer);
				//bDirty = 1;
				bIsBuffer = 1;
				return *this;
			}
			Slot& operator=(ImageResource* image)
			{
				resource = refctd<Resource>(image);
				//bDirty = 1;
				bIsBuffer = 0;
				return *this;
			}

			refctd<Resource> resource;

			//uint8_t bDirty : 1;
			uint8_t bIsBuffer : 1;
		};

		Slot& operator[](size_t ix) { return slots[ix]; }
		const Slot& operator[](size_t ix) const { return slots[ix]; }

		Slot slots[ResourceMapSlots];
	};

	class DescriptorSet : public nbl::core::Uncopyable
	{
	public:
		enum : uint32_t
		{
			MaxBindings = 16U
		};

		refctd<nbl::video::IGPUDescriptorSet> m_ds;
		refctd<Resource> m_resources[MaxBindings];

		DescriptorSet() : m_ds(nullptr) {}
		explicit DescriptorSet(refctd<nbl::video::IGPUDescriptorSet>&& ds) : m_ds(std::move(ds))
		{
			// Note: renderer fills all m_resources with defaults in createDescriptoSet
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

	class Material : public nbl::core::IReferenceCounted
	{
	public:
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

			BindingSlotCount
		};
#define kris_bnd(bnd)		kris::Material::BindingSlot::bnd
#define kris_bndbit(bnd)	(1U << kris::Material::BindingSlot::bnd)
		static_assert(BindingSlotCount == DescriptorSet::MaxBindings);

		static bool isTextureBindingSlot(BindingSlot slot)
		{
			return (slot >= BindingSlot::t0) && (slot < BindingSlot::BindingSlotCount);
		}

		enum EPass : uint32_t
		{
			BasePass = 0U,

			NumPasses
		};


		explicit Material(uint32_t passmask, uint32_t bndmask) : m_passMask(passmask), m_bndMask(bndmask)
		{
			for (uint32_t i = 0U; i < DescriptorSet::MaxBindings; ++i)
			{
				m_bindings[i].rmapIx = isTextureBindingSlot((BindingSlot)i) ? DefaultImageResourceMapSlot : DefaultBufferResourceMapSlot;
				m_bindings[i].descCategory = isTextureBindingSlot((BindingSlot)i) ? nbl::asset::IDescriptor::EC_IMAGE : nbl::asset::IDescriptor::EC_BUFFER;
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
					m_bindings[i].info.image.format = nbl::asset::EF_R8_UNORM;
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

		void updateDescSet(nbl::video::ILogicalDevice* device, uint32_t frameIx, const ResourceMap* map)
		{
			auto& ds = m_ds3[frameIx];

			nbl::video::IGPUDescriptorSet::SWriteDescriptorSet writes[DescriptorSet::MaxBindings];
			nbl::video::IGPUDescriptorSet::SDescriptorInfo infos[DescriptorSet::MaxBindings];
			uint32_t updated = 0U;

			for (uint32_t b = 0U; b < DescriptorSet::MaxBindings; ++b)
			{
				if ((m_bndMask & (1U << b)) == 0U)
					continue;

				auto& bnd = m_bindings[b];
				auto& slot = map->slots[bnd.rmapIx];

				if (!slot.resource->compareIds(ds.m_resources[b].get())) // dirty
				{
					if (bnd.descCategory == nbl::asset::IDescriptor::E_CATEGORY::EC_BUFFER || bnd.descCategory == nbl::asset::IDescriptor::E_CATEGORY::EC_BUFFER_VIEW)
					{
						KRIS_ASSERT(slot.bIsBuffer);

						if (bnd.descCategory == nbl::asset::IDescriptor::E_CATEGORY::EC_BUFFER_VIEW)
						{
							KRIS_ASSERT(bnd.info.buffer.format != nbl::asset::E_FORMAT::EF_UNKNOWN);
							ds.update(device, writes + updated, infos + updated, b, static_cast<BufferResource*>(slot.resource.get()),
								bnd.info.buffer.format, (uint32_t)bnd.info.buffer.offset, (uint32_t)bnd.info.buffer.size);
						}
						else
						{
							ds.update(device, writes + updated, infos + updated, b, static_cast<BufferResource*>(slot.resource.get()),
								(uint32_t)bnd.info.buffer.offset, (uint32_t)bnd.info.buffer.size);
						}
					}
					else if (bnd.descCategory == nbl::asset::IDescriptor::E_CATEGORY::EC_IMAGE)
					{
						ds.update(device, writes + updated, infos + updated, b, static_cast<ImageResource*>(slot.resource.get()),
							bnd.sampler.get(), bnd.info.image.layout,
							bnd.info.image.viewtype, bnd.info.image.format, bnd.info.image.aspect,
							bnd.info.image.mipOffset, bnd.info.image.mipCount,
							bnd.info.image.layerOffset, bnd.info.image.layerCount
						);
					}
					else
					{
						KRIS_ASSERT(false);
					}

					updated++;
				}
			}

			if (updated > 0U)
			{
				device->updateDescriptorSets(updated, writes, 0U, nullptr);
			}
		}

		bool livesInPass(EPass pass)
		{
			KRIS_ASSERT(m_passMask != 0U);
			return (m_passMask & (1U << pass)) != 0U;
		}

		virtual nbl::asset::E_PIPELINE_BIND_POINT getMtlType() const = 0;

		Renderer* m_creatorRenderer;
		DescriptorSet m_ds3[FramesInFlight];

		struct Binding
		{
			uint32_t rmapIx = DefaultBufferResourceMapSlot;
			nbl::asset::IDescriptor::E_CATEGORY descCategory = nbl::asset::IDescriptor::EC_COUNT;
			refctd<nbl::video::IGPUSampler> sampler = nullptr;
			union ExtraInfo
			{
				ExtraInfo()
				{
					image.viewtype = nbl::video::IGPUImageView::E_TYPE::ET_COUNT;
					image.format = nbl::asset::EF_UNKNOWN;
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
					nbl::asset::E_FORMAT format;
					nbl::core::bitflag<nbl::asset::IImage::E_ASPECT_FLAGS> aspect;
					nbl::video::IGPUImage::LAYOUT layout;
					uint32_t mipOffset;
					uint32_t mipCount;
					uint32_t layerOffset;
					uint32_t layerCount;
				} image;
			} info;
		};
		Binding m_bindings[DescriptorSet::MaxBindings];
		uint32_t m_passMask;
		uint32_t m_bndMask;
		// TODO: push constants?
	};

	class GfxMaterial : public Material
	{
	public:
		using Material::Material;

		nbl::asset::E_PIPELINE_BIND_POINT getMtlType() const override { return nbl::asset::EPBP_GRAPHICS; }

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
		};

		struct PsoCache
		{
			PsoCacheEntry entries[PipelineCacheCapacity];
			uint32_t size = 0U;
		} m_psoCache;
	};

	class ComputeMaterial : public Material
	{
	public:
		using Material::Material;

		nbl::asset::E_PIPELINE_BIND_POINT getMtlType() const override { return nbl::asset::EPBP_COMPUTE; }

		refctd<nbl::video::IGPUComputePipeline> m_computePso[NumPasses];
	};
}
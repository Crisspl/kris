#include "material.h"
#include "renderer.h"

static bool validateVtxinput(const nbl::asset::SVertexInputParams& vtxinput)
{
	if (vtxinput.enabledBindingFlags != 0b0001U)
	{
		return false;
	}
	if (vtxinput.enabledAttribFlags < 1U)
	{
		return false;
	}
	for (uint32_t i = 0U; i < 16U; ++i)
	{
		if ((vtxinput.enabledAttribFlags & (1U << i)) == 0U)
			continue;
		if (vtxinput.attributes[i].binding != 0U)
		{
			return false;
		}
	}

	return true;
}

static bool compareVtxInputs(const nbl::asset::SVertexInputParams& lhs, const nbl::asset::SVertexInputParams& rhs)
{
	if (lhs.enabledAttribFlags != rhs.enabledAttribFlags)
		return false;

	if (lhs.bindings[0] != rhs.bindings[0])
		return false;

	for (uint32_t i = 0U; i < 16U; ++i)
	{
		if ((lhs.enabledAttribFlags & (1U << i)) == 0U)
			continue;
		if (lhs.attributes[i] != rhs.attributes[i])
		{
			return false;
		}
	}

	return true;
}


namespace kris
{
	static void writeImageBarrier(ImageResource* image, 
		Material::ProtoImageBarrier* barrier,
		nbl::core::bitflag<nbl::asset::ACCESS_FLAGS> access,
		nbl::core::bitflag<nbl::asset::PIPELINE_STAGE_FLAGS> stages,
		nbl::video::IGPUImage::LAYOUT layout)
	{
			barrier[0] = {
				.image = image,
				.dstaccess = access,
				.dststages = stages,
				.dstlayout = layout
			};
	}

	static void writeBufferBarrier(BufferResource* buffer,
		Material::ProtoBufferBarrier* barrier,
		nbl::core::bitflag<nbl::asset::ACCESS_FLAGS> access,
		nbl::core::bitflag<nbl::asset::PIPELINE_STAGE_FLAGS> stages)
	{
		barrier[0] = {
			.buffer = buffer,
			.dstaccess = access,
			.dststages = stages
		};
	}

	BarrierCounts Material::updateDescSet(nbl::video::ILogicalDevice* device, uint32_t frameIx, ProtoBufferBarrier* bbarriers, ProtoImageBarrier* ibarriers)
	{
		auto& ds = m_ds3[frameIx];
		ResourceMap* rmap = &m_creatorRenderer->resourceMap;

		nbl::video::IGPUDescriptorSet::SWriteDescriptorSet writes[MaterialDescriptorSet::MaxBindings];
		nbl::video::IGPUDescriptorSet::SDescriptorInfo infos[MaterialDescriptorSet::MaxBindings];
		uint32_t updated = 0U;

		BarrierCounts barrierCounts;

		const auto dstStages = this->getMtlShadersPipelineStageFlags();

		for (uint32_t b = 0U; b < MaterialDescriptorSet::MaxBindings; ++b)
		{
			if ((m_bndMask & (1U << b)) == 0U)
				continue;

			auto& bnd = m_bindings[b];
			auto& slot = rmap->slots[bnd.rmapIx];

			Resource* const resource = slot.resource.get();

			const bool needToUpdate = !resource->compareIds(ds.m_resources[b].get());

			if (isBufferBindingSlot((BindingSlot)b))
			{
				//KRIS_ASSERT(slot.bIsBuffer);
				BufferResource* const bufferResource = static_cast<BufferResource*>(resource);

#if 0
				if (bnd.descCategory == nbl::asset::IDescriptor::E_CATEGORY::EC_BUFFER_VIEW)
				{
					if (needToUpdate)
					{
						KRIS_ASSERT(bnd.info.buffer.format != nbl::asset::E_FORMAT::EF_UNKNOWN);
						ds.update(device, writes + updated, infos + updated, b, static_cast<BufferResource*>(resource),
							bnd.info.buffer.format, (uint32_t)bnd.info.buffer.offset, (uint32_t)bnd.info.buffer.size);
					}

					KRIS_ASSERT(0); // buffer views are not supported for now
				}
				else
#endif
				{
					if (needToUpdate)
					{
						ds.update(device, writes + updated, infos + updated, b, static_cast<BufferResource*>(resource),
							(uint32_t)bnd.info.buffer.offset, (uint32_t)bnd.info.buffer.size);
					}

					writeBufferBarrier(bufferResource, 
						bbarriers + barrierCounts.buffer, 
						getAccessFromBndNum((BindingSlot)b),
						dstStages);
					barrierCounts.buffer++;
				}
			}
			else if (isTextureBindingSlot((BindingSlot)b))
			{
				ImageResource* const imageResource = static_cast<ImageResource*>(resource);

				if (needToUpdate)
				{
					ds.update(device, writes + updated, infos + updated, b, imageResource,
						bnd.sampler.get(), bnd.info.image.layout,
						bnd.info.image.viewtype, imageResource->getImage()->getCreationParameters().format, bnd.info.image.aspect,
						bnd.info.image.mipOffset, bnd.info.image.mipCount,
						bnd.info.image.layerOffset, bnd.info.image.layerCount
					);
				}

				writeImageBarrier(imageResource,
					ibarriers + barrierCounts.image,
					getAccessFromBndNum((BindingSlot)b), 
					dstStages,
					nbl::video::IGPUImage::LAYOUT::READ_ONLY_OPTIMAL);
				barrierCounts.image++;
			}
			else
			{
				KRIS_ASSERT(false);
			}

			updated += (needToUpdate ? 1U : 0U);
		}

		if (updated > 0U)
		{
			device->updateDescriptorSets(updated, writes, 0U, nullptr);
		}

		return barrierCounts;
	}

	nbl::video::IGPUGraphicsPipeline* GfxMaterial::getGfxPipeline(EPass pass, const nbl::asset::SVertexInputParams& vtxinput)
	{
		const bool valid = validateVtxinput(vtxinput);
		KRIS_ASSERT(valid);
		if (!valid)
			return nullptr;

		auto& psoCache = m_psoCache[pass];

		for (uint32_t i = 0U; i < PipelineCacheCapacity; ++i)
		{
			if (!psoCache.entries[i].pso) // if we encountered null here, all further will also be null
				break;
			if (compareVtxInputs(psoCache.entries[i].vtxinput, vtxinput))
				return psoCache.getPsoAt(i);
		}

		auto pso = m_creatorRenderer->createGraphicsPipelineForMaterial(pass, m_gfxShaders[pass], vtxinput);

		// Look for the longest untouched pso, or for empty spot
		uint32_t ixToReplace = 0U;
		for (uint32_t i = 0U; i < PipelineCacheCapacity; ++i)
		{
			if (!psoCache.entries[i].pso)
			{
				return psoCache.setPsoAt(i, vtxinput, std::move(pso));
			}
			if (psoCache.entries[ixToReplace].timestamp > psoCache.entries[i].timestamp)
			{
				ixToReplace = i;
			}
		}

		return psoCache.setPsoAt(ixToReplace, vtxinput, std::move(pso));
	}
}

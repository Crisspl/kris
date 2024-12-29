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
	void Material::updateDescSet(nbl::video::ILogicalDevice* device, uint32_t frameIx)
	{
		auto& ds = m_ds3[frameIx];
		ResourceMap* rmap = &m_creatorRenderer->resourceMap;

		nbl::video::IGPUDescriptorSet::SWriteDescriptorSet writes[DescriptorSet::MaxBindings];
		nbl::video::IGPUDescriptorSet::SDescriptorInfo infos[DescriptorSet::MaxBindings];
		uint32_t updated = 0U;

		for (uint32_t b = 0U; b < DescriptorSet::MaxBindings; ++b)
		{
			if ((m_bndMask & (1U << b)) == 0U)
				continue;

			auto& bnd = m_bindings[b];
			auto& slot = rmap->slots[bnd.rmapIx];

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

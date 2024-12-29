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

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

		for (uint32_t i = 0U; i < m_psoCache.size; ++i)
		{
			if (compareVtxInputs(m_psoCache.entries[i].vtxinput, vtxinput))
				return m_psoCache.entries[i].pso.get();
		}

		if (m_psoCache.size == PipelineCacheCapacity)
		{
			KRIS_ASSERT(false);
			return nullptr;
		}

		m_psoCache.entries[m_psoCache.size++] = {
			.vtxinput = vtxinput,
			.pso = m_creatorRenderer->createGraphicsPipelineForMaterial(pass, m_gfxShaders[pass], vtxinput)
		};

		return m_psoCache.entries[m_psoCache.size - 1U].pso.get();
	}
}

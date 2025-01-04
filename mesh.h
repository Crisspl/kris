#pragma once

#include "material.h"

namespace kris
{
    class Mesh : public nbl::core::IReferenceCounted
    {
    public:
        using transform_t = nbl::core::matrix3x4SIMD;

        nbl::asset::SVertexInputParams m_vtxinput;
        uint32_t m_idxCount;
        nbl::asset::E_INDEX_TYPE m_idxtype;

        refctd<BufferResource> m_vtxBuf;
        refctd<BufferResource> m_idxBuf;

        refctd<GfxMaterial> m_mtl;

        struct ResourceMapping
        {
            uint32_t rmapIx;
            refctd<Resource> res;
        } m_resources[Material::BindingSlotCount];

        uint32_t getPassMask()
        {
            return m_mtl ? m_mtl->m_passMask : 0U;
        }

        nbl::video::IGPUGraphicsPipeline* getPipeline(Material::EPass pass)
        {
            return m_mtl->getGfxPipeline(pass, m_vtxinput);
        }

        void updateResourceMap(ResourceMap* rmap)
        {
            for (auto& mapping : m_resources)
            {
                if (mapping.res)
                {
                    KRIS_ASSERT(mapping.rmapIx >= FirstUsableResourceMapSlot);
                    (*rmap)[mapping.rmapIx].resource = mapping.res;
                }
            }
        }
    };
}
#pragma once

#include "material.h"
#include "renderer.h"

namespace kris
{
    class MaterialBuilder
    {
        refctd<nbl::asset::CHLSLCompiler> m_compiler;

    public:
        explicit MaterialBuilder(nbl::system::ISystem* sys) :
            m_compiler(nbl::core::make_smart_refctd_ptr<nbl::asset::CHLSLCompiler>(refctd<nbl::system::ISystem>(sys)))
        {

        }

        refctd<Material> buildMaterial(Renderer* renderer, nbl::system::ILogger* logger, const nbl::system::path& filepath);

        template <nbl::asset::E_PIPELINE_BIND_POINT Type>
        refctd<Material> buildTypedMaterial(Renderer* renderer, nbl::system::ILogger* logger, const nbl::system::path& filepath)
        {
            auto mtl = buildMaterial(renderer, logger, filepath);
            KRIS_ASSERT(mtl->getMtlType() == Type);
            return nbl::core::smart_refctd_ptr_static_cast<GfxMaterial>(std::move(mtl));
        }

        refctd<GfxMaterial> buildGfxMaterial(Renderer* renderer, nbl::system::ILogger* logger, const nbl::system::path& filepath)
        {
            return nbl::core::smart_refctd_ptr_static_cast<GfxMaterial>(buildTypedMaterial<nbl::asset::EPBP_GRAPHICS>(renderer, logger, filepath));
        }
        refctd<ComputeMaterial> buildComputeMaterial(Renderer* renderer, nbl::system::ILogger* logger, const nbl::system::path& filepath)
        {
            return nbl::core::smart_refctd_ptr_static_cast<ComputeMaterial>(buildTypedMaterial<nbl::asset::EPBP_COMPUTE>(renderer, logger, filepath));
        }
    };
}
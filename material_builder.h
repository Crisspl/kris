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

        refctd<Material> buildMaterial(Renderer* renderer, nbl::system::ILogger* logger, nbl::video::IGPURenderpass* renderpass, const nbl::system::path& filepath);
    };
}
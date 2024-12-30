#include "material_builder.h"

namespace kris
{

    static const char* nextline(const char* str)
    {
        if (str = strchr(str, '\n'))
            return str + 1;
        return nullptr;
    }

    static size_t parseNumber(const char* str)
    {
        char strnum[20]{};
        for (uint32_t i = 0U; std::isdigit(str[i]); ++i)
        {
            KRIS_ASSERT(i < sizeof(strnum));
            strnum[i] = str[i];
        }
        return (size_t) atoi(strnum);
    }

    static size_t parseSize(const char* str)
    {
        const char FULLstr[] = "FULL";
        if (strncmp(str, FULLstr, sizeof(FULLstr)-1ULL) == 0)
            return Size_FullRange;
        return parseNumber(str);
    }
    
    static void parseTextureBindingSlot(const char* str, Material::Binding& bnd)
    {

    }

    static void parseBufferBindingSlot(const char* str, Material::Binding& bnd)
    {
        const char* rmap = nullptr;
        const char* offset = nullptr;
        const char* size = nullptr;

        struct BndParam
        {
            const char** start;
            const char* name;

            size_t namelen() const { return strlen(name); }
        } params[] = {
            {
                &rmap,
                "rmap="
            },
            {
                &offset,
                "offset="
            },
            {
                &size,
                "size="
            }
        };

        while ((str = nextline(str)) && str[0]!='$')
        {
            for (auto& p : params)
            {
                const size_t namelen = p.namelen();
                if (strncmp(p.name, str, namelen) == 0)
                {
                    *p.start = str + namelen;
                    break;
                }
            }
        }

        // Note: bnd.descCategory is correctly set by Material constructor
        bnd.info.buffer.format = nbl::asset::EF_UNKNOWN;
        if (rmap)
        {
            bnd.rmapIx = parseNumber(rmap);
        }
        if (offset)
        {
            bnd.info.buffer.offset = parseNumber(offset);
        }
        if (size)
        {
            bnd.info.buffer.size = parseSize(size);
        }
    }

    static uint32_t parseBindings(const char* const _str, Material::Binding bindings[Material::BindingSlotCount])
    {
        std::string_view str(_str);

        uint32_t bndMask = 0U;

        size_t offset = 0ULL;
        while (1)
        {
            offset = str.find('$', offset);
            if (offset == str.npos || str[offset+1ULL]!='$') // end of string or another param start (not sub-param)
                break;

            offset += 2ULL;

            Material::BindingSlot slot = kris_bnd(b0);
            Material::BindingSlot slotmax = kris_bnd(bMAX);
            switch (str[offset])
            {
            case 'b':
                slot = kris_bnd(b0);
                slotmax = kris_bnd(bMAX);
                break;
            case 't':
                slot = kris_bnd(t0);
                slotmax = kris_bnd(tMAX);
                break;
            default:
                KRIS_ASSERT(false);
                break;
            }

            {
                slot = (Material::BindingSlot) (slot + parseNumber(str.data() + offset + 1ULL));
                KRIS_ASSERT_MSG(slot <= slotmax, "Slot number greater than its max value!");
            }

            bndMask |= (1U << slot);

            auto& bnd = bindings[slot];
            if (Material::isTextureBindingSlot(slot))
            {
                parseTextureBindingSlot(nextline(str.data()), bnd);
            }
            else
            {
                parseBufferBindingSlot(nextline(str.data()), bnd);
            }
        }

        return bndMask;
    }

    static refctd<nbl::asset::ICPUShader> parseShader(nbl::asset::CHLSLCompiler* compiler, nbl::system::ILogger* logger, const std::string& id, nbl::hlsl::ShaderStage stage, const char* str)
    {
        const char* const extension = [stage]()
            {
                if (stage == nbl::hlsl::ESS_COMPUTE)
                    return ".comp";
                else if (stage == nbl::hlsl::ESS_VERTEX)
                    return ".vert";
                else if (stage == nbl::hlsl::ESS_FRAGMENT)
                    return ".frag";
                KRIS_ASSERT(false);
                return "";
            }();

        const std::string identifier = id + extension;

        const char* strend = strchr(str, '$'); 
        // if strend is null, it means this shader source is last param and we can take it all
        const std::string_view hlsl = strend ? std::string_view(str, strend) : std::string_view(str);

        nbl::asset::CHLSLCompiler::SOptions options = {};
        // really we should set it to `ESS_COMPUTE` since we know, but we'll test the `#pragma` handling fur teh lulz
        options.stage = stage;
#if !KRIS_CFG_SHIPPING
        // want as much debug as possible
        options.debugInfoFlags |= nbl::asset::IShaderCompiler::E_DEBUG_INFO_FLAGS::EDIF_LINE_BIT;
#endif
        // this lets you source-level debug/step shaders in renderdoc
        //if (m_physicalDevice->getLimits().shaderNonSemanticInfo)
        //    options.debugInfoFlags |= IShaderCompiler::E_DEBUG_INFO_FLAGS::EDIF_NON_SEMANTIC_BIT;
        // if you don't set the logger and source identifier you'll have no meaningful errors
        options.preprocessorOptions.sourceIdentifier = identifier.c_str();
        options.preprocessorOptions.logger = logger;
        options.preprocessorOptions.includeFinder = compiler->getDefaultIncludeFinder();

        return compiler->compileToSPIRV(hlsl, options);
    }

    refctd<Material> MaterialBuilder::buildMaterial(Renderer* renderer, nbl::system::ILogger* logger, const nbl::system::path& filepath)
    {
        const char* bindings = nullptr;
        const char* compute = nullptr;
        const char* vertex = nullptr;
        const char* pixel = nullptr;

        struct MtlParam
        {
            const char** start;
            const char* name;

            size_t namelen() const { return strlen(name); }
        } params[] = {
            {
                &bindings,
                "$bindings"
            },
            {
                &compute,
                "$compute"
            },
            {
                &vertex,
                "$vertex"
            },
            {
                &pixel,
                "$pixel"
            }
        };
        
        const std::string filepath_str = filepath.string();

        nbl::core::string str;
        {
            FILE* file = fopen(filepath_str.c_str(), "rb");
            fseek(file, 0, SEEK_END);
            long fsize = ftell(file);
            fseek(file, 0, SEEK_SET);

            str.resize(fsize, 0);
            fread(str.data(), fsize, 1, file);

            fclose(file);
        }

        size_t offset = 0ULL;
        while (1)
        {
            offset = str.find('$', offset);
            if (offset == str.npos)
                break;
            if (str[offset + 1ULL] == '$') //$$ = subparam
            {
                offset += 2ULL;
                continue;
            }

            for (auto& p : params)
            {
                const size_t namelen = p.namelen();
                if (strncmp(p.name, str.c_str()+offset, namelen) == 0)
                {
                    *p.start = str.c_str() + offset + namelen;
                    break;
                }
            }

            offset += 1ULL;
        }

        refctd<Material> mtl;
        if (compute)
        {
            KRIS_ASSERT(vertex == nullptr && pixel == nullptr);

            auto cmtl = renderer->createComputeMaterial(1U << Material::BasePass, 0U /*to be adjusted*/);

            auto comp = parseShader(m_compiler.get(), logger, filepath_str, nbl::hlsl::ESS_COMPUTE, compute);
            cmtl->m_computePso[Material::BasePass] = renderer->createComputePipelineForMaterial(comp.get());

            mtl = std::move(cmtl);
        }
        else
        {
            KRIS_ASSERT(compute == nullptr);

            auto gmtl = renderer->createGfxMaterial(1U << Material::BasePass, 0U /*to be adjusted*/);

            auto cpuvert = parseShader(m_compiler.get(), logger, filepath_str, nbl::hlsl::ESS_VERTEX, vertex);
            KRIS_ASSERT(cpuvert);
            auto cpufrag = parseShader(m_compiler.get(), logger, filepath_str, nbl::hlsl::ESS_FRAGMENT, pixel);
            KRIS_ASSERT(cpufrag);
            
            gmtl->m_gfxShaders[Material::BasePass].vertex = renderer->createShader(cpuvert.get());
            gmtl->m_gfxShaders[Material::BasePass].pixel = renderer->createShader(cpufrag.get());

            mtl = std::move(gmtl);
        }

        if (bindings)
        {
            mtl->m_bndMask = parseBindings(bindings, mtl->m_bindings);
        }

        return mtl;
    }
}
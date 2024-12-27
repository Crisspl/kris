#pragma once

#include "kris_common.h"
#include "resource_allocator.h"
#include "material.h"

namespace kris
{
	class DescriptorSet;

	class DeferredAllocDeletion final
	{
		using resouce_ptr = refctd<ResourceAllocator::Allocation>;
		nbl::core::vector<resouce_ptr> m_resources;

	public:
		DeferredAllocDeletion() = default;

		DeferredAllocDeletion(const DeferredAllocDeletion&) = delete;
		inline DeferredAllocDeletion(DeferredAllocDeletion&& other)
		{
			this->operator=(std::move(other));
		}

		DeferredAllocDeletion& operator=(const DeferredAllocDeletion&) = delete;
		inline DeferredAllocDeletion& operator=(DeferredAllocDeletion&& other)
		{
			m_resources = std::move(other.m_resources);
			return *this;
		}

		void addResource(resouce_ptr&& resource)
		{
			m_resources.emplace_back(std::move(resource));
		}

		inline void operator()()
		{
			m_resources.clear();
		}
	};

	struct CommandRecorder
	{
		enum : uint32_t
		{
			MaterialDescSetIndex = 0U // TODO should be 3
		};

		struct Result
		{
			refctd<nbl::video::IGPUCommandBuffer> cmdbuf;
			DeferredAllocDeletion resources;
		};

		Material::EPass pass = Material::EPass::NumPasses;
		refctd<nbl::video::IGPUCommandBuffer> cmdbuf;

		explicit CommandRecorder(Material::EPass _pass, refctd<nbl::video::IGPUCommandBuffer>&& cb) :
			pass(_pass),
			cmdbuf(std::move(cb))
		{
			cmdbuf->begin(nbl::video::IGPUCommandBuffer::USAGE::ONE_TIME_SUBMIT_BIT); // TODO: do we always have one-time submitted cmdbufs?
		}

		void endAndObtainResources(Result& out_Result)
		{
			cmdbuf->end();
			out_Result.cmdbuf = std::move(cmdbuf);
			out_Result.resources = std::move(m_resources);
		}

		void dispatch(nbl::video::ILogicalDevice* device, uint32_t frameIx, Material::EPass pass, const ResourceMap* rmap, 
			Material* mtl, uint32_t wgcx, uint32_t wgcy, uint32_t wgcz)
		{
			setMaterial(device, frameIx, pass, rmap, mtl);

			cmdbuf->dispatch(wgcx, wgcy, wgcz);
		}

		void setMaterial(nbl::video::ILogicalDevice* device, uint32_t frameIx, Material::EPass pass, const ResourceMap* rmap, Material* mtl)
		{
			KRIS_ASSERT(mtl->livesInPass(pass));

			const nbl::video::IGPUPipelineLayout* layout = nullptr;
			if (mtl->getMtlType() == nbl::asset::EPBP_COMPUTE)
			{
				auto& pso = mtl->m_computePso[pass];
				layout = pso->getLayout();
				cmdbuf->bindComputePipeline(pso.get());
			}
			else
			{
				auto& pso = mtl->m_gfxPso[pass];
				layout = pso->getLayout();
				cmdbuf->bindGraphicsPipeline(pso.get());
			}

			const bool hasDescSet = layout->getDescriptorSetLayout(MaterialDescSetIndex) != nullptr;

			if (hasDescSet)
			{
				mtl->updateDescSet(device, frameIx, rmap);

				bindDescriptorSet(mtl->getMtlType(), layout, MaterialDescSetIndex, &mtl->m_ds3[frameIx]);
			}
		}

	private:
		void bindDescriptorSet(
			nbl::asset::E_PIPELINE_BIND_POINT q,
			const nbl::video::IGPUPipelineLayout* layout,
			uint32_t dsIx,
			const DescriptorSet* ds)
		{
			cmdbuf->bindDescriptorSets(q, layout, dsIx, 1U, &ds->m_ds.get());
			for (auto& res : ds->m_resources)
			{
				if (res) // TODO, do we have to check for null?
					//TODO check if default resource
				{
					m_resources.addResource(refctd<Resource>(res));
				}
			}
		}

		DeferredAllocDeletion m_resources;
	};
}
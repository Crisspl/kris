#pragma once

#include "kris_common.h"
#include "resource_allocator.h"
#include "material.h"
#include "mesh.h"
#include "scene.h"
#include "passes/pass_common.h"

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
		struct Result
		{
			refctd<nbl::video::IGPUCommandBuffer> cmdbuf;
			DeferredAllocDeletion resources;
		};

		uint32_t frameIx = 0U;
		Material::EPass pass = Material::EPass::NumPasses;
		refctd<nbl::video::IGPUCommandBuffer> cmdbuf;

		explicit CommandRecorder(uint32_t _frameix, Material::EPass _pass, refctd<nbl::video::IGPUCommandBuffer>&& cb) :
			frameIx(_frameix),
			pass(_pass),
			cmdbuf(std::move(cb))
		{
			// cmdbuf must be already in recording state when passed to CommandRecorder!
			KRIS_ASSERT(cmdbuf->getState() == nbl::video::IGPUCommandBuffer::STATE::RECORDING);
		}

		void endAndObtainResources(Result& out_Result)
		{
			cmdbuf->end();
			out_Result.cmdbuf = std::move(cmdbuf);
			out_Result.resources = std::move(m_resources);
		}

		void dispatch(nbl::video::ILogicalDevice* device, Material::EPass pass,
			ComputeMaterial* mtl, uint32_t wgcx, uint32_t wgcy, uint32_t wgcz)
		{
			setComputeMaterial(device, pass, mtl);

			emitBarrierCmd();

			cmdbuf->dispatch(wgcx, wgcy, wgcz);
		}

		void setGfxMaterial(nbl::video::ILogicalDevice* device, Material::EPass pass, 
			const nbl::asset::SVertexInputParams& vtxinput, GfxMaterial* mtl)
		{
			KRIS_ASSERT(mtl->livesInPass(pass));

			nbl::video::IGPUGraphicsPipeline* pso = mtl->getGfxPipeline(pass, vtxinput);
			const nbl::video::IGPUPipelineLayout* layout = pso->getLayout();
			cmdbuf->bindGraphicsPipeline(pso);

			setMaterialCommon(device, layout, mtl);
		}

		void setComputeMaterial(nbl::video::ILogicalDevice* device, Material::EPass pass, ComputeMaterial* mtl)
		{
			KRIS_ASSERT(mtl->livesInPass(pass));

			auto& pso = mtl->m_computePso[pass];
			const nbl::video::IGPUPipelineLayout* layout = pso->getLayout();
			cmdbuf->bindComputePipeline(pso.get());

			setMaterialCommon(device, layout, mtl);
		}

		void setupDrawSceneNode(nbl::video::ILogicalDevice* device, SceneNode* mesh);
		void drawSceneNode(nbl::video::ILogicalDevice* device, Material::EPass pass, SceneNode* mesh);

		void setupDrawMesh(nbl::video::ILogicalDevice* device, Mesh* mesh);
		void drawMesh(nbl::video::ILogicalDevice* device, Material::EPass pass, Mesh* mesh);

		void setupMaterial(nbl::video::ILogicalDevice* device, Material* mtl)
		{
			Material::ProtoBufferBarrier bbarriers[Material::BufferBindingCount];
			Material::ProtoImageBarrier ibarriers[Material::TextureBindingCount];

			BarrierCounts count = mtl->updateDescSet(device, frameIx, bbarriers, ibarriers);

			for (uint32_t i = 0U; i < count.buffer; ++i)
			{
				auto& bb = bbarriers[i];
				pushBarrier(bb.buffer, bb.dstaccess, bb.dststages);
			}
			for (uint32_t i = 0U; i < count.image; ++i)
			{
				auto& ib = ibarriers[i];
				pushBarrier(ib.image, ib.dstaccess, ib.dststages, ib.dstlayout);
			}
		}

		void beginRenderPass(const VkRect2D& area,
			const nbl::video::IGPUCommandBuffer::SClearColorValue& clearcolor,
			const nbl::video::IGPUCommandBuffer::SClearDepthStencilValue& cleardepth,
			const Framebuffer& fb)
		{
			const nbl::video::IGPUCommandBuffer::SRenderpassBeginInfo info =
			{
				.framebuffer = fb.m_fb.get(),
				.colorClearValues = &clearcolor,
				.depthStencilClearValues = &cleardepth,
				.renderArea = area
			};

			for (uint32_t i = 0U; i < fb.m_colorCount; ++i)
			{
				m_resources.addResource(refctd(fb.m_colors[i]));
			}
			if (fb.m_depth)
			{
				m_resources.addResource(refctd(fb.m_depth));
			}

			cmdbuf->beginRenderPass(info, nbl::video::IGPUCommandBuffer::SUBPASS_CONTENTS::INLINE);
		}

		void endRenderPass()
		{
			cmdbuf->endRenderPass();
		}

	private:
		void setMaterialCommon(nbl::video::ILogicalDevice* device, const nbl::video::IGPUPipelineLayout* layout, Material* mtl)
		{
			bindDescriptorSet(mtl->getMtlType(), layout, MaterialDescSetIndex, mtl->m_bndMask, &mtl->m_ds3[frameIx]);
		}

		void bindDescriptorSet(
			nbl::asset::E_PIPELINE_BIND_POINT q,
			const nbl::video::IGPUPipelineLayout* layout,
			uint32_t dsIx,
			uint32_t bndmask,
			const DescriptorSet* ds)
		{
			cmdbuf->bindDescriptorSets(q, layout, dsIx, 1U, &ds->m_ds.get());

			const auto rsrcRange = ds->getResources();

			for (uint32_t i = 0U; i < (uint32_t) rsrcRange.size(); ++i)
			{
				if (bndmask & (1U << i))
				{
					auto& res = rsrcRange.begin()[i];
					KRIS_ASSERT(res);
					m_resources.addResource(refctd(res));
				}
			}
		}

		void emitBarrierCmd()
		{
			using bbarrier_t = nbl::video::IGPUCommandBuffer::SBufferMemoryBarrier<nbl::video::IGPUCommandBuffer::SOwnershipTransferBarrier>;
			using ibarrier_t = nbl::video::IGPUCommandBuffer::SImageMemoryBarrier<nbl::video::IGPUCommandBuffer::SOwnershipTransferBarrier>;

			if (m_barriers.count.buffer == 0U && m_barriers.count.image == 0U)
			{
				return;
			}

			bbarrier_t bbarriers[MaxBarriers];
			for (uint32_t i = 0U; i < m_barriers.count.buffer; ++i)
			{
				const auto& b = m_barriers.buffers[i];
				auto* const buffer = b.buffer;
				auto& barrier = bbarriers[i];

				barrier = {
					.barrier = {
						.dep = {
							.srcStageMask = b.srcstages,
							.srcAccessMask = b.srcaccess,
							.dstStageMask = b.dststages,
							.dstAccessMask = b.dstaccess
							}
						},
						.range = {
							.offset = 0ULL,
							.size = buffer->getSize(),
							.buffer = refctd<nbl::video::IGPUBuffer>(buffer->getBuffer())
						}
				};
			}

			ibarrier_t ibarriers[MaxBarriers];
			for (uint32_t i = 0U; i < m_barriers.count.image; ++i)
			{
				const auto& b = m_barriers.images[i];
				auto* const image = b.image;
				auto& dst = ibarriers[i];

				dst = {
					.barrier = {
						.dep = {
							// first usage doesn't need to sync against anything, so leave src default
							.srcStageMask = b.srcstages,
							.srcAccessMask = b.srcaccess,
							.dstStageMask = b.dststages,
							.dstAccessMask = b.dstaccess
						}
					},
					.image = image->getImage(),
					.subresourceRange = {
						.aspectMask = nbl::video::IGPUImage::EAF_COLOR_BIT, // TODO
						// all the mips
						.baseMipLevel = 0,
						.levelCount = image->getImage()->getCreationParameters().mipLevels,
						// all the layers
						.baseArrayLayer = 0,
						.layerCount = image->getImage()->getCreationParameters().arrayLayers
					},
					.oldLayout = b.srclayout,
					.newLayout = b.dstlayout
				};
			}

			cmdbuf->pipelineBarrier(nbl::asset::EDF_NONE,
				{
					.memBarriers = {},
					.bufBarriers = {bbarriers, m_barriers.count.buffer},
					.imgBarriers = {ibarriers, m_barriers.count.image} });

			m_barriers.reset();
		}
		void emitBarrierCmdIfNeeded(uint32_t bufToBePushed, uint32_t imgToBePushed)
		{
			if (m_barriers.shouldBarrierCmdBeEmitted(bufToBePushed, imgToBePushed))
			{
				emitBarrierCmd();
			}
		}

		bool pushBarrier(BufferResource* buffer, nbl::core::bitflag<nbl::asset::ACCESS_FLAGS> access, nbl::core::bitflag<nbl::asset::PIPELINE_STAGE_FLAGS> stages)
		{
			emitBarrierCmdIfNeeded(1U, 0U);
			return m_barriers.pushBarrier(BufferBarrier{
				.buffer = buffer,
				.srcaccess = buffer->lastAccesses,
				.dstaccess = access,
				.srcstages = buffer->lastStages,
				.dststages = stages,
				});
		}
		bool pushBarrier(ImageResource* image, nbl::core::bitflag<nbl::asset::ACCESS_FLAGS> access, nbl::core::bitflag<nbl::asset::PIPELINE_STAGE_FLAGS> stages, nbl::video::IGPUImage::LAYOUT layout)
		{
			emitBarrierCmdIfNeeded(0U, 1U);
			return m_barriers.pushBarrier(ImageBarrier{
				.image = image,
				.srcaccess = image->lastAccesses,
				.dstaccess = access,
				.srcstages = image->lastStages,
				.dststages = stages,
				.srclayout = image->layout,
				.dstlayout = layout
				});
		}

		static inline constexpr uint32_t MaxBarriers = 50U;
		struct {
			BufferBarrier buffers[MaxBarriers];
			ImageBarrier images[MaxBarriers];
			BarrierCounts count;

			bool pushBarrier(const BufferBarrier& bb)
			{
				bb.buffer->lastAccesses = bb.dstaccess;
				bb.buffer->lastStages = bb.dststages;
				if (isBarrierNeededCommon(bb.srcaccess, bb.dstaccess))
				{
					buffers[count.buffer++] = bb;


					return true;
				}
				return false;
			}
			bool pushBarrier(const ImageBarrier& ib)
			{
				ib.image->lastAccesses = ib.dstaccess;
				ib.image->lastStages = ib.dststages;
				if (isBarrierNeededCommon(ib.srcaccess, ib.dstaccess) || ib.srclayout != ib.dstlayout)
				{
					images[count.image++] = ib;

					ib.image->layout = ib.dstlayout;

					return true;
				}
				return false;
			}

			bool shouldBarrierCmdBeEmitted(uint32_t bufToBePushed, uint32_t imgToBePushed)
			{
				// if next updateDescSet() call might potentially overflow barriers buffer
				return (count.buffer + bufToBePushed > MaxBarriers) ||
					(count.image + imgToBePushed > MaxBarriers);
			}
			void reset()
			{
				count.reset();
			}

			BufferBarrier* getBuffersPtr() { return buffers + count.buffer; }
			ImageBarrier* getImagesPtr() { return images + count.image; }

		private:
			bool isBarrierNeededCommon(nbl::core::bitflag<nbl::asset::ACCESS_FLAGS> srcaccess, nbl::core::bitflag<nbl::asset::ACCESS_FLAGS> dstAccess)
			{
				KRIS_UNUSED_PARAM(dstAccess);

				return srcaccess.hasAnyFlag(nbl::asset::ACCESS_FLAGS::MEMORY_WRITE_BITS);
			}
		} m_barriers;
		DeferredAllocDeletion m_resources;
	};
}
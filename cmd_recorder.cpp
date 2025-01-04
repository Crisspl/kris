#include "cmd_recorder.h"

#include "renderer.h"

namespace kris
{
	void CommandRecorder::setupDrawSceneNode(nbl::video::ILogicalDevice* device, SceneNode* node)
	{
		{
			BufferResource* uboResource = static_cast<BufferResource*>(node->m_ds.m_resources[0].get());
			auto* ubo = uboResource->getBuffer();

			nbl::asset::SBufferRange<nbl::video::IGPUBuffer> rng;
			rng.buffer = refctd<nbl::video::IGPUBuffer>(ubo);
			rng.offset = 0;
			rng.size = rng.buffer->getSize();
			cmdbuf->updateBuffer(rng, &node->m_data);

			pushBarrier(uboResource, nbl::asset::ACCESS_FLAGS::UNIFORM_READ_BIT, nbl::asset::PIPELINE_STAGE_FLAGS::VERTEX_SHADER_BIT);
		}

		setupDrawMesh(device, node->m_mesh.get());
	}

	void CommandRecorder::drawSceneNode(nbl::video::ILogicalDevice* device, Material::EPass pass, SceneNode* node)
	{
		auto* mesh = node->m_mesh.get();
		// bind node ds

		bindDescriptorSet(nbl::asset::EPBP_GRAPHICS, mesh->getPipeline(pass)->getLayout(), SceneNodeDescSetIndex, SceneNode::DescSetBndMask, &node->m_ds);

		drawMesh(device, pass, mesh);
	}

	void CommandRecorder::setupDrawMesh(nbl::video::ILogicalDevice* device, Mesh* mesh)
	{
		KRIS_ASSERT((mesh->getPassMask() & (1U << pass)) != 0U);

		{
			auto* vtxbuf = mesh->m_vtxBuf.get();
			pushBarrier(vtxbuf, nbl::asset::ACCESS_FLAGS::VERTEX_ATTRIBUTE_READ_BIT, nbl::asset::PIPELINE_STAGE_FLAGS::VERTEX_INPUT_BITS);
		}
		{
			auto* idxbuf = mesh->m_idxBuf.get();
			pushBarrier(idxbuf, nbl::asset::ACCESS_FLAGS::INDEX_READ_BIT, nbl::asset::PIPELINE_STAGE_FLAGS::INDEX_INPUT_BIT);
		}

		Renderer* rend = mesh->m_mtl->m_creatorRenderer;
		mesh->updateResourceMap(&rend->resourceMap);

		setupMaterial(device, mesh->m_mtl.get());

		emitBarrierCmd();
	}

	void CommandRecorder::drawMesh(nbl::video::ILogicalDevice* device, Material::EPass pass, Mesh* mesh)
	{
		KRIS_ASSERT((mesh->getPassMask() & (1U << pass)) != 0U);

		{
			auto* vtxbuf = mesh->m_vtxBuf.get();
			nbl::asset::SBufferBinding<const nbl::video::IGPUBuffer> bnd;
			bnd.buffer = kris::refctd<const nbl::video::IGPUBuffer>(vtxbuf->getBuffer());
			bnd.offset = 0;
			cmdbuf->bindVertexBuffers(0U, 1U, &bnd);
		}
		{
			auto* idxbuf = mesh->m_idxBuf.get();
			nbl::asset::SBufferBinding<const nbl::video::IGPUBuffer> bnd;
			bnd.buffer = kris::refctd<const nbl::video::IGPUBuffer>(idxbuf->getBuffer());
			bnd.offset = 0;
			cmdbuf->bindIndexBuffer(bnd, mesh->m_idxtype);
		}

		setGfxMaterial(device, pass, mesh->m_vtxinput, mesh->m_mtl.get());

		cmdbuf->drawIndexed(mesh->m_idxCount, 1, 0, 0, 0);
	}
}
#include "cmd_recorder.h"

#include "renderer.h"

namespace kris
{
	void CommandRecorder::drawMesh(nbl::video::ILogicalDevice* device, Material::EPass pass, Mesh* mesh)
	{
		KRIS_ASSERT((mesh->getPassMask() & (1U << pass)) != 0U);

		{
			nbl::asset::SBufferBinding<const nbl::video::IGPUBuffer> bnd;
			bnd.buffer = kris::refctd<const nbl::video::IGPUBuffer>(mesh->m_vtxBuf->getBuffer());
			bnd.offset = 0;
			cmdbuf->bindVertexBuffers(0U, 1U, &bnd);
		}
		{
			nbl::asset::SBufferBinding<const nbl::video::IGPUBuffer> bnd;
			bnd.buffer = kris::refctd<const nbl::video::IGPUBuffer>(mesh->m_idxBuf->getBuffer());
			bnd.offset = 0;
			cmdbuf->bindIndexBuffer(bnd, mesh->m_idxtype);
		}

		Renderer* rend = mesh->m_mtl->m_creatorRenderer;
		mesh->updateResourceMap(&rend->resourceMap);

		setGfxMaterial(device, pass, mesh->m_vtxinput, mesh->m_mtl.get());

		cmdbuf->drawIndexed(mesh->m_idxCount, 1, 0, 0, 0);
	}
}
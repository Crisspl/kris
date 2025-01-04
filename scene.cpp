#include "scene.h"
#include "renderer.h"

namespace kris
{
    refctd<SceneNode> Scene::createMeshSceneNode(nbl::video::ILogicalDevice* device, ResourceAllocator* ra)
    {
        auto node = nbl::core::make_smart_refctd_ptr<SceneNode>();
        node->m_mesh = nbl::core::make_smart_refctd_ptr<Mesh>();
        node->m_ds = m_renderer->createSceneNodeDescriptorSet();

        nbl::video::IGPUBuffer::SCreationParams ci = {};
        ci.usage = nbl::core::bitflag<nbl::asset::IBuffer::E_USAGE_FLAGS>(nbl::asset::IBuffer::EUF_UNIFORM_BUFFER_BIT) |
            nbl::asset::IBuffer::EUF_TRANSFER_DST_BIT |
            nbl::asset::IBuffer::EUF_INLINE_UPDATE_VIA_CMDBUF;
        ci.size = sizeof(SceneNode::UBOData);
        auto ubo = ra->allocBuffer(device, std::move(ci), device->getPhysicalDevice()->getDeviceLocalMemoryTypeBits());

        nbl::video::IGPUDescriptorSet::SWriteDescriptorSet w;
        nbl::video::IGPUDescriptorSet::SDescriptorInfo info;
        node->m_ds.update(device, &w, &info, 0U, ubo.get());
        device->updateDescriptorSets({ &w, 1 }, {});

        return node;
    }
}
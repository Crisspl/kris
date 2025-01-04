#pragma once

#include "mesh.h"

namespace kris
{
    class Renderer; // fwd decl

    using SceneNodeDescriptorSet = DescriptorSetTemplate<1U>;

    class SceneNode : public nbl::core::IReferenceCounted
    {
    public:
        using transform_t = nbl::core::matrix3x4SIMD;

        refctd<Mesh> m_mesh;

        enum : uint32_t
        {
            DescSetBndMask = SceneNodeDescriptorSet::FullBndMask,
        };

        struct UBOData
        {
            transform_t worldMatrix;
        };

        SceneNodeDescriptorSet m_ds;
        UBOData m_data;

        transform_t& getTransform()
        {
            return m_data.worldMatrix;
        }
    };

    class Scene
    {
    public:
        void init(Renderer* rend)
        {
            m_renderer = rend;
        }

        refctd<SceneNode> createMeshSceneNode(nbl::video::ILogicalDevice* device, ResourceAllocator* ra);

        Renderer* m_renderer;
    };
}
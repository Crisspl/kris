#pragma once

#include "mesh.h"

namespace kris
{
    class Renderer; // fwd decl

    using SceneNodeDescriptorSet = DescriptorSetTemplate<1U>;

    class SceneNode : public nbl::core::IReferenceCounted
    {
    public:
        enum : uint32_t
        {
            DescSetBndMask = SceneNodeDescriptorSet::FullBndMask,
        };

        using transform_t = nbl::core::matrix3x4SIMD;
        struct UBOData
        {
            transform_t worldMatrix;
        };

        refctd<Mesh> m_mesh;
        SceneNodeDescriptorSet m_ds;
        UBOData m_data;
        transform_t m_localTform;
        nbl::core::list<refctd<SceneNode>> m_children;

        transform_t& getLocalTransform()
        {
            return m_localTform;
        }
        const transform_t& getGlobalTransform() const
        {
            return m_data.worldMatrix;
        }

        void updateTransformTree(const transform_t& parentTform = transform_t())
        {
            m_data.worldMatrix = transform_t::concatenateBFollowedByA(getLocalTransform(), parentTform);

            for (auto& child : m_children)
            {
                child->updateTransformTree(getGlobalTransform());
            }
        }

        void addChild(refctd<SceneNode>&& child)
        {
            m_children.push_back(std::move(child));
        }
    };

    class Scene
    {
    public:
        void init(Renderer* rend)
        {
            m_renderer = rend;
        }

        refctd<SceneNode> createMeshSceneNode(nbl::video::ILogicalDevice* device, ResourceAllocator* ra, Mesh* mesh);

        Renderer* m_renderer;
    };
}
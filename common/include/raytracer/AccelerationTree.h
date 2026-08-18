#pragma once
#include "raytracer/Triangle.h"
#include "raytracer/AABB.h"
#include "raytracer/Ray.h"
#include <optional>
#include <vector>

namespace crt
{
    // The result of a tree query: the winning triangle (a copy) plus the barycentric hit
    // info needed to shade it. Deliberately free of any Material/Scene knowledge — the tree
    // only ever deals in geometry; turning this into a HitRecord is the caller's job.
    struct AccelerationHit
    {
        double t;
        double u;
        double v;
        CRTTriangle triangle;
    };

    // A node in the acceleration tree. Non-leaf nodes leave m_triangles empty and point at
    // their two children; leaf nodes hold the triangles for their sub-space and no children
    // (m_leftChildIdx/m_rightChildIdx stay -1). -1 in m_parentIdx marks the root.
    class AccelerationNode
    {
    public:
        AccelerationNode(const AABB& boundingBox, const int parentIdx, const int leftChildIdx,
                         const int rightChildIdx, const std::vector<CRTTriangle>& triangles);

        std::vector<CRTTriangle> m_triangles;
        AABB m_boundingBox;
        int m_parentIdx;
        int m_leftChildIdx;
        int m_rightChildIdx;

        // Tests this node's own triangles against ray (meaningful only for leaf nodes; a
        // non-leaf has an empty m_triangles, so this is a harmless no-op there). Overwrites
        // bestHit only if a strictly closer valid hit is found.
        void intersect(const Ray& ray, std::optional<AccelerationHit>& bestHit) const;

        // Like intersect, but stops at the first valid hit closer than maxT and skips
        // triangles with castsShadow == false. Used for shadow rays, which only need a
        // yes/no answer, not the closest hit.
        bool intersectAny(const Ray& ray, double maxT) const;
    };

    // Owns the flat node array and both builds and queries the acceleration tree.
    // AccelerationTree::build(...) is the only way to get a populated instance; a
    // default-constructed AccelerationTree is empty (no nodes).
    class AccelerationTree
    {
    public:
        // Builds a full acceleration tree over an already-flattened triangle list.
        static AccelerationTree build(const std::vector<CRTTriangle>& triangles);

        // Walks the tree and returns the closest triangle hit by ray, if any.
        std::optional<AccelerationHit> intersectClosest(const Ray& ray) const;

        // Walks the tree and returns true as soon as any valid hit closer than maxT is
        // found, skipping the rest of the walk. For shadow rays: cheaper than
        // intersectClosest because it doesn't need the *closest* hit, just whether one exists.
        bool intersectAny(const Ray& ray, double maxT) const;

    private:
        // Appends a new node (with unresolved -1 children) and returns its index.
        int addNode(const AABB& boundingBox, int parentIdx, const std::vector<CRTTriangle>& triangles);

        // Recursively splits parentIdx's box (alternating split axis by depth) until each leaf
        // holds few enough triangles or the max depth is reached.
        void buildAccTree(int parentIdx, int depth, const std::vector<CRTTriangle>& triangles);

        std::vector<AccelerationNode> m_nodes;
    };
}

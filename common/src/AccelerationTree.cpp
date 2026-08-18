#include "raytracer/AccelerationTree.h"
#include <stack>

namespace crt
{
namespace
{
    constexpr int kMaxAccTreeDepth = 20;
    constexpr size_t kMaxLeafTriangleCount = 8;
    constexpr int kSplitAxisCount = 3; // alternate x/y/z as we go deeper

    // Doesn't touch tree state, so it stays a free function rather than a member.
    AABB triangleBounds(const CRTTriangle& triangle)
    {
        AABB box;
        box.expand(triangle.v0);
        box.expand(triangle.v1);
        box.expand(triangle.v2);
        return box;
    }
} // namespace

AccelerationNode::AccelerationNode(const AABB& boundingBox, const int parentIdx, const int leftChildIdx,
                                    const int rightChildIdx, const std::vector<CRTTriangle>& triangles)
    : m_triangles(triangles)
    , m_boundingBox(boundingBox)
    , m_parentIdx(parentIdx)
    , m_leftChildIdx(leftChildIdx)
    , m_rightChildIdx(rightChildIdx)
{
}

void AccelerationNode::intersect(const Ray& ray, std::optional<AccelerationHit>& bestHit) const
{
    for (const CRTTriangle& triangle : m_triangles)
    {
        const std::optional<TriangleHit> triangleHit = triangle.intersect(ray);
        if (!triangleHit.has_value())
        {
            continue;
        }
        if (!bestHit.has_value() || triangleHit->t < bestHit->t)
        {
            bestHit = AccelerationHit{triangleHit->t, triangleHit->u, triangleHit->v, triangle};
        }
    }
}

bool AccelerationNode::intersectAny(const Ray& ray, double maxT) const
{
    for (const CRTTriangle& triangle : m_triangles)
    {
        if (!triangle.castsShadow)
        {
            continue;
        }
        const std::optional<TriangleHit> triangleHit = triangle.intersect(ray);
        if (triangleHit.has_value() && triangleHit->t < maxT)
        {
            return true;
        }
    }
    return false;
}

int AccelerationTree::addNode(const AABB& boundingBox, int parentIdx, const std::vector<CRTTriangle>& triangles)
{
    m_nodes.emplace_back(boundingBox, parentIdx, -1, -1, triangles);
    return static_cast<int>(m_nodes.size() - 1);
}

void AccelerationTree::buildAccTree(int parentIdx, int depth, const std::vector<CRTTriangle>& triangles)
{
    if (depth >= kMaxAccTreeDepth || triangles.size() <= kMaxLeafTriangleCount)
    {
        m_nodes[parentIdx].m_triangles = triangles;
        return;
    }

    const auto [child0Box, child1Box] = m_nodes[parentIdx].m_boundingBox.split(depth % kSplitAxisCount);

    std::vector<CRTTriangle> child0Triangles;
    std::vector<CRTTriangle> child1Triangles;
    for (const CRTTriangle& triangle : triangles)
    {
        const AABB triangleBox = triangleBounds(triangle);
        if (triangleBox.overlaps(child0Box))
        {
            child0Triangles.push_back(triangle);
        }
        if (triangleBox.overlaps(child1Box))
        {
            child1Triangles.push_back(triangle);
        }
    }

    // m_nodes may reallocate on addNode/recursion, so parentIdx is re-looked-up by index
    // every time rather than being cached as a reference/pointer.
    if (!child0Triangles.empty())
    {
        const int child0Idx = addNode(child0Box, parentIdx, {});
        m_nodes[parentIdx].m_leftChildIdx = child0Idx;
        buildAccTree(child0Idx, depth + 1, child0Triangles);
    }
    if (!child1Triangles.empty())
    {
        const int child1Idx = addNode(child1Box, parentIdx, {});
        m_nodes[parentIdx].m_rightChildIdx = child1Idx;
        buildAccTree(child1Idx, depth + 1, child1Triangles);
    }
}

AccelerationTree AccelerationTree::build(const std::vector<CRTTriangle>& triangles)
{
    AABB rootBox;
    for (const CRTTriangle& triangle : triangles)
    {
        rootBox.expand(triangle.v0);
        rootBox.expand(triangle.v1);
        rootBox.expand(triangle.v2);
    }

    AccelerationTree tree;
    const int rootIdx = tree.addNode(rootBox, -1, {});
    tree.buildAccTree(rootIdx, 0, triangles);
    return tree;
}

std::optional<AccelerationHit> AccelerationTree::intersectClosest(const Ray& ray) const
{
    if (m_nodes.empty())
    {
        return std::nullopt;
    }

    std::optional<AccelerationHit> bestHit;
    std::stack<int> nodeIndicesToCheck;
    nodeIndicesToCheck.push(0); // root

    while (!nodeIndicesToCheck.empty())
    {
        const int currentNodeIdx = nodeIndicesToCheck.top();
        nodeIndicesToCheck.pop();

        const AccelerationNode& currentNode = m_nodes[currentNodeIdx];
        if (!currentNode.m_boundingBox.intersects(ray))
        {
            continue;
        }

        if (!currentNode.m_triangles.empty())
        {
            // Leaf node: test its triangles directly.
            currentNode.intersect(ray, bestHit);
        }
        else
        {
            if (currentNode.m_leftChildIdx != -1)
            {
                nodeIndicesToCheck.push(currentNode.m_leftChildIdx);
            }
            if (currentNode.m_rightChildIdx != -1)
            {
                nodeIndicesToCheck.push(currentNode.m_rightChildIdx);
            }
        }
    }

    return bestHit;
}

bool AccelerationTree::intersectAny(const Ray& ray, double maxT) const
{
    if (m_nodes.empty())
    {
        return false;
    }

    std::stack<int> nodeIndicesToCheck;
    nodeIndicesToCheck.push(0); // root

    while (!nodeIndicesToCheck.empty())
    {
        const int currentNodeIdx = nodeIndicesToCheck.top();
        nodeIndicesToCheck.pop();

        const AccelerationNode& currentNode = m_nodes[currentNodeIdx];
        if (!currentNode.m_boundingBox.intersects(ray))
        {
            continue;
        }

        if (!currentNode.m_triangles.empty())
        {
            if (currentNode.intersectAny(ray, maxT))
            {
                return true;
            }
        }
        else
        {
            if (currentNode.m_leftChildIdx != -1)
            {
                nodeIndicesToCheck.push(currentNode.m_leftChildIdx);
            }
            if (currentNode.m_rightChildIdx != -1)
            {
                nodeIndicesToCheck.push(currentNode.m_rightChildIdx);
            }
        }
    }

    return false;
}

} // namespace crt

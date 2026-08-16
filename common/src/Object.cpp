#include "raytracer/Object.h"

namespace crt {

void Object::computeBoundingBox()
{
    m_boundingBox = AABB{};
    for (const CRTTriangle& triangle : m_triangles)
    {
        m_boundingBox.expand(triangle.v0);
        m_boundingBox.expand(triangle.v1);
        m_boundingBox.expand(triangle.v2);
    }
}

} // namespace crt

#pragma once
#include "raytracer/AABB.h"
#include "raytracer/Vector.h"
#include "raytracer/Triangle.h"
#include <vector>
#include <utility>

namespace crt {

// A 3D object defined by a collection of triangles
struct Object {
    
    public:
        Object(std::vector<CRTTriangle> triangles= {}, int materialIndex = -1, bool isEmissive = false)
            : m_triangles(std::move(triangles)),
             m_materialIndex(materialIndex)
            , m_isEmissive(isEmissive)
            {
                //Constructor do nothing, excpet for intializing the member variables
            }
        void calculateNormalsForSmoothShading( std::vector<CRTVector>& vertexNormals) const;

        // Recomputes m_boundingBox from the current m_triangles; call after all triangles are added.
        void computeBoundingBox();
        
    std::vector<CRTTriangle> m_triangles;
    int m_materialIndex;
    bool m_isEmissive;
    AABB m_boundingBox;
   
};

} // namespace crt

#pragma once
#include "raytracer/Vector.h"
namespace crt {

// A light source defined by a position
struct CRTLight {
    
    public:
        CRTLight(const CRTVector& position, const double intensity = 1.0) :
            position(position), 
            intensity(intensity) {}

        const CRTVector& getPosition() const { return position; }
        const double getIntensity() const { return intensity; }
    private:
        CRTVector position; // World-space position of the light source
        double intensity = 1.0; // Intensity of the light source
        
};

} // namespace crt

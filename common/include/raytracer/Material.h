#pragma once
namespace crt {
    enum class MaterialType{
        DIFFUSE,
        REFLECTIVE,
        REFRACTIVE
    };

    struct Material
    {

    Material(const double albedoValues[3], MaterialType materialType, bool smoothShadingFlag)
    : m_type(materialType), m_smoothShading(smoothShadingFlag) {
    m_albedo[0] = albedoValues[0];
    m_albedo[1] = albedoValues[1];
    m_albedo[2] = albedoValues[2];
    }

    double m_albedo[3] = {0.0, 0.0, 0.0};
    MaterialType m_type=MaterialType::DIFFUSE;
    bool m_smoothShading=false;
    void setIndexOfRefraction(double indexOfRefraction) 
    {
        m_indexOfRefractionWasSet = true;
        m_indexOfRefraction = indexOfRefraction;
    }
    std::optional<double> getIndexOfRefraction() const 
    {
        return m_indexOfRefractionWasSet ? std::optional<double>(m_indexOfRefraction) : std::nullopt;
    }
    double m_indexOfRefraction = 1.0;
    bool m_indexOfRefractionWasSet = false;
    };
}

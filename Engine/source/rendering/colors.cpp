#include "rendering/colors.hpp"
#include "tools/log.hpp"
#include "math/math.hpp"

bool bee::colorScheme::createColorScheme(const std::string& name,
                                         const float from,
                                         const glm::vec3& fromColor,
                                         const float to,
                                         const glm::vec3& toColor)
{
    if (to == from)
    {
        Log::Error("Values can not be the same");
        return false;
    }

    m_colorSchemes.emplace(name, std::vector<numberColor>{numberColor(from, fromColor), numberColor(to, toColor)});

    return true;
}

void bee::colorScheme::addColor(const std::string& name, const float value, const glm::vec3& color)
{
    m_colorSchemes.at(name).push_back(numberColor(value, color));
    //TODO: possibly add check if 2 values are the same
}

bool bee::colorScheme::getColor(const std::string& name, const float value, glm::vec3& returnColor) 
{ 
    auto& vector = m_colorSchemes.at(name);

    //Check if we need to order
    if (!vector.back().sorted)
    {
        std::sort(vector.begin(), vector.end(), [](const numberColor& a, const numberColor& b) { return a.number < b.number; });
        vector.back().sorted = true; // Set last sorted value to true
    }
    // points to value -> it->number > value
    auto it = std::lower_bound(vector.begin(), vector.end(), value, [](const numberColor& n, const float v) { return n.number < v; });

    // Cases where we want to immediatly return
    if (it == vector.end())
    {
        returnColor = (it - 1)->color;
        return false;
    }
    if (it == vector.begin() || it == it - 1 || it->number == value)
    {
        returnColor = it->color;
        return false;
    }
    else if (value == (it - 1)->number)
    {
        returnColor = (it - 1)->color;
        return false;
    }

    // Map value from 0 to 1
    const float range = (value - (it - 1)->number) / (it->number - (it - 1)->number);    

    const auto& color1 = (it - 1)->color;
    const auto& color2 = it->color;
    returnColor = Lerp(color1, color2, range);

    return true; 
}

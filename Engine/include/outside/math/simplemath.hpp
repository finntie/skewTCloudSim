#pragma once

#include <glm/glm.hpp>

namespace smplMath
{

// Lerps value, t should be in between 0 and 1.
template <class T>
T lerpValue(T a, T b, float t)
{
    return a * (1.0f - t) + b * t;
}


}
#include "core/input.hpp"
#include "core/engine.hpp"

#include "core/device.hpp"

using namespace bee;

glm::vec2 Input::GetMousePositionInViewport() const
{
    glm::vec2 mousePos(GetMousePosition());
    return glm::vec2(2 * mousePos.x / Engine.Device().GetWidth() - 1,
                     -(2 * mousePos.y / Engine.Device().GetHeight() - 1));
}
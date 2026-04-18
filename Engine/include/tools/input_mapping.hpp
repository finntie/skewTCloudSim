#pragma once

#include <map>
#include <optional>
#include <string>

#include "core/engine.hpp"
#include "core/input.hpp"

namespace bee
{

class InputAction_Button
{
public:
    InputAction_Button() {}
    InputAction_Button(std::optional<Input::KeyboardKey> key,
                       std::optional<Input::GamepadButton> button,
                       std::optional<std::pair<Input::GamepadAxis, float>> axis)
        : m_key(key), m_button(button), m_axis(axis)
    {
    }

    /// <summary>
    /// Checks and returns whether or not this action has been triggered in the current input frame.
    /// </summary>
    /// <param name="gamepadID">The ID of the gamepad to check.</param>
    bool IsTriggered(int gamepadID) const
    {
        if (m_key.has_value() && Engine.Input().GetKeyboardKeyOnce(m_key.value())) return true;
        if (gamepadID >= 0 && Engine.Input().IsGamepadAvailable(gamepadID))
        {
            if (m_button.has_value() && Engine.Input().GetGamepadButtonOnce(gamepadID, m_button.value())) return true;
            if (m_axis.has_value() && Engine.Input().GetGamepadAxis(gamepadID, m_axis.value().first) >= m_axis.value().second &&
                Engine.Input().GetGamepadAxisPrevious(gamepadID, m_axis.value().first) < m_axis.value().second)
                return true;
        }

        return false;
    }

private:
    std::optional<Input::KeyboardKey> m_key = std::nullopt;
    std::optional<Input::GamepadButton> m_button = std::nullopt;
    std::optional<std::pair<Input::GamepadAxis, float>> m_axis = std::nullopt;
};

class InputAction_Axis
{
public:
    InputAction_Axis() {}
    InputAction_Axis(std::optional<Input::KeyboardKey> negativeKey,
                     std::optional<Input::KeyboardKey> positiveKey,
                     std::optional<std::pair<Input::GamepadAxis, float>> gamepadAxis)
        : m_negativeKey(negativeKey), m_positiveKey(positiveKey), m_gamepadAxis(gamepadAxis)
    {
    }

    /// <summary>
    /// Gets the current axis value (taken from either gamepad or keyboard) associated to this input action.
    /// </summary>
    /// <param name="gamepadID">The ID of the gamepad to check.</param>
    /// <returns></returns>
    float GetValue(int gamepadID) const
    {
        // if a gamepad axis is specified, use that one only
        if (m_gamepadAxis.has_value() && gamepadID >= 0 && Engine.Input().IsGamepadAvailable(gamepadID))
        {
            float axisValue = Engine.Input().GetGamepadAxis(gamepadID, m_gamepadAxis.value().first);
            float deadZone = m_gamepadAxis.value().second;
            if (fabsf(axisValue) >= deadZone) return axisValue;
        }

        // otherwise, check the keyboard as well
        float result = 0.f;
        if (m_negativeKey.has_value() && Engine.Input().GetKeyboardKey(m_negativeKey.value())) result -= 1.f;
        if (m_positiveKey.has_value() && Engine.Input().GetKeyboardKey(m_positiveKey.value())) result += 1.f;

        return result;
    }

private:
    std::optional<Input::KeyboardKey> m_negativeKey = std::nullopt;
    std::optional<Input::KeyboardKey> m_positiveKey = std::nullopt;
    std::optional<std::pair<Input::GamepadAxis, float>> m_gamepadAxis = std::nullopt;
};

class InputMapping
{
public:
    void Register(const std::string& name, InputAction_Button action) { m_buttons[name] = action; }
    void Register(const std::string& name, InputAction_Axis axis) { m_axes[name] = axis; }

    bool IsActionTriggered(const std::string& name) const { return m_buttons.find(name)->second.IsTriggered(gamepadID); }
    float GetActionAxis(const std::string& name) const { return m_axes.find(name)->second.GetValue(gamepadID); }

private:
    int gamepadID = 0;
    std::map<std::string, InputAction_Button> m_buttons;
    std::map<std::string, InputAction_Axis> m_axes;
};

}  // namespace bee
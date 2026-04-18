#pragma once
#include <random>

namespace bee
{

/// <summary>
/// Linearly interpolates between two values.
/// </summary>
template <class T>
T Lerp(T a, T b, float t)
{
    t = std::max(0.0f, std::min(t, 1.0f));
    return a * (1.0f - t) + b * t;
}

/// <summary>
/// Returns the parameter t that was used to linearly interpolate between a and b.
/// </summary>
template <class T>
float InvLerp(T a, T b, T v)
{
    float t = (v - a) / (b - a);
    t = std::max(0.0f, std::min(t, 1.0f));
    return t;
}

/// <summary>
/// Remaps a value from one range to another.
/// </summary>
/// <param name="iF">Input range from</param>
/// <param name="iT">Input range to</param>
/// <param name="oF">Output range from</param>
/// <param name="oT">Output range to</param>
/// <param name="v">Value to remap</param>
template <class T>
T Remap(T iF, T iT, T oF, T oT, T v)
{
    float t = InvLerp(iF, iT, v);
    return Lerp(oF, oT, t);
}

/// <summary>
/// Framerate-independent exponential dampening.
/// </summary>
/// <typeparam name="T">Any type on which Lerp and InvLerp are defined.</typeparam>
/// <param name="a">From parameter</param>
/// <param name="b">To parameter</param>
/// <param name="lambda">Speed of dampening</param>
/// <param name="dt">Time since last frame</param>
template <class T>
T Damp(T a, T b, float lambda, float dt)
{
    return Lerp(a, b, 1 - exp(-lambda * dt));
}

}  // namespace bee

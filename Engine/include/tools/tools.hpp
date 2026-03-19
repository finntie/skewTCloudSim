#pragma once

#include <string>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace bee
{

inline void SwitchOnBitFlag(unsigned int& flags, unsigned int bit) { flags |= bit; }

inline void SwitchOffBitFlag(unsigned int& flags, unsigned int bit) { flags &= (~bit); }

inline bool CheckBitFlag(unsigned int flags, unsigned int bit) { return (flags & bit) == bit; }

inline bool CheckBitFlagOverlap(unsigned int flag0, unsigned int flag1) { return (flag0 & flag1) != 0; }

// https://stackoverflow.com/questions/2590677/how-do-i-combine-hash-values-in-c0x
inline uint32_t HashCombine(uint32_t lhs, uint32_t rhs)
{
    lhs ^= rhs + 0x9e3779b9 + (lhs << 6) + (lhs >> 2);
    return lhs;
}

inline glm::vec3 to_vec3(const glm::vec2& vec) { return glm::vec3(vec.x, vec.y, 0.0f); }

inline glm::vec3 to_vec3(std::vector<double> array) { return glm::vec3((float)array[0], (float)array[1], (float)array[2]); }

inline glm::quat to_quat(std::vector<double> array)
{
    return glm::quat((float)array[3], (float)array[0], (float)array[1], (float)array[2]);
}

inline glm::vec4 to_vec4(std::vector<double> array)
{
    return glm::vec4((float)array[0], (float)array[1], (float)array[2], (float)array[3]);
}

/// Replace all occurrences of the search string with the replacement string.
///
/// @param subject The string being searched and replaced on, otherwise known as the haystack.
/// @param search The value being searched for, otherwise known as the needle.
/// @param replace The replacement value that replaces found search values.
/// @return a new string with all occurrences replaced.
///
std::string StringReplace(const std::string& subject, const std::string& search, const std::string& replace);

/// Determine whether or not a string ends with the given suffix. Does
/// not create an internal copy.
///
/// @param subject The string being searched in.
/// @param prefix The string to search for.
/// @return a boolean indicating if the suffix was found.
///
bool StringEndsWith(const std::string& subject, const std::string& suffix);

/// Determine whether or not a string starts with the given prefix. Does
/// not create an internal copy.
///
/// @param subject The string being searched in.
/// @param prefix The string to search for.
/// @return a boolean indicating if the prefix was found.
///
bool StringStartsWith(const std::string& subject, const std::string& prefix);

/// Splits a string according to a delimiter
///
/// @param input The string to split.
/// @param delimiter The string to use as a splitting point.
/// @return a list of strings, representing the input string split around each delimiter occurrence.
///
std::vector<std::string> SplitString(const std::string& input, const std::string& delim);

/// <summary>
/// Generates and returns a random floating-point number between two values.
/// </summary>
/// <param name="min">A minimum value.</param>
/// <param name="max">A maximum value.</param>
/// <param name="decimals">The number of decimals to which the number should be (approximately) rounded.
/// Use a higher value to get a denser range of possible outcomes.</param>
float GetRandomNumber(float min, float max, int decimals = 3);


/// <summary>
/// Converts an HSV color to an RGB color.
/// </summary>
glm::vec3 HSVtoRGB(glm::vec3 hsv);

/// <summary>
/// Creates a random color with a given index, saturation and value.
/// The colors are generated in a way that they are visually distinct.
/// </summary>
glm::vec3 RandomNiceColor(int i, float s = 0.5f, float v = 0.95f);

}  // namespace bee
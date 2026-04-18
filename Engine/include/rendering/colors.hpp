#pragma once
#include <glm/glm.hpp>
#include <unordered_map>
#include <string>

namespace bee
{

struct numberColor
{
    numberColor(float n, glm::vec3 c) : number(n), color(c) {};
    float number{0};
    glm::vec3 color{0.0f, 0.0f, 0.0f};
    bool sorted{false};
};

class colorScheme
{
public:
    bool createColorScheme(const std::string& name, const float from, const glm::vec3& fromColor, const float to, const glm::vec3& toColor);

    void addColor(const std::string& name, const float value, const glm::vec3& color);

    bool getColor(const std::string& name, const float value, glm::vec3& returnColor);

private:
    std::unordered_map<std::string, std::vector<numberColor>> m_colorSchemes;

};

namespace Colors
{

inline glm::vec4 Black = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
inline glm::vec4 White = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
inline glm::vec4 Grey = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
inline glm::vec4 Red = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
inline glm::vec4 Green = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
inline glm::vec4 Blue = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
inline glm::vec4 DodgerBlue = glm::vec4(0.0f, 0.5f, 1.0f, 1.0f);
inline glm::vec4 Orange = glm::vec4(1.0f, 0.66f, 0.0f, 1.0f);
inline glm::vec4 Cyan = glm::vec4(0.0f, 1.0f, 1.0f, 1.0f);
inline glm::vec4 Magenta = glm::vec4(1.0f, 0.0f, 1.0f, 1.0f);
inline glm::vec4 Yellow = glm::vec4(1.0f, 1.0, 0.0f, 1.0f);
inline glm::vec4 Purple = glm::vec4(0.55f, 0.0, 0.65f, 1.0f);
inline glm::vec4 Pink = glm::vec4(1.0f, 0.0, 0.72f, 1.0f);
inline glm::vec4 Brown = glm::vec4(0.5f, 0.3f, 0.0f, 1.0f);

}  // namespace Colors

}  // namespace bee

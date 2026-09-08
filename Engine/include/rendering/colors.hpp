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

inline glm::vec4 BlackA = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
inline glm::vec4 WhiteA = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
inline glm::vec4 GreyA = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
inline glm::vec4 RedA = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
inline glm::vec4 GreenA = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
inline glm::vec4 BlueA = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
inline glm::vec4 DodgerBlueA = glm::vec4(0.0f, 0.5f, 1.0f, 1.0f);
inline glm::vec4 OrangeA = glm::vec4(1.0f, 0.66f, 0.0f, 1.0f);
inline glm::vec4 CyanA = glm::vec4(0.0f, 1.0f, 1.0f, 1.0f);
inline glm::vec4 MagentaA = glm::vec4(1.0f, 0.0f, 1.0f, 1.0f);
inline glm::vec4 YellowA = glm::vec4(1.0f, 1.0, 0.0f, 1.0f);
inline glm::vec4 PurpleA = glm::vec4(0.55f, 0.0, 0.65f, 1.0f);
inline glm::vec4 PinkA = glm::vec4(1.0f, 0.0, 0.72f, 1.0f);
inline glm::vec4 BrownA = glm::vec4(0.5f, 0.3f, 0.0f, 1.0f);

inline glm::vec3 Black = glm::vec3(0.0f, 0.0f, 0.0f);
inline glm::vec3 White = glm::vec3(1.0f, 1.0f, 1.0f);
inline glm::vec3 Grey = glm::vec3(0.5f, 0.5f, 0.5f);
inline glm::vec3 Red = glm::vec3(1.0f, 0.0f, 0.0f);
inline glm::vec3 Green = glm::vec3(0.0f, 1.0f, 0.0f);
inline glm::vec3 Blue = glm::vec3(0.0f, 0.0f, 1.0f);
inline glm::vec3 DodgerBlue = glm::vec3(0.0f, 0.5f, 1.0f);
inline glm::vec3 Orange = glm::vec3(1.0f, 0.66f, 0.0f);
inline glm::vec3 Cyan = glm::vec3(0.0f, 1.0f, 1.0f);
inline glm::vec3 Magenta = glm::vec3(1.0f, 0.0f, 1.0f);
inline glm::vec3 Yellow = glm::vec3(1.0f, 1.0, 0.0f);
inline glm::vec3 Purple = glm::vec3(0.55f, 0.0, 0.65f);
inline glm::vec3 Pink = glm::vec3(1.0f, 0.0, 0.72f);
inline glm::vec3 Brown = glm::vec3(0.5f, 0.3f, 0.0f);


}  // namespace Colors

}  // namespace bee

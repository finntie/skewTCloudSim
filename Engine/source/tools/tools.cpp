#include "tools/tools.hpp"

using namespace std;

// Courtesy of: http://stackoverflow.com/questions/5878775/how-to-find-and-replace-string
string bee::StringReplace(const string& subject, const string& search, const string& replace)
{
    string result(subject);
    size_t pos = 0;

    while ((pos = subject.find(search, pos)) != string::npos)
    {
        result.replace(pos, search.length(), replace);
        pos += search.length();
    }

    return result;
}

bool bee::StringEndsWith(const string& subject, const string& suffix)
{
    // Early out test:
    if (suffix.length() > subject.length()) return false;

    // Resort to difficult to read C++ logic:
    return subject.compare(subject.length() - suffix.length(), suffix.length(), suffix) == 0;
}

bool bee::StringStartsWith(const string& subject, const std::string& prefix)
{
    // Early out, prefix is longer than the subject:
    if (prefix.length() > subject.length()) return false;

    // Compare per character:
    for (size_t i = 0; i < prefix.length(); ++i)
        if (subject[i] != prefix[i]) return false;

    return true;
}

std::vector<std::string> bee::SplitString(const std::string& input, const std::string& delim)
{
    std::vector<std::string> result;
    size_t pos = 0, pos2 = 0;
    while ((pos2 = input.find(delim, pos)) != std::string::npos)
    {
        result.push_back(input.substr(pos, pos2 - pos));
        pos = pos2 + 1;
    }

    result.push_back(input.substr(pos));

    return result;
}

float bee::GetRandomNumber(float min, float max, int decimals)
{
    auto p = static_cast<int>(pow(10, decimals));
    auto imin = static_cast<int>(min * p);
    auto imax = static_cast<int>(max * p);

    int irand = imin + rand() % (imax - imin);
    float val = (float)irand / p;
    return val;
}

glm::vec3 bee::HSVtoRGB(glm::vec3 hsv)
{
    // HSV to RGB conversion - hsv components are in the range [0, 1]
    float h = hsv.x * 360;
    float s = hsv.y;
    float v = hsv.z;

    float c = v * s;
    float x = c * (1 - (float)abs(fmod(h / 60.0f, 2) - 1));
    float m = v - c;

    float r, g, b;
    if (h >= 0 && h < 60)
    {
        r = c;
        g = x;
        b = 0;
    }
    else if (h >= 60 && h < 120)
    {
        r = x;
        g = c;
        b = 0;
    }
    else if (h >= 120 && h < 180)
    {
        r = 0;
        g = c;
        b = x;
    }
    else if (h >= 180 && h < 240)
    {
        r = 0;
        g = x;
        b = c;
    }
    else if (h >= 240 && h < 300)
    {
        r = x;
        g = 0;
        b = c;
    }
    else
    {
        r = c;
        g = 0;
        b = x;
    }

    return glm::vec3(r + m, g + m, b + m);

}

glm::vec3 bee::RandomNiceColor(int i, float s, float v)
{
    float h = (i * 0.618033988749895f) - (int)(i * 0.618033988749895f);
    return HSVtoRGB(glm::vec3(h, s, v));
}


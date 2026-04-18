#pragma once

#ifdef BEE_PLATFORM_PC
#define NOMINMAX
#include <Windows.h>
#endif  // BEE_PLATFORM_PC

// This should be only place glad is included
#include <glad/glad.h>
#include <string>

namespace bee
{
#ifdef BEE_DEBUG
#define BEE_DEBUG_ONLY(x) (x)
void InitDebugMessages();
#else
#define BEE_DEBUG_ONLY(x)
inline void InitDebugMessages() {}
#endif
void LabelGL(GLenum type, GLuint name, const std::string& label);
}  // namespace bee

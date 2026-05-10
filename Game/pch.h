#pragma once

#include <sstream>
#include <iostream>
#include <cassert>
#include <memory>
#include <chrono>
#include <ctime>
#include <algorithm> //For std::clamp
#include <cstdlib>
#include <crtdbg.h>
#include <random>

// Data structures
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <map>


// Engine (not soon to be changed)
#include "core/engine.hpp"
#include "core/input.hpp"
#include "core/transform.hpp"
#include "core/ecs.hpp"
#include "core/resources.hpp"
#include "core/device.hpp"

#include "rendering/render.hpp"
#include "rendering/colors.hpp"
#include "rendering/debug_render.hpp"
#include "rendering/model.hpp"
#include "rendering/mesh.hpp"

#include "math/geometry.hpp"

#include "platform/opengl/draw_image.hpp"
#include "platform/opengl/render_gl.hpp"

#include "tools/inspector.hpp"
#include "tools/profiler.hpp"
#include "tools/log.hpp"
#include "tools/inspectable.hpp"


// External libraries/files
#include "Superluminal/PerformanceAPI.h"
#include "imgui/imgui.h"
#include "imgui/IconsFontAwesome.h"
#include "imgui/imgui_internal.h"

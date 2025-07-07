
#include "core/ecs.hpp"
#include "core/engine.hpp"

#include "core/transform.hpp"
#include "rendering/debug_render.hpp"
#include "rendering/render.hpp"
#include "Superluminal/PerformanceAPI.h"
#include "core/input.hpp"
#include <string>


//Game includes
#include "gameSystem.h"
#include "environment.h"

using namespace bee;
using namespace std;

int main(int, char**)
{
    Engine.Initialize();

    //Create systems
    Engine.ECS().CreateSystem<Renderer>();
    Engine.ECS().CreateSystem<environment>();
    Engine.ECS().CreateSystem<gameSystem>();

    auto cameraEntity = Engine.ECS().CreateEntity();
    auto& transform = Engine.ECS().CreateComponent<Transform>(cameraEntity);
    transform.Name = "Camera";
    Engine.ECS().CreateComponent<Camera>(cameraEntity).Projection = glm::perspective(glm::radians(60.0f), 1.77f, 0.2f, 500.0f);
    auto view = glm::lookAt(glm::vec3(0, 0, 40), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
    transform.SetFromMatrix(glm::inverse(view));
    transform.SetTranslation(glm::vec3(10, 30, 70));
    

    //Run the engine and thus start the game
    Engine.Run();
    Engine.Shutdown();
    
    _CrtDumpMemoryLeaks();

    return 0;
}

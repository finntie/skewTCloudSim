#define _CRTDBG_MAP_ALLOC

//Game includes
#include "core/engine.hpp"
#include "core/ecs.hpp"
#include "core/transform.hpp"
#include "rendering/render.hpp"
#include "rendering/model.hpp"

#include <glm/glm.hpp>
using namespace bee;
using namespace std;

int main(int, char**)
{
    //kerneltest();

    //_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    Engine.Initialize();
     
    //Create systems
    Engine.ECS().CreateSystem<Renderer>(); //<-- bee renderer
   
    //Quick setup of camera and light
    {
        auto cameraEntity = Engine.ECS().CreateEntity();
        auto& transform = Engine.ECS().CreateComponent<Transform>(cameraEntity);
        transform.Name = "Camera";
        Engine.ECS().CreateComponent<Camera>(cameraEntity).Projection = glm::perspective(glm::radians(60.0f), 1.77f, 0.2f, 100000.0f);
        auto view = glm::lookAt(glm::vec3(0, 0, 40), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
        transform.SetFromMatrix(glm::inverse(view));
        transform.SetTranslation(glm::vec3(10, 30, 70));

        //Add light
        Entity lightEntity = Engine.ECS().CreateEntity();
        Transform& transformLight = Engine.ECS().CreateComponent<Transform>(lightEntity);
        transformLight.Name = "Light";
        transformLight.SetTranslation(glm::vec3(0, 0, 10));
        transformLight.SetRotation(glm::quatLookAt(glm::vec3(0, 0, 0), glm::vec3(0, 0, 1)));
        Engine.ECS().CreateComponent<Light>(lightEntity) = Light(glm::vec3(1.0f), 500.0f, 10000.0f, Light::Type::Directional);

        Entity sphereEntity = Engine.ECS().CreateEntity();
        Transform& transformSphere = Engine.ECS().CreateComponent<Transform>(sphereEntity);
        transformSphere.Name = "Village";
        transformSphere.SetTranslation(glm::vec3(0, 0, 0));
        transformSphere.SetRotation(glm::quatLookAt(glm::vec3(0, 0, 0), glm::vec3(0, 0, 1)));
        //Model& sphereModel = Engine.ECS().CreateComponent<Model>(sphereEntity, FileIO::Directory::Assets, "models/solarpunk_village/scene.gltf");
        Model& sphereModel = Engine.ECS().CreateComponent<Model>(sphereEntity, FileIO::Directory::Assets, "models/volgar_village/scene.gltf");

        sphereModel.Instantiate(sphereEntity);
    }

    //Run the engine and thus start the game
    Engine.Run();
    Engine.Shutdown();
    
    //_CrtDumpMemoryLeaks();

    return 0;
}

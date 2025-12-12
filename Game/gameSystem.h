#pragma once
#include <glm/glm.hpp>
#include "core/ecs.hpp"
#include "tools/inspectable.hpp"

using namespace bee; 


class gameSystem : public bee::System, public bee::IPanel
{
public:
	gameSystem();
	~gameSystem();

	void Update(float dt) override;
	void Render() override;

//#ifdef BEE_INSPECTOR
	void OnPanel() override;
	std::string GetName() const override;
	std::string GetIcon() const override;
//#endif


private:

};

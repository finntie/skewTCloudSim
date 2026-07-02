#pragma once

using namespace bee; 


enum gameStates
{
	STARTMENU, CHOOSEDATE, SKEWTMAKER, SIMULATION
};

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

	void startMenu();
//#endif


private:

	bool loaded{ false };
	bool makingSkewT{ false };
	gameStates m_currentState = STARTMENU;
	std::string m_countrySelected{};
	std::string m_yearSelected{};
	std::string m_monthSelected{};
	std::string m_daySelected{};
};

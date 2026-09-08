#pragma once

using namespace bee; 

/*
 *													GameStates / Menus
 * 
 * 
 *  +-----------+     +----------------------+    											 +---------------------------+				+--------------+
 *  | Main Menu |---->|  Create Simulation   |---------------------------------------------->|	     SkewT Creator       |------------->| Simulation   |
 *  +-----------+     +----------------------+    											 +---------------------------+				+--------------+
 *        |                          |             +---------------------------+						   ^
 *        |                          +------------>| Observed Sounding Select. |---------------------------+
 *        |                          |             +---------------------------+				     	   |
 *        |                          |             +---------------------------+						   |
 *        |                          +------------>| Custom Environment Select.|---------------------------+
 *        |                                        +---------------------------+														
 *        |
 *		  |			  +---------------------------+											    +--------------------+
 *		  +---------->| View Simulation Selection |-------------------------------------------->| View Simulation    |
 *  				  +---------------------------+											    +--------------------+
 */

enum gameStates
{
	MAINMENU, 
	CREATE_SIMULATION, 
	SKEWT_CREATOR, 
	OBSERVED_SOUNDING_SELECTION, 
	CUSTOM_ENVIRONMENT_SELECTION, 
	SIMULATION,
	VIEW_SIMULATION_SELECTION, 
	VIEW_SIMULATION,
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
	gameStates m_currentState = MAINMENU;
	std::string m_countrySelected{};
	std::string m_yearSelected{};
	std::string m_monthSelected{};
	std::string m_daySelected{};
};

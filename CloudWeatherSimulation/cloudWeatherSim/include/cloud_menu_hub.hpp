#pragma once

/*
 *													GameStates / Menus
 * 
 * 
 *                    +----------------------+        +---------------------+				 +---------------------------+				+--------------+
 *        +---------->|  Create Simulation   |--------|  Create Environment |--------------->|	     SkewT Creator       |------------->| Simulation   |
 *        |           +----------------------+        +---------------------+		    	 +---------------------------+				+--------------+
 *        |                          |             +---------------------------+						   ^
 *        |                          +------------>| Observed Sounding Select  |---------------------------+
 *  +-----------+                    |             +---------------------------+				     	   |
 *  | Main Menu |                    |             +---------------------------+						   |
 *  +-----------+                    +------------>| Custom Environment Select |---------------------------+
 *        |                                        +---------------------------+														
 *        |
 *		  |			  +---------------------------+											    +--------------------+
 *		  +---------->| View Simulation Selection |-------------------------------------------->| View Simulation    |
 *  				  +---------------------------+											    +--------------------+
 */

class simulationHub;
class cloudRenderer;
struct ImGuiContext;
struct ImPlotContext;
struct GLFWwindow;

// Class so it is useable in other headers
enum class gameStates
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

class cloudMenu
{
public:
    cloudMenu(simulationHub* simHubObj, cloudRenderer* renderHubObj);

    ~cloudMenu();

    // Update menu
    void update();

    // Getters
    bool getIsPanelSelected() { return m_panelSelected; }

    // We need to handle ImGui callbacks ourself due to possibly having multiple contexts
    void callbackImGuiCursorPos(GLFWwindow* win, double xpos, double ypos);
    void callbackImGuiScroll(GLFWwindow* win, double xoffset, double yoffset);
    void callbackImGuiKey(GLFWwindow* win, int key, int scan, int action, int mods);
    void callbackImGuiMouseButton(GLFWwindow* win, int button, int action, int mods);


private:

    bool initializeImGui();

    void otherMenus(const bool initState, bool& setInitState);

    void checkPanelSelection();

    void startMenu(const bool initState, bool& setInitState);
    void styleStartMenu(bool reset);

    void setImGuiContext();
    void unsetImGuiContext();

    void imguiStyle();

    simulationHub* m_simHubObj{nullptr};  // Cloud Hub owns this object
    cloudRenderer* m_renderHubObj{nullptr};  // Cloud Hub owns this object
    gameStates m_currentState{gameStates::MAINMENU};
    bool m_gameInitStateChange = true; // When state changes goes to true, calling initial values, after one pass will be false

    // ImGui 
    bool m_ImGuiInitialized{false};
    bool m_panelHovered{false};
    bool m_panelSelected{false};
    ImGuiContext* m_ImGuiContext{nullptr};
    ImPlotContext* m_ImPlotContext{nullptr};
    ImGuiContext* m_PrevImGuiContext{nullptr};
    ImPlotContext* m_PrevImPlotContext{nullptr};

};
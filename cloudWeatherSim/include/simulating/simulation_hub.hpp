#pragma once

#include <thread>
#include <atomic>

class readTable;
class skewTMaker;
class skewTFile;
class cloudFile;
class environmentGPU;
class simulationEditor;
enum class gameStates;

class simulationHub
{
public:

	simulationHub();
    ~simulationHub();

    void shutDown();

    void updateSimulation(float dt);

	// ImGui Handler
    void simulationMenuPanel(gameStates& currentState, const bool initState, bool& setInitState);
    void simulationPanel(gameStates& currentState, const bool initState, bool& setInitState);
    void simulationMainPanel();

	readTable& ReadTable() { return *m_readTableObj; }
    skewTMaker& SkewTMaker() { return *m_skewTMakerObj; }
    skewTFile& SkewTFile() { return *m_skewTFileObj; }
    environmentGPU& Environment() { return *m_environmentObj; }
    simulationEditor& SimEditor() { return *m_simulationEditObj; }

private:

    void setSimulation();
    bool chooseObservedSounding(bool init);

	readTable* m_readTableObj{nullptr};
    skewTMaker* m_skewTMakerObj{nullptr};
    skewTFile* m_skewTFileObj{nullptr};
    environmentGPU* m_environmentObj{nullptr};
    simulationEditor* m_simulationEditObj{nullptr};

    gameStates* m_gameState{nullptr};

    // Simulation variables
    bool m_simulationActive{false};
    int m_simulationSteps{false};
    float m_simulationSpeed{1.0f};
    bool m_editing{false};
    bool m_skewT{false};

    // Thread data
    std::thread simThread;
    std::atomic<bool> m_running;
    std::atomic<float> m_speed;
    const float m_maxDeltaTimeSimulation = 1.0f / 2.5f;  // set max delatime of 2.5 fps
    bool m_simulationInitialized{false};

};
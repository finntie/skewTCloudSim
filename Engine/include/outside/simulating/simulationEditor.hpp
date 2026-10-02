#pragma once
#include "outside/config.h"
#include "outside/simulating/environment.hpp"

#include <glm/glm.hpp>

class skewTer;
class colorScheme;
class dataClass;
class tracing;
struct envDebugData;


class simulationEditor
{
public:

    simulationEditor();
    ~simulationEditor();

    // Should be initialized after environment
    void init();

    // Convert GPU values to CPU values
    void GPUSetEnv(bool setIsentropics);

    // Initializes the skewT data, Make sure to call AFTER #GPUSetEnv()
    void initializeSkewT();

    void update(bool editing, bool skewT);

    // Handles ImGui
    void viewPanel();
    void editPanel();
    void skewTPanel();
    void dataPanel(bool simActive);

private:


    void updateEditing(bool editMode);
    void updateSkewT(bool skewTView);

    void setColorScheme();

    void updateValues();

    void dataToSkewTData(float* potentialTemp, float* dewpoint);

    void setSliceMinMax(bool fullView);

    void controls();

    // ImGui
    void viewParamValuesPanel(); // Show parameter info on the selected index
    void viewParamsPanel(); // Buttons to view the parameters
    void slicePanel();
    void editParamsPanel();
    void skewTTexturePanel();
    void setSkewTDataPanel();
    void dataViewPanel(bool simActive);
    void dataSettingsPanel();
    void shortCutViewDataPanel();
    void viewTooltipDataPanel();
    void viewPickerPanel();
    int chooseDateDay();  // Choose day of the year

    // ImGui Helpers
    int getDaysInMonth(int month);
    void dayToMonthDay(int dayOfYear, int& month, int& dayOfMonth);
    glm::vec2 getMinMaxVaueParam(parameter param);
    const char* getFormatParam(parameter param, int& flagOutput);
    void vectorArrow();

    // Editor
    void viewBrush();
    void applyBrush();
    void viewSelect();
    void applySelect();
    void usePicker();

    // Visualizing
    void viewDebugSky();
    void viewDebugGround();


    // Other helpers
    float getValueParam(const int index, parameter param);


    skewTer* m_skewTObj{nullptr};
    envDebugData* m_envEditData{nullptr};
    colorScheme* m_colorSchemeObj{nullptr};
    dataClass* m_dataClassObj{nullptr};
    tracing* m_tracingObj{nullptr};


    // Class wide deltatime, easy access
    float m_deltatime = 1.0f / 60.0f;

    // Parameters 
    parameter m_viewParamSky{POTTEMP};
    parameterGround m_viewParamGround{TEMP};
    parameter m_editParamSky{POTTEMP};

    // Mouse selection in grid
    int m_mousePointingIndex{0};
    glm::ivec3 m_mousePointingPos{0};
    bool m_selectionInGrid{false};

    // Skew-T variables
    int m_skewTidx{0};
    glm::ivec3 m_skewTPos{0};

    // Viewing Settings
    bool m_viewSlice{true};
    bool m_viewGround{false};
    bool m_viewSky{false};
    bool m_wireFrame{false};
    // X = 0, Y = 1, Z = 2
    int m_viewSliceCoord{0};
    int m_atSliceViewSlice{0};
    int m_minViewX{0};
    int m_minViewY{0};
    int m_minViewZ{0};
    int m_maxViewX{GRIDSIZESKYX};
    int m_maxViewY{GRIDSIZESKYY};
    int m_maxViewZ{GRIDSIZESKYZ};

    // Controls
    float m_mouseWheel{0.0f};

    // Editing variables
    bool m_brushing{false};
    bool m_selecting{false};
    float m_brushSize{1.0f};
    float m_brushIntensity{1.0f};  // TODO: change to parameter specific
    float m_brushSmoothness{1.0f};
    bool m_groundErase = false;
    // Apply values of brush
    float m_applyValue{1.0f};
    glm::vec3 m_valueDir{0, 0, 1};
    // Selecting values, saving previous select location, resetting and more
    glm::ivec3 m_saveSelectPos{0};
    bool m_selectReset{false};
    glm::ivec3 m_corners[2]{};
    bool m_microPhysDataSelect{false};
    bool m_justViewSelection{false};
    
	// Diurnal cycle variables
    float m_time = 43200.0f;                 // 0 to 86.400 time in seconds
    const float m_dayLightDuration = 14.0f;  // TODO: should be calculated using longitude and day
    const float m_hourOfSunrise = 6.0f;
    bool m_timeChanged = false;
    float m_longitude = 52.37f;  // Longitude on earth, 52.37 is Amsterdam
    int m_day = 130;             // Day of the year
    float m_sunStrength = 1.0f;
    bool m_pauseDiurnal = false;
};



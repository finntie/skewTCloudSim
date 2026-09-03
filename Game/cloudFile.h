#pragma once
#include "environment.h"
#include <string>
#include <vector>

class cloudFile
{
public:

	cloudFile() = default;
	~cloudFile() = default;

	void panel();
	void deletePopup();
	void confirmPopup();
	bool coloredSelectable(const char* label, bool* value);

	bool isOneTypeSelected();
	int totalTypesSelected();
	float* typeToPointer(int type, int frame, bool sky);
	std::string typeToString(int type, bool sky);

	// Check if file is valid, also include extension (.bin)
	bool checkFile(const char* fileName, std::string& outputFullPath);

	// Checks if a frame has to be created based on recording status and time. 
	// Copies data into one frame if needed
	// Returns true if frame was created
	bool tryCreateFrame(environment::gridDataSky& skyData, environment::gridDataGround& groundData, float time);

	void saveToFile();


private:

	// Data of all frames
	std::vector<environment::gridDataSky> m_skyData;
	std::vector<environment::gridDataGround> m_groundData;


	std::string m_fileName;
	std::string m_fullFilePath;

	int m_totalFrames = 0;
	int m_framesPerHour = 60;
	bool m_recording = false;
	bool m_paused = false;
	float m_currentTime = 0.0f;
	bool m_validFileName = false;

	// Qw, Qc, Qr, Qs, Qi, Qv, Temp, Wind, Pressure
	bool m_typesSky[9]{ false, false,false,false,false,false,false,false,false };
	// Temp, Water, Qr, Qs, Qi
	bool m_typesGround[5]{ false,false,false,false,false };

	// Stores timeframes of the frames
	std::vector<float> m_frames;

};

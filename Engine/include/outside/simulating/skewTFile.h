#pragma once

#include <vector>
#include <string>
#include <glm/glm.hpp>

struct fileInfo
{
	std::string fileName{};
	std::string pathToFile{};

	std::string country{};
	std::string institute{};
	std::string station{}; // Station number or name (for example DeBilt or 01303)

	std::vector<std::string> dates{}; // Possibility that file includes multiple dates of data

	bool hasExtraInfoFile{ false }; // Custom written file to include some extra data
};

class skewTFile
{
public:

	void init(); 


	// Reader functions
	void openAndReadFile(fileInfo& file, std::string& date);
	void readMWXFile(const char* _file);
	void readDWDFile(fileInfo& file, std::string& _date);

	std::vector<fileInfo> m_allFiles{};
	std::vector<int> m_uniqueDates{};

	void getAvailableYears(std::vector<std::string>& years);
	void getAvailableMonths(std::string& year, std::vector<std::string>& months);
	void getAvailableDays(std::string& year, std::string& month, std::vector<std::string>& days);
	void getAvailableFiles(std::string& year, std::string& month, std::string& day, std::vector<fileInfo>& availableFiles);

private:

	// Helper Init functions
	void checkKNMIFiles();
	void checkDWDFiles();


	// Helper functions
	void degreeToDir(std::vector<float>& degreeRotation, std::vector<glm::vec2>& dirRotation);




};


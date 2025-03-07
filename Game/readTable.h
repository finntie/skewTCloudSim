#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <map>


struct radioScondePart
{
	float altitude = 0.0f; //In meter
	float pressure = 0.0f; //In hPa
	float dewPoint = 0.0f; //In Celcius
	float temperature = 0.0f; //In Celcius
	float windSpeed = 0.0f;
	float windDir = 0.0f;
};

struct skewTInfo
{
	std::vector<radioScondePart> data;
	std::map<float, float> heightToPressure;
	std::map<float, float> pressureToHeight;
	std::string stationNumberName{};

};

class readTable
{
public:

	void readKNMIFile(const char* file);

	void readDWDFile(const char* file);

	void heightToPressureCalculate();
	float getPressureAtHeight(float height);
	float getHeightAtPressure(float pressure);
	
	glm::vec2 convertToPlottingCoordinates(float temp, float pressure);

	void debugDrawData();

	float angle = 45.0f;
	float liquid = 1.0f;
	float plotHeight = 0.005f;
	bool useI = false;

private:

	skewTInfo testInfo;


};


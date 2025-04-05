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
	
	//Converts temp and height in meters or pressure to plotting so its easily modified.
	//Warning: Not using pressure (so height in meters) could lead up to different values due to observed not being the same as standard.
	glm::vec2 convertToPlottingCoordinates(const float temp, const float value, const bool pressure, const float scaleWidht, const float height);

	void debugDrawData();

	float angle = 45.0f;
	float liquid = 1.0f;
	float plotHeight = 0.005f;
	bool useI = false;
	float CAPE = 0.0f;
	const float tanTheta = glm::tan(glm::radians(45.0f));
	glm::vec2 sizeSkewT = { 1.0f,100 };


	skewTInfo testInfo;
private:



};


#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <map>
#include <memory>


struct radioSondeData
{
	std::unique_ptr<float[]> altitude; //In meter
	std::unique_ptr<float[]> pressure; //In hPa
	std::unique_ptr<float[]> dewPoint; //In Celcius
	std::unique_ptr<float[]> temperature; //In Celcius
	std::unique_ptr<float[]> windSpeed;
	std::unique_ptr<float[]> windDir;

	size_t dataSize = 0;

	void allocate(size_t size)
	{
		altitude = std::make_unique<float[]>(size);
		pressure = std::make_unique<float[]>(size);
		dewPoint = std::make_unique<float[]>(size);
		temperature = std::make_unique<float[]>(size);
		windSpeed = std::make_unique<float[]>(size);
		windDir = std::make_unique<float[]>(size);
		dataSize = size;
	}

	radioSondeData() = default;

	// Add move constructor
	radioSondeData(radioSondeData&&) = default;
	radioSondeData& operator=(radioSondeData&&) = default;
	// Prevent copying
	radioSondeData(const radioSondeData&) = delete;
	radioSondeData& operator=(const radioSondeData&) = delete;
};

struct skewTInfo
{
	radioSondeData data = radioSondeData();
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


	skewTInfo skewTData;
private:



};


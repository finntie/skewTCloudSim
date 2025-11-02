#include "readTable.h"

//Engine specific
#include "core/engine.hpp"
#include "rendering/debug_render.hpp"
#include "rendering/colors.hpp"

#include "zip_file.hpp"
#include "math/constants.hpp"
#include "math/meteoformulas.h"
#include "environment.h"
#include "game.h"

#include <sstream>
#include <iostream>
#include <cassert>
#include <memory>

#include <CUDA/include/cuda_runtime.h>
#include <CUDA/include/cuda.h>
#include "kernelTest.cuh"

using namespace Constants;
using namespace bee;


void readTable::readKNMIFile(const char* _file)
{
	//Open the MWX file
	printf("Reading file...\n");
	miniz_cpp::zip_file file(_file);
	printf("File read\n");

	if (!file.has_file("SynchronizedSoundingData.xml")) //Should always be included
	{
		perror("Error, file: SynchronizedSoundingData.xml is not included\n");
		return;
	}

	std::string soundingDataFile = file.read("SynchronizedSoundingData.xml");

	std::stringstream s(soundingDataFile);
	std::string word;
	std::string line;
	int row = 0;
	//Vector for every data type
	std::vector<float> pressure;
	std::vector<float> temperature;
	std::vector<float> dewPoint;
	std::vector<float> windDir;
	std::vector<float> windSpeed;
	std::vector<float> altitude;

	std::getline(s, line); //Skip first line
	while (std::getline(s, line))
	{
		std::stringstream ss(line);
		int count = 0;
		while (ss >> word)
		{
			switch (count)
			{
			case 4://pressure
				word = &word[10];
				word.pop_back();
				pressure.push_back(std::stof(word));
				break;
			case 5://Temp in K
				word = &word[13];
				word.pop_back();
				temperature.push_back(std::stof(word) - 273.15f);
				break;
			case 6://Dew in humidity
				//For increased speed but less accurasy, the part after the Multiplication could be removed 
				word = &word[10];
				word.pop_back();
				dewPoint.push_back(temperature[row] - ((100 - std::stof(word)) * 0.2f) * ((temperature[row] + 273.15f) * 0.01f));
				break;
			case 7://WindDir
				word = &word[9];
				word.pop_back();
				windDir.push_back(std::stof(word));
				break;
			case 8://WindSpeed
				word = &word[11];
				word.pop_back();
				windSpeed.push_back(std::stof(word));
				break;
			default:
				//Because values have been added, these parts will not be hard-coded
				if (word.substr(0, 8) == "Altitude")
				{
					word = &word[10];
					word.pop_back();
					altitude.push_back(std::stof(word));
					break;
				}
				if (word.substr(0, 8) == "Dropping")
				{
					word = &word[10];
					word.pop_back();
					//int dropping = std::stoi(word);
					if (false/*dropping*/) continue; //We discard when the balloon pops
				}
				break;
			}
			count++;
		}
		row++;
	}
	row--; //Go back once

	//Copy the data over
	{
		//Copy the data over
		{
			skewTData.data.allocate(row);
			std::memcpy(skewTData.data.temperature, temperature.data(), row * sizeof(float));
			std::memcpy(skewTData.data.dewPoint, dewPoint.data(), row * sizeof(float));
			std::memcpy(skewTData.data.windDir, windDir.data(), row * sizeof(float));
			std::memcpy(skewTData.data.windSpeed, windSpeed.data(), row * sizeof(float));
			std::memcpy(skewTData.data.pressure, pressure.data(), row * sizeof(float));
			std::memcpy(skewTData.data.altitude, altitude.data(), row * sizeof(float));
		}
	}

	heightToPressureCalculate();
}




void readTable::readDWDFile(const char* _file)
{
	//Open the zip file
	printf("Reading zip file...\n");
	miniz_cpp::zip_file file(_file);
	printf("Zip file read\n");

	std::string stationNumber;
	std::vector<std::string> AllDates;
	std::vector<float> groundTempsAtDates;
	std::string targetDate;

	//Get stationNumber
	{
		std::stringstream s(_file);
		std::string word;
		int count = 0;
		while (std::getline(s, word, '_'))
		{
			if (count == 2)
			{
				stationNumber = word;
				break;
			}
			count++;
		}
	}


	//Check all dates that will be in the file
	{
		std::string fileName = "Metadaten_Sekunde_Aero_" + stationNumber + ".txt";

		if (!file.has_file(fileName)) //Should always be included
		{
			perror("Error, file: Metadaten_Sekunde_Aero_....txt is not included\n");
			return;
		}
		printf("Reading meta file...\n");
		std::string metaSoundingDataFile = file.read(fileName);
		printf("Meta File read\n");

		std::stringstream s(file.read(fileName));
		std::string line;

		std::getline(s, line); //skip first one
		while (std::getline(s, line))
		{
			std::stringstream ss(line);
			std::string word;
			int count = 0;

			while (std::getline(ss, word, ';'))
			{
				if (count == 1)
				{
					word.pop_back(); //Remove time
					word.pop_back();
					AllDates.push_back(word);
				}
				else if (count == 8) //Ground temps
				{
					groundTempsAtDates.push_back(std::stof(word));
					break;
				}
				count++;
			}
		}

	}

	//TODO: Could do something with the dates, i.e. select a date, but for now we will grab the latest.
	targetDate = AllDates[161] + "12"; // 2025 03 24 

	//targetDate = AllDates[243] + "12"; // 2024 05 01 - 1700 Cape?

	printf("Reading file %s\n", targetDate.c_str());

	std::string fileName = "produkt_sec_aero_" + AllDates[0] + "_" + AllDates[AllDates.size() - 1] + "_" + stationNumber + ".txt";

	if (!file.has_file(fileName)) //Should always be included
	{
		perror("Error, file: produkt_sec_aero_n_n_n.txt is not included\n");
		return;
	}

	std::string soundingDataFile = file.read(fileName);

	std::stringstream s(soundingDataFile);
	bool reachedDate = false;
	std::string line;

	//Skip lines until reached desired date
	while (!reachedDate)
	{
		std::getline(s, line); //Skip to beginning of next line
		std::getline(s, line);

		std::string word;
		std::stringstream ss(line);
		int count = 0;
		while (std::getline(ss, word, ';'))
		{
			if (count == 1) //date
			{
				if (word == targetDate)
				{
					s.seekg(-100000, std::ios::cur); //Move back
					if (s.fail()) s.clear(); //Remove error flag
					reachedDate = true;
					std::getline(s, line);
					break;
				}
				else
				{
					s.seekg(100000, std::ios::cur);
					break;
				}
			}
			count++;
		}
	}
	reachedDate = false;

	std::string word;
	int row = 0;
	bool quit = false;
	//Vector for every data type
	std::vector<float> pressure;
	std::vector<float> temperature;
	std::vector<float> dewPoint;
	std::vector<float> windDir;
	std::vector<float> windSpeed;
	std::vector<float> altitude;

	//Move getline until we find our exact position
	while (std::getline(s, line) && !quit)
	{
		std::stringstream ss(line);
		int count = 0;
		while (std::getline(ss, word, ';'))
		{
			if (!reachedDate)
			{
				if (count == 1) //date
				{
					if (word == targetDate)
					{
						reachedDate = true;
						count--;
					}
				}
				count++;
			}

			if (reachedDate)
			{
				switch (count)
				{
				case 1:
					if (word != targetDate) //Done with this date
					{
						reachedDate = false;
						quit = true;
					}
					break;
				case 6://Altitude
					altitude.push_back(std::stof(word));
					break;
				case 7://Pressure
					pressure.push_back(std::stof(word));
					break;
				case 8://Temperature in C
					temperature.push_back(std::stof(word));
					break;
				case 10://Dew point in C
					dewPoint.push_back(std::stof(word));
					break;
				case 11://WindSpeed
					windSpeed.push_back(std::stof(word));
					break;
				case 12://Wind Direction
					windDir.push_back(std::stof(word));
					break;
				default:
					break;
				}
				count++;
			}
		}
		if (reachedDate) row++;
	}

	//Copy the data over
	{
		skewTData.data.allocate(row);
		std::memcpy(skewTData.data.temperature, temperature.data(), row * sizeof(float));
		std::memcpy(skewTData.data.dewPoint, dewPoint.data(), row * sizeof(float));
		std::memcpy(skewTData.data.windDir, windDir.data(), row * sizeof(float));
		std::memcpy(skewTData.data.windSpeed, windSpeed.data(), row * sizeof(float));
		std::memcpy(skewTData.data.pressure, pressure.data(), row * sizeof(float));
		std::memcpy(skewTData.data.altitude, altitude.data(), row * sizeof(float));
	}

	heightToPressureCalculate();
}

static float lerpEnvValue(const float H1, const float H2, const float HC, const float V1, const float V2)
{
	if (HC == H1) return V1;
	else if (HC == H2) return V2;
	const float t = (HC - H1) / (H2 - H1);
	return V1 + t * (V2 - V1);
}

void readTable::initEnvironment()
{

	std::vector<int> indices;
	std::vector<float> potTempSmall;
	std::vector<double> potTemp;
	std::vector<glm::vec2> velField;
	std::vector<float> Qv;

	indices.resize(GRIDSIZESKYY);
	potTempSmall.resize(GRIDSIZESKYY);
	potTemp.resize(GRIDSIZESKY);
	velField.resize(GRIDSIZESKY);
	Qv.resize(GRIDSIZESKY);

	std::vector<double> groundTemp;
	std::vector<float>  groundPressure;
	std::vector<float>  pressures;

	groundTemp.resize(GRIDSIZEGROUND);
	groundPressure.resize(GRIDSIZEGROUND);
	pressures.resize(GRIDSIZESKYY);

	int j = 0;
	for (float y = 0; y < GRIDSIZESKYY * VOXELSIZE; y += VOXELSIZE)
	{
		const float H0 = skewTData.data.altitude[0];
		int i = getIndexAtHeight(y + H0);
		int Pi = i == 0 ? 0 : i - 1;
		pressures[j] = lerpEnvValue(skewTData.data.altitude[Pi] - H0, skewTData.data.altitude[i] - H0, y, skewTData.data.pressure[Pi], skewTData.data.pressure[i]);
		potTempSmall[j] = lerpEnvValue(skewTData.data.altitude[Pi] - H0, skewTData.data.altitude[i] - H0, y, skewTData.data.temperature[Pi], skewTData.data.temperature[i]);
		//potTempSmall[j] = skewTData.data.temperature[i];
		indices[j] = i;
		j++;
	}

	meteoformulas::getPotentialTempArray(potTempSmall.data(), skewTData.data.pressure[0], pressures.data(), potTempSmall.data(), GRIDSIZESKYY);
	//Duplicate across x direction
	for (int i = 0; i < GRIDSIZESKYY; i++)
	{
		for (int x = 0; x < GRIDSIZESKYX; x++)
		{
			potTemp[x + i * int(GRIDSIZESKYX)] = static_cast<double>(potTempSmall[i] + 273.15f);
		}
	}

	//Converting 2D into 1D: 90degrees = -1, 270 degrees = 1
	for (int i = 0; i < GRIDSIZESKYY; i++)
	{
		const float H0 = skewTData.data.altitude[0];
		const int idx = indices[i];
		int Pidx = idx == 0 ? 0 : idx - 1;

		float velFieldValue = std::sinf((skewTData.data.windDir[idx] - 180.0f) * (PI / 180.0f)) * skewTData.data.windSpeed[idx];
		float QvValue = meteoformulas::ws(lerpEnvValue(skewTData.data.altitude[Pidx] - H0, skewTData.data.altitude[idx] - H0, i * VOXELSIZE, skewTData.data.dewPoint[Pidx], skewTData.data.dewPoint[idx]), 
			pressures[idx]);

		for (int x = 0; x < GRIDSIZESKYX; x++)
		{
			velField[i * GRIDSIZESKYX + x] = { velFieldValue, 0 };
			Qv[i * GRIDSIZESKYX + x] = QvValue;
		}
	}

	//Convert velfield into MAC-grid - TODO: could be done in loop above if loop was counting down.
	for (int y = 0; y < GRIDSIZESKYY; y++)
	{
		for (int x = 0; x < GRIDSIZESKYX; x++)
		{
			const int Nx = x + 1 >= GRIDSIZESKYX ? x : x + 1;
			const int Ny = y + 1 >= GRIDSIZESKYY ? y : y + 1;

			velField[x + y * GRIDSIZESKYX] = { velField[x + y * GRIDSIZESKYX].x + velField[Nx + y * GRIDSIZESKYX].x / 2, (velField[x + y * GRIDSIZESKYX].y + velField[x + Ny * GRIDSIZESKYX].y) / 2 };
		}
	}


	for (int x = 0; x < GRIDSIZEGROUND; x++)
	{
		groundTemp[x] = static_cast<double>(skewTData.data.temperature[0] + 273.15f);
		groundPressure[x] = skewTData.data.pressure[0];
	}

	Game.Environment().init(potTemp.data(), velField.data(), Qv.data(), groundTemp.data(), groundPressure.data(), pressures.data());
}
 

//TODO: just remove?
void readTable::heightToPressureCalculate()
{
	for (int i = 0; i < int(skewTData.data.dataSize); i++)
	{
		skewTData.heightToPressure.emplace(skewTData.data.altitude[i], skewTData.data.pressure[i]);
		skewTData.pressureToHeight.emplace(skewTData.data.pressure[i], skewTData.data.altitude[i]);
	}
}

//TODO: just remove?
std::pair<float, int> readTable::getPressureAtHeight(float height)
{
	auto upper = skewTData.heightToPressure.lower_bound(height);

	if (upper == skewTData.heightToPressure.begin()) return { upper->second, 0 };
	else if (upper == skewTData.heightToPressure.end()) return { std::prev(upper)->second, int(skewTData.heightToPressure.size() - 1) };

	auto lower = std::prev(upper);

	int index = std::max(int(std::distance(skewTData.heightToPressure.begin(), lower)),0);

	//P = P1 + (P2 - P1) * (H - H1) / (H2 - H1)
	float P1 = lower->second, P2 = upper->second;
	float H1 = lower->first, H2 = upper->first;

	return { P1 + (P2 - P1) * (height - H1) / (H2 - H1), index };
}

int readTable::getIndexAtHeight(float height)
{
	auto upper = skewTData.heightToPressure.lower_bound(height);

	for (int i = 0; i < skewTData.data.dataSize; i++)
	{
		if (height <= skewTData.data.altitude[i])
		{
			return i;
		}
	}

	return -1;
}

float readTable::getHeightAtPressure(float pressure)
{
	auto upper = skewTData.pressureToHeight.lower_bound(pressure);

	if (upper == skewTData.pressureToHeight.begin()) return upper->second;
	else if (upper == skewTData.pressureToHeight.end()) return std::prev(upper)->second;

	auto lower = std::prev(upper);

	//P = P1 + (P2 - P1) * (H - H1) / (H2 - H1)
	float P1 = lower->second, P2 = upper->second;
	float H1 = lower->first, H2 = upper->first;

	return P1 + (P2 - P1) * (pressure - H1) / (H2 - H1);
}

glm::vec2 readTable::convertToPlottingCoordinates(const float temp, const float value, const bool pressure, const float scaleWidth, const float maxHeight)
{
	//Respect hPa for height in meter using standard pressure
	float height = value;
	if (!pressure) height = meteoformulas::getStandardPressureAtHeight(0, value); //TODO: use standard height/pressure?


	//---------------------Log()----------------------

	height = (log10f(height) - log10f(100)) / (log10f(1000) - log10f(100)) * maxHeight;
	height = maxHeight - height;

	//-------------------------------------------------


	//Skew value
	float skewedTemp = temp + tanTheta * height;
	skewedTemp *= scaleWidth;

	return { skewedTemp, height };
}




void readTable::debugDrawData()
{
	const float divV = plotHeight;
	const float pi = 3.14159265359f;
	const glm::vec2 hodoOffset(90, 100);

	//Making pressures array
	const int pressuresSize = 900;
	std::unique_ptr<float[]> pressures = std::make_unique<float[]>(pressuresSize);
	{
		int i = 0;
		for (float p = 1000.0f; p > 100; p -= 1.0f)
		{
			pressures[i] = p;
			i++;
		}
	}
	std::unique_ptr<float[]> potTemps = std::make_unique<float[]>(pressuresSize);
	std::unique_ptr<float[]> temps = std::make_unique<float[]>(skewTData.data.dataSize);



	for (float p = 0; p < 1000; p += 100) 
	{
		glm::vec2 coords = convertToPlottingCoordinates(0, p, true, sizeSkewT.x, sizeSkewT.y);

		bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(-40, coords.y, 0), glm::vec3(40, coords.y, 0), bee::Colors::Grey);
	}
	for (float i = -100; i < 40; i += 10)
	{

		glm::vec2 coords = convertToPlottingCoordinates(i, 30000, false, sizeSkewT.x, sizeSkewT.y);

		bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(i, 0, 0), glm::vec3(coords, 0), bee::Colors::Grey);
	}
	bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(-40, 0, 0), glm::vec3(40, 0, 0), bee::Colors::Black);
	bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(-40, 0, 0), glm::vec3(-40, 30000 * divV, 0), bee::Colors::Black);


	//Dry and moist adiabatic 
	for (float i = -40; i <= 40; i += 5)
	{
		//Dry adiabatic (potential temps)
		{
			meteoformulas::getDryAdiabatic(i, 1000.0f, pressures.get(), potTemps.get(), pressuresSize);

			for (int j = 10; j < pressuresSize; j += 10)
			{
				glm::vec2 coords = convertToPlottingCoordinates(potTemps[j], pressures[j], true, sizeSkewT.x, sizeSkewT.y);
				glm::vec2 coordsPrev = convertToPlottingCoordinates(potTemps[j - 10], pressures[j - 10], true, sizeSkewT.x, sizeSkewT.y);

				bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(coords, 0), glm::vec3(coordsPrev, 0), bee::Colors::Grey);
			}
		}

		//Moist adiabatic
		{
			int offset = 0;
			meteoformulas::getMoistTemp(i, 1000.0f, pressures.get(), potTemps.get(), pressuresSize, offset);
			if (offset == -1) continue;

			for (int j = 10 + offset; j < pressuresSize; j += 10)
			{
				glm::vec2 coords = convertToPlottingCoordinates(potTemps[j], pressures[j], true, sizeSkewT.x, sizeSkewT.y);
				glm::vec2 coordsPrev = convertToPlottingCoordinates(potTemps[j - 10], pressures[j - 10], true, sizeSkewT.x, sizeSkewT.y);

				bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(coords, 0), glm::vec3(coordsPrev, 0), bee::Colors::Grey);
			}
		}
	}


	//LCL
	glm::vec3 LCL = meteoformulas::getLCL(skewTData.data.temperature[0], skewTData.data.pressure[0], 0, skewTData.data.dewPoint[0]);
	glm::vec2 coords = convertToPlottingCoordinates(LCL.x, LCL.y, true, sizeSkewT.x, sizeSkewT.y);
	bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(35, coords.y, 0), glm::vec3(40, coords.y, 0), bee::Colors::Green);


	//CCL
	glm::vec3 CCL = meteoformulas::getCCL(skewTData.data.pressure[0], skewTData.data.dewPoint[0], skewTData.data.pressure, skewTData.data.temperature, skewTData.data.dataSize);
	coords = convertToPlottingCoordinates(CCL.x, CCL.y, true, sizeSkewT.x, sizeSkewT.y);
	bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(35, coords.y, 0), glm::vec3(40, coords.y, 0), bee::Colors::White);
	glm::vec2 coords2 = convertToPlottingCoordinates(skewTData.data.dewPoint[0], skewTData.data.pressure[0], true, sizeSkewT.x, sizeSkewT.y);
	bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(coords.x, coords.y, 0), glm::vec3(coords2.x, coords2.y, 0), bee::Colors::Grey);

	coords = convertToPlottingCoordinates(CCL.z, skewTData.data.pressure[0], true, sizeSkewT.x, sizeSkewT.y);
	bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(coords.x, 0, 0), glm::vec3(coords.x, 2, 0), bee::Colors::Red);


	//Dry adiabatic to LCL
	meteoformulas::getDryAdiabatic(skewTData.data.temperature[0], skewTData.data.pressure[0], skewTData.data.pressure, temps.get(), skewTData.data.dataSize);

	for (int j = 1; j < skewTData.data.dataSize; j++)
	{
		//TODO: should convertToPlottingCoordinates include setting default pressure height? (maybe an extra function that sets it)
		coords = convertToPlottingCoordinates(temps[j], skewTData.data.pressure[j], true, sizeSkewT.x, sizeSkewT.y);
		glm::vec2 coordsPrev = convertToPlottingCoordinates(temps[j - 1], skewTData.data.pressure[j - 1], true, sizeSkewT.x, sizeSkewT.y);

		bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(coords, 0), glm::vec3(coordsPrev, 0), bee::Colors::Black);
	}

	//Moist adiabatic at LCL
	int offset = 0;
	meteoformulas::getMoistTemp(LCL.x, LCL.y, skewTData.data.pressure, temps.get(), skewTData.data.dataSize, offset);
	if (offset != -1)
	{
		for (int j = 1 + offset; j < skewTData.data.dataSize; j++)
		{
			//TODO: should convertToPlottingCoordinates include setting default pressure height? (maybe an extra function that sets it)
			coords = convertToPlottingCoordinates(temps[j], skewTData.data.pressure[j], true, sizeSkewT.x, sizeSkewT.y);
			glm::vec2 coordsPrev = convertToPlottingCoordinates(temps[j - 1], skewTData.data.pressure[j - 1], true, sizeSkewT.x, sizeSkewT.y);

			bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(coords, 0), glm::vec3(coordsPrev, 0), bee::Colors::Black);
		}
	}

	//LFC
	const glm::vec3 LFC = meteoformulas::getLFC(skewTData.data.temperature[0], skewTData.data.pressure[0], skewTData.data.altitude[0], skewTData.data.dewPoint[0], skewTData.data.pressure, skewTData.data.temperature, skewTData.data.altitude, skewTData.data.dataSize);
	coords = convertToPlottingCoordinates(skewTData.data.temperature[0], LFC.y, true, sizeSkewT.x, sizeSkewT.y);
	bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(35, coords.y, 0), glm::vec3(40, coords.y, 0), bee::Colors::Yellow);

	//EL
	const glm::vec3 EL = meteoformulas::getEL(skewTData.data.temperature[0], skewTData.data.pressure[0], skewTData.data.altitude[0], skewTData.data.dewPoint[0], skewTData.data.pressure, skewTData.data.temperature, skewTData.data.altitude, skewTData.data.dataSize);
	coords = convertToPlottingCoordinates(skewTData.data.temperature[0], EL.y, true, sizeSkewT.x, sizeSkewT.y);
	bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(35, coords.y, 0), glm::vec3(40, coords.y, 0), bee::Colors::Pink);

	//CAPE
	CAPE = meteoformulas::calculateCAPE(skewTData.data.temperature[0], skewTData.data.pressure[0], 0, skewTData.data.dewPoint[0], skewTData.data.pressure, skewTData.data.temperature, skewTData.data.altitude, skewTData.data.dataSize);


	bee::Engine.DebugRenderer().AddCircle(bee::DebugCategory::All, glm::vec3(hodoOffset, 0), 0.1f, glm::vec3(0, 0, 1), bee::Colors::Black);
	bee::Engine.DebugRenderer().AddCircle(bee::DebugCategory::All, glm::vec3(hodoOffset, 0), 10, glm::vec3(0, 0, 1), bee::Colors::Black);
	bee::Engine.DebugRenderer().AddCircle(bee::DebugCategory::All, glm::vec3(hodoOffset, 0), 20, glm::vec3(0, 0, 1), bee::Colors::Black);
	bee::Engine.DebugRenderer().AddCircle(bee::DebugCategory::All, glm::vec3(hodoOffset, 0), 30, glm::vec3(0, 0, 1), bee::Colors::Black);
	bee::Engine.DebugRenderer().AddCircle(bee::DebugCategory::All, glm::vec3(hodoOffset, 0), 40, glm::vec3(0, 0, 1), bee::Colors::Black);

	glm::vec2 prevDir = { 0,0 };

	//Due to how skew-T's are drawn, the observed data (in this case) starts at hPa 1000 or the first line.
	//TODO: research this, what if station has higher offset?
	//const float heightAt0 = meteoformulas::getStandardHeightAtPressure(0, 1000, 1013.25f);
	//const float offset = skewTData.data.altitude[0] - heightAt0;

	for (int i = 1; i < skewTData.data.dataSize; i++)
	{
		//float newTemp = skewTData.data[i].temperature + tanTheta * skewTData.data[i].altitude * divV;
		//float newDew = skewTData.data[i].dewPoint + tanTheta * skewTData.data[i].altitude * divV;
		//
		//float newPrevTemp = skewTData.data[i - 1].temperature + tanTheta * skewTData.data[i - 1].altitude * divV;
		//float newPrevDew = skewTData.data[i - 1].dewPoint + tanTheta * skewTData.data[i - 1].altitude * divV;

		glm::vec2 tempCoords = convertToPlottingCoordinates(skewTData.data.temperature[i], skewTData.data.pressure[i], true, sizeSkewT.x, sizeSkewT.y);
		glm::vec2 tempPrevCoords = convertToPlottingCoordinates(skewTData.data.temperature[i - 1], skewTData.data.pressure[i - 1], true, sizeSkewT.x, sizeSkewT.y);

		glm::vec2 dewCoords = convertToPlottingCoordinates(skewTData.data.dewPoint[i], skewTData.data.pressure[i], true, sizeSkewT.x, sizeSkewT.y);
		glm::vec2 dewPrevCoords = convertToPlottingCoordinates(skewTData.data.dewPoint[i - 1], skewTData.data.pressure[i - 1], true, sizeSkewT.x, sizeSkewT.y);


		bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(tempCoords, 0.0f), glm::vec3(tempPrevCoords, 0.0f), bee::Colors::Red);
		bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(dewCoords, 0.0f), glm::vec3(dewPrevCoords, 0.0f), bee::Colors::Green);


		glm::vec4 color = bee::Colors::Black;

		if (skewTData.data.altitude[i] > 0) color = bee::Colors::Purple;
		if (skewTData.data.altitude[i] > 1000) color = bee::Colors::Red;
		if (skewTData.data.altitude[i] > 2000) color = bee::Colors::Orange;
		if (skewTData.data.altitude[i] > 6000) color = bee::Colors::Yellow;
		if (skewTData.data.altitude[i] > 9000) color = bee::Colors::Cyan;


		//Hodograph
		//Get wind dir
		float radians = skewTData.data.windDir[i] * (pi / 180.0f);
		glm::vec2 dir = { -glm::sin(radians), -glm::cos(radians) };
		dir *= skewTData.data.windSpeed[i];

		bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(dir + hodoOffset, 0.0f), glm::vec3(prevDir + hodoOffset, 0.0f), color);
		prevDir = dir;

	}
}



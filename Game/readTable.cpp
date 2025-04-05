#include "readTable.h"

#include "zip_file.hpp"
#include "constants.hpp"
using namespace Constants;

//Engine specific
#include "core/engine.hpp"
#include "rendering/debug_render.hpp"
using namespace bee;

#include "meteoformulas.h"

#include <sstream>
#include <iostream>

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
	//bool end = false;

	std::getline(s, line); //Skip first line
	skewTInfo STInfo;

	while (std::getline(s, line))
	{
		row++;
		std::stringstream ss(line);
		radioScondePart RSP;
		int count = 0;
		while (ss >> word)
		{

			switch (count)
			{
			case 4://pressure
				word = &word[10];
				word.pop_back();
				RSP.pressure = std::stof(word);
				break;
			case 5://Temp in K
				word = &word[13];
				word.pop_back();
				RSP.temperature = std::stof(word) - 273.15f;
				break;
			case 6://Dew in humidity
				//For increased speed but less accurasy, the part after the Multiplication could be removed 
				word = &word[10];
				word.pop_back();
				RSP.dewPoint = RSP.temperature - ((100 - std::stof(word)) * 0.2f) * ((RSP.temperature + 273.15f) * 0.01f);
				break;
			case 7://WindDir
				word = &word[9];
				word.pop_back();
				RSP.windDir = std::stof(word);
				break;
			case 8://WindSpeed
				word = &word[11];
				word.pop_back();
				RSP.windSpeed = std::stof(word);
				break;
			default:
				//Because values have been added, these parts will not be hard-coded
				if (word.substr(0, 8) == "Altitude")
				{
					word = &word[10];
					word.pop_back();
					RSP.altitude = std::stof(word);
					break;
				}
				if (word.substr(0, 8) == "Dropping")
				{
					word = &word[10];
					word.pop_back();
					//int dropping = std::stoi(word);
					if (false/*dropping*/) continue; //We discard when the balloon pops
					else //TODO: possibly discard reading further
					{
						STInfo.data.push_back(RSP);
					}
				}


				break;
			}

			count++;

		}
	}

	testInfo = STInfo;

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
	//targetDate = AllDates[161] + "12";

	targetDate = AllDates[243] + "12"; //1700 Cape?

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


	skewTInfo STInfo;
	bool quit = false;

	//Move getline until we find our exact position
	while (std::getline(s, line) && !quit)
	{
		std::string word;
		std::stringstream ss(line);
		radioScondePart RSP;
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
					RSP.altitude = std::stof(word);
					break;
				case 7://Pressure
					RSP.pressure = std::stof(word);
					break;
				case 8://Temperature in C
					RSP.temperature = std::stof(word);
					break;
				case 10://Dew point in C
					RSP.dewPoint = std::stof(word);
					break;
				case 11://WindSpeed  TODO: Not sure if km.h or knots
					RSP.windSpeed = std::stof(word);
					break;
				case 12://Wind Direction
					RSP.windDir = std::stof(word);
					STInfo.data.push_back(RSP);
					break;
				default:
					break;
				}
				count++;
			}

		}
	}

	testInfo = STInfo;

	heightToPressureCalculate();


}



void readTable::heightToPressureCalculate()
{
	for (int i = 0; i < int(testInfo.data.size()); i++)
	{
		testInfo.heightToPressure.emplace(testInfo.data[i].altitude, testInfo.data[i].pressure);
		testInfo.pressureToHeight.emplace(testInfo.data[i].pressure, testInfo.data[i].altitude);
	}
}

float readTable::getPressureAtHeight(float height)
{
	auto upper = testInfo.heightToPressure.lower_bound(height);

	if (upper == testInfo.heightToPressure.begin()) return upper->second;
	else if (upper == testInfo.heightToPressure.end()) return std::prev(upper)->second;

	auto lower = std::prev(upper);

	//P = P1 + (P2 - P1) * (H - H1) / (H2 - H1)
	float P1 = lower->second, P2 = upper->second;
	float H1 = lower->first, H2 = upper->first;

	return P1 + (P2 - P1) * (height - H1) / (H2 - H1);
}

float readTable::getHeightAtPressure(float pressure)
{
	auto upper = testInfo.pressureToHeight.lower_bound(pressure);

	if (upper == testInfo.pressureToHeight.begin()) return upper->second;
	else if (upper == testInfo.pressureToHeight.end()) return std::prev(upper)->second;

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


	for (float h = 0; h < 1000; h += 100) 
	{
		glm::vec2 coords = convertToPlottingCoordinates(0, h, true, sizeSkewT.x, sizeSkewT.y);

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
	std::vector<float> pressures = {};
	for (float i = -40; i <= 40; i += 5)
	{
		pressures.clear();
		for (float p = 1000.0f; p > 100; p -= 1.0f)
		{
			//const float Pressure = meteoformulas::getStandardPressureAtHeight(i, y, 0 , 1013.25f);
			float i2 = 0;
			if (useI) i2 = i;
			pressures.push_back(p);
		}
		

		//Dry adiabatic (potential temps)
		{
			std::vector<float> potTemps = meteoformulas::getPotentialTemp(i, 1000.0f, pressures);

			for (int j = 10; j < int(potTemps.size()); j += 10)
			{
				glm::vec2 coords = convertToPlottingCoordinates(potTemps[j], pressures[j], true, sizeSkewT.x, sizeSkewT.y);
				glm::vec2 coordsPrev = convertToPlottingCoordinates(potTemps[j - 10], pressures[j - 10], true, sizeSkewT.x, sizeSkewT.y);

				bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(coords, 0), glm::vec3(coordsPrev, 0), bee::Colors::Grey);
			}
		}

		//Moist adiabatic
		{
			std::vector<float> temperatures = meteoformulas::getMoistTemp(i, 1000.0f, pressures);

			for (int j = 10; j < int(temperatures.size()); j += 10)
			{
				glm::vec2 coords = convertToPlottingCoordinates(temperatures[j], pressures[j], true, sizeSkewT.x, sizeSkewT.y);
				glm::vec2 coordsPrev = convertToPlottingCoordinates(temperatures[j - 10], pressures[j - 10], true, sizeSkewT.x, sizeSkewT.y);

				bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(coords, 0), glm::vec3(coordsPrev, 0), bee::Colors::Grey);
			}
		}
	}


	//LCL
	glm::vec3 LCL = meteoformulas::getLCL(testInfo.data[0].temperature, testInfo.data[0].pressure, 0, testInfo.data[0].dewPoint);
	glm::vec2 coords = convertToPlottingCoordinates(LCL.x, LCL.y, true, sizeSkewT.x, sizeSkewT.y);
	bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(35, coords.y, 0), glm::vec3(40, coords.y, 0), bee::Colors::Green);


	std::vector<float> OPressures;
	for (float i = testInfo.data[0].pressure; i > LCL.y; i--) OPressures.push_back(i);
	std::vector<float> OPressures2{};
	for (float i = LCL.y; i > 100; i--) OPressures2.push_back(i);

	//CCL
	glm::vec3 CCL = meteoformulas::getCCL(testInfo.data[0].pressure, testInfo.data[0].dewPoint, testInfo);
	coords = convertToPlottingCoordinates(CCL.x, CCL.y, true, sizeSkewT.x, sizeSkewT.y);
	bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(35, coords.y, 0), glm::vec3(40, coords.y, 0), bee::Colors::White);
	glm::vec2 coords2 = convertToPlottingCoordinates(testInfo.data[0].dewPoint, testInfo.data[0].pressure, true, sizeSkewT.x, sizeSkewT.y);
	bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(coords.x, coords.y, 0), glm::vec3(coords2.x, coords2.y, 0), bee::Colors::Grey);

	coords = convertToPlottingCoordinates(CCL.z, testInfo.data[0].pressure, true, sizeSkewT.x, sizeSkewT.y);
	bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(coords.x, 0, 0), glm::vec3(coords.x, 2, 0), bee::Colors::Red);


	//Dry adiabatic to LCL
	OPressures.push_back(LCL.y);

	std::vector<float> temperatures = meteoformulas::getPotentialTemp(testInfo.data[0].temperature, testInfo.data[0].pressure, OPressures);

	for (int j = 1; j < int(temperatures.size()); j++)
	{
		//TODO: should convertToPlottingCoordinates include setting default pressure height? (maybe an extra function that sets it)
		coords = convertToPlottingCoordinates(temperatures[j], OPressures[j], true, sizeSkewT.x, sizeSkewT.y);
		glm::vec2 coordsPrev = convertToPlottingCoordinates(temperatures[j - 1], OPressures[j - 1], true, sizeSkewT.x, sizeSkewT.y);

		bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(coords, 0), glm::vec3(coordsPrev, 0), bee::Colors::Black);
	}

	//Moist adiabatic at LCL
	
	temperatures.clear();
	temperatures = meteoformulas::getMoistTemp(LCL.x, LCL.y, OPressures2);
	
	for (int j = 1; j < int(temperatures.size()); j++)
	{
		//TODO: should convertToPlottingCoordinates include setting default pressure height? (maybe an extra function that sets it)
		coords = convertToPlottingCoordinates(temperatures[j], OPressures2[j], true, sizeSkewT.x, sizeSkewT.y);
		glm::vec2 coordsPrev = convertToPlottingCoordinates(temperatures[j - 1], OPressures2[j - 1], true, sizeSkewT.x, sizeSkewT.y);

		bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(coords, 0), glm::vec3(coordsPrev, 0), bee::Colors::Black);
	}

	//LFC
	const glm::vec3 LFC = meteoformulas::getLFC(testInfo.data[0].temperature, testInfo.data[0].pressure, testInfo.data[0].altitude, testInfo.data[0].dewPoint, testInfo);
	coords = convertToPlottingCoordinates(testInfo.data[0].temperature, LFC.y, true, sizeSkewT.x, sizeSkewT.y);
	bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(35, coords.y, 0), glm::vec3(40, coords.y, 0), bee::Colors::Yellow);

	//EL
	const glm::vec3 EL = meteoformulas::getEL(testInfo.data[0].temperature, testInfo.data[0].pressure, testInfo.data[0].altitude, testInfo.data[0].dewPoint, testInfo);
	coords = convertToPlottingCoordinates(testInfo.data[0].temperature, EL.y, true, sizeSkewT.x, sizeSkewT.y);
	bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(35, coords.y, 0), glm::vec3(40, coords.y, 0), bee::Colors::Pink);

	//CAPE
	CAPE = meteoformulas::calculateCAPE(testInfo.data[0].temperature, testInfo.data[0].pressure, 0, testInfo.data[0].dewPoint, testInfo);


	bee::Engine.DebugRenderer().AddCircle(bee::DebugCategory::All, glm::vec3(hodoOffset, 0), 0.1f, glm::vec3(0, 0, 1), bee::Colors::Black);
	bee::Engine.DebugRenderer().AddCircle(bee::DebugCategory::All, glm::vec3(hodoOffset, 0), 10, glm::vec3(0, 0, 1), bee::Colors::Black);
	bee::Engine.DebugRenderer().AddCircle(bee::DebugCategory::All, glm::vec3(hodoOffset, 0), 20, glm::vec3(0, 0, 1), bee::Colors::Black);
	bee::Engine.DebugRenderer().AddCircle(bee::DebugCategory::All, glm::vec3(hodoOffset, 0), 30, glm::vec3(0, 0, 1), bee::Colors::Black);
	bee::Engine.DebugRenderer().AddCircle(bee::DebugCategory::All, glm::vec3(hodoOffset, 0), 40, glm::vec3(0, 0, 1), bee::Colors::Black);

	glm::vec2 prevDir = { 0,0 };

	//Due to how skew-T's are drawn, the observed data (in this case) starts at hPa 1000 or the first line.
	//TODO: research this, what if station has higher offset?
	//const float heightAt0 = meteoformulas::getStandardHeightAtPressure(0, 1000, 1013.25f);
	//const float offset = testInfo.data[0].altitude - heightAt0;

	for (int i = 1; i < int(testInfo.data.size()); i++)
	{
		//float newTemp = testInfo.data[i].temperature + tanTheta * testInfo.data[i].altitude * divV;
		//float newDew = testInfo.data[i].dewPoint + tanTheta * testInfo.data[i].altitude * divV;
		//
		//float newPrevTemp = testInfo.data[i - 1].temperature + tanTheta * testInfo.data[i - 1].altitude * divV;
		//float newPrevDew = testInfo.data[i - 1].dewPoint + tanTheta * testInfo.data[i - 1].altitude * divV;

		glm::vec2 tempCoords = convertToPlottingCoordinates(testInfo.data[i].temperature, testInfo.data[i].pressure, true, sizeSkewT.x, sizeSkewT.y);
		glm::vec2 tempPrevCoords = convertToPlottingCoordinates(testInfo.data[i - 1].temperature, testInfo.data[i - 1].pressure, true, sizeSkewT.x, sizeSkewT.y);

		glm::vec2 dewCoords = convertToPlottingCoordinates(testInfo.data[i].dewPoint, testInfo.data[i].pressure, true, sizeSkewT.x, sizeSkewT.y);
		glm::vec2 dewPrevCoords = convertToPlottingCoordinates(testInfo.data[i-1].dewPoint, testInfo.data[i - 1].pressure, true, sizeSkewT.x, sizeSkewT.y);


		bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(tempCoords, 0.0f), glm::vec3(tempPrevCoords, 0.0f), bee::Colors::Red);
		bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(dewCoords, 0.0f), glm::vec3(dewPrevCoords, 0.0f), bee::Colors::Green);


		glm::vec4 color = bee::Colors::Black;

		if (testInfo.data[i].altitude > 0) color = bee::Colors::Purple;
		if (testInfo.data[i].altitude > 1000) color = bee::Colors::Red;
		if (testInfo.data[i].altitude > 2000) color = bee::Colors::Orange;
		if (testInfo.data[i].altitude > 6000) color = bee::Colors::Yellow;
		if (testInfo.data[i].altitude > 9000) color = bee::Colors::Cyan;


		//Hodograph
		//Get wind dir
		float radians = testInfo.data[i].windDir * (pi / 180.0f);
		glm::vec2 dir = { -glm::sin(radians), -glm::cos(radians) };
		dir *= testInfo.data[i].windSpeed;

		bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(dir + hodoOffset, 0.0f), glm::vec3(prevDir + hodoOffset, 0.0f), color);
		prevDir = dir;

	}

}



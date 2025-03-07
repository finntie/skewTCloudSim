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
	targetDate = AllDates[2] + "12";
	
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

glm::vec2 readTable::convertToPlottingCoordinates(float x, float y)
{
	



}




void readTable::debugDrawData()
{
	const float divV = plotHeight;
	const float pi = 3.14159265359f;
	const glm::vec2 hodoOffset(90, 100);
	const float tanTheta = glm::tan(glm::radians(45.0f));


	for (float h = 0; h < 1000; h += 100) 
	{
		float newH = meteoformulas::getStandardHeightAtPressure(0, h, 1000) * divV;
		//float newH = convertToPlottingCoordinates(0, h).y * 50;
		bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(-40, newH, 0), glm::vec3(40, newH, 0), bee::Colors::Grey);
	}
	for (float i = -100; i < 40; i += 10)
	{
		float skewedi = i + tanTheta * 30000 * divV;

		bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(i, 0, 0), glm::vec3(skewedi, 30000 * divV, 0), bee::Colors::Grey);
	}
	bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(-40, 0, 0), glm::vec3(40, 0, 0), bee::Colors::Black);
	bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(-40, 0, 0), glm::vec3(-40, 30000 * divV, 0), bee::Colors::Black);


	//Debuggin MLR
	std::vector<float> press = { 925, 850, 700,500, 300,200 };
	meteoformulas::getMoistTemp(5, 925, press);



	//Dry and moist adiabatic 
	std::vector<float> pressures = {};
	for (float i = -40; i <= 40; i += 5)
	{
		std::vector<float> heights = {};
		pressures.clear();
		for (float p = 1000.0f; p > 100; p -= 25.0f)
		{
			//const float Pressure = meteoformulas::getStandardPressureAtHeight(i, y, 0 , 1013.25f);
			float i2 = 0;
			if (useI) i2 = i;
			pressures.push_back(p);
			heights.push_back(meteoformulas::getStandardHeightAtPressure(i2, p, 1013.25f));
		}
		

		//Dry adiabatic (potential temps)
		{
			std::vector<float> potTemps = meteoformulas::getPotentialTemp(i, 1000.0f, pressures);

			for (int j = 1; j < int(potTemps.size()); j++)
			{
				float skewedValue = potTemps[j] + tanTheta * heights[j] * divV;
				float prevSkewedValue = potTemps[j - 1] + tanTheta * heights[j - 1] * divV;

				bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(skewedValue, heights[j] * divV, 0), glm::vec3(prevSkewedValue, heights[j - 1] * divV, 0), bee::Colors::Grey);
			}
		}

		//Moist adiabatic
		{
			std::vector<float> temperatures = meteoformulas::getMoistTemp(i, 1000.0f, pressures);

			for (int j = 1; j < int(temperatures.size()); j++)
			{
				float skewedValue = temperatures[j] + tanTheta * heights[j] * divV;
				float prevSkewedValue = temperatures[j - 1] + tanTheta * heights[j - 1] * divV;

				bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(skewedValue, heights[j] * divV, 0), glm::vec3(prevSkewedValue, heights[j - 1] * divV, 0), bee::Colors::Red);
			}
		}
	}


	//LCL
	glm::vec3 LCL = meteoformulas::getLCL(testInfo.data[0].temperature, testInfo.data[0].pressure, testInfo.data[0].altitude, testInfo.data[0].dewPoint);
	bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(testInfo.data[0].temperature + 10, LCL.z * divV, 0), glm::vec3(testInfo.data[0].temperature + 15, LCL.z * divV, 0), bee::Colors::Green);


	//Moist adiabatic at LCL

	std::vector<float> OPressures;
	for (float i = LCL.y; i > 25; i -= 25) OPressures.push_back(i);
	
	std::vector<float> temperatures = meteoformulas::getMoistTemp(LCL.x, LCL.y, OPressures);
	
	for (int j = 1; j < int(temperatures.size()); j++)
	{
		const float heightNew = meteoformulas::getStandardHeightAtPressure(LCL.x, OPressures[j], 1013.25f);
		const float heightPrev = meteoformulas::getStandardHeightAtPressure(LCL.x, OPressures[j - 1], 1013.25f);

		float skewedValue = temperatures[j] + tanTheta * heightNew * divV;
		float prevSkewedValue = temperatures[j - 1] + tanTheta * heightPrev * divV;
		bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(skewedValue, heightNew * divV, 0), glm::vec3(prevSkewedValue, heightPrev * divV, 0), bee::Colors::Black);
	}


	//EL
	float EL = meteoformulas::getEL(testInfo.data[0].temperature, testInfo.data[0].pressure, testInfo.data[0].altitude, testInfo.data[0].dewPoint, testInfo);
	bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(testInfo.data[0].temperature + 10, EL * divV, 0), glm::vec3(testInfo.data[0].temperature + 15, EL * divV, 0), bee::Colors::Pink);



	bee::Engine.DebugRenderer().AddCircle(bee::DebugCategory::All, glm::vec3(hodoOffset, 0), 0.1f, glm::vec3(0, 0, 1), bee::Colors::Black);
	bee::Engine.DebugRenderer().AddCircle(bee::DebugCategory::All, glm::vec3(hodoOffset, 0), 10, glm::vec3(0, 0, 1), bee::Colors::Black);
	bee::Engine.DebugRenderer().AddCircle(bee::DebugCategory::All, glm::vec3(hodoOffset, 0), 20, glm::vec3(0, 0, 1), bee::Colors::Black);
	bee::Engine.DebugRenderer().AddCircle(bee::DebugCategory::All, glm::vec3(hodoOffset, 0), 30, glm::vec3(0, 0, 1), bee::Colors::Black);
	bee::Engine.DebugRenderer().AddCircle(bee::DebugCategory::All, glm::vec3(hodoOffset, 0), 40, glm::vec3(0, 0, 1), bee::Colors::Black);

	glm::vec2 prevDir = { 0,0 };

	//Due to how skew-T's are drawn, the observed data (in this case) starts at hPa 1000 or the first line.
	//TODO: research this, what if station has higher offset?
	const float heightAt0 = meteoformulas::getStandardHeightAtPressure(0, 1000, 1013.25f);
	const float offset = testInfo.data[0].altitude - heightAt0;

	for (int i = 1; i < int(testInfo.data.size()); i++)
	{
		float newTemp = testInfo.data[i].temperature + tanTheta * testInfo.data[i].altitude * divV;
		float newDew = testInfo.data[i].dewPoint + tanTheta * testInfo.data[i].altitude * divV;

		float newPrevTemp = testInfo.data[i - 1].temperature + tanTheta * testInfo.data[i - 1].altitude * divV;
		float newPrevDew = testInfo.data[i - 1].dewPoint + tanTheta * testInfo.data[i - 1].altitude * divV;

		bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(newTemp, (testInfo.data[i].altitude - offset) * divV, 0.0f), glm::vec3(newPrevTemp, (testInfo.data[i - 1].altitude - offset) * divV, 0.0f), bee::Colors::Red);
		bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(newDew,  (testInfo.data[i].altitude - offset) * divV, 0.0f), glm::vec3(newPrevDew, (testInfo.data[i - 1].altitude - offset) * divV, 0.0f), bee::Colors::Green);


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



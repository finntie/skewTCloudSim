#include "readTable.h"

#include "zip_file.hpp"
#include "constants.hpp"
using namespace Constants;

//Engine specific
#include "core/engine.hpp"
#include "rendering/debug_render.hpp"
using namespace bee;

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
					int dropping = std::stoi(word);
					if (dropping) continue; //We discard when the balloon pops
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
					break;
				}
				count++;
			}
		}

	}

	//TODO: Could do something with the dates, i.e. select a date, but for now we will grab the latest.
	targetDate = AllDates[AllDates.size() - 1] + "00";
	
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

	//Move getline until we find our exact position
	while (std::getline(s, line) && !reachedDate)
	{
		std::string word;
		std::stringstream ss(line);
		int count = 0;
		while (std::getline(ss, word, ';'))
		{
			if (count == 1) //date
			{
				if (word == targetDate)
				{
					reachedDate = true;
					break;
				}
			}
			count++;
		}
	}


	skewTInfo STInfo;
	std::string word;
	int row = 0;

	while (std::getline(s, line) && reachedDate)
	{
		row++;
		std::stringstream ss(line);
		radioScondePart RSP;
		int count = 0;
		while (std::getline(ss, word, ';') && reachedDate)
		{

			switch (count)
			{
			case 1:
				if (word != targetDate) //Done with this date
				{
					reachedDate = false;
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

float readTable::se(const float T)
{
	return 6.112f * float(exp( (17.67f * T) / (T + 243.5f)));
}

float readTable::RS(const float T, const float P)
{
	float es = se(T);
	return (E * es) / (P - es);
}

float readTable::MLR(const float T, const float P)
{
	float TCelsius = T - 273.15f;
	float ws = RS(TCelsius, P);

	float Tw = (Rsd * T + Hv * ws) / (Cpd + (Hv * Hv * ws * E) / (Rsd * T * T));
	return Tw / P;

	//return ((Rsd * T + Hv * ws) / (Cpd + (Hv * Hv * ws * E) / (Rsd * T * T))) * (1 / P);
}


std::vector<float> readTable::getMoistTemp(float T0, float Pref, const std::vector<float>& pressures)
{
	std::vector<float> temps;

	float T = T0;
	float P = Pref;
	float h = 1.0f;

	for (float Pnext : pressures)
	{
		float dP = Pnext - P;
		// Runge-Kutta 4th Order Method to solve the ODE
		float k1 = h * MLR(T, P);
		float k2 = h * MLR(T + 0.5f * k1, P + 0.5f * dP * h);
		float k3 = h * MLR(T + 0.5f * k2, P + 0.5f * dP * h);
		float k4 = h * MLR(T + k3, P + dP + h);
		//T = T + (1.0f / 6.0f) * (k1 + 2 * k2 + 2 * k3 + k4);

		//float dT = MLR(T, P);
		T = (1.0f / 6.0f) * (k1 + 2 * k2 + 2 * k3 + k4) * dP +T;
		temps.push_back(T - 273.15f);
		P = Pnext;
	}

	return temps;
}

glm::vec2 readTable::convertToPlottingCoordinates(float x, float y)
{

	glm::vec2 output{};
	output.y = log(1000 / y);
	output.x = x + angle * output.y;
	return output;


	//const float Xmax = 80.0f;
	////const float Xmin = -40.0f;

	//const float Tmax = 40.0f;
	//const float Tmin = -40.0f;

	////Formula from https://www.researchgate.net/publication/303686619_Realistic_Simulation_and_Animation_of_Clouds_using_SkewTLogP_Diagrams
	//glm::vec2 output{};

	//output.y = Ymax - (Ymax * (log10(y)-log10(Pmin))) / (log10(Pmax) - log10(Pmin));
	//output.x = ((Xmax * (x - Tmin)) / (Tmax - Tmin)) + output.y * 0.5f;
	//output.x -= Tmax;
	//return output;
}




void readTable::debugDrawData()
{
	const float divV = 0.005f;
	const float pi = 3.14159265359f;
	const glm::vec2 hodoOffset(70, 50);
	const float tanTheta = glm::tan(glm::radians(45.0f));


	for (float h = 0; h < 1000; h += 100) 
	{
		float newH = convertToPlottingCoordinates(0, h).y * 50;
		bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(-40, newH, 0), glm::vec3(40, newH, 0), bee::Colors::Grey);
	}
	for (float i = -100; i < 40; i += 10)
	{
		float skewedi = i + tanTheta * 30000 * divV;

		bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(i, 0, 0), glm::vec3(skewedi, 30000 * divV, 0), bee::Colors::Grey);
	}
	bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(-40, 0, 0), glm::vec3(40, 0, 0), bee::Colors::Black);
	bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(-40, 0, 0), glm::vec3(-40, 30000 * divV, 0), bee::Colors::Black);


	//Dry and moist adiabatic 
	float prevTw = -9999.0f;
	for (float i = -40; i <= 40; i += 5)
	{
		for (float y = testInfo.data[0].altitude + 300; y < 30000; y += 300)
		{
			float value = (i + 273.15f) * glm::pow((getPressureAtHeight(y) / 1000), Rsd /Cpd);
			float prevValue = (i + 273.15f) * glm::pow((getPressureAtHeight(y - 300) / 1000), Rsd / Cpd);

			value -= 273.15f;
			prevValue -= 273.15f;
			value = value + tanTheta * y * divV;
			prevValue = prevValue + tanTheta * (y - 300) * divV;


			bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(value, y * divV, 0), glm::vec3(prevValue, (y - 300) * divV, 0), bee::Colors::Grey);
		}
		for (float y = 300; y < 30000; y += 30000)
		{
			float iKelvin = i + 273.15f;

			//Vapor pressure https://www.weather.gov/media/epz/wxcalc/vaporPressure.pdf
			float es = 6.11f * float(glm::pow(10, (7.5f * i) / (237.3f + i)));
			es *= 100; //hPa to Pa			


			//float rs = (es * E) / (getPressureAtHeight(y) - es);

			//float rs = getPressureAtHeight(0) * exp((17.67f * i) / (i - 29.65f));

			float ws = (E * es) / (getPressureAtHeight(y) * 100 - es);
			//float ws = E * (es / (getPressureAtHeight(y) * 100));


			//https://badpets.net/Documents/Atmos_Thermodynamics.pdf
			//float a0 = 23.832241f - 5.02808f * log10(iKelvin);
			//float a1 = 0.00000013816f * (float)pow(10, 11.344f - 0.0303998f * iKelvin);
			//float a2 = 0.0081328f * (float)pow(10, 3.49149f - 1302.8844f / iKelvin);
			//
			//float ESAT = float(glm::pow(10, a0 - a1 + a2 - 2949.076f / iKelvin));
			//float W = 622 * ESAT / (getPressureAtHeight(y) * 100 - ESAT);
			//
			//float OS = iKelvin * pow(1000 / (getPressureAtHeight(y) * 100), 0.288f) / exp(-2.6518986f * W / iKelvin);
			//
			//float TSA = OS * exp(-2.6518986f * W / iKelvin) - iKelvin * glm::pow(1000 / (getPressureAtHeight(y) * 100), 0.288f);



			//float DV = (i + 273.15f) * glm::pow((getPressureAtHeight(y) / 1000), Rsd / Cpd);
			//float DVPrev = (i + 273.15f) * glm::pow((getPressureAtHeight(y - 300) / 1000), Rsd / Cpd);

			//float r = E * e * (getPressureAtHeight(y) * e);

			//float Tw = g * ((1.0f + (Hv * r) / (Rsd * iKelvin)) / 
			//		   (Cpd + ((Hv * Hv) * r) / (Rsw * (iKelvin * iKelvin))));

			//Tw *= 1000; //Convert from K/m to C/km

			//float LvT = (-6.14342e-5f * float(glm::pow(iKelvin, 3))
			//		+ ((1.58927e-3f) * float(glm::pow(iKelvin, 2)))
			//		+ (-2.36418f * iKelvin) 
			//		+ 2500.79f) 
			//		* 1000;

			//float Tw = (1 / getPressureAtHeight(y)) * ((Rsd * iKelvin + Hv * rs) / (Cpd + ((Hv * Hv) * rs * E) / (Rsd * (iKelvin * iKelvin))));
			//Tw *= 100;
			float Tw = (Rsd * iKelvin + Hv * ws) / (Cpd + (Hv * Hv * ws * E) / (Rsd * iKelvin * iKelvin));
			Tw = Tw / getPressureAtHeight(y);
			Tw *= 100;
			//Tw = Tw * (getPressureAtHeight(y) - 1000);

			//float Tw = glm::pow((getPressureAtHeight(y) / 1000), Rsd / Cpd) * ((1 + (Hv * ws) / (Rsd * iKelvin)) / (1 + (E * (Hv * Hv) * ws) / (Cpd * Rsd * (iKelvin * iKelvin))));

			//float Tw = (Constants::g / Constants::Cpd) *
			//	((1 + (Constants::Hv / Constants::Rsd) * (ws / iKelvin))
			//	/ (1 + ((Constants::E * (Constants::Hv * Constants::Hv)) / (Constants::Cpd * Constants::Rsd)) * (ws / (iKelvin * iKelvin))));
			//Tw *= 1000;


			//float Tw = g * ((1 + ws) * ((1 + (Hv * ws) / Rsd * iKelvin)) / (Cpd + ws * Cpv + (Hv * ws * (E * ws)) / (Rsd * iKelvin * iKelvin)));
			//float value = iKelvin + (Tw / (p * g)) * (getPressureAtHeight(y) - 1000);
			//float prevValue = iKelvin + (prevTw / (pPrev * g)) * (getPressureAtHeight(y - 300) - 1000);
			//value -= 273.15f;
			//prevValue -= 273.15f;

			if (prevTw == -9999.0f) prevTw = 9.54f;


			std::vector<float> pressures = { /*925, 850, 700, 500, 300, 200, 100, 99, 88*/ }; // Target pressure levels
			for (float p = 1000; p > 100; p -= 25)
			{
				pressures.push_back(p);
			}



			std::vector<float> temperatures = getMoistTemp(iKelvin, getPressureAtHeight(0), pressures);
			temperatures;

			for (int j = 1; j < int(temperatures.size()); j++)
			{
				float skewedValue = temperatures[j] + tanTheta * (getHeightAtPressure(pressures[j])) * divV;
				float prevSkewedValue = temperatures[j - 1] + tanTheta * (getHeightAtPressure(pressures[j - 1])) * divV;
				//float newH = convertToPlottingCoordinates(0, pressures[j]).y * 50;
				//float newHPrev = convertToPlottingCoordinates(0, pressures[j - 1]).y * 50;

				bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(skewedValue, getHeightAtPressure(pressures[j])* divV, 0), glm::vec3(prevSkewedValue, getHeightAtPressure(pressures[j - 1])* divV, 0), bee::Colors::Red);
			}

			//float value = iKelvin * Tw;
			//float prevValue = iKelvin * prevTw;
			//value -= 273.15f;
			//prevValue -= 273.15f;



			//float value = iKelvin - Tw * y * 0.001f;
			//float prevValue = iKelvin - prevTw * (y - 300) * 0.001f;
			//value -= 273.15f;
			//prevValue -= 273.15f;
			//float skewedValue = value + tanTheta * y * divV;
			//float prevSkewedValue = prevValue + tanTheta * (y - 300) * divV;

			//bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(skewedValue, y * divV, 0), glm::vec3(prevSkewedValue, (y - 300) * divV, 0), bee::Colors::Red);
			//prevTw = Tw;
		}


	}


	bee::Engine.DebugRenderer().AddCircle(bee::DebugCategory::All, glm::vec3(hodoOffset, 0), 0.1f, glm::vec3(0, 0, 1), bee::Colors::Black);
	bee::Engine.DebugRenderer().AddCircle(bee::DebugCategory::All, glm::vec3(hodoOffset, 0), 10, glm::vec3(0, 0, 1), bee::Colors::Black);
	bee::Engine.DebugRenderer().AddCircle(bee::DebugCategory::All, glm::vec3(hodoOffset, 0), 20, glm::vec3(0, 0, 1), bee::Colors::Black);
	bee::Engine.DebugRenderer().AddCircle(bee::DebugCategory::All, glm::vec3(hodoOffset, 0), 30, glm::vec3(0, 0, 1), bee::Colors::Black);
	bee::Engine.DebugRenderer().AddCircle(bee::DebugCategory::All, glm::vec3(hodoOffset, 0), 40, glm::vec3(0, 0, 1), bee::Colors::Black);

	glm::vec2 prevDir = { 0,0 };

	for (int i = 1; i < int(testInfo.data.size()); i++)
	{
		float newTemp = testInfo.data[i].temperature + tanTheta * testInfo.data[i].altitude * divV;
		float newDew = testInfo.data[i].dewPoint + tanTheta * testInfo.data[i].altitude * divV;

		float newPrevTemp = testInfo.data[i - 1].temperature + tanTheta * testInfo.data[i - 1].altitude * divV;
		float newPrevDew = testInfo.data[i - 1].dewPoint + tanTheta * testInfo.data[i - 1].altitude * divV;

		bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(newTemp, testInfo.data[i].altitude * divV, 0.0f), glm::vec3(newPrevTemp, testInfo.data[i - 1].altitude * divV, 0.0f), bee::Colors::Red);
		bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(newDew, testInfo.data[i].altitude * divV, 0.0f), glm::vec3(newPrevDew, testInfo.data[i - 1].altitude * divV, 0.0f), bee::Colors::Green);


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



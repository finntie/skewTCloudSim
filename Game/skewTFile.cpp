#include "pch.h"

#include "skewTFile.h"

#include "zip_file.hpp"
#include "game.h"
#include "readTable.h"
#include "skewTMaker.h"

#include "math/constants.hpp"

using namespace std::filesystem;

void skewTFile::init()
{
	// Goes through all files and saves them to the appropriate variables
	checkKNMIFiles();
	checkDWDFiles();

	// Now make unique dates actually unique
	std::sort(m_uniqueDates.begin(), m_uniqueDates.end()); // sort
	// Move all doubles to end of vector and get iterator to new end
	auto itToRemove = std::unique(m_uniqueDates.begin(), m_uniqueDates.end());
	// Remove all duplicates
	m_uniqueDates.erase(itToRemove, m_uniqueDates.end());
}


void skewTFile::checkKNMIFiles()
{
	path KNMIPath = "assets/input/KNMI";
	if (!exists(KNMIPath))
	{
		// No KNMI files exists, we do create already a file though
		create_directories(KNMIPath);
		return;
	}

	// Go through all files
	for (const auto& entry : directory_iterator(KNMIPath))
	{
		fileInfo file;

		// Extract the date from the file name
		file.fileName = entry.path().filename().string();
		bool afterDigit = false;
		std::string date{};
		for (int i = 0; i < int(file.fileName.size()); i++)
		{
			char CChar = file.fileName[i];
			if (afterDigit)
			{
				m_uniqueDates.push_back(std::stoi(date));
				int num = (file.fileName[i] - '0') * 10 + (file.fileName[i + 1] - '0');
				// Round to 0, 3, 6, etc
 				num = ((num + 1) / 3) * 3; // This works since int math truncs. 
				num = num == 24 ? 0 : num; // Make sure that 24 is 0
				date += "_";
				date += num < 10 ? ("0" + std::to_string(num)) : std::to_string(num);
				break;
			}

			// Push back digits of the date and check if we have added all of them
			if (!afterDigit && std::isdigit(CChar)) date.push_back(CChar);
			else if (CChar == '_' && !date.empty()) afterDigit = true;
		}
		file.dates.push_back(date);
		file.country = "The Netherlands";
		file.institute = "KNMI";
		file.pathToFile = entry.path().string();
		file.station = "De Bilt";
		file.hasExtraInfoFile = exists(KNMIPath / (file.fileName + "ExtraInfo")); // Check if extra info file exists
		m_allFiles.push_back(file);
	}
}

void skewTFile::checkDWDFiles()
{
	path DWDPath = "assets/input/DWD";
	if (!exists(DWDPath))
	{
		// No KNMI files exists, we do create already a file though
		create_directories(DWDPath);
		return;
	}

	// Go through all files
	for (const auto& entry : directory_iterator(DWDPath))
	{
		fileInfo file;
		file.fileName = entry.path().filename().string();
		file.pathToFile = entry.path().string();

		//Get stationNumber from the filename 
		{
			std::stringstream s(file.pathToFile);
			std::string word;
			int count = 0;
			while (std::getline(s, word, '_'))
			{
				if (count == 2)
				{
					file.station = word;
					break;
				}
				count++;
			}
		}

		// For DWD, we can not get all the info from just the file name, inside each file/zip, there is the info we need.
		try
		{
			mz_zip_archive zip = {};
			// Read zip and check if it is valid using low level to only read the files we need and not extract the whole zip
			if (mz_zip_reader_init_file(&zip, file.pathToFile.c_str(), 0))
			{
				int numFiles = mz_zip_reader_get_num_files(&zip);

				// Loop over all files within the zip to compare if we have the file
				for (int i = 0; i < numFiles; i++)
				{
					char fileName[256];
					mz_zip_reader_get_filename(&zip, i, fileName, sizeof(fileName));

					// Check if this file is the meta data file
					if (std::string(fileName).find("Metadaten_Sekunde_Aero_") != std::string::npos)
					{
						// We have to read it, it is luckily not that large
						// But we can now extract the date
						size_t size;
						void* data = mz_zip_reader_extract_to_heap(&zip, i, &size, 0);
						std::string metaSoundingDataFile(static_cast<char*>(data), size);
						free(data);

						std::stringstream s(metaSoundingDataFile);
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
									word.insert(word.end() - 2, '_'); // Add seperator between date and time
									file.dates.push_back(word);
									word.resize(word.size() - 3);
									m_uniqueDates.push_back(std::stoi(word)); // Add new date
								}
								count++;
							}
							// Other info
							file.country = "Germany";
							file.hasExtraInfoFile = exists(DWDPath / (file.fileName + "ExtraInfo")); // Check if extra info file exists
							file.institute = "DWD";
						}
					}
				}
				mz_zip_reader_end(&zip);
				m_allFiles.push_back(file);
			}
		}
		catch (...)
		{
			continue;
		}
	}
}


void skewTFile::openAndReadFile(fileInfo& file, std::string& date)
{
	// Based on country/institute, read correct files
	if (file.country == "Germany") readDWDFile(file, date);
	else if (file.country == "The Netherlands") readMWXFile(file.pathToFile.c_str());

}

void skewTFile::readMWXFile(const char* _file)
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

	bool valid = true;

	std::getline(s, line); //Skip first line
	while (valid && std::getline(s, line))
	{
		std::stringstream ss(line);
		int count = 0;
		while (valid && ss >> word)
		{
			switch (count)
			{
			case 4://pressure
				word = &word[10];
				word.pop_back();
				if (std::stof(word) < 100.0f)
				{
					valid = false;
					break; // 100 hPa is high enough
				}
				pressure.push_back(std::stof(word));
				break;
			case 5://Temp in K
				word = &word[13];
				word.pop_back();
				temperature.push_back(std::stof(word) - 273.15f);
				break;
			case 6://Dew in humidity to celcius
			{
				word = &word[10];
				word.pop_back();
				const float y = log(std::stof(word) * 0.01f) + (17.625f * (temperature[row]) / (243.04f + temperature[row]));
				dewPoint.push_back(243.04f * y / (17.625f - y));
			}
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
					if (std::stoi(word))//We discard when the balloon pops
					{
						valid = false;
						continue; 
					}
				}
				break;
			}
			count++;
		}
		row++;
	}
	row--; //Go back once


	std::vector<glm::vec2> windDirVec{};
	degreeToDir(windDir, windDirVec);

	Game.SkewTMaker().loadData(temperature, dewPoint, pressure, windSpeed, windDirVec);
	Game.SkewTMaker().init();
}


void skewTFile::readDWDFile(fileInfo& _file, std::string& _date)
{
	//Open the zip file
	printf("Reading zip file...\n");
	miniz_cpp::zip_file file(_file.pathToFile);
	printf("Zip file read\n");

	std::string stationNumber;
	std::vector<std::string> AllDates;
	//std::vector<float> groundTempsAtDates;
	std::string targetDate;

	//Get stationNumber
	{
		std::stringstream s(_file.pathToFile);
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

	// Get correct date
	std::string time = std::string(_date.end() - 2, _date.end());
	targetDate = _date;
	targetDate.resize(targetDate.size() - 3); // Remove time and _
	targetDate += time;

	//targetDate = AllDates[161] + "12"; // 2025 03 24 
	//734
	//srand(static_cast<unsigned int>(std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1)));
	//int dateNum = rand() % 734;
	//targetDate = AllDates[dateNum] + "12"; // AllDates[243] = 2024 05 01 - 1700 Cape?

	printf("Reading file %s\n", targetDate.c_str());

	//std::string fileName = "produkt_sec_aero_" + AllDates[0] + "_" + AllDates[AllDates.size() - 1] + "_" + stationNumber + ".txt";


	std::string fileName{};
	for (auto& name : file.namelist())
	{
		if (name.find("produkt_sec_aero_") != std::string::npos)
		{
			fileName = name;
		}
	}

	if (fileName.empty())
	{
		perror("Error, file: produkt_sec_aero_n_n_n.txt is not included or there is another issue with the file name\n");
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
		if (s.tellg() == -1)
		{
			printf("Error, could not find date: %s\n", targetDate.c_str());
			return;
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
		if (s.tellg() == -1)
		{
			printf("Error, could not find date: %s\n", targetDate.c_str());
			return;
		}
	}

	std::vector<glm::vec2> windDirVec{};
	degreeToDir(windDir, windDirVec);

	Game.SkewTMaker().loadData(temperature, dewPoint, pressure, windSpeed, windDirVec);
	Game.SkewTMaker().init();
}

void skewTFile::getAvailableYears(std::vector<std::string>& years)
{
	if (m_uniqueDates.empty()) return;
	years.clear();
	int yearToCheck = m_uniqueDates.front() / 10000; // Start with very first year we have since our vector is sorted
	for (int date : m_uniqueDates)
	{
		date /= 10000; // Screw the last 4 digits
		if (date >= yearToCheck)
		{
			years.push_back(std::to_string(date));
			yearToCheck = date + 1;
		}
	}
}

void skewTFile::getAvailableMonths(std::string& year, std::vector<std::string>& months)
{
	if (m_uniqueDates.empty()) return;
	months.clear();
	const int yearI = std::stoi(year);
	int monthToCheck = 1;
	for (int date : m_uniqueDates)
	{
		int dateYear = date / 10000; // Screw the last 4 digits
		if (dateYear == yearI)
		{
			int dateMonth = (date / 100) - (dateYear * 100); // Screw last 2 digits, and 4 first digits
			if (dateMonth >= monthToCheck)
			{
				months.push_back(std::to_string(dateMonth));
				monthToCheck = dateMonth + 1;
			}
		}
	}
}

void skewTFile::getAvailableDays(std::string& year, std::string& month, std::vector<std::string>& days)
{
	if (m_uniqueDates.empty()) return;
	days.clear();
	const int yearI = std::stoi(year);
	const int monthI = std::stoi(month);
	int dayToCheck = 1;
	for (int date : m_uniqueDates)
	{
		int dateYear = date / 10000; // Screw the last 4 digits
		if (dateYear == yearI)
		{
			int dateMonth = (date / 100) - (dateYear * 100); // Screw last 2 digits, and 4 first digits
			if (dateMonth == monthI)
			{
				int dateDay = date - (dateYear * 10000) - (dateMonth * 100);
				if (dateDay >= dayToCheck)
				{
					days.push_back(std::to_string(dateDay));
					dayToCheck = dateDay + 1;
				}
			}
		}
	}
}

void skewTFile::getAvailableFiles(std::string& year, std::string& month, std::string& day, std::vector<fileInfo>& availableFiles)
{
	if (m_uniqueDates.empty()) return;
	availableFiles.clear();

	month = month.size() == 1 ? "0" + month : month;
	day = day.size() == 1 ? "0" + day : day;

	std::string fullDate = year + month + day;

	for (fileInfo& file : m_allFiles)
	{
		// Create new file and fill with dates that are actually inside the existing files
		fileInfo newFile = file;
		newFile.dates.clear();

		for (std::string date : file.dates)
		{
			if (date.compare(0, fullDate.size(), fullDate) == 0)
			{
				newFile.dates.push_back(date);
			}
		}
		if (!newFile.dates.empty()) availableFiles.push_back(newFile);
	}

}



void skewTFile::degreeToDir(std::vector<float>& degreeRotation, std::vector<glm::vec2>& dirRotation)
{
	if (degreeRotation.size() != dirRotation.size())
	{
		dirRotation.resize(degreeRotation.size()); // Make sure they are the same size
	}

	for (int i = 0; i < int(degreeRotation.size()); i++)
	{
		const float dir = degreeRotation[i] + 1e-16f;
		float velFieldValueX = std::sinf((dir - 180.0f) * (Constants::PI / 180.0f)) * dir;
		float velFieldValueZ = std::cosf((dir - 180.0f) * (Constants::PI / 180.0f)) * dir;
		dirRotation[i] = glm::normalize(glm::vec2(velFieldValueX, velFieldValueZ));
	}
}
#include "cloudFile.cuh"


#include "game.h"
#include "environment.h"
#include "editor.h"
#include "platform/cuda/cuda_render.cuh"

// Extras
#include "rendering/render.hpp"
#include "rendering/model.hpp"
#include "imgui/imgui.h"
#include "imgui/IconsFontAwesome.h"

#include <cuda_runtime.h>

#include <fstream>
#include <filesystem>
#include <sstream>

#include <execution>
#include <iostream>

#include <utility>

cloudFile::~cloudFile()
{
	if (m_GPUDataInitialized)
	{
		for (int i = 0; i < m_sizeGPUData + 1; i++)
		{
			cudaFree(m_groundDataGPU[i].T);
			cudaFree(m_groundDataGPU[i].t);
			cudaFree(m_groundDataGPU[i].P);
			cudaFree(m_groundDataGPU[i].Qgi);
			cudaFree(m_groundDataGPU[i].Qgs);
			cudaFree(m_groundDataGPU[i].Qgr);
			cudaFree(m_groundDataGPU[i].Qrs);

			cudaFree(m_skyDataGPU[i].pressure);
			cudaFree(m_skyDataGPU[i].velfieldZ);
			cudaFree(m_skyDataGPU[i].velfieldY);
			cudaFree(m_skyDataGPU[i].velfieldX);
			cudaFree(m_skyDataGPU[i].potTemp);
			cudaFree(m_skyDataGPU[i].Qi);
			cudaFree(m_skyDataGPU[i].Qs);
			cudaFree(m_skyDataGPU[i].Qr);
			cudaFree(m_skyDataGPU[i].Qc);
			cudaFree(m_skyDataGPU[i].Qw);
			cudaFree(m_skyDataGPU[i].Qv);
		}
	}

}

void cloudFile::panel()
{
	// Used Variables
	static char fileName[128];
	static std::string fileInfo = "Input fileName";
	bool oneTypeSelected = false;

	//Create a border when recording
	if (m_recording)
	{
		ImVec2 winPos = ImGui::GetWindowPos();
		ImVec2 winSize = ImGui::GetWindowSize();
		ImU32 color = IM_COL32(255, 0, 0, 255);
		if (m_paused) color = IM_COL32(50, 0, 200, 255);
		ImGui::GetWindowDrawList()->AddRect(winPos, ImVec2(winPos.x + winSize.x, winPos.y + winSize.y), color, 0.0f, 0, 3.0f);
	}


	ImGui::Begin("Save");


	//Again a border, so it is very visible that you are recording
	if (m_recording)
	{
		ImVec2 winPos = ImGui::GetWindowPos();
		ImVec2 winSize = ImGui::GetWindowSize();
		ImU32 color = IM_COL32(255, 0, 0, 255);
		if (m_paused) color = IM_COL32(50, 0, 200, 255);
		ImGui::GetWindowDrawList()->AddRect(winPos, ImVec2(winPos.x + winSize.x, winPos.y + winSize.y), color, 0.0f, 0, 3.0f);
	}

	// Diable part when recording
	ImGui::BeginDisabled(m_recording);
	
	// File name 
	ImGui::Text("File Name");
	ImGui::InputText("##FileNameInput", fileName, sizeof(fileName)); ImGui::SameLine();
	if (ImGui::Button("Ok##FileNameOk"))
	{
		// Add file extension
		char fileNameBuffer[128];
		std::string filePathBuffer;
		sprintf_s(fileNameBuffer, sizeof(fileNameBuffer), "%s.bin", fileName); 
		if (checkFile(fileNameBuffer, filePathBuffer))
		{
			// Check if file is valid
			m_fileName = fileName;
			m_fullFilePath = filePathBuffer;
			fileInfo = "Saving to: " + m_fullFilePath;
			m_validFileName = true;
		}
		else fileInfo = "Invalid file name, already in use!";
	}
	ImGui::TextWrapped(fileInfo.c_str());
	ImGui::EndDisabled();

	ImGui::Dummy(ImVec2(40, 10));
	ImGui::Text("Current Frames Saved: %i", m_totalFrames);
	ImGui::Dummy(ImVec2(40, 10));


	ImGui::BeginDisabled(m_recording);

	// Type selection
	ImGui::BeginGroup();
	if (ImGui::TreeNode("Include Type"))
	{
		ImGui::Dummy(ImVec2(10, 10));
		ImGui::Text("Impact visibility");
		ImGui::Separator();
		coloredSelectable("Water Cloud", &m_typesSky[0]);
		coloredSelectable("Ice Cloud", &m_typesSky[1]);
		coloredSelectable("Rain", &m_typesSky[2]);
		coloredSelectable("Snow", &m_typesSky[3]);
		coloredSelectable("Hail", &m_typesSky[4]);
		coloredSelectable("Water Vapor", &m_typesSky[5]);
		ImGui::Dummy(ImVec2(10, 10));

		ImGui::Text("Optional");
		ImGui::Separator();
		coloredSelectable("Temperature", &m_typesSky[6]);
		if (coloredSelectable("Wind", &m_typesSky[7])) m_typesSky[8] = m_typesSky[9] = m_typesSky[7];
		coloredSelectable("Pressure", &m_typesSky[10]);
		ImGui::Dummy(ImVec2(10, 10));

		ImGui::Text("Ground types");
		ImGui::Separator();
		coloredSelectable("Temperature##Ground", &m_typesGround[0]);
		coloredSelectable("Watercontent##Ground", &m_typesGround[1]);
		coloredSelectable("Rain##Ground", &m_typesGround[2]);
		coloredSelectable("Snow##Ground", &m_typesGround[3]);
		coloredSelectable("Hail##Ground", &m_typesGround[4]);
		ImGui::Dummy(ImVec2(10, 10));

		ImGui::TreePop();
	}
	ImGui::EndGroup();
	ImGui::SetItemTooltip("Including types which will be saved to the file. \nCan not be changed after first frame has been saved.");
	oneTypeSelected = isOneTypeSelected();

	ImGui::EndDisabled();


	ImGui::Dummy(ImVec2(40, 40));

	// Record, pause and discard 
	ImGui::Text("Record Pause Discard");

	if (!m_validFileName) ImGui::Text("--Invalid FileName--");
	if (!oneTypeSelected) ImGui::Text("--Select at least 1 type to save--");

	// Record
	ImGui::BeginDisabled((!m_paused && m_recording) || !m_validFileName || !oneTypeSelected);
	if (ImGui::Button(ICON_FA_PLAY_CIRCLE))  
	{
		m_recording = true;
		m_paused = false;
	}
	ImGui::SetItemTooltip("Start recording, automatically calculates based on Frames Per Hour when to save a frame.");
	ImGui::EndDisabled();

	ImGui::SameLine();

	// Pause
	ImGui::BeginDisabled(!m_recording || m_paused);
	if (ImGui::Button(ICON_FA_PAUSE) && m_recording)
	{
		m_paused = true;
	}
	ImGui::SetItemTooltip("Pause recording, will not save any frames happening while paused, can change frames per hour.");
	ImGui::EndDisabled();

	ImGui::SameLine();

	// Confirm Recording
	ImGui::BeginDisabled(!m_recording);
	if (ImGui::Button("Confirm"))
	{
		ImGui::OpenPopup("Confirm Recording?");
	}
	ImGui::SetItemTooltip("Confirm the recording, saving it to a file.");
	ImGui::EndDisabled();
	confirmPopup();

	ImGui::SameLine();

	// Discard
	ImGui::BeginDisabled(!m_recording);
	if (ImGui::Button(ICON_FA_TRASH))
	{
		ImGui::OpenPopup("Discard Recording?");
	}
	ImGui::SetItemTooltip("Discard recording, removes saved files, starting over again.");
	ImGui::EndDisabled();
	deletePopup();

	ImGui::Dummy(ImVec2(40, 10));
	ImGui::BeginDisabled(m_recording && !m_paused);

	// Frames per hour
	ImGui::Text("Frames Per Hour");
	ImGui::SliderInt("##FramesPerHourSlider", &m_framesPerHour, 1, 240);
	ImGui::SetItemTooltip("How much frames will be saved per (in-game) hour when recording. \n"
		"Automatically calculates based on seconds passed if a frame should be saved.");

	ImGui::EndDisabled();

	ImGui::End();

}

void cloudFile::deletePopup()
{
	// Opening delete popup
	// Always center this window when appearing
	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

	if (ImGui::BeginPopupModal("Discard Recording?", NULL, ImGuiChildFlags_AlwaysAutoResize))
	{
		ImGui::Text("Current recording will be discarded\n"
			"No way of retrieving them again");
		ImGui::Separator();
		if (ImGui::Button("Ok"))
		{
			m_totalFrames = 0;
			m_currentTime = 0.0f;
			m_recording = false;
			m_paused = false;
			m_skyData.clear();
			m_groundData.clear();
			m_frames.clear();

			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel"))
		{
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
}

void cloudFile::confirmPopup()
{
	// Opening confirm popup
	// Always center this window when appearing
	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

	if (ImGui::BeginPopupModal("Confirm Recording?", NULL, ImGuiChildFlags_AlwaysAutoResize))
	{
		ImGui::Text("Current recording will be confirmed and saved to file\n"
			"Recording will be stopped.");
		ImGui::Separator();
		if (ImGui::Button("Ok"))
		{
			saveToFile();

			// Reset
			m_totalFrames = 0;
			m_currentTime = 0.0f;
			m_recording = false;
			m_paused = false;
			m_validFileName = false;
			m_fileName.clear();
			m_fullFilePath.clear();
			m_skyData.clear();
			m_groundData.clear();
			m_frames.clear();
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel"))
		{
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

bool coloredSelectable(const char* label, bool* value)
{
	// Since you can not color selectables in a style manner, we have to do it manually per item
	ImVec4 darkGrey = ImVec4(0.05f, 0.05f, 0.05f, 1.0f);
	ImVec4 lightGrey = ImVec4(0.35f, 0.50f, 0.35f, 1.0f);
	ImVec4 bgColor = *value ? lightGrey : darkGrey;
	ImGui::PushStyleColor(ImGuiCol_Header, bgColor);
	ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(bgColor.x + 0.1f, bgColor.y + 0.1f, bgColor.z + 0.1f, bgColor.w));
	ImGui::PushStyleColor(ImGuiCol_HeaderActive, lightGrey);
	
	std::string labelText = *value ? "O " : "X ";
	labelText += label;

	bool clicked = false;
	clicked = ImGui::Selectable(labelText.c_str(), value);

	ImGui::PopStyleColor(3);
	return clicked;
}

bool cloudFile::isOneTypeSelected()
{
	for (int i = 0; i < 11; i++)
	{
		if (m_typesSky[i]) return true;
	}
	for (int i = 0; i < 5; i++)
	{
		if (m_typesGround[i]) return true;
	}
	return false;
}

int cloudFile::totalTypesSelected()
{
	int types = 0;
	for (int i = 0; i < 11; i++)
	{
		if (m_typesSky[i]) types++;
	}
	for (int i = 0; i < 5; i++)
	{
		if (m_typesGround[i]) types++;
	}
	return types;
}

float* cloudFile::typeToPointer(int type, environment::gridDataSky* skyData, environment::gridDataGround* groundData, bool sky)
{
	if (sky && skyData)
	{
		switch (type)
		{
		case 0: return (*skyData).Qw;
		case 1: return (*skyData).Qc;
		case 2: return (*skyData).Qr;
		case 3: return (*skyData).Qs;
		case 4: return (*skyData).Qi;
		case 5: return (*skyData).Qv;
		case 6: return (*skyData).potTemp;
		case 7: return (*skyData).velFieldX;
		case 8: return (*skyData).velFieldY;
		case 9: return (*skyData).velFieldZ;
		case 10: return (*skyData).pressure;
		default: break;
		}
	}
	else if (groundData)
	{
		switch (type)
		{
		case 0: return (*groundData).T;
		case 1: return (*groundData).Qrs;
		case 2: return (*groundData).Qgr;
		case 3: return (*groundData).Qgs;
		case 4: return (*groundData).Qgi;
		default: break;
		}
	}
	return nullptr;
}

float* cloudFile::typeToPointerGPU(int type, environment::gridDataSkyGPU* skyData, environment::gridDataGroundGPU* groundData, bool sky)
{
	if (sky && skyData)
	{
		switch (type)
		{
		case 0: return (*skyData).Qw;
		case 1: return (*skyData).Qc;
		case 2: return (*skyData).Qr;
		case 3: return (*skyData).Qs;
		case 4: return (*skyData).Qi;
		case 5: return (*skyData).Qv;
		case 6: return (*skyData).potTemp;
		case 7: return (*skyData).velfieldX;
		case 8: return (*skyData).velfieldY;
		case 9: return (*skyData).velfieldZ;
		case 10: return (*skyData).pressure;
		default: break;
		}
	}
	else if (groundData)
	{
		switch (type)
		{
		case 0: return (*groundData).T;
		case 1: return (*groundData).Qrs;
		case 2: return (*groundData).Qgr;
		case 3: return (*groundData).Qgs;
		case 4: return (*groundData).Qgi;
		default: break;
		}
	}
	return nullptr;
}

std::string cloudFile::typeToString(int type, bool sky)
{
	if (sky)
	{
		switch (type)
		{
		case 0: return "Qw";
		case 1: return "Qc";
		case 2: return "Qr";
		case 3: return "Qs";
		case 4: return "Qi";
		case 5: return "Qv";
		case 6: return "Temp";
		case 7: return "Wind";
		case 8: return "Wind";
		case 9: return "Wind";
		case 10: return "Pressure";
		default: break;
		}
	}
	else
	{
		switch (type)
		{
		case 0: return "GTemp";
		case 1: return "GWater";
		case 2: return "GQr";
		case 3: return "GQs";
		case 4: return "GQi";
		default: break;
		}
	}

	return "";
}

int cloudFile::stringToType(std::string type, bool& outputSky)
{
	if (type == "Qw") { outputSky = true; return 0; }
	else if (type == "Qc") { outputSky = true; return 1; }
	else if (type == "Qr") { outputSky = true; return 2; }
	else if (type == "Qs") { outputSky = true; return 3; }
	else if (type == "Qi") { outputSky = true; return 4; }
	else if (type == "Qv") { outputSky = true; return 5; }
	else if (type == "Temp") { outputSky = true; return 6; }
	else if (type == "Wind") { outputSky = true; return 7; } // Wind accounts for X Y and Z
	else if (type == "Pressure") { outputSky = true; return 10; }
	
	else if (type == "GTemp") { outputSky = false; return 0; }
	else if (type == "GWater") { outputSky = false; return 1; }
	else if (type == "GQr") { outputSky = false; return 2; }
	else if (type == "GQs") { outputSky = false; return 3; }
	else if (type == "GQi") { outputSky = false; return 4; }

	outputSky = false;
	return -1;
}

bool cloudFile::checkFile(const char* fileName, std::string& outputFullPath)
{
	std::filesystem::path filePath = "assets/output";
	// Check for the directory, create if it does not exist yet
	if (!std::filesystem::exists(filePath)) std::filesystem::create_directories(filePath);
	filePath = filePath / fileName;
	// Return true if file does NOT exist, meaning we can save safely.
	std::filesystem::path fullFilePath = std::filesystem::absolute(filePath);
	outputFullPath = fullFilePath.string();
	return !std::filesystem::exists(filePath);
}

bool cloudFile::tryCreateFrame(environment::gridDataSky& _skyData, environment::gridDataGround& _groundData, float time)
{
	if (!m_recording || m_paused) return false;

	float secondsPerFrame = 3600.0f / m_framesPerHour;
	float timeChange = time - m_currentTime;

	// If enough time passed, we create a new frame
	if (timeChange >= secondsPerFrame)
	{
		lockGlobal();

		environment::gridDataSky skyData;
		environment::gridDataGround groundData;
		skyData.init(GRIDSIZESKY);
		groundData.init(GRIDSIZEGROUND);

		// Based on active types, save correct data
		if (m_typesSky[0]) // Qw
		{
			memcpy_s(skyData.Qw, GRIDSIZESKY * sizeof(float), _skyData.Qw, GRIDSIZESKY * sizeof(float));
		}
		if (m_typesSky[1]) // Qc
		{
			memcpy_s(skyData.Qc, GRIDSIZESKY * sizeof(float), _skyData.Qc, GRIDSIZESKY * sizeof(float));
		}
		if (m_typesSky[2]) // Qr
		{
			memcpy_s(skyData.Qr, GRIDSIZESKY * sizeof(float), _skyData.Qr, GRIDSIZESKY * sizeof(float));
		}
		if (m_typesSky[3]) // Qs
		{
			memcpy_s(skyData.Qs, GRIDSIZESKY * sizeof(float), _skyData.Qs, GRIDSIZESKY * sizeof(float));
		}
		if (m_typesSky[4]) // Qi
		{
			memcpy_s(skyData.Qi, GRIDSIZESKY * sizeof(float), _skyData.Qi, GRIDSIZESKY * sizeof(float));
		}
		if (m_typesSky[5]) // Qv
		{
			memcpy_s(skyData.Qv, GRIDSIZESKY * sizeof(float), _skyData.Qv, GRIDSIZESKY * sizeof(float));
		}
		if (m_typesSky[6]) // Temp
		{
			memcpy_s(skyData.potTemp, GRIDSIZESKY * sizeof(float), _skyData.potTemp, GRIDSIZESKY * sizeof(float));
		}
		if (m_typesSky[7] || m_typesSky[8] || m_typesSky[9]) // Wind, all the same
		{
			memcpy_s(skyData.velFieldX, GRIDSIZESKY * sizeof(float), _skyData.velFieldX, GRIDSIZESKY * sizeof(float));
			memcpy_s(skyData.velFieldY, GRIDSIZESKY * sizeof(float), _skyData.velFieldY, GRIDSIZESKY * sizeof(float));
			memcpy_s(skyData.velFieldZ, GRIDSIZESKY * sizeof(float), _skyData.velFieldZ, GRIDSIZESKY * sizeof(float));
		}
		if (m_typesSky[10]) // Pressure
		{
			memcpy_s(skyData.pressure, GRIDSIZESKY * sizeof(float), _skyData.pressure, GRIDSIZESKY * sizeof(float));
		}
		
		// Ground values

		if (m_typesGround[0]) // Temp
		{
			memcpy_s(groundData.T, GRIDSIZEGROUND * sizeof(float), _groundData.T, GRIDSIZEGROUND * sizeof(float));
		}
		if (m_typesGround[1]) // Water
		{
			memcpy_s(groundData.Qrs, GRIDSIZEGROUND * sizeof(float), _groundData.Qrs, GRIDSIZEGROUND * sizeof(float));
		}
		if (m_typesGround[2]) // Qr
		{
			memcpy_s(groundData.Qgr, GRIDSIZEGROUND * sizeof(float), _groundData.Qgs, GRIDSIZEGROUND * sizeof(float));
		}
		if (m_typesGround[3]) // Qs
		{
			memcpy_s(groundData.Qgs, GRIDSIZEGROUND * sizeof(float), _groundData.Qgs, GRIDSIZEGROUND * sizeof(float));
		}
		if (m_typesGround[4]) // Qi
		{
			memcpy_s(groundData.Qgi, GRIDSIZEGROUND * sizeof(float), _groundData.Qgi, GRIDSIZEGROUND * sizeof(float));
		}

		m_skyData.push_back(std::move(skyData));
		m_groundData.push_back(std::move(groundData));
		m_totalFrames++;
		m_frames.push_back(time);

		m_currentTime = time;
		unlockGlobal();
		return true;
	}

	return false;
}

void cloudFile::saveToFile()
{	
	std::string fullFile = "assets/output/" + m_fileName + ".bin";

	std::ofstream writeFile(fullFile.c_str(), std::ios::binary);

	// Simulation Size
	writeFile << GRIDSIZESKYX << " " << GRIDSIZESKYY << " " << GRIDSIZESKYZ << "\n";
	// Voxel Size
	writeFile << VOXELSIZE << "\n";
	// Total Frames
	writeFile << m_totalFrames << "\n";
	// Time frames
	writeFile.write(reinterpret_cast<char*>(m_frames.data()), m_frames.size() * sizeof(float));
	// Total amount of types
	writeFile << totalTypesSelected() << "\n";
	// Types in order
	for (int i = 0; i < 11; i++) if (m_typesSky[i]) writeFile << typeToString(i, true) << " ";
	for (int i = 0; i < 5; i++) if (m_typesGround[i]) writeFile << typeToString(i, false) << " ";
	writeFile << "\n";
	// Data of all types in order
	lockGlobal(); // To be certain
	for (int i = 0; i < m_totalFrames; i++)
	{
		for (int j = 0; j < 11; j++)
		{
			if (m_typesSky[j])
			{
				writeFile.write(reinterpret_cast<char*>(typeToPointer(j, &m_skyData[i], &m_groundData[i], true)), GRIDSIZESKY * sizeof(float));
			}
		}
		for (int j = 0; j < 5; j++)
		{
			if (m_typesGround[j])
			{
				writeFile.write(reinterpret_cast<char*>(typeToPointer(j, &m_skyData[i], &m_groundData[i], false)), GRIDSIZEGROUND * sizeof(float));
				writeFile << "\n";
			}
		}
	}
	unlockGlobal();

	writeFile.close();
}

std::vector<std::string> cloudFile::getSimulationFiles()
{
	std::vector <std::string> output;
	std::filesystem::path directory = "assets/output";

	// If directory does not exist, we just do not have any files yet
	if (!std::filesystem::exists(directory)) return output;

	// Go through all files and add them
	for (const auto& entry : std::filesystem::directory_iterator(directory))
	{
		output.push_back(entry.path().filename().string());
	}
	return output;
}

bool cloudFile::getMetaData(const char* fileName, int& sizeX, int& sizeY, int& sizeZ, float& voxelSize, int& totalFrames, std::string& includedTypes)
{
	if (!loadFile(fileName, true)) return false;
	sizeX = m_tempSizeX;
	sizeY = m_tempSizeY;
	sizeZ = m_tempSizeZ;
	voxelSize = m_tempVoxelSize;
	totalFrames = m_totalFrames;
	includedTypes.clear();

	for (int j = 0; j < 11; j++)
	{
		if (m_typesSky[j])
		{
			includedTypes += typeToString(j, true) + " ";	
		}
	}
	for (int j = 0; j < 5; j++)
	{
		if (m_typesGround[j])
		{
			includedTypes += typeToString(j, false) + " ";
		}
	}
	return true;
}

bool cloudFile::loadFile(const char* fileName, bool onlyMeta)
{
	// Reset everything
	m_tempSizeX = 0;
	m_tempSizeY = 0;
	m_tempSizeZ = 0;
	m_tempVoxelSize = 0.0f;
	m_amountTypes = 0;
	m_totalFrames = 0;
	memset(m_typesSky, 0, 11 * sizeof(bool));
	memset(m_typesGround, 0, 5 * sizeof(bool));

	std::filesystem::path directory = "assets/output";
	directory = directory / fileName;

	// TODO: has extension in filename?
	if (!std::filesystem::exists(directory))
	{
		std::printf("Error, could not find file %s to load", fileName);
		return false;
	}
	
	// Data to be received
	int gridSizeFull = 0;
	int gridsizeGround = 0;


	/*	File Template
	*
	*	1. Simulation Size
	* 	2. Voxel Size
	* 	3. Total amount of frames
	* 	4. Timestamp of each frame (0 to 24 hours with decimal for minute and second)
	* 	5. Amount of types we store
	* 	6. Types we store in order
	* 	7. Data for each type
	* 
	* 	Example:
	* 
	* 	1. 32 32 32
	* 	2. 128
	* 	3. 240
	* 	4. 12.0341 12.0349 12.0357 12.0365 12.0383 12.0381 (etc) (In Binary)
	* 	5. 7
	* 	6. Qw Qc Qr Qs Qi GTemp Grs
	* 	7. 0.0 0.0 0.0 0.0 0.00001 0.0002 0.00001 0.0 0.0 (etc x for each type) (In Binray)
	*/


	bool valid = true;
	bool keepGoing = true;
	bool skipGetLine = false;

	// Open file in binary
	std::ifstream loadedFile(directory, std::ios::binary);
	std::string line;
	std::string word;
	int row = 0;
	
	// For the binary data we do not want to getLine, since this will offset the cursor.
	while (valid && keepGoing && (skipGetLine || std::getline(loadedFile, line)))
	{
		row++;
		std::stringstream ss(line);
		int count = 0;
		while (keepGoing && valid && ss >> word)
		{
			switch (row)
			{
			case 1: // Simulation Size
				if (count == 0) m_tempSizeX = std::stoi(word);
				else if (count == 1) m_tempSizeY = std::stoi(word);
				else if (count == 2)
				{
					m_tempSizeZ = std::stoi(word);
					gridSizeFull = m_tempSizeX * m_tempSizeY * m_tempSizeZ;
					gridsizeGround = m_tempSizeX * m_tempSizeZ;
				}
				break;
			case 2: // Voxel Size
				m_tempVoxelSize = std::stof(word);
				break;
			case 3: // Total amount of frames
				m_totalFrames = std::stoi(word);
				m_frames.resize(m_totalFrames);
				skipGetLine = true;
				break;
			case 4: // Timestamp of each frame (Binary)
				// Advance cursor if not interested in the data itself
				if (onlyMeta) loadedFile.seekg(m_totalFrames * sizeof(float), std::ios::cur);
				else loadedFile.read(reinterpret_cast<char*>(m_frames.data()), m_totalFrames * sizeof(float));
				skipGetLine = false;
				break;
			case 5: // Amount of types we store
				m_amountTypes = std::stoi(word);
				break;
			case 6: // Types we store in order
			{
				bool sky = false;
				int type = 0;
				type = stringToType(word, sky);
				if (type == -1) { valid = false; break; } // Invalid
				if (sky) m_typesSky[type] = true;
				else m_typesGround[type] = true;
				skipGetLine = true;
			}
				break;
			case 7: // Data for each type (Binary)

				// Not interested if only reading meta data
				if (onlyMeta)
				{
					keepGoing = false;
					break;
				}
				m_skyData.resize(m_totalFrames);
				m_groundData.resize(m_totalFrames);

				for (int i = 0; i < m_totalFrames; i++)
				{
					m_skyData[i].init(gridSizeFull);
					m_groundData[i].init(gridsizeGround);

					for (int j = 0; j < 11; j++)
					{
						if (m_typesSky[j])
						{
							loadedFile.read(reinterpret_cast<char*>(typeToPointer(j, &m_skyData[i], &m_groundData[i], true)), gridSizeFull * sizeof(float));
						}
					}
					for (int j = 0; j < 5; j++)
					{
						if (m_typesGround[j])
						{
							loadedFile.read(reinterpret_cast<char*>(typeToPointer(j, &m_skyData[i], &m_groundData[i], false)), gridsizeGround * sizeof(float));
						}
					}
				}
				skipGetLine = false;
				keepGoing = false;
				break;
			default:
				break;
			}

			count++;
			if (count > 999) valid = false;
		}
		if (row > 999) valid = false;
	}
	// Check if everything is valid
	if (m_tempSizeX <= 0 || m_tempSizeX >= 2048) valid = false;
	if (m_tempSizeY <= 0 || m_tempSizeY >= 2048) valid = false;
	if (m_tempSizeZ <= 0 || m_tempSizeZ >= 2048) valid = false;
	if (m_tempVoxelSize <= 0 || m_tempVoxelSize >= 4096) valid = false;
	if (m_amountTypes <= 0 || m_amountTypes > 11 + 5) valid = false;
	if (m_totalFrames <= 0 || m_totalFrames > 10'000) valid = false; // Lets assume 10.000 frames is too much
	if ((m_frames.empty() || m_frames.size() >= 10'000) && !onlyMeta) valid = false;
	if (!isOneTypeSelected()) valid = false;
	if (m_skyData.empty() && m_groundData.empty() && !onlyMeta) valid = false;

	if (!valid)
	{
		printf("Error, something went wrong with loading data, make sure the file is correct\n");
	}

	return valid;
}

inline int iDivUp(int a, int b) { return (a % b != 0) ? (a / b + 1) : (a / b); }

void cloudFile::getLerpedFrameData(environment::gridDataSkyGPU*& outputSkyData, environment::gridDataGroundGPU* outputGroundData, int currentFrame, float frameTime)
{
	int t1 = currentFrame;
	int t2 = currentFrame + 1;
	float t = frameTime;
	if (t2 >= m_skyData.size()) t2 = t1;

	auto time0 = std::chrono::high_resolution_clock::now();

	// Check if we need to update the GPU data
	checkIfUpdateGPU(t1, t2);

	outputSkyData = &m_skyDataGPU[m_sizeGPUData]; // Extra index acts as storage

	// Lerp data on the GPU, input the correct array index from the GPU array
	dim3 blockSize(8, 8, 8);
	dim3 gridSize = dim3(unsigned(iDivUp(GRIDSIZESKYX, blockSize.x)),
		unsigned(iDivUp(GRIDSIZESKYY, blockSize.y)),
		unsigned(iDivUp(GRIDSIZESKYZ, blockSize.z)));
	for (int i = 0; i < 11; i++) if (m_typesSky[i]) lerpFramesType << <gridSize, blockSize, 0, getStream() >> > (
		typeToPointerGPU(i, outputSkyData, outputGroundData, true),
		typeToPointerGPU(i, &m_skyDataGPU[m_GPUFrames[t1 - m_firstFrameGPU]], nullptr, true),
		typeToPointerGPU(i, &m_skyDataGPU[m_GPUFrames[t2 - m_firstFrameGPU]], nullptr, true), 
		t, 
		make_int3(GRIDSIZESKYX, GRIDSIZESKYY, GRIDSIZESKYZ));

	cudaError_t err = cudaGetLastError();
	if (err != cudaSuccess) {
		std::cerr << "error: " << cudaGetErrorString(err) << std::endl;
		__debugbreak();
	}


	auto time1 = std::chrono::high_resolution_clock::now();
	std::cout << "Raw loop (not valid since work is done asynchronisely on the GPU): " << std::chrono::duration<double, std::milli>(time1 - time0).count() << " ms\n";
}

bool cloudFile::getSurroundedFrameTimes(float time, float& outputBeforeTime, float& outputAfterTime, int& beforeFrameNum)
{
	outputBeforeTime = -1.0f;
	outputAfterTime = -1.0f;
	if (m_frames.empty()) return false;

	// Use lower bound to get the first value which is greater or the same as the input
	// If (1, 3, 5, 7, 9) with input of value 6, it returns index to 7 (index[3])
	auto it = std::lower_bound(m_frames.begin(), m_frames.end(), time);

	// If none was found, time is larger than any value in the vector
	if (it == m_frames.end())
	{
		beforeFrameNum = int(m_frames.size()) - 1;
		outputBeforeTime = m_frames.back();
		return true;
	}
	// If time is smaller than any value in the vector
	else if (*it == m_frames.front())
	{
		beforeFrameNum = 0;
		outputAfterTime = m_frames.front();
		return true;
	}

	// time is in between values
	beforeFrameNum = int(std::distance(std::begin(m_frames), it)) - 1;
	outputBeforeTime = *(it - 1);
	outputAfterTime = *(it);

	return true;
}

void cloudFile::initGPUData(void* stream)
{
	// Include 1 extra, this will hold the lerped data that is used
	m_skyDataGPU = new environment::gridDataSkyGPU[m_sizeGPUData + 1]();
	m_groundDataGPU = new environment::gridDataGroundGPU[m_sizeGPUData + 1]();

	// Environment Values
	for (int i = 0; i < m_sizeGPUData + 1; i++)
	{
		cudaMallocAsync((void**)&m_skyDataGPU[i].Qv, GRIDSIZESKY * sizeof(float), reinterpret_cast<cudaStream_t>(stream));
		cudaMallocAsync((void**)&m_skyDataGPU[i].Qw, GRIDSIZESKY * sizeof(float), reinterpret_cast<cudaStream_t>(stream));
		cudaMallocAsync((void**)&m_skyDataGPU[i].Qc, GRIDSIZESKY * sizeof(float), reinterpret_cast<cudaStream_t>(stream));
		cudaMallocAsync((void**)&m_skyDataGPU[i].Qr, GRIDSIZESKY * sizeof(float), reinterpret_cast<cudaStream_t>(stream));
		cudaMallocAsync((void**)&m_skyDataGPU[i].Qs, GRIDSIZESKY * sizeof(float), reinterpret_cast<cudaStream_t>(stream));
		cudaMallocAsync((void**)&m_skyDataGPU[i].Qi, GRIDSIZESKY * sizeof(float), reinterpret_cast<cudaStream_t>(stream));
		cudaMallocAsync((void**)&m_skyDataGPU[i].potTemp, GRIDSIZESKY * sizeof(float), reinterpret_cast<cudaStream_t>(stream));
		cudaMallocAsync((void**)&m_skyDataGPU[i].velfieldX, GRIDSIZESKY * sizeof(float), reinterpret_cast<cudaStream_t>(stream));
		cudaMallocAsync((void**)&m_skyDataGPU[i].velfieldY, GRIDSIZESKY * sizeof(float), reinterpret_cast<cudaStream_t>(stream));
		cudaMallocAsync((void**)&m_skyDataGPU[i].velfieldZ, GRIDSIZESKY * sizeof(float), reinterpret_cast<cudaStream_t>(stream));
		cudaMallocAsync((void**)&m_skyDataGPU[i].pressure, GRIDSIZESKY * sizeof(float), reinterpret_cast<cudaStream_t>(stream));
	}

	// Ground values
	for (int i = 0; i < m_sizeGPUData + 1; i++)
	{
		cudaMallocAsync((void**)&m_groundDataGPU[i].Qrs, GRIDSIZEGROUND * sizeof(float), reinterpret_cast<cudaStream_t>(stream));
		cudaMallocAsync((void**)&m_groundDataGPU[i].Qgr, GRIDSIZEGROUND * sizeof(float), reinterpret_cast<cudaStream_t>(stream));
		cudaMallocAsync((void**)&m_groundDataGPU[i].Qgs, GRIDSIZEGROUND * sizeof(float), reinterpret_cast<cudaStream_t>(stream));
		cudaMallocAsync((void**)&m_groundDataGPU[i].Qgi, GRIDSIZEGROUND * sizeof(float), reinterpret_cast<cudaStream_t>(stream));
		cudaMallocAsync((void**)&m_groundDataGPU[i].P, GRIDSIZEGROUND * sizeof(float), reinterpret_cast<cudaStream_t>(stream));
		cudaMallocAsync((void**)&m_groundDataGPU[i].t, GRIDSIZEGROUND * sizeof(float), reinterpret_cast<cudaStream_t>(stream));
		cudaMallocAsync((void**)&m_groundDataGPU[i].T, GRIDSIZEGROUND * sizeof(float), reinterpret_cast<cudaStream_t>(stream));
	}

	cudaError_t err = cudaGetLastError();
	if (err != cudaSuccess) {
		std::cerr << "error: " << cudaGetErrorString(err) << std::endl;
		__debugbreak();
	}

	m_GPUDataInitialized = true;

}


void cloudFile::checkIfUpdateGPU(int frame1, int frame2)
{
	// First check if the first frames are already initialized
	if (m_firstFrameGPU == m_lastFrameGPU && m_firstFrameGPU == 0 && m_lastFrameGPU == 0)
	{
		// Not yet initialized
		// So, initialize every type that is saved for the amount of frames we want to copy to the GPU
		m_firstFrameGPU = frame1;
		for (int j = 0; j < m_sizeGPUData; j++)
		{
			// Make sure to only copy types that we saved and not to overshoot it.
			if (frame1 + j < int(m_frames.size()))
			{
				for (int i = 0; i < 11; i++)
				{
					if (m_typesSky[i]) copyOver1Frame(&m_skyDataGPU[j], i, frame1 + j);
				}
				m_lastFrameGPU = frame1 + j;
			}
			m_GPUFrames[j] = j; // Initialize GPU frames which we know is in order currently
		}
		return;
	}


	if (frame2 >= m_lastFrameGPU)
	{
		// Save frames onwards starting from frame1

		int range = frame1 + m_sizeGPUData - 1;
		if (range >= int(m_frames.size())) range = int(m_frames.size()) - 1;
		copyNewFrames(frame1, range);
		return;
	}
	if (frame1 < m_firstFrameGPU)
	{
		// Save previous frames, since it looks like we are going backwards

		int range = frame2 - m_sizeGPUData + 1;
		if (range < 0) range = 0;
		copyNewFrames(range, frame2);
		return;
	}
}

void cloudFile::copyNewFrames(int firstFrame, int lastFrame)
{
	// We should always check to copy max amount of frames
	if (lastFrame - firstFrame > m_sizeGPUData || lastFrame - firstFrame <= 0)
	{
		printf("Warning, unable to copy new frames over, firstFrame: %i, lastFrame: %i\n", firstFrame, lastFrame);
		return;
	}

	// Key = CPU frame
	// Value = (Index , Index in the GPU array) 
	std::unordered_map<int, std::pair<int, int>> overlapData2;


	// Check which frames overlap and need to move the values of
	for (int i = 0; i < m_lastFrameGPU - m_firstFrameGPU + 1; i++)
	{
		// Write overlapped frames
		// Saving (CPU frame), as key and (GPU array index) as value
		if (m_firstFrameGPU + i >= firstFrame && m_firstFrameGPU + i <= lastFrame) overlapData2[m_firstFrameGPU + i] = { i, m_GPUFrames[i] };
	}
	
	// So, new begin of frames will be #firstFrame, we put the old frames in the correct order, then we can copy over the other data
	for (int i = 0; i < m_sizeGPUData; i++)
	{
		// Loop over the new frames, starting at firstFrame
		// If we already had this value saved, we reuse it by setting it in the new correct position
		auto it = overlapData2.find(firstFrame + i);
		if (it != overlapData2.end())
		{
			// We found it! Pfew, lets reuse it and not copy!
			// We do this by swapping the values, after this, we have all the reusables at the correct place
			int value = m_GPUFrames[i];
			m_GPUFrames[i] = it->second.second;
			m_GPUFrames[it->second.first] = value;
		}
	}
	
	// Now that we have the reusable frames at the correct place, we ignore those indices
	// We don't loop over the full GPU size, since maybe we want to copy less, so loop over the inputted frame range
	for (int i = 0; i < lastFrame - firstFrame + 1; i++)
	{
		auto it = overlapData2.find(firstFrame + i);
		// If we did NOT find it, we will copy over
		if (it == overlapData2.end())
		{
			for (int j = 0; j < 11; j++)
			{
				if (m_typesSky[j]) copyOver1Frame(&m_skyDataGPU[m_GPUFrames[i]], j, firstFrame + i);
			}
		}
	}

	cudaError_t err = cudaGetLastError();
	if (err != cudaSuccess) {
		std::cerr << "error: " << cudaGetErrorString(err) << std::endl;
		__debugbreak();
	}


	// Set data
	m_firstFrameGPU = firstFrame;
	m_lastFrameGPU = lastFrame;
}

void cloudFile::copyOver1Frame(environment::gridDataSkyGPU* dst, int type, int frame)
{
	cudaMemcpyAsync(typeToPointerGPU(type, dst, nullptr, true), typeToPointer(type, &m_skyData[frame], &m_groundData[frame], true), GRIDSIZESKY * sizeof(float), cudaMemcpyHostToDevice, getStream());
}

__global__ void lerpFramesType(float* result, float* frame1Data, float* frame2Data, float t, int3 size)
{
	int x = threadIdx.x + blockDim.x * blockIdx.x;
	int y = threadIdx.y + blockDim.y * blockIdx.y;
	int z = threadIdx.z + blockDim.z * blockIdx.z;

	if (x >= size.x || y >= size.y || z >= size.z) return;

	int idx = x + y * size.x + z * size.x * size.y;

	const float a = frame1Data[idx];
	const float b = frame2Data[idx];

	// Possible room for change/improvement
	result[idx] = a + t * (b - a);
}

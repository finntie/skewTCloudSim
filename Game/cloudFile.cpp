#include "pch.h"
#include "cloudFile.h"

#include "game.h"
#include "environment.h"
#include "editor.h"


#include <fstream>
#include <filesystem>






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
		coloredSelectable("Wind", &m_typesSky[7]);
		coloredSelectable("Pressure", &m_typesSky[8]);
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

bool cloudFile::coloredSelectable(const char* label, bool* value)
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
	for (int i = 0; i < 9; i++)
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
	for (int i = 0; i < 9; i++)
	{
		if (m_typesSky[i]) types++;
	}
	for (int i = 0; i < 5; i++)
	{
		if (m_typesGround[i]) types++;
	}
	return types;
}

float* cloudFile::typeToPointer(int type, int frame, bool sky)
{
	if (sky)
	{
		switch (type)
		{
		case 0: return m_skyData[frame].Qw;
		case 1: return m_skyData[frame].Qc;
		case 2: return m_skyData[frame].Qr;
		case 3: return m_skyData[frame].Qs;
		case 4: return m_skyData[frame].Qi;
		case 5: return m_skyData[frame].Qv;
		case 6: return m_skyData[frame].potTemp;
		case 7: return nullptr; // Velocity field, is not a float
		case 8: return m_skyData[frame].pressure;
		default: break;
		}
	}
	else
	{
		switch (type)
		{
		case 0: return m_groundData[frame].T;
		case 1: return m_groundData[frame].Qrs;
		case 2: return m_groundData[frame].Qgr;
		case 3: return m_groundData[frame].Qgs;
		case 4: return m_groundData[frame].Qgi;
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
		case 8: return "Pressure";
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
		if (m_typesSky[7]) // Wind
		{
			memcpy_s(skyData.velField, GRIDSIZESKY * sizeof(glm::vec3), _skyData.velField, GRIDSIZESKY * sizeof(glm::vec3));
		}
		if (m_typesSky[8]) // Pressure
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

	std::ofstream writeFile(fullFile.c_str());

	// Simulation Size
	writeFile << GRIDSIZESKYX << " " << GRIDSIZESKYY << " " << GRIDSIZESKYZ << "\n";
	// Voxel Size
	writeFile << VOXELSIZE << "\n";
	// Total Frames
	writeFile << m_totalFrames << "\n";
	// Time frames
	writeFile.write(reinterpret_cast<char*>(m_frames.data()), m_frames.size() * sizeof(float));
	writeFile << "\n";
	// Total amount of types
	writeFile << totalTypesSelected() << "\n";
	// Types in order
	for (int i = 0; i < 9; i++) if (m_typesSky[i]) writeFile << typeToString(i, true) << " ";
	for (int i = 0; i < 5; i++) if (m_typesGround[i]) writeFile << typeToString(i, false) << " ";
	writeFile << "\n";
	// Data of all types in order
	lockGlobal(); // To be certain
	for (int i = 0; i < m_totalFrames; i++)
	{
		for (int j = 0; j < 9; j++)
		{
			if (m_typesSky[j])
			{
				if (j == 7) // Exception for the velocity field, since this is a vec3
				{
					writeFile.write(reinterpret_cast<char*>(m_skyData[i].velField), GRIDSIZESKY * sizeof(glm::vec3));
					writeFile << "\n";
				}
				else
				{
					writeFile.write(reinterpret_cast<char*>(typeToPointer(j, i, true)), GRIDSIZESKY * sizeof(float));
					writeFile << "\n";
				}
			}
		}
		for (int j = 0; j < 5; j++)
		{
			if (m_typesGround[j])
			{
				writeFile.write(reinterpret_cast<char*>(typeToPointer(j, i, false)), GRIDSIZEGROUND * sizeof(float));
				writeFile << "\n";
			}
		}
	}
	unlockGlobal();

	writeFile.close();
}

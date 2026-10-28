#include "rendering/cloudFile_hub.hpp"

#include "rendering/cloudFile.cuh"
#include "cloud_hub.hpp"
#include "new_input.hpp"

#include "utils.cuh"
#include "config.hpp"

#include "imgui/imgui.h"
#include "imgui/IconsFontAwesome.h"

#include <string>
#include <iostream>
#include <fstream>
#include <filesystem>

cloudFileHub::cloudFileHub(cloudFile* cloudFileObject)
{
	m_cloudFileObj = cloudFileObject;
}

cloudFileHub::~cloudFileHub()
{
}

void cloudFileHub::saveCloudFilePanel()
{
	// Used Variables
	static char fileName[128];
	static std::string fileInfo = "Input fileName";
	bool oneTypeSelected = false;

	const cloudFileData* CFData = m_cloudFileObj->getCloudFileData();
	cloudFileData CFDataWrite = *CFData; // Copy the data

	//Create a border when recording
	if (CFData->m_recording)
	{
		ImVec2 winPos = ImGui::GetWindowPos();
		ImVec2 winSize = ImGui::GetWindowSize();
		ImU32 color = IM_COL32(255, 0, 0, 255);
		if (CFData->m_paused) color = IM_COL32(50, 0, 200, 255);
		ImGui::GetWindowDrawList()->AddRect(winPos, ImVec2(winPos.x + winSize.x, winPos.y + winSize.y), color, 0.0f, 0, 3.0f);
	}


	ImGui::Begin("Save");


	//Again a border, so it is very visible that you are recording
	if (CFData->m_recording)
	{
		ImVec2 winPos = ImGui::GetWindowPos();
		ImVec2 winSize = ImGui::GetWindowSize();
		ImU32 color = IM_COL32(255, 0, 0, 255);
		if (CFData->m_paused) color = IM_COL32(50, 0, 200, 255);
		ImGui::GetWindowDrawList()->AddRect(winPos, ImVec2(winPos.x + winSize.x, winPos.y + winSize.y), color, 0.0f, 0, 3.0f);
	}

	// Diable part when recording
	ImGui::BeginDisabled(CFData->m_recording);

	// File name 
	ImGui::Text("File Name");
	ImGui::InputText("##FileNameInput", fileName, sizeof(fileName)); ImGui::SameLine();
	if (ImGui::Button("Ok##FileNameOk"))
	{
		// Add file extension
		char fileNameBuffer[128];
		std::string filePathBuffer;
		snprintf(fileNameBuffer, sizeof(fileNameBuffer), "%s.bin", fileName);
		if (checkFile(fileNameBuffer, filePathBuffer))
		{
			// Check if file is valid
			CFDataWrite.m_fileName = fileName;
			CFDataWrite.m_fullFilePath = filePathBuffer;
			fileInfo = "Saving to: " + CFDataWrite.m_fullFilePath;
			CFDataWrite.m_validFileName = true;
		}
		else fileInfo = "Invalid file name, already in use!";
	}
	ImGui::TextWrapped("%s", fileInfo.c_str());
	ImGui::EndDisabled();

	ImGui::Dummy(ImVec2(40, 10));
	ImGui::Text("Current Frames Saved: %i", CFData->m_totalFrames);
	ImGui::Dummy(ImVec2(40, 10));


	ImGui::BeginDisabled(CFData->m_recording);

	// Type selection
	ImGui::BeginGroup();
	if (ImGui::TreeNode("Include Type"))
	{
		ImGui::Dummy(ImVec2(10, 10));
		ImGui::Text("Impact visibility");
		ImGui::Separator();
		coloredSelectable("Water Cloud", &CFDataWrite.m_typesSky[0]);
		coloredSelectable("Ice Cloud", &CFDataWrite.m_typesSky[1]);
		coloredSelectable("Rain", &CFDataWrite.m_typesSky[2]);
		coloredSelectable("Snow", &CFDataWrite.m_typesSky[3]);
		coloredSelectable("Hail", &CFDataWrite.m_typesSky[4]);
		coloredSelectable("Water Vapor", &CFDataWrite.m_typesSky[5]);
		ImGui::Dummy(ImVec2(10, 10));

		ImGui::Text("Optional");
		ImGui::Separator();
		coloredSelectable("Temperature", &CFDataWrite.m_typesSky[6]);
		if (coloredSelectable("Wind", &CFDataWrite.m_typesSky[7])) CFDataWrite.m_typesSky[8] = CFDataWrite.m_typesSky[9] = CFDataWrite.m_typesSky[7];
		coloredSelectable("Pressure", &CFDataWrite.m_typesSky[10]);
		ImGui::Dummy(ImVec2(10, 10));

		ImGui::Text("Ground types");
		ImGui::Separator();
		coloredSelectable("Temperature##Ground", &CFDataWrite.m_typesGround[0]);
		coloredSelectable("Watercontent##Ground", &CFDataWrite.m_typesGround[1]);
		coloredSelectable("Rain##Ground", &CFDataWrite.m_typesGround[2]);
		coloredSelectable("Snow##Ground", &CFDataWrite.m_typesGround[3]);
		coloredSelectable("Hail##Ground", &CFDataWrite.m_typesGround[4]);
		ImGui::Dummy(ImVec2(10, 10));

		ImGui::TreePop();
	}
	ImGui::EndGroup();
	ImGui::SetItemTooltip("Including types which will be saved to the file. \nCan not be changed after first frame has been saved.");
	oneTypeSelected = m_cloudFileObj->isOneTypeSelected(CFDataWrite);

	ImGui::EndDisabled();


	ImGui::Dummy(ImVec2(40, 40));

	// Record, pause and discard 
	ImGui::Text("Record Pause Discard");

	if (!CFData->m_validFileName) ImGui::Text("--Invalid FileName--");
	if (!oneTypeSelected) ImGui::Text("--Select at least 1 type to save--");

	// Record
	ImGui::BeginDisabled((!CFData->m_paused && CFData->m_recording) || !CFData->m_validFileName || !oneTypeSelected);
	if (ImGui::Button(ICON_FA_PLAY_CIRCLE))
	{
		CFDataWrite.m_recording = true;
		CFDataWrite.m_paused = false;
	}
	ImGui::SetItemTooltip("Start recording, automatically calculates based on Frames Per Hour when to save a frame.");
	ImGui::EndDisabled();

	ImGui::SameLine();

	// Pause
	ImGui::BeginDisabled(!CFData->m_recording || CFData->m_paused);
	if (ImGui::Button(ICON_FA_PAUSE) && CFData->m_recording)
	{
		CFDataWrite.m_paused = true;
	}
	ImGui::SetItemTooltip("Pause recording, will not save any frames happening while paused, can change frames per hour.");
	ImGui::EndDisabled();

	ImGui::SameLine();

	// Confirm Recording
	ImGui::BeginDisabled(!CFData->m_recording);
	if (ImGui::Button("Confirm"))
	{
		ImGui::OpenPopup("Confirm Recording?");
	}
	ImGui::SetItemTooltip("Confirm the recording, saving it to a file.");
	ImGui::EndDisabled();
	confirmPopup(CFDataWrite);

	ImGui::SameLine();

	// Discard
	ImGui::BeginDisabled(!CFData->m_recording);
	if (ImGui::Button(ICON_FA_TRASH))
	{
		ImGui::OpenPopup("Discard Recording?");
	}
	ImGui::SetItemTooltip("Discard recording, removes saved files, starting over again.");
	ImGui::EndDisabled();
	deletePopup(CFDataWrite);

	ImGui::Dummy(ImVec2(40, 10));
	ImGui::BeginDisabled(CFData->m_recording && !CFData->m_paused);

	// Frames per hour
	ImGui::Text("Frames Per Hour");
	ImGui::SliderInt("##FramesPerHourSlider", &CFDataWrite.m_framesPerHour, 1, 240);
	ImGui::SetItemTooltip("How much frames will be saved per (in-game) hour when recording. \n"
		"Automatically calculates based on seconds passed if a frame should be saved.");

	ImGui::EndDisabled();

	ImGui::End();

	m_cloudFileObj->setCloudFileData(CFDataWrite);

}

void cloudFileHub::saveRenderSettingsPanel(renderSettings& settings)
{
	static char fileName[128];
	static std::string fileInfo = "Input fileName";
	static std::string filePathBuffer;

	const cloudFileData* CFData = m_cloudFileObj->getCloudFileData();
	cloudFileData CFDataWrite = *CFData; // Copy the data


	// File name 
	ImGui::Dummy(ImVec2(20, 20));
	ImGui::Text("Save Settings");
	ImGui::InputText("##SettingsFileNameInput", fileName, sizeof(fileName)); ImGui::SameLine();
	if (ImGui::Button("Save##SettingsFileNameSave"))
	{
		// Add file extension
		char fileNameBuffer[128];
		snprintf(fileNameBuffer, sizeof(fileNameBuffer), "%s.bin", fileName);
		if (checkFile(fileNameBuffer, filePathBuffer))
		{
			CFDataWrite.m_fileName = fileName;
			CFDataWrite.m_fullFilePath = filePathBuffer;
			fileInfo = "Saved settings to: " + CFDataWrite.m_fullFilePath;
			CFDataWrite.m_validFileName = true;

			m_cloudFileObj->saveSettingsToFile(settings, CFDataWrite.m_fileName);
		}
		else
		{
			ImGui::OpenPopup("Overwrite File");
		}
	}

	// If chosen to overwrite
	if (overWriteFilePopup(fileName))
	{
		CFDataWrite.m_fileName = fileName;
		CFDataWrite.m_fullFilePath = filePathBuffer;
		fileInfo = "Saved settings to: " + CFDataWrite.m_fullFilePath;
		CFDataWrite.m_validFileName = true;

		m_cloudFileObj->saveSettingsToFile(settings, CFDataWrite.m_fileName);
	}

	ImGui::TextWrapped("%s", fileInfo.c_str());

	m_cloudFileObj->setCloudFileData(CFDataWrite);
}

bool cloudFileHub::cloudViewMenuPanel()
{
	// Variables
	bool output = false;
	static bool lerping = true;

	const cloudFileData* CFData = m_cloudFileObj->getCloudFileData();
	cloudFileData CFDataWrite = *CFData; // Copy the data

	// Menu for the cloud viewer
	if (ImGui::Button(ICON_FA_PLAY))
	{
		CFDataWrite.m_cloudViewActive = true;
	}
	ImGui::SameLine();
	if (ImGui::Button(ICON_FA_STOP))
	{
		CFDataWrite.m_cloudViewActive = false;
	}

	// Step forwards and step backwards
	if (ImGui::Button(ICON_FA_ARROW_LEFT) || CloudHub.InputObj().keyDown(newInput::LEFT_ARROW))
	{
		CFDataWrite.m_cloudViewStepOnce = -1;
	}
	ImGui::SameLine();

	if (ImGui::Button(ICON_FA_ARROW_RIGHT) || CloudHub.InputObj().keyDown(newInput::RIGHT_ARROW))
	{
		CFDataWrite.m_cloudViewStepOnce = 1;
	}

	float firstTime = m_cloudFileObj->getFirstFrameTime();
	float lastTime = m_cloudFileObj->getLastFrameTime();
	static float slideTimeInput = 0.0f;
	slideTimeInput = CFDataWrite.m_time - firstTime;
	if (ImGui::SliderFloat("Time", &slideTimeInput, 0.0f, lastTime - firstTime))
	{
		CFDataWrite.m_time = slideTimeInput + firstTime;
	}

	ImGui::Dummy(ImVec2(30, 30));

	// Calculate the current time of day from seconds
	const int hours = int(std::floor(CFDataWrite.m_time / (60.0f * 60.0f)));
	const int minutes = int(std::floor(CFDataWrite.m_time / 60.0f - (hours * 60.0f)));
	const int seconds = int(std::floor(CFDataWrite.m_time - (hours * 60.0f * 60.0f) - (minutes * 60.0f)));
	std::string time = std::to_string(hours) + ":" + std::to_string(minutes) + ":" + std::to_string(seconds);
	ImGui::Text("Time of day: %s", time.c_str());

	ImGui::Text("Speed Multiplier");
	ImGui::SliderFloat("##SpeedMult", &CFDataWrite.m_cloudViewSpeedMult, 1.0f, 10000.0f, "%.3f", ImGuiSliderFlags_Logarithmic);

	ImGui::Checkbox("Enable Lerping Frames", &lerping);
	ImGui::SetItemTooltip("Disabling lerping will snap simulaion to the nearest captured frame");

	// Type selection
	ImGui::BeginGroup();
	if (ImGui::TreeNode("Show Type"))
	{
		ImGui::Dummy(ImVec2(10, 10));
		ImGui::Separator();

		// If changed, we have to updateInput the visibility
		bool update{ false };
		if (coloredSelectable("Water Cloud", &CFDataWrite.m_visibleTypes[0])) update = true;
		if (coloredSelectable("Ice Cloud", &CFDataWrite.m_visibleTypes[1])) update = true;
		if (coloredSelectable("Rain", &CFDataWrite.m_visibleTypes[2])) update = true;
		if (coloredSelectable("Snow", &CFDataWrite.m_visibleTypes[3])) update = true;
		if (coloredSelectable("Hail", &CFDataWrite.m_visibleTypes[4])) update = true;
		if (coloredSelectable("Water Vapor", &CFDataWrite.m_visibleTypes[5])) update = true;
		if (coloredSelectable("Wind", &CFDataWrite.m_visibleTypes[6])) update = true;
		if (update)
		{
			output = true;
		}

		ImGui::Dummy(ImVec2(10, 10));
		ImGui::TreePop();
	}
	ImGui::EndGroup();
	ImGui::SetItemTooltip("Show or hide different types of choosing");

	m_cloudFileObj->setCloudFileData(CFDataWrite);

	return output;
}

void cloudFileHub::deletePopup(cloudFileData& CFData)
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
			CFData.m_totalFrames = 0;
			CFData.m_currentTime = 0.0f;
			CFData.m_recording = false;
			CFData.m_paused = false;
			CFData.m_frames.clear();
			m_cloudFileObj->resetRecording();

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

void cloudFileHub::confirmPopup(cloudFileData& CFData)
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
			m_cloudFileObj->saveToFile(CFData);

			// Reset
			CFData.m_totalFrames = 0;
			CFData.m_currentTime = 0.0f;
			CFData.m_recording = false;
			CFData.m_paused = false;
			CFData.m_validFileName = false;
			CFData.m_fileName.clear();
			CFData.m_fullFilePath.clear();
			CFData.m_frames.clear();
			m_cloudFileObj->resetRecording();

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

bool cloudFileHub::overWriteFilePopup(const char* file)
{
	bool output = false;

	if (ImGui::BeginPopupModal("Overwrite File", NULL, ImGuiChildFlags_AlwaysAutoResize))
	{
		ImGui::Text("Current file '%s' already exists.\n"
			"Do you wish to overwrite the existing file?", file);
		ImGui::Separator();

		if (ImGui::Button("Overwrite"))
		{
			ImGui::CloseCurrentPopup();
			output = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel"))
		{
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
	return output;
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


bool cloudFileHub::checkFile(const char* fileName, std::string& outputFullPath)
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
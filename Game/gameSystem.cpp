#include "pch.h"
#include "gameSystem.h"

#include "skewTMaker.h"
#include "readTable.h"
#include "environment.h"
#include "editor.h"
#include "skewTer.h"
#include "game.h"

#include "platform/cuda/cuda_render_gl.h"
#include "environment.cuh"
#include <vector_types.h>


#include "skewTFile.h"
#include "cloudFile.h"

using namespace std;
using namespace bee;


gameSystem::gameSystem()
{
	Title = "game";
	
	Game.Initialize();

	Game.Editor().setColors();
	//Game.ReadTable().readKNMIFile("assets/input/KNMI/DeBilt_20250807_113122.mwx");
	//Game.ReadTable().readDWDFile("assets/input/DWD/sekundenwerte_aero_01303_akt.zip");
	//Game.ReadTable().readDWDFile("assets/input/DWD/sekundenwerte_aero_01303_20240101_20241231_hist.zip");

	//Game.ReadTable().initEnvironment();
}


gameSystem::~gameSystem() 
{
	Game.shutdown();
}

//Update function 
void gameSystem::Update(float dt)
{
	if (m_currentState == SKEWT_CREATOR)
	{
		Game.SkewTMaker().update(dt);
		if (Game.SkewTMaker().doneMakingSkewT)
		{
			m_currentState = SIMULATION;
			// Set slower render speed in trade of higher simulation speed
			Game.cudaRenderer().setOnlyRenderResource(false);
			Game.Editor().setMode(editor::SIMULATING);
		}
	}
	if (m_currentState == SIMULATION || m_currentState == VIEW_SIMULATION)
	{
		Game.Update(dt);
	}
	//Game.ReadTable().debugDrawData();
}

void gameSystem::Render() 
{
	Game.Render();
}

// ImGui integration.
std::string gameSystem::GetName() const { return Title; }
std::string gameSystem::GetIcon() const { return ICON_FA_GAMEPAD; }


void gameSystem::OnPanel()
{
	switch (m_currentState)
	{
	case OBSERVED_SOUNDING_SELECTION:
	case CUSTOM_ENVIRONMENT_SELECTION:
	case VIEW_SIMULATION_SELECTION:
	case CREATE_SIMULATION:
	case MAINMENU:
		startMenu();
		break;
	case SKEWT_CREATOR:
		Game.SkewTMaker().panel();
		break;
	case SIMULATION:
		Game.CloudFile().panel();
		Game.Editor().panel();
		break;
	case VIEW_SIMULATION:
		Game.Editor().panel();
		break;
	default:
		break;
	}

	//ImGui::Begin("NewWindow");
	//
	//ImGui::SliderFloat("Angle", &Game.ReadTable().angle, 0, 90);
	//ImGui::SliderFloat("Liquid", &Game.ReadTable().liquid, 0, 1);
	//ImGui::Checkbox("Use i", &Game.ReadTable().useI);
	//
	//ImGui::SliderFloat("Width", &Game.ReadTable().sizeSkewT.x, 0.01f, 2.0f);
	//ImGui::SliderFloat("Height", &Game.ReadTable().sizeSkewT.y, 0.1f, 100.0f);
	//
	//ImGui::SliderFloat("GroundTemp", &Game.ReadTable().skewTData.data.temperature[0], -50.0f, 50.0f);
	//ImGui::SliderFloat("GroundDew", &Game.ReadTable().skewTData.data.dewPoint[0], -50.0f, 50.0f);
	//
	//ImGui::Text("CAPE: %f", Game.ReadTable().CAPE);
	//
	//ImGui::End();
}

void gameSystem::startMenu()
{
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.3f, 0.3f, 0.7f, 1.0f));

	ImGui::Begin("StartMenu", 0, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar);

	// Set window settings
	ImGui::SetWindowPos(ImVec2(0,0));
	auto io = ImGui::GetIO();
	ImGui::SetWindowSize(io.DisplaySize);
	ImGui::SetWindowFocus();
	ImVec2 centerOfScreen = ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
	ImVec2 returnButSize = ImVec2(100, 50);
	ImVec2 returnButLoc = ImVec2(io.DisplaySize.x - returnButSize.x - 10, io.DisplaySize.y - returnButSize.y - 10);

	// Button customization
	ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1, 1, 1, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.45f, 0.3f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.55f, 0.4f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.3f, 0.15f, 1.0f));

	// ListBox customization
	ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.1f, 0.45f, 0.3f, 1.0f));   
	ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.2f, 0.55f, 0.4f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.1f, 0.3f, 0.15f, 1.0f));



	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 2.0f);

	static std::vector<std::string> availableYears;
	static std::vector<std::string> availableMonths;
	static std::vector<std::string> availableDays;
	static std::vector<fileInfo> availableFiles;

	switch (m_currentState)
	{
	case MAINMENU:

		// Actual buttons
		ImGui::SetCursorPos(ImVec2(centerOfScreen.x - 175, centerOfScreen.y - 200));
		if (ImGui::Button("Create Simulation", ImVec2(350, 100)))
		{
			m_currentState = CREATE_SIMULATION;
		}
		ImGui::SetCursorPos(ImVec2(centerOfScreen.x - 175, centerOfScreen.y));
		if (ImGui::Button("View Simulation", ImVec2(350, 100)))
		{
			m_currentState = VIEW_SIMULATION_SELECTION;
		}

		break;

	case CREATE_SIMULATION:

		// Menu to all option to create a simulation, includes loading soundings
		ImGui::SetCursorPos(ImVec2(centerOfScreen.x - 175, centerOfScreen.y - 250));
		if (ImGui::Button("Create Skew-T", ImVec2(350, 100)))
		{
			Game.SkewTMaker().init();
			m_currentState = SKEWT_CREATOR;
		}
		ImGui::SetCursorPos(ImVec2(centerOfScreen.x - 175, centerOfScreen.y - 75));
		if (ImGui::Button("Load Observed Skew-T", ImVec2(350, 100)))
		{
			Game.SkewTFile().init();
			availableYears.clear();
			Game.SkewTFile().getAvailableYears(availableYears);
			m_currentState = OBSERVED_SOUNDING_SELECTION;
		}
		ImGui::SetCursorPos(ImVec2(centerOfScreen.x - 175, centerOfScreen.y + 100));
		if (ImGui::Button("Load Custom Skew-T", ImVec2(350, 100)))
		{
			m_currentState = CUSTOM_ENVIRONMENT_SELECTION;
		}

		ImGui::SetCursorPos(returnButLoc);
		if (ImGui::Button("Back", returnButSize)) m_currentState = MAINMENU;

		break;

	case OBSERVED_SOUNDING_SELECTION:

		// Make player choose from year, month and day

	{
		// Using lambda to convert string to const char data
		auto getter = [](void* data, int idx, const char** outText) -> bool {
			auto& vec = *static_cast<std::vector<std::string>*>(data);
			*outText = vec[idx].c_str();
			return true;
			};

		static int currentYear = -1;
		ImGui::PushItemWidth(ImGui::CalcTextSize("  0000  ").x);
		if (ImGui::ListBox("##Year", &currentYear, getter, &availableYears, int(availableYears.size()), 10))
		{
			Game.SkewTFile().getAvailableMonths(availableYears[currentYear], availableMonths);
		}
		ImGui::SameLine();

		static int currentMonth = -1;
		if (ImGui::ListBox("##Month", &currentMonth, getter, &availableMonths, int(availableMonths.size()), 12))
		{
			Game.SkewTFile().getAvailableDays(availableYears[currentYear], availableMonths[currentMonth], availableDays);
		}
		ImGui::SameLine();

		static int currentDay = -1;
		static int fileAmount = 0;
		static std::vector<bool> selected(fileAmount, false);
		if (ImGui::ListBox("##Day", &currentDay, getter, &availableDays, int(availableDays.size()), 10))
		{
			Game.SkewTFile().getAvailableFiles(availableYears[currentYear], availableMonths[currentMonth], availableDays[currentDay], availableFiles);
			fileAmount = 0;
			for (fileInfo& file : availableFiles)
			{
				for (int i = 0; i < int(file.dates.size()); i++)
				{
					fileAmount++;
				}
			}
			selected.resize(fileAmount);
			std::fill(selected.begin(), selected.end(), false);
		}

		ImGui::PopItemWidth();

		ImGui::SameLine();
		if (ImGui::BeginTable("Files", 5))
		{
			// Introduction
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("File #");
			ImGui::TableNextColumn();
			ImGui::Text("Country");
			ImGui::TableNextColumn();
			ImGui::Text("Station");
			ImGui::TableNextColumn();
			ImGui::Text("Date");

			// Actual selectables
			int count = 0;
			for (fileInfo& file : availableFiles)
			{
				for (std::string& date : file.dates)
				{
					// Show all info
					count++;
					char label[32];
					snprintf(label, sizeof(label), "File %d", count);
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					bool isSelected = selected[count - 1];
					if (ImGui::Selectable(label, isSelected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick))
					{
						std::fill(selected.begin(), selected.end(), false);
						selected[count - 1] = true;
						if (ImGui::IsMouseDoubleClicked(0))
						{
							Game.SkewTFile().openAndReadFile(file, date);
							m_currentState = SKEWT_CREATOR;
						}
					}
					ImGui::TableNextColumn();
					ImGui::Text(file.country.c_str());
					ImGui::TableNextColumn();
					ImGui::Text(file.station.c_str());
					ImGui::TableNextColumn();
					ImGui::Text(date.c_str());
				}
			}
			ImGui::EndTable();
		}
	}
		ImGui::SetCursorPos(returnButLoc);
		if (ImGui::Button("Back", returnButSize)) m_currentState = CREATE_SIMULATION;

		break;

	case CUSTOM_ENVIRONMENT_SELECTION:
	
		// Nothing here yet...

		ImGui::SetCursorPos(returnButLoc);
		if (ImGui::Button("Back", returnButSize)) m_currentState = CREATE_SIMULATION;

		break;

	case VIEW_SIMULATION_SELECTION:

		// View all saved simulation


		static std::vector<std::string> simulationFiles = Game.CloudFile().getSimulationFiles();
		static std::vector<cloudFileInfo> simulationFilesInfo(simulationFiles.size());

		

		// Using lambda to convert string to const char data
		auto getter = [](void* data, int idx, const char** outText) -> bool {
			auto& vec = *static_cast<std::vector<std::string>*>(data);
			*outText = vec[idx].c_str();
			return true;
			};


		ImGui::SameLine();
		if (ImGui::BeginTable("Files", 8))
		{
			// Introduction
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("Loading");
			ImGui::TableNextColumn();
			ImGui::Text("File");
			ImGui::TableNextColumn();
			ImGui::Text("Simulation Size X");
			ImGui::TableNextColumn();
			ImGui::Text("Simulation Size Y");
			ImGui::TableNextColumn();
			ImGui::Text("Simulation Size Z");
			ImGui::TableNextColumn();
			ImGui::Text("Size of One Voxel");
			ImGui::TableNextColumn();
			ImGui::Text("Total Amount of Frames");
			ImGui::TableNextColumn();
			ImGui::Text("All Included Types");


			// List all files and note down meta data info if wanted to load
			for (int i = 0; i < simulationFiles.size(); i++)
			{
				std::string file = simulationFiles[i];
				cloudFileInfo& info = simulationFilesInfo[i];

				// Show all info
				char label[32];
				snprintf(label, sizeof(label), "Load Info ##%i", i);
				ImGui::TableNextRow();
				ImGui::TableNextColumn();

				// Load meta data if button is pressed
				if (!info.loaded && ImGui::Button(label))
				{
					info.loaded = Game.CloudFile().getMetaData(simulationFiles[i].c_str(), info.sizeX, info.sizeY, info.sizeZ, info.voxelSize, info.totalFrames, info.types);
				}
				ImGui::TableNextColumn();

				snprintf(label, sizeof(label), "File %s", file.c_str());
				static bool isSelected = false;
				if (ImGui::Selectable(label, isSelected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick))
				{
					if (ImGui::IsMouseDoubleClicked(0))
					{
						if (!info.loaded) info.loaded = Game.CloudFile().getMetaData(simulationFiles[i].c_str(), info.sizeX, info.sizeY, info.sizeZ, info.voxelSize, info.totalFrames, info.types);
						Game.CloudFile().loadFile(simulationFiles[i].c_str(), false);
						GRIDSIZESKYX = info.sizeX;
						GRIDSIZESKYY = info.sizeY;
						GRIDSIZESKYZ = info.sizeZ;
						VOXELSIZE = info.voxelSize;
						GRIDSIZESKY = GRIDSIZESKYX * GRIDSIZESKYY * GRIDSIZESKYZ;
						GRIDSIZEGROUND = GRIDSIZESKYX * GRIDSIZESKYZ;

						// Get grid and blockdim from a cu file where we can check our specs
						dim3 blockDim;
						dim3 gridDim;
						Game.EnvGPU().getGridBlockDims(gridDim, blockDim);

						Game.Editor().initCloudViewSkyData();
						Game.Editor().setMode(editor::CLOUDVIEW);

						// Set faster render speed
						Game.cudaRenderer().setOnlyRenderResource(true);

						// Load viewer
						m_currentState = VIEW_SIMULATION;
						Game.cudaRenderer().initEnvironmentData(info.sizeX, info.sizeY, info.sizeZ, info.voxelSize, gridDim, blockDim);
					}
				}
				ImGui::TableNextColumn();
				if (info.loaded) ImGui::Text(std::to_string(info.sizeX).c_str());
				ImGui::TableNextColumn();
				if (info.loaded) ImGui::Text(std::to_string(info.sizeY).c_str());
				ImGui::TableNextColumn();
				if (info.loaded) ImGui::Text(std::to_string(info.sizeZ).c_str());
				ImGui::TableNextColumn();
				if (info.loaded) ImGui::Text(std::to_string(info.voxelSize).c_str());
				ImGui::TableNextColumn();
				if (info.loaded) ImGui::Text(std::to_string(info.totalFrames).c_str());
				ImGui::TableNextColumn();
				if (info.loaded) ImGui::Text(info.types.c_str());
			}

			ImGui::EndTable();
		}


		ImGui::SetCursorPos(returnButLoc);
		if (ImGui::Button("Back", returnButSize)) m_currentState = MAINMENU;

		break;
	}


	ImGui::PopStyleColor(7);
	ImGui::PopStyleVar(2);

	ImGui::End();

	ImGui::PopStyleColor();
}
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>


#include "environment.h"

#include "math/meteoformulas.h"
#include "math/constants.hpp"
#include "math/math.hpp"
#include "core/engine.hpp"
#include "imgui/IconsFontAwesome.h"
#include "rendering/colors.hpp"

#include "math/geometry.hpp"
#include "rendering/debug_render.hpp"
#include "core/input.hpp"

#include "imgui/imgui.h"


//|-----------------------------------------------------------------------------------------------------------|
//|                                                 ImGui                                                     |
//|-----------------------------------------------------------------------------------------------------------|

#ifdef BEE_INSPECTOR

// ImGui integration.
std::string environment::GetName() const { return Title; }
std::string environment::GetIcon() const { return ICON_FA_TERMINAL; }
void environment::OnPanel()
{
	if (ImGui::Button(ICON_FA_PLAY))
	{
		simulationActive = true;
	}
	ImGui::SameLine();
	if (ImGui::Button(ICON_FA_STOP))
	{
		simulationActive = false;
	}
	ImGui::SameLine();
	//Edit Mode
	{
		std::string mode = m_editMode ? "View" : "Edit";
		if (ImGui::Button(mode.c_str()))
		{
			m_editMode = m_editMode ? false : true;
		}
	}

	//Step forwards and step backwards (IMPORTANT: step backwards could cause inaccurancies)
	if (ImGui::Button(ICON_FA_ARROW_LEFT) || bee::Engine.Input().GetKeyboardKey(bee::Input::KeyboardKey::ArrowLeft))
	{
		simulationStep = -1;
	}
	ImGui::SameLine();
	if (ImGui::Button(ICON_FA_ARROW_RIGHT) || bee::Engine.Input().GetKeyboardKey(bee::Input::KeyboardKey::ArrowRight))
	{
		simulationStep = 1;
	}
	
	//Information
	ImGui::Indent();
	ImGui::Text(std::string("X: " + std::to_string(mousePointingIndex % GRIDSIZESKYX) + ", Y: " + std::to_string(int(float(mousePointingIndex) / GRIDSIZESKYX))).c_str());
	ImGui::Text("Pot Temp: \nQv: \nQw: \nQc: \nQr: \nQs: \nQi: \nWind: \nDebug1: \nDebug2: \nDebug3: "); ImGui::SameLine();
	ImGui::Text((std::to_string(m_envGrid.potTemp[mousePointingIndex]) + "\n" +
		std::to_string(m_envGrid.Qv[mousePointingIndex]) + "\n" + 
		std::to_string(m_envGrid.Qw[mousePointingIndex]) + "\n" +
		std::to_string(m_envGrid.Qc[mousePointingIndex]) + "\n" +
		std::to_string(m_envGrid.Qr[mousePointingIndex]) + "\n" +
		std::to_string(m_envGrid.Qs[mousePointingIndex]) + "\n" +
		std::to_string(m_envGrid.Qi[mousePointingIndex]) + "\n" +
	//ImGui::Text((std::string("Wind: ") + std::to_string(getUV(mousePointingIndex).x) + ", " + std::to_string(getUV(mousePointingIndex).y)).c_str());
		std::to_string(m_envGrid.velField[mousePointingIndex].x) + ", " + std::to_string(m_envGrid.velField[mousePointingIndex].y) + "\n" +
		std::to_string(m_debugArray0[mousePointingIndex]) + "\n" +
		std::to_string(m_debugArray1[mousePointingIndex]) + "\n" +
		std::to_string(m_debugArray2[mousePointingIndex]) + "\n"		
		).c_str());

	//ImGui::Text((std::to_string(m_envGrid.Qv[mousePointingIndex])).c_str());
	//ImGui::Text((std::to_string(m_envGrid.Qw[mousePointingIndex])).c_str());
	//ImGui::Text((std::to_string(m_envGrid.Qc[mousePointingIndex])).c_str());
	//ImGui::Text((std::to_string(m_envGrid.Qr[mousePointingIndex])).c_str());
	//ImGui::Text((std::to_string(m_envGrid.Qs[mousePointingIndex])).c_str());
	//ImGui::Text((std::to_string(m_envGrid.Qi[mousePointingIndex])).c_str());
	//ImGui::Text((std::to_string(m_envGrid.velField[mousePointingIndex].x) + ", " + std::to_string(m_envGrid.velField[mousePointingIndex].y)).c_str());
	//ImGui::Text((std::to_string(m_debugArray0[mousePointingIndex])).c_str());
	//ImGui::Text((std::to_string(m_debugArray1[mousePointingIndex])).c_str());
	//ImGui::Text((std::to_string(m_debugArray2[mousePointingIndex])).c_str());
	ImGui::Unindent();

	if (ImGui::TreeNode("Sky"))
	{
		ImGui::Text("View parameter of:");
		if (ImGui::Button("Temp")) debugViewSky = 0;	ImGui::SameLine();
		if (ImGui::Button("Wind")) debugViewSky = 7;	ImGui::SameLine();
		if (ImGui::Button("Qv")) debugViewSky = 1;		ImGui::SameLine();
		if (ImGui::Button("Qw")) debugViewSky = 2;		
		if (ImGui::Button("Qc")) debugViewSky = 3;		ImGui::SameLine();
		if (ImGui::Button("Qr")) debugViewSky = 4;		ImGui::SameLine();
		if (ImGui::Button("Qs")) debugViewSky = 5;		ImGui::SameLine();
		if (ImGui::Button("Qi")) debugViewSky = 6;		

		if (ImGui::Button("Debug1")) debugViewSky = 8;	  ImGui::SameLine();
		if (ImGui::Button("Debug2")) debugViewSky = 9;	  ImGui::SameLine();
		if (ImGui::Button("Debug3")) debugViewSky = 10;

		ImGui::TreePop();
	}
	if (ImGui::TreeNode("Ground"))
	{
		ImGui::Text("View parameter of:");
		if (ImGui::Button("Temp")) debugViewGround = 0;			ImGui::SameLine();
		if (ImGui::Button("WaterContent")) debugViewGround = 1;
		if (ImGui::Button("Qr")) debugViewGround = 2;				ImGui::SameLine();
		if (ImGui::Button("Qs")) debugViewGround = 3;				ImGui::SameLine();
		if (ImGui::Button("Qi")) debugViewGround = 4;

		ImGui::TreePop();
	}
	//Resetting
	if (ImGui::TreeNode("Resetting"))
	{
		ImGui::Text("Reset one parameter:");
		if (ImGui::Button("Wind")) memset(m_envGrid.velField, 0, sizeof(glm::vec2) * GRIDSIZESKY); ImGui::SameLine();
		if (ImGui::Button("Qv")) memset(m_envGrid.Qv, 0, sizeof(half_float::half) * GRIDSIZESKY); ImGui::SameLine();
		if (ImGui::Button("Qw")) memset(m_envGrid.Qw, 0, sizeof(half_float::half) * GRIDSIZESKY); ImGui::SameLine();
		if (ImGui::Button("Qc")) memset(m_envGrid.Qc, 0, sizeof(half_float::half) * GRIDSIZESKY);
		if (ImGui::Button("Qr")) memset(m_envGrid.Qr, 0, sizeof(half_float::half) * GRIDSIZESKY); ImGui::SameLine();
		if (ImGui::Button("Qs")) memset(m_envGrid.Qs, 0, sizeof(half_float::half) * GRIDSIZESKY); ImGui::SameLine();
		if (ImGui::Button("Qi")) memset(m_envGrid.Qi, 0, sizeof(half_float::half) * GRIDSIZESKY);

		ImGui::TreePop();
	}

	if (m_editMode)
	{
		ImGui::Begin("Edit Variables");

		if (ImGui::TreeNode("Parameters"))
		{
			ImGui::Text("Edit parameter of:");
			if (ImGui::Button("Temp")) m_debugEditParam = 0;	ImGui::SameLine();
			if (ImGui::Button("Wind")) m_debugEditParam = 7;	ImGui::SameLine();
			if (ImGui::Button("Qv")) m_debugEditParam = 1;		ImGui::SameLine();
			if (ImGui::Button("Qw")) m_debugEditParam = 2;
			if (ImGui::Button("Qc")) m_debugEditParam = 3;		ImGui::SameLine();
			if (ImGui::Button("Qr")) m_debugEditParam = 4;		ImGui::SameLine();
			if (ImGui::Button("Qs")) m_debugEditParam = 5;		ImGui::SameLine();
			if (ImGui::Button("Qi")) m_debugEditParam = 6;

			ImGui::TreePop();
		}

		ImGui::End();

	}
}


#endif

environment::environment()
{


	for (int i = 0; i < GRIDSIZESKY; i++)
	{
		m_envGrid.Qv[i] = 0.0f;
		m_envGrid.Qw[i] = 0.0f;
		m_envGrid.Qc[i] = 0.0f;
		m_envGrid.Qr[i] = 0.0f;
		m_envGrid.Qs[i] = 0.0f;
		m_envGrid.Qi[i] = 0.0f;
		m_envGrid.potTemp[i] = 301.15f;
		m_envGrid.velField[i] = { 0,1 };
	}
	for (int i = 0; i < GRIDSIZEGROUND; i++)
	{
		m_groundGrid.Qrs[i] = 0.001f; 
		m_groundGrid.Qgr[i] = 0.0f; 
		m_groundGrid.Qgs[i] = 0.0f; 
		m_groundGrid.Qgi[i] = 0.0f; 
		m_groundGrid.P[i] = 1000.0f;
		m_groundGrid.T[i] = 315.15f;
		m_groundGrid.t[i] = 0.0f;
	}
	auto& colorScheme = bee::Engine.DebugRenderer().GetColorScheme();
	colorScheme.createColorScheme("TemperatureSky", -40 + 273.15f, bee::Colors::Blue, 40 + 273.15f, bee::Colors::Red);
	colorScheme.addColor("TemperatureSky", -10 + 273.15f, bee::Colors::Cyan);
	colorScheme.addColor("TemperatureSky", 10 + 273.15f, bee::Colors::Green);
	colorScheme.addColor("TemperatureSky", 20 + 273.15f, bee::Colors::Yellow);

	colorScheme.createColorScheme("mixingRatio", 0.0f, bee::Colors::Blue, 1.0f, bee::Colors::Red);
	colorScheme.addColor("mixingRatio", 0.0005f, bee::Colors::Cyan);
	colorScheme.addColor("mixingRatio", 0.001f, bee::Colors::Green);
	colorScheme.addColor("mixingRatio", 0.01f, bee::Colors::Yellow);

	colorScheme.createColorScheme("velField", 0, bee::Colors::Blue, 100, bee::Colors::Red);
	colorScheme.addColor("velField", 10, bee::Colors::Cyan);
	colorScheme.addColor("velField", 25, bee::Colors::Green);
	colorScheme.addColor("velField", 50, bee::Colors::Yellow);

	colorScheme.createColorScheme("debugColor", -1, bee::Colors::Blue, 1, bee::Colors::Red);
	colorScheme.addColor("debugColor", -0.001f, bee::Colors::DodgerBlue);
	colorScheme.addColor("debugColor", -0.0001f, bee::Colors::Cyan);
	colorScheme.addColor("debugColor", 0.0f, bee::Colors::Green);
	colorScheme.addColor("debugColor", 0.0001f, bee::Colors::Yellow);
	colorScheme.addColor("debugColor", 0.001f, bee::Colors::Orange);
}

environment::~environment()
{

}

void environment::init(float* potTemps, glm::vec2* velField, half_float::half* Qv, float* groundTemp, half_float::half* groundPres)
{
	memcpy(m_envGrid.potTemp, potTemps, GRIDSIZESKY * sizeof(float));
	memcpy(m_envGrid.velField, velField, GRIDSIZESKY * sizeof(glm::vec2));
	memcpy(m_envGrid.Qv, Qv, GRIDSIZESKY * sizeof(half_float::half));

	memcpy(m_groundGrid.T, groundTemp, GRIDSIZEGROUND * sizeof(float));
	memcpy(m_groundGrid.P, groundPres, GRIDSIZEGROUND * sizeof(half_float::half));

	//DEBUG TODO:
	m_envGrid.velField[16 * 32 + 15] = { 20,0 };
	m_envGrid.velField[16 * 32 + 16] = { 20,0 };
	m_envGrid.velField[15 * 32 + 15] = { -20,0 };
	m_envGrid.velField[15 * 32 + 16] = { -20,0 };

	//for (int i = 0; i < GRIDSIZESKYY; i++)
	//{
	//	m_envGrid.velField[i * 32 + 16] = { 20,0 };
	//}
}




//|-----------------------------------------------------------------------------------------------------------|
//|                                                  Code                                                     |
//|-----------------------------------------------------------------------------------------------------------|

using namespace half_float;
using namespace Constants;

void environment::DebugRender()
{
	auto& colorScheme = bee::Engine.DebugRenderer().GetColorScheme();

	for (int y = 0; y < GRIDSIZESKYY; y++)
	{
		for (int x = 0; x < GRIDSIZESKYX; x++)
		{
			glm::vec3 color{};
			switch (debugViewSky)
			{
			case 0:
			{
				//Get temp
				const float height = y * VOXELSIZE;
				const float pressure = meteoformulas::getStandardPressureAtHeight(m_groundGrid.T[int(x)], height);
				const float T = m_envGrid.potTemp[int(x) + int(y) * GRIDSIZESKYX] * glm::pow(pressure / m_groundGrid.P[int(x)], Constants::Rsd / Constants::Cpd);
				colorScheme.getColor("TemperatureSky", T, color);
				break;
			}
			case 1:
				colorScheme.getColor("mixingRatio", m_envGrid.Qv[int(x) + int(y) * GRIDSIZESKYX], color);
				break;
			case 2:
				colorScheme.getColor("mixingRatio", m_envGrid.Qw[int(x) + int(y) * GRIDSIZESKYX], color);
				break;
			case 3:
				colorScheme.getColor("mixingRatio", m_envGrid.Qc[int(x) + int(y) * GRIDSIZESKYX], color);
				break;
			case 4:
				colorScheme.getColor("mixingRatio", m_envGrid.Qr[int(x) + int(y) * GRIDSIZESKYX], color);
				break;
			case 5:
				colorScheme.getColor("mixingRatio", m_envGrid.Qs[int(x) + int(y) * GRIDSIZESKYX], color);
				break;
			case 6:
				colorScheme.getColor("mixingRatio", m_envGrid.Qi[int(x) + int(y) * GRIDSIZESKYX], color);
				break;
			case 7:
				const glm::vec2 VelUV = m_envGrid.velField[x + y * GRIDSIZESKYX];// getUV(int(x) + int(y) * GRIDSIZESKYX);
				colorScheme.getColor("velField", glm::length(VelUV), color);
				bee::Engine.DebugRenderer().AddArrow(bee::DebugCategory::All, glm::vec3(x + 0.5f, y + 0.5f, 0.1f), glm::vec3(0.0f, 0.0f, 1.0f), VelUV, 0.9f, bee::Colors::Black);
				break;
			case 8:
				colorScheme.getColor("debugColor", m_debugArray0[int(x) + int(y) * GRIDSIZESKYX], color);
				break;
			case 9:
				colorScheme.getColor("debugColor", m_debugArray1[int(x) + int(y) * GRIDSIZESKYX], color);
				break;
			case 10:
				colorScheme.getColor("debugColor", m_debugArray2[int(x) + int(y) * GRIDSIZESKYX], color);
				break;
			default:
				break;
			}

			const float testValue1 = static_cast<float>(m_envGrid.Qv[int(x) + int(y) * GRIDSIZESKYX]);
			const float testValue2 = static_cast<float>(m_envGrid.Qw[int(x) + int(y) * GRIDSIZESKYX]);
			const float testValue3 = static_cast<float>(m_envGrid.Qc[int(x) + int(y) * GRIDSIZESKYX]);
			const float testValue4 = static_cast<float>(m_envGrid.Qr[int(x) + int(y) * GRIDSIZESKYX]);
			const float testValue5 = static_cast<float>(m_envGrid.Qs[int(x) + int(y) * GRIDSIZESKYX]);
			const float testValue6 = static_cast<float>(m_envGrid.Qi[int(x) + int(y) * GRIDSIZESKYX]);
			testValue1;
			testValue2;
			testValue3;
			testValue4;
			testValue6;
			testValue5;

			bee::Engine.DebugRenderer().AddSquare(bee::DebugCategory::All, glm::vec3(float(x) + 0.5f, float(y) + 0.5f, 0.0f), 1.0f, glm::vec3(0, 0, 1), bee::Colors::White);
			bee::Engine.DebugRenderer().AddFilledSquare(bee::DebugCategory::All, glm::vec3(float(x) + 0.5f, float(y) + 0.5f, 0.01f), 1.0f, glm::vec3(0, 0, 1), { color, 1.0f });

		}
	}
	for (int x = 0; x < GRIDSIZEGROUND; x++)
	{
		glm::vec3 color{};
		switch (debugViewGround)
		{
		case 0:
			colorScheme.getColor("TemperatureSky", m_groundGrid.T[x], color);
			break;
		case 1:
			colorScheme.getColor("mixingRatio", m_groundGrid.Qrs[int(x)], color);
			break;
		case 2:
			colorScheme.getColor("mixingRatio", m_groundGrid.Qgr[int(x)], color);
			break;
		case 3:
			colorScheme.getColor("mixingRatio", m_groundGrid.Qgs[int(x)], color);
			break;
		case 4:
			colorScheme.getColor("mixingRatio", m_groundGrid.Qgi[int(x)], color);
			break;
		}

		const float testValue1 = static_cast<float>(m_groundGrid.Qrs[int(x)]);
		const float testValue2 = static_cast<float>(m_groundGrid.Qgr[int(x)]);
		const float testValue3 = static_cast<float>(m_groundGrid.Qgs[int(x)]);
		const float testValue4 = static_cast<float>(m_groundGrid.Qgi[int(x)]);
		testValue1;
		testValue2;
		testValue3;
		testValue4;

		bee::Engine.DebugRenderer().AddSquare(bee::DebugCategory::All, glm::vec3(x + 0.5f, -0.5f, 0.0f), 1.0f, glm::vec3(0, 0, 1), bee::Colors::White);
		bee::Engine.DebugRenderer().AddFilledSquare(bee::DebugCategory::All, glm::vec3(x + 0.5f, -0.5f, 0.01f), 1.0f, glm::vec3(0, 0, 1), { color, 1.0f });

	}

	//Mouse pos debugging
	{
		glm::vec3 MousePos3D = bee::screenToGround(bee::Engine.Input().GetMousePosition());
		//MousePos3D *= 2;
		if (int(MousePos3D.x) < GRIDSIZESKYX && int(MousePos3D.x) >= 0 && int(MousePos3D.y) < GRIDSIZESKYY && int(MousePos3D.y) >= 0)
		{
			mousePointingIndex = (int(MousePos3D.x) + int(MousePos3D.y) * GRIDSIZESKYX);
		}
	}

	if (m_editMode)
	{
		
	}

}

void environment::Update(float dt)
{
	DebugRender();
	
	if (!simulationActive)
	{
		if (simulationStep > 0) simulationStep--;
		else if (simulationStep < 0) 
		{ 
			simulationStep++;
			dt *= -1; 
		}
		else return;
	}

	// 1. Update total incoming solar radiation 
	//Avarage solar irradiance: https://globalsolaratlas.info/map?c=51.793328,5.633017,9&r=NLD
	float avarageIrradiance = 115.0f; // W/m2
	float Irradiance = avarageIrradiance * std::max(std::sin(PI * 0.5f * (1 - ((m_time - m_hourOfSunrise - m_dayLightDuration * 0.5f) / (m_dayLightDuration * 0.5f)))), 0.0f);
	
	
	// Update Ground
	for (int i = 0; i < GRIDSIZEGROUND; i++)
	{
		// 1. Compute Cloud Covering Fraction 
		// 2. Update ground temperature
		// 3. Diffuse and advect water content Qjg
		// 4. Update microphysic process of Qjg
		// 5. Compute heat transfer at ground level due to phase transition
	
		// 1.
		float LC = groundCoverageFactor(i);
		
		// 2.
		updateGroundTemps(dt, i, Irradiance, LC);
	
		// 3. and 4.
		const float pQgr = half_cast<float>(m_groundGrid.Qgr[i]), pQgs = half_cast<float>(m_groundGrid.Qgs[i]), pQgi = half_cast<float>(m_groundGrid.Qgi[i]);
		updateMicroPhysicsGround(dt, i);
	
		// 5.
		m_groundGrid.T[i] += dt * calculateSumPhaseHeatGround(i, pQgr, pQgs, pQgi);
	}


	// Fill Pressures
	for (int i = 0; i < GRIDSIZESKY; i++)
	{
		const float height = std::floorf(float(i) / GRIDSIZESKYY) * VOXELSIZE;
		m_pressures[i] = meteoformulas::getStandardPressureAtHeight(m_groundGrid.T[i % GRIDSIZESKYX], height);
	}
	
	// Update sky
	{
		// --	Loop	--
		// 1. Update Sky Temps
		// 2. Diffuse and Advect potential temp
		// 3. Diffuse and Advect velocity field

		// --	Own Loop	--
		// 4. Pressure Project velocity field

		// --	Loop	--
		// 5. Regain Sky Temps
		// 6. Diffuse and Advect water content qj
		// 7. water transfer in ground and vorticity confinement. 
		// 8. Update microphysics
		// 9. Compute heat transfer (form. 67)

		std::vector<float> debugVector;
		debugVector.resize(GRIDSIZESKY);

		// --	Loop 1.	--
		for (int loop = 0; loop < 2; loop++)
		{
			for (int i = 0; i < GRIDSIZESKY; i++)
			{
				// we want not to update neighbouring cells, so we do the Red-Black Gauss-Seidel tactic
				if ((i + loop + int(float(i) / GRIDSIZESKYY)) % 2 == 0) continue;

				// 1.
				const float T = m_envGrid.potTemp[i] * glm::pow(m_pressures[i] / m_groundGrid.P[i % GRIDSIZESKYX], Rsd / Cpd);

				// 2.
				diffuseAndAdvectTemp(dt, i, m_envGrid.potTemp);

				// 3.
				const float B = calculateBuoyancy(i, m_pressures, T);
				const glm::vec2 F = vorticityConfinement(i);
				debugVector[i] = F.y;

				updateVelocityField(dt, i, B, F);
				
			}
		}
		setDebugArray(debugVector);

		// 4.
		//TODO: Residual variable goes up, something wrong...
		pressureProjectVelField(dt);


		// --	Loop 2.	--
		for (int loop = 0; loop < 2; loop++)
		{
			for (int i = 0; i < GRIDSIZESKY; i++)
			{
				// we want not to update neighbouring cells, so we do the Red-Black Gauss-Seidel tactic
				if ((i + loop + int(float(i) / GRIDSIZESKYY)) % 2 == 0) continue;

				// 5.
				const float T = m_envGrid.potTemp[i] * glm::pow(m_pressures[i] / m_groundGrid.P[i % GRIDSIZESKYX], Rsd / Cpd);
				const float Tv = T * (0.608f * m_envGrid.Qv[i] + 1); //TODO: this becomes negative
				const float density = m_pressures[i] * 100 / (Rsd * Tv); //Convert Pha to Pa

				// 6.
				glm::vec3 fallVelocitiesPrecip = calculateFallingVelocity(dt, i, density); //TODO: do we need to calculate density or just density of standard air?
				diffuseAndAdvect(dt, i, m_envGrid.Qv, true);
				diffuseAndAdvect(dt, i, m_envGrid.Qw);
				diffuseAndAdvect(dt, i, m_envGrid.Qc);
				diffuseAndAdvect(dt, i, m_envGrid.Qr, false, fallVelocitiesPrecip.x);
				diffuseAndAdvect(dt, i, m_envGrid.Qs, false, fallVelocitiesPrecip.y);
				diffuseAndAdvect(dt, i, m_envGrid.Qi, false, fallVelocitiesPrecip.z);

				// 7.
				const float pQv = m_envGrid.Qv[i], pQw = m_envGrid.Qw[i], pQc = m_envGrid.Qc[i], pQr = m_envGrid.Qr[i], pQs = m_envGrid.Qs[i], pQi = m_envGrid.Qi[i];
				updateMicroPhysics(dt, i, m_pressures, T, density);

				// 8.
				const float sumPhaseHeat = calculateSumPhaseHeat(i, T, pQv, pQw, pQc, pQr, pQs, pQi);
				computeHeatTransfer(dt, i, sumPhaseHeat);

			}
		}
	}

	const float speed = 60; //Minute per second
	m_time += dt * 2.77778e-4f * speed;
	if (m_time > 24.0f) m_time = 0.0f;
}

float environment::groundCoverageFactor(const int i)
{
	float totalCloudContent = 0.0f;
	const float qfull = 1.2f; // a threshold value where all incoming radiation is reflected by cloud matter: http://meto.umd.edu/~zli/PDF_papers/Li%20Nature%20Article.pdf
	for (int y = 0; y < GRIDSIZESKYY; y++) totalCloudContent += m_envGrid.Qc[i + y * GRIDSIZESKYX] + m_envGrid.Qw[i + y * GRIDSIZESKYX];
	return std::min(totalCloudContent / qfull, 1.0f);
}

void environment::updateGroundTemps(const float dt, const int i, const float Irradiance, const float LC)
{
	//const float surfaceArea = GRIDSIZEGROUND; //TODO: not used?
	const float absorbedRadiationAlbedo = 1.5f; 
	const float groundThickness = VOXELSIZE; //TODO: or groundsize?
	const float densityGround = 1500.0f;

	m_groundGrid.T[i] += dt * ((1 - LC) * (((1 - absorbedRadiationAlbedo) * Irradiance - Constants::ge * Constants::oo * m_groundGrid.T[i] * m_groundGrid.T[i] * m_groundGrid.T[i] * m_groundGrid.T[i]) / (groundThickness * Constants::Cpds * densityGround)));
}

void environment::updateMicroPhysicsGround(const float dt, const int i)
{
	// Uses some of the variables used in updateMicroPhysics
	// TODO: re-use or not?

	const int pX = (i % GRIDSIZEGROUND == 0) ? i : i - 1;
	const int nX = ((i + 1) % GRIDSIZEGROUND == 0) ? i : i + 1;

	float FR = 0.0f; // Growth of ice (precip) by freezing rain
	float MS = 0.0f; // Snow melting
	float MI = 0.0f; // Ice (precip) melting
	float ER = 0.0f; // Evaporation of rain
	float ES = 0.0f; // Evaporation of snow
	float EI = 0.0f; // Evaporation of ice (precip)
	float GR = 0.0f; // Rain hit ground
	float GS = 0.0f; // Snow hit ground
	float GI = 0.0f; // Ice (precip) hit ground
	float IR = 0.0f; //	Water flowwing through the ground (through holes in ground)
	float EG = 0.0f; // Evaporation of dry ground
	float DG = 0.0f; // diffusion coefficient for ground rain water
	float DS = 0.0f; // diffusion coefficient for subsurface water

	const float BFR{ 5e-3f };					  // Aggregation rate of freezing rain to ice (precip) rate coefficient:
	const float BMI{ 5e-4f };					  // Aggregation rate of ice to rain rate coefficient:
	const float BER{ 5e-4f };					  // Aggregation rate of rain to vapor rate coefficient:
	const float BES{ 5e-4f };					  // Aggregation rate of snow to vapor rate coefficient:
	const float BEI{ 5e-4f };					  // Aggregation rate of ice to vapor rate coefficient:
	const float BIR{ 1e-4f };					  // Evaporation rate of subsurface water
	const float k{ 1.0f };						  // How easy water flows through ground
	const float BEG{ 1e-4f };					  // Evaporation rate of dry ground

	float T = m_groundGrid.T[i];
	float p = m_groundGrid.P[i];
	const float QWS = meteoformulas::ws((T - 273.15f), p);
	const float QWI = meteoformulas::wi((T - 273.15f), p);

	if (T < -8 + 273.15f) FR = BFR * (T + 8) * (T + 8);

	if (T > 0 + 273.15f) //Melting
	{
		// 𝛿(𝑋𝑐 + 𝑋𝑠 + 𝑋𝑖) ≤ 𝑐𝑝air / 𝐿𝑓 * 𝑇, == First melt cloud ice, then snow, then precip ice, when the criteria is met between any step, stop melting.

		const float check = Cpd / Lf * T;
		float heatSum = 0.0f;

		const float Qs = static_cast<float>(m_groundGrid.Qgs[i]);
		const float Qi = static_cast<float>(m_groundGrid.Qgi[i]);
		const float MIt = BMI * T;

		//Melt snow if possible
		if (Qs > 0.0f && heatSum < check)
		{
			const float melting = std::min(Qs, check - heatSum);
			MS = melting;
			heatSum += melting;
		}
		if (Qi > 0.0f && heatSum < check)
		{
			MI = std::min(MIt, std::min(Qi, check - heatSum));
		}
	}

	//Constraint on how much can be evaporated (don't evap if air can't hold more)
	//TODO: this does not contrain the maximum amount we can evap in a timestep
	//i represent the first sky voxel (the voxel above the ground)
	ER = BER * std::min(static_cast<float>(m_envGrid.Qw[i]) + static_cast<float>(m_groundGrid.Qgr[i]), std::max(QWS - static_cast<float>(m_envGrid.Qv[i]), 0.0f));
	ES = BES * std::min(static_cast<float>(m_envGrid.Qc[i]) + static_cast<float>(m_groundGrid.Qgs[i]) + static_cast<float>(m_groundGrid.Qgi[i]), std::max(QWI - static_cast<float>(m_envGrid.Qv[i]), 0.0f));
	EI = BEI * std::min(static_cast<float>(m_envGrid.Qc[i]) + static_cast<float>(m_groundGrid.Qgs[i]) + static_cast<float>(m_groundGrid.Qgi[i]), std::max(QWI - static_cast<float>(m_envGrid.Qv[i]), 0.0f));

	GR = static_cast<float>(m_envGrid.Qr[i]);
	GS = static_cast<float>(m_envGrid.Qs[i]);
	GI = static_cast<float>(m_envGrid.Qi[i]);
	const float D_ = 1.0f; // Weigthed mean diffusivity of the ground //TODO: add this
	const float O_ = 1.0f; // evaporative ground water storage coefficient // TODO: also this
	IR = BIR * k * m_groundGrid.Qgr[i];
	//Only if Qgj = 0 (Precip falling)
	if (m_groundGrid.Qgr[i] == 0 && m_groundGrid.Qgs[i] == 0 && m_groundGrid.Qgi[i] == 0)
	{
		EG = BEG * BEG * D_ * m_groundGrid.Qrs[i] * exp(-m_groundGrid.t[i] / 86400 * O_);
		m_groundGrid.t[i] += dt;
	}
	else
	{
		m_groundGrid.t[i] = 0;
	}

	const float slope = 0.0f; //TODO: if slopes, add this to calculations
	const float nablaR = (m_groundGrid.Qgr[pX] + m_groundGrid.Qgr[nX] - 2 * m_groundGrid.Qgr[i]) / (VOXELSIZE * VOXELSIZE);
	DG = 1e-6f;
	m_groundGrid.Qgr[i] += dt * half_cast<half>(DG * nablaR + GR + MS + MI - ER - FR - IR);
	float test = half_cast<float>( m_groundGrid.Qgr[i]);
	test;
	const float nablaSR = (m_groundGrid.Qrs[pX] + m_groundGrid.Qrs[nX] - 2 * m_groundGrid.Qrs[i]) / (VOXELSIZE * VOXELSIZE);
	DS = 1e-6f;
	m_groundGrid.Qrs[i] += dt * (DS * nablaSR + slope + IR - EG);
	m_groundGrid.Qgs[i] += dt * (GS - MS - ES);
	m_groundGrid.Qgi[i] += dt * (GI + FR - EI - MI);

	if (m_groundGrid.Qrs[i] < 0|| m_groundGrid.Qgs[i] < 0 || m_groundGrid.Qgi[i] < 0 ||
		m_groundGrid.Qrs[i] != m_groundGrid.Qrs[i] || m_groundGrid.Qgs[i] != m_groundGrid.Qgs[i] || m_groundGrid.Qgi[i] != m_groundGrid.Qgi[i])
	{
		GS = 0;
	}
}

float environment::calculateSumPhaseHeatGround(const int i, const float pQgr, const float pQgs, const float pQgi)
{
	float sumPhaseheat = 0.0f;

	float dQgr = static_cast<float>(m_groundGrid.Qgr[i]) - pQgr;
	float dQgs = static_cast<float>(m_groundGrid.Qgs[i]) - pQgs;
	float dQgi = static_cast<float>(m_groundGrid.Qgi[i]) - pQgi;
	dQgr = dQgr == 0 ? 0 : dQgr / (1 + dQgr);
	dQgs = dQgs == 0 ? 0 : dQgs / (1 + dQgs);
	dQgi = dQgi == 0 ? 0 : dQgi / (1 + dQgi);

	const float invCpdRatio = 1.0f / Cpws;

	sumPhaseheat += Ls * invCpdRatio * dQgr;
	sumPhaseheat += Ls * invCpdRatio * dQgs;
	sumPhaseheat += Lf * invCpdRatio * dQgi;
	return sumPhaseheat;
}

void environment::diffuseAndAdvectTemp(const float dt, const int i, float* array)
{
	// Diffuse
	//const float viscocity = 2.2e-5f;

	const bool atLeft = i % GRIDSIZESKYX == 0;
	const bool atRight = (i + 1) % GRIDSIZESKYX == 0;
	const bool atBottom = i - GRIDSIZESKYX < 0;
	const bool atTop = i + GRIDSIZESKYX >= GRIDSIZESKY;

	const int pX = i - 1;
	const int nX = i + 1;
	const int pY = i - GRIDSIZESKYX;
	const int nY = i + GRIDSIZESKYX;

	// For temperature, we use ambient as boundary condition, we use an isentropic profile. i.e.: 𝑇𝐺 (𝑥, 𝑦, 𝑡) + Γ0z
	const float left = atLeft ? getIsentropicTemp(i) : array[pX];
	const float right = atRight ? getIsentropicTemp(i) : array[nX];
	const float down = atBottom ? 0.0f : array[pY];
	const float up = atTop ? getIsentropicTemp(i) : array[nY];

	//Advection
	// Current Position of value
	const glm::vec2 CPos = { i % GRIDSIZESKYX, int(i / GRIDSIZESKYX) };
	// Current Velocity of the exact cell, also applying falling velocity
	const glm::vec2 CVel = getUV(i);
	// Previous Position of value
	const glm::vec2 PPos = CPos - dt * CVel / VOXELSIZE; //Get previous position of particle (TODO: division by VOXELSIZE?)

	float value = 0.0f;
	//Get current value of previous position
	getInterpolValueTemp(array, PPos, value);

	array[i] = value;


	//Advect
	const glm::vec2 nabla0 = //∇0
	{
		(right - left) / (2.0f * float(VOXELSIZE)),
		(up - down) / (2.0f * float(VOXELSIZE))
	};

	//TODO: getUV maybe not setting to 0 at boundaries?
	//const float dot = glm::dot(getUV(i), nabla0);
	//array[i] += dt * dot;

	if (array[i] != array[i] || array[i] < 0)
	{
		array[i] = 0;
	}
}

void environment::getInterpolValueTemp(float* array, const glm::vec2 Ppos, float& output)
{
	//Get surrounding values 
	glm::vec2 coord1 = { (std::floor(Ppos.x)), std::floor(Ppos.y) };

	const int index00 = int(coord1.x) + int(coord1.y) * GRIDSIZESKYX;
	const int index10 = int(coord1.x + 1) + int(coord1.y) * GRIDSIZESKYX;
	const int index01 = index00 + GRIDSIZESKYX;
	const int index11 = index10 + GRIDSIZESKYX;

	//Use Isentropic Temp if outside of grid
	const float value00 = outside(index00) ? getIsentropicTemp(index00) : array[index00];
	const float value10 = outside(index10) ? getIsentropicTemp(index10) : array[index10];
	const float value01 = outside(index01) ? getIsentropicTemp(index01) : array[index01];
	const float value11 = outside(index11) ? getIsentropicTemp(index11) : array[index11];
		
	//Interpolate using bilinear interpolation, could be compressed

	float interpX = ((coord1.x + 1) - Ppos.x) / ((coord1.x + 1) - coord1.x) * value00 +
		(Ppos.x - coord1.x) / ((coord1.x + 1) - coord1.x) * value10;
	float interpX2 = ((coord1.x + 1) - Ppos.x) / ((coord1.x + 1) - coord1.x) * value01 +
		(Ppos.x - coord1.x) / ((coord1.x + 1) - coord1.x) * value11;
	output = ((coord1.y + 1) - Ppos.y) / ((coord1.y + 1) - coord1.y) * interpX +
		(Ppos.y - coord1.y) / ((coord1.y + 1) - coord1.y) * interpX2;

	if (output != output || output < 0)
	{
		output = 0;
	}
}

void environment::diffuseAndAdvect(const float dt, const int i, half* array, bool vapor, const float falVel)
{
	// Diffuse
	//const float viscocity = 2.2e-5f;

	const bool atLeft = i % GRIDSIZESKYX == 0;
	const bool atRight = (i + 1) % GRIDSIZESKYX == 0;
	const bool atBottom = i - GRIDSIZESKYX < 0;
	const bool atTop = i + GRIDSIZESKYX >= GRIDSIZESKY;

	const int pX = i - 1;
	const int nX = i + 1;
	const int pY = i - GRIDSIZESKYX;
	const int nY = i + GRIDSIZESKYX;

	float left = 0.0f;
	float right = 0.0f;
	float up = 0.0f;
	float down = 0.0f;


	if (vapor) //If vapor, we wrap the edges
	{
		left = atLeft ? static_cast<float>(array[i - 1 + GRIDSIZESKYX]) : static_cast<float>(array[pX]);
		right = atRight ? static_cast<float>(array[i + 1 - GRIDSIZESKYX]) : static_cast<float>(array[pX]);
		up = atTop ? 0 : static_cast<float>(array[nY]);
		down = atBottom ? static_cast<float>(array[i]) : static_cast<float>(array[pY]);
	}
	else
	{
		left = atLeft ? 0 : static_cast<float>(array[pX]);
		right = atRight ? 0 : static_cast<float>(array[nX]);
		up = atTop ? 0 : static_cast<float>(array[nY]);
		down = atBottom ? 0 : static_cast<float>(array[pY]);
	}

	//Diffusion
	//TODO: fix this I believe, check out how the viscocity should work with this type of advection
	//const float nabla02 = //∇θ^2
	//{
	//	(right - (2.0f * array[i]) + left) / (float(VOXELSIZE) * float(VOXELSIZE)) +
	//	(down - (2.0f * array[i]) + up) / (float(VOXELSIZE) * float(VOXELSIZE))
	//};
	//
	//array[i] += dt * viscocity * nabla02;


	//Advection
	// Current Position of value
	const glm::vec2 CPos = { i % GRIDSIZESKYX, int(i / GRIDSIZESKYX) };
	// Current Velocity of the exact cell, also applying falling velocity
	//TODO: - or + (is falVel positive or negative?)
	const glm::vec2 CVel = getUV(i) - glm::vec2(0.0f, falVel);	
	// Previous Position of value
	const glm::vec2 PPos = CPos - dt * CVel / VOXELSIZE; //Get previous position of particle (TODO: division by VOXELSIZE?)

	float value = 0.0f;
	//Get current value of previous position
	if (!getInterpolValue(array, PPos, vapor, value))
	{
		value = static_cast<float>(array[i]);
	}

	array[i] = value;

	if (array[i] != array[i] || array[i] < 0)
	{
		array[i] = 0;
	}
}

bool environment::getInterpolValue(half_float::half* array, const glm::vec2 Ppos, const bool vapor, float& output)
{
	//Get surrounding values 
	glm::vec2 coord1 = { (std::floor(Ppos.x)), std::floor(Ppos.y) };
	const bool left = coord1.x < 0;
	const bool right = coord1.x >= GRIDSIZESKYX - 1;

	//Using - 1 on x and y because we are taking the bottom left grid cell, meaning that right and top will be one further away.
	if (((left || right) && !vapor) || coord1.y < 0 || coord1.y >= GRIDSIZESKYY - 1)
	{
		return false;
	}

	//Calculate by how much the index should loop, in most cases it will just be 1 or -1.
	int changeIndexByL = left ? GRIDSIZESKYX: right ? -GRIDSIZESKYX : 0; //We dont apply how much we should move to the left or right since this will be covered by our already offset pos.
	int changeIndexByR = changeIndexByL;
	//If we are just at the left or right, we only want the Left or Right cells to loop.
	changeIndexByL = int(coord1.x) == GRIDSIZESKYX - 1 ? 0 : changeIndexByL;
	changeIndexByR = int(coord1.x) == -1 ? 0 : changeIndexByR; //Plus 2 because we check the right cell, not the current left cell.

	//We can just use the changeIndex by, because if it was not a vapor, it would already have returned false
	const int index00 = int(coord1.x) + int(coord1.y) * GRIDSIZESKYX + changeIndexByL;
	const int index10 = int(coord1.x + 1) + int(coord1.y) * GRIDSIZESKYX + changeIndexByR;
	const int index01 = index00 + GRIDSIZESKYX;
	const int index11 = index10 + GRIDSIZESKYX;

	float value00 = 0, value01 = 0, value10 = 0, value11 = 0;

	value00 = array[index00];
	value10 = array[index10];
	value01 = array[index01];
	value11 = array[index11];



	//Interpolate using bilinear interpolation, could be compressed

	float interpX = ((coord1.x + 1) - Ppos.x) / ((coord1.x + 1) - coord1.x) * value00 +
		(Ppos.x - coord1.x) / ((coord1.x + 1) - coord1.x) * value10;
	float interpX2 = ((coord1.x + 1) - Ppos.x) / ((coord1.x + 1) - coord1.x) * value01 +
		(Ppos.x - coord1.x) / ((coord1.x + 1) - coord1.x) * value11;
	output = ((coord1.y + 1) - Ppos.y) / ((coord1.y + 1) - coord1.y) * interpX +
		(Ppos.y - coord1.y) / ((coord1.y + 1) - coord1.y) * interpX2;

	if (output != output || output < 0)
	{
		output = 0;
	}

	return true;
}

using namespace Constants;

float environment::calculateBuoyancy(const int i, const float* ps, const float T)
{
	const float moleFrac = m_envGrid.Qv[i] / (m_envGrid.Qv[i] + 1);
	const float avMolarMass = moleFrac * Mw + (1 - moleFrac) * Mda;
	const float massFrac = moleFrac * (Mw / avMolarMass);

	const float isenExpAir = 1.4f;
	const float isenExpVapor = 1.33f;

	const float isenExponent = massFrac * isenExpVapor + (1 - massFrac) * isenExpAir;

	const float dT = m_groundGrid.T[i % GRIDSIZESKYX] * static_cast<float>(glm::pow(ps[i] / m_groundGrid.P[i % GRIDSIZESKYX], static_cast<float>(glm::pow(isenExponent, -2))));

	return g * ((Mda / avMolarMass) * (dT / T) - 1) / VOXELSIZE;
}

glm::vec2 environment::vorticityConfinement(const int i)
{
	//Not anymore (Using) https://www.researchgate.net/publication/239547604_Modification_of_the_Euler_equations_for_vorticity_confinement''_Application_to_the_computation_of_interacting_vortex_rings
	//Now using https://sci-hub.se/downloads/2020-12-24/50/10.1080@10618562.2020.1856822.pdf

	const float epsilon = 0.032f; //to change the strength of the vorticity confinement
	const float dynVisc = 1.8e-5f;
	const float density = 1.225f; //Air density

	const bool atLeft = i % GRIDSIZESKYX == 0;
	const bool atRight = (i + 1) % GRIDSIZESKYX == 0;
	const bool atBottom = i - GRIDSIZESKYX < 0;
	const bool atTop = i + GRIDSIZESKYX >= GRIDSIZESKY;

	const int pX = i - 1;
	const int nX = i + 1;
	const int pY = i - GRIDSIZESKYX;
	const int nY = i + GRIDSIZESKYX;

	const float center = curl(i, true);
	const float left = atLeft ? center : curl(pX);	// Neumann 
	const float right = atRight ? center : curl(nX);	// Neumann 
	const float up = atTop ? 0.0f : curl(nY); // dirichlet
	const float down = atBottom ? 0.0f : curl(pY); // dirichlet

	// stress tensor (incompressible) http://www.astro.yale.edu/vdbosch/astro320_summary6.pdf
	const glm::vec2 stressTensor = dynVisc * lap(i); //TODO: just a laplacian?

	const glm::vec2 gradient = { right - left == 0 ? 0.0f : (right - left) / (2.0f * VOXELSIZE),
								up - down == 0 ? 0.0f : (up - down) / (2.0f * VOXELSIZE) };

	//TODO: in 3D this part is different, could look at the fluid paper.
	const glm::vec2 nablaVector = glm::sqrt(std::abs(gradient.x * gradient.x + gradient.y * gradient.y)) < 1e-16f ? gradient : glm::normalize(gradient);
	const glm::vec2 s = nablaVector * curl(i, true); //Use cross(?) in 3D

	const float magGradient = glm::sqrt(gradient.x * gradient.x + gradient.y * gradient.y);

	const float confinementParam = epsilon * glm::sqrt((VOXELSIZE * VOXELSIZE) + (VOXELSIZE * VOXELSIZE)) * magGradient;

	//TODO: not sure how this works for 2D, but for now we rotate it just 90 degrees

	glm::vec2 result = stressTensor - density * confinementParam * s;

	if (result != result)
	{
		result = glm::vec2{0,0};
	}
	return result;
}

//Curl: https://en.wikipedia.org/wiki/Del
float environment::curl(const int i, bool raw)
{
	//TODO: in 3D this part is different, could look at the fluid paper. (appendix A1.3)

	const bool atLeft = i % GRIDSIZESKYX == 0;
	const bool atRight = (i + 1) % GRIDSIZESKYX == 0;
	const bool atBottom = i - GRIDSIZESKYX < 0;
	const bool atTop = i + GRIDSIZESKYX >= GRIDSIZESKY;

	const int pX = i - 1;
	const int nX = i + 1;
	const int pY = i - GRIDSIZESKYX;
	const int nY = i + GRIDSIZESKYX;

	const float left = atLeft   ? getUV(i).x : getUV(pX).x; 
	const float right = atRight ? getUV(i).x : getUV(nX).x;	
	const float up = atTop	    ? 0.0f : getUV(nY).y; 
	const float down = atBottom ? 0.0f : getUV(pY).y;

	const float deltaU = (right - left) / (2.0f * VOXELSIZE);
	const float deltaV = (up - down) / (2.0f * VOXELSIZE);

	//TODO: in 3D not just a abs anymore (due to magnitude)
	return raw ? deltaU - deltaV : fabs(deltaU - deltaV);
}

//Divergence: https://en.wikipedia.org/wiki/Del
float environment::div(const int i)
{
	//TODO: in 3D this part is different, could look at the fluid paper.

	const bool atLeft = i % GRIDSIZESKYX == 0;
	const bool atRight = (i + 1) % GRIDSIZESKYX == 0;
	const bool atBottom = i - GRIDSIZESKYX < 0;
	const bool atTop = i + GRIDSIZESKYX >= GRIDSIZESKY;

	const int pX = i - 1;
	const int nX = i + 1;
	const int pY = i - GRIDSIZESKYX;
	const int nY = i + GRIDSIZESKYX;

	const float left = atLeft ? getUV(i).x : getUV(pX).x; //Neumann
	const float right = atRight ? getUV(i).x : getUV(nX).x; //Neumann
	const float up = atTop ? 0.0f : getUV(nY).y; //Dirichlet
	const float down = atBottom ? 0.0f : getUV(pY).y; //Dirichlet

	const float deltaU = (right - left) / (2.0f * VOXELSIZE);
	const float deltaV = (up - down) / (2.0f * VOXELSIZE);

	return deltaU + deltaV;
}

//Laplacian: https://en.wikipedia.org/wiki/Del
glm::vec2 environment::lap(const int i)
{
	//TODO: in 3D this part is different, could look at the fluid paper.

	const bool atLeft = i % GRIDSIZESKYX == 0;
	const bool atRight = (i + 1) % GRIDSIZESKYX == 0;
	const bool atBottom = i - GRIDSIZESKYX < 0;
	const bool atTop = i + GRIDSIZESKYX >= GRIDSIZESKY;

	const int pX = i - 1;
	const int nX = i + 1;
	const int pY = i - GRIDSIZESKYX;
	const int nY = i + GRIDSIZESKYX;

	const glm::vec2 center = getUV(i);
	const glm::vec2 left = atLeft ? center : getUV(pX); //Neumann
	const glm::vec2 right = atRight ? center : getUV(nX); //Neumann
	const glm::vec2 up = atTop ? glm::vec2(0.0f) : getUV(nY); //Dirichlet
	const glm::vec2 down = atBottom ? glm::vec2(0.0f) : getUV(pY); //Dirichlet

	return { (right.x - 2 * center.x + left.x) / (VOXELSIZE * VOXELSIZE) +
			 (up.x - 2 * center.x + down.x) / (VOXELSIZE * VOXELSIZE),
			 (right.y - 2 * center.y + left.y) / (VOXELSIZE * VOXELSIZE) +
			 (up.y - 2 * center.y + down.y) / (VOXELSIZE * VOXELSIZE) };
}


void environment::updateVelocityField(const float dt, const int i, float B, glm::vec2 F)
{
	//Full Navier-Stroke formula
	//u[t+1] = u + deltaTime * ( -(u * ∇)u - (1/ρ)∇p + v*(∇^2u) + b + f ); 
	//But in this part we will only advect and add forces based on the Eulerian fluid solver of Hädrich et al. [2020]:
	//• Set uA = advect(un, ∆t, un)
	//• Add uB = uA + ∆t*g

	//const float viscocity{ 1e-5f };
	const bool atLeft = i % GRIDSIZESKYX == 0;
	const bool atRight = (i + 1) % GRIDSIZESKYX == 0;
	const bool atBottom = i - GRIDSIZESKYX < 0;
	const bool atTop = i + GRIDSIZESKYX >= GRIDSIZESKY;

	const int pX = i - 1;
	const int nX = i + 1;
	const int pY = i - GRIDSIZESKYX;
	const int nY = i + GRIDSIZESKYX;

	const glm::vec2 left = atLeft ? m_envGrid.velField[i] : m_envGrid.velField[pX];
	const glm::vec2 right = atRight ? m_envGrid.velField[i] : m_envGrid.velField[nX];
	const glm::vec2 up = atTop ? glm::vec2(m_envGrid.velField[i].x, 0.0f) : m_envGrid.velField[nY]; //Using Free slip
	const glm::vec2 down = atBottom ? glm::vec2(0.0f) : m_envGrid.velField[pY];
	const glm::vec2 center = m_envGrid.velField[i];

	//---Advection of velocity using Semi-Langrasian Advection---
	//We use the old value to simulate where the particle was, we get the velocity from that exact location and use that velocity for our next step
	//If our value lies outside the grid, we just use our old velocity.

	//TODO: what about vicosity / smoothing?

	// Current Position of U and V 
	const glm::vec2 CposU = {i % GRIDSIZESKYX + 0.5f, int(i / GRIDSIZESKYX)};
	const glm::vec2 CposV = {i % GRIDSIZESKYX, int(i / GRIDSIZESKYX) + 0.5f};
	// Current Velocity of U and V
	const glm::vec2 CVelU = { center.x, (left.y + right.y + up.y + down.y) / 4 }; //TODO: x was swapped with Y before, should that?
	const glm::vec2 CVelV = { (left.x + right.x + up.x + down.x) / 4, center.y };
	// Previous Position of U and V
	const glm::vec2 PposU = CposU - dt * CVelU / VOXELSIZE; //Get previous position of particle (TODO: division by VOXELSIZE?)
	const glm::vec2 PposV = CposV - dt * CVelV / VOXELSIZE; //Get previous position of particle (TODO: division by VOXELSIZE?)

	float valueU = 0.0f;
	float valueV = 0.0f;

	//---U---
	if (!getInterpolVel(PposU, true, valueU))
	{
		valueU = m_envGrid.velField[i].x;
	}
	//---V---
	if (!getInterpolVel(PposV, false, valueV))
	{
		valueV = m_envGrid.velField[i].y;
	}
	B; F;

	//----New Value------ = ----------Advection------ + --Other Forces--
	m_envGrid.velField[i] = glm::vec2(valueU, valueV) + dt * ((glm::vec2(0, B) + F)); //TODO: Buoyance makes the most right edge go up since y will never update here.

	if (m_envGrid.velField[i] != m_envGrid.velField[i] || m_envGrid.velField[i].x > 100 || m_envGrid.velField[i].y > 100 || m_envGrid.velField[i].x < -100 || m_envGrid.velField[i].y < -100)
	{
		m_envGrid.velField[i].x = 0;
	}

}

bool environment::getInterpolVel(glm::vec2 Ppos, bool U, float& output)
{
	//Get surrounding values 
	glm::vec2 indexFull;
	glm::vec2 coord1;
	//We don't have to account for any boundary conditions, this is due to the UV being at (0.5,0.5), when in the top or right, we return false (handling it outside this function)
	//Else, the points are all on the grid, right, top and topright. (no left or bottom)

	if (U)
	{
		indexFull = { (std::round(Ppos.x) - 1), std::floor(Ppos.y) };
		coord1 = { indexFull.x + 0.5f, indexFull.y };

		if (indexFull.x < 0 || indexFull.x >= GRIDSIZESKYX - 1 || indexFull.y < 0 || indexFull.y >= GRIDSIZESKYY - 1)
		{
			return false;
		}
	}
	else
	{
		indexFull = { std::floor(Ppos.x), (std::round(Ppos.y) - 1) };
		coord1 = { indexFull.x, indexFull.y + 0.5f };

		if (indexFull.x < 0 || indexFull.x >= GRIDSIZESKYX - 1 || indexFull.y < 0 || indexFull.y >= GRIDSIZESKYY - 1)
		{
			return false;
		}
	}

	//If mainpoint is outside the barrier, we just use Neumann on the old value, thus return false here.

	const int index00 = int(indexFull.x) + int(indexFull.y) * GRIDSIZESKYX;
	const int index10 = index00 + 1;
	const int index01 = index00 + GRIDSIZESKYX;
	const int index11 = index01 + 1;

	//Interpolate using bilinear interpolation, could be compressed
	if (U)
	{
		//----U----
		float interpX = ((coord1.x + 1) - Ppos.x) / ((coord1.x + 1) - coord1.x) * m_envGrid.velField[index00].x +
			(Ppos.x - coord1.x) / ((coord1.x + 1) - coord1.x) * m_envGrid.velField[index10].x;
		float interpX2 = ((coord1.x + 1) - Ppos.x) / ((coord1.x + 1) - coord1.x) * m_envGrid.velField[index01].x +
			(Ppos.x - coord1.x) / ((coord1.x + 1) - coord1.x) * m_envGrid.velField[index11].x;

		output = ((coord1.y + 1) - Ppos.y) / ((coord1.y + 1) - coord1.y) * interpX +
			(Ppos.y - coord1.y) / ((coord1.y + 1) - coord1.y) * interpX2;
		return true;
	}
	else
	{
		//----V----
		float interpX = ((coord1.x + 1) - Ppos.x) / ((coord1.x + 1) - coord1.x) * m_envGrid.velField[index00].y +
			(Ppos.x - coord1.x) / ((coord1.x + 1) - coord1.x) * m_envGrid.velField[index10].y;
		float interpX2 = ((coord1.x + 1) - Ppos.x) / ((coord1.x + 1) - coord1.x) * m_envGrid.velField[index01].y +
			(Ppos.x - coord1.x) / ((coord1.x + 1) - coord1.x) * m_envGrid.velField[index11].y;

		output = ((coord1.y + 1) - Ppos.y) / ((coord1.y + 1) - coord1.y) * interpX +
			(Ppos.y - coord1.y) / ((coord1.y + 1) - coord1.y) * interpX2;
		return true;
	}
}

void environment::pressureProjectVelField(const float dt)
{
	std::vector<float> presProjections;
	presProjections.resize(GRIDSIZESKY);
	calculatePresProj(presProjections);

	//Debug
	std::vector<float> presProjectionsDebug;
	presProjectionsDebug.resize(GRIDSIZESKY);


	const float density = 1.225f; //Air density

	for (int i = 0; i < GRIDSIZESKY; i++)
	{
		const float NxPresProj = i % GRIDSIZESKYX + 1 >= GRIDSIZESKYX ? 0.0f : presProjections[i + 1];
		const float NyPresProj = i + GRIDSIZESKYX >= GRIDSIZESKY ? 0.0f : presProjections[i + GRIDSIZESKYX];

		//TODO: not sure which boundary condition to use
		m_envGrid.velField[i].x += ((dt / density) * ((NxPresProj - presProjections[i]) / VOXELSIZE));
		m_envGrid.velField[i].y += ((dt / density) * ((NyPresProj - presProjections[i]) / VOXELSIZE));
		presProjectionsDebug[i] = (dt / density) * ((NyPresProj - presProjections[i]) / VOXELSIZE); //Debug
		presProjections[i] = (dt / density) * ((NxPresProj - presProjections[i]) / VOXELSIZE); //Debug
	}
	setDebugArray(presProjections, 1); // X
	setDebugArray(presProjectionsDebug, 2); // Y
}

void environment::calculatePresProj(std::vector<float>& p)
{
	//https://www.researchgate.net/publication/234801901_Fluid_simulation_SIGGRAPH_2007_course_notes_Video_files_associated_with_this_course_are_available_from_the_citation_page
	//  Contstruct MIC(0) (Modified Incomplete Cholesky)
	//  Calculate the divergence
	//  Solve Ap = d (to get p) using the PCG algorithm with the MIC(0) as preconditioner
	
	std::vector<float> precon;
	std::vector<float> divergence;
	std::vector<float> q; //Storage
	std::vector<float> z; //auxiliary vector
	std::vector<float> s; //search vector
	std::vector<float> r; //Residual vector
	std::vector<glm::ivec3> A; //Matrix (neighbour check; -1 or 0)
	float sigma = 0;
	float sigmaNew = 0;
	float B = 0;
	float maxr = 0;
	const float tolValue = 1e-6f; //TODO: THIS SHOULD BE CORRECTED MAYBE
	const int MAXITERATION = 200;

	divergence.resize(GRIDSIZESKY);
	precon.resize(GRIDSIZESKY);
	q.resize(GRIDSIZESKY);
	z.resize(GRIDSIZESKY);
	s.resize(GRIDSIZESKY);
	p.assign(p.size(), 0.0f);
	r.resize(GRIDSIZESKY);
	A.resize(GRIDSIZESKY);

	//Fill matrix A	
	for (int y = 0; y < GRIDSIZESKYY; y++)
	{
		for (int x = 0; x < GRIDSIZESKYX; x++)
		{
			//Because there are extra rows, we add 1 everywhere where needed
			//TODO: Here you can add if right/up is nonfluid
			A[x + y * GRIDSIZESKYX].x = /*x < GRIDSIZESKYX ? -1 : 0*/ -1;
			A[x + y * GRIDSIZESKYX].y = y < GRIDSIZESKYY - 1 ? -1 : 0;
			A[x + y * GRIDSIZESKYX].z = 0;
			//TODO: Nuemann is using sides, so they are fluids
			/*if (x < GRIDSIZESKYX - 1)*/ A[x + y * GRIDSIZESKYX].z++;
			if (y < GRIDSIZESKYY - 1) A[x + y * GRIDSIZESKYX].z++;
			/*if (x > 0)*/ A[x + y * GRIDSIZESKYX].z++;
			if (y > 0) A[x + y * GRIDSIZESKYX].z++;
		}
	}
	

	{
		calculateDivergence(divergence);
	}

	r = divergence;

	//Checking max residual 
	{
		for (int i = 0; i < GRIDSIZESKY; i++)
		{
			maxr = std::max(maxr, fabsf(r[i]));
		}
		if (maxr == 0) return;
	}

	calculatePrecon(precon, A);
	
	applyPreconditioner(precon, divergence, A, q, z);
	s = z;
	//setDebugArray(r);

	//Dotproduct
	{
		for (int i = 0; i < GRIDSIZESKY; i++)
		{
			sigma += z[i] * r[i];
		}
	}

	//Loop until done or max iteration reached
	MAXITERATION;
	for (int i = 0; i < MAXITERATION; i++)
	{
		applyA(s, A, z);
		//Dotproduct
		sigmaNew = 0;
		for (int j = 0; j < GRIDSIZESKY; j++)
		{
			sigmaNew += z[j] * s[j];
		}
		sigmaNew = sigma / sigmaNew;

		float totalP = 0.0f;
		//Adding up pressure value and reducing residual value
		for (int j = 0; j < GRIDSIZESKY; j++)
		{
			p[j] += sigmaNew * s[j];
			totalP += p[j];
			r[j] -= sigmaNew * z[j];
		}

		//Checking max residual 
		maxr = 0;
		for (int j = 0; j < GRIDSIZESKY; j++)
		{
			maxr = std::max(maxr, fabsf(r[j]));
		}
		if (maxr < tolValue)
		{
			return;
		}

		applyPreconditioner(precon, r, A, q, z);

		//Dotproduct
		sigmaNew = 0;
		for (int j = 0; j < GRIDSIZESKY; j++)
		{
			sigmaNew += z[j] * r[j];
		}
		B = sigmaNew / sigma;

		//Setting search vector s
		for (int j= 0; j < GRIDSIZESKY; j++)
		{
			s[j] = z[j] + B * s[j];
		}
		sigma = sigmaNew;

	}

	//P is filled as max as MAXITERATIONS could.
}

void environment::calculateDivergence(std::vector<float>& output)
{
	for (int i = 0; i < GRIDSIZESKY; i++)
	{
		const float Ucurr = m_envGrid.velField[i].x;
		const float Umin1 = i % GRIDSIZESKYX == 0 ? m_envGrid.velField[i].x : m_envGrid.velField[i - 1].x; //Using Neumann boundary condition
		const float Vcurr = m_envGrid.velField[i].y;
		const float Vmin1 = i - GRIDSIZESKYX < 0 ? 0.0f : m_envGrid.velField[i - GRIDSIZESKYX].y; //Using no-slip boundary condition for ground

		output[i] = (Ucurr - Umin1) / VOXELSIZE + (Vcurr - Vmin1) / VOXELSIZE; //TODO: Division by voxelsize?
	}
}

void environment::calculatePrecon(std::vector<float>& output, std::vector<glm::ivec3>& A)
{
	const float Tune = 0.97f;
	//const float voxelTest = 1 / VOXELSIZE;

	for (int y = 0; y < GRIDSIZESKYY; y++)
	{
		for (int x = 0; x < GRIDSIZESKYX; x++)
		{
			//TODO: for 3D, look at formula 4.34
			 
			const int idx = x + y * GRIDSIZESKYX;

			const int Aminxx = x == 0 ? -1 : A[idx - 1].x; //Minus X looking at x
			const int Aminxy = x == 0 ? -1 : A[idx - 1].y; //Minus X looking at y
			const int Aminyy = y == 0 ? 0 : A[idx - GRIDSIZESKYX].y; //Minus Y looking at y
			const int Aminyx = y == 0 ? 0 : A[idx - GRIDSIZESKYX].x; //Minus Y looking at x

			const float Preconi = x - 1 < 0 || Aminxx == 0 ? 0.0f : output[idx - 1];
			const float Preconj = y - 1 < 0 || Aminyy == 0 ? 0.0f : output[idx - GRIDSIZESKYX];


			const float e = A[idx].z
				- (Aminxx * Preconi) * (Aminxx * Preconi)
				- (Aminyy * Preconj) * (Aminyy * Preconj)
				- Tune * (
					Aminxx * Aminxy * (Preconi * Preconi) +
					Aminyy * Aminyx * (Preconj * Preconj));
			output[x + y * GRIDSIZESKYX] = (1 / sqrtf(e + 1e-30f)); //Prevent division by 0 using small number;
		}
	}
}

void environment::applyPreconditioner(std::vector<float>& precon, std::vector<float>& r, std::vector<glm::ivec3>& A, std::vector<float>& q, std::vector<float>& output)
{
	q.assign(q.size(), 0.0f);
	output.assign(output.size(), 0.0f);

	//Solve Lq = d
	for (int y = 0; y < GRIDSIZESKYY; y++)
	{
		for (int x = 0; x < GRIDSIZESKYX; x++)
		{
			const int idx = x + y * GRIDSIZESKYX;

			const int Aminxx = x == 0 ? -1 : A[idx - 1].x; //Minus X looking at x
			const int Aminyy = y == 0 ? 0 : A[idx - GRIDSIZESKYX].y; //Minus Y looking at y

			const float qpi = x - 1 < 0 ? q[x + y * GRIDSIZESKYX] : q[x - 1 + y * GRIDSIZESKYX];
			const float qpj = y - 1 < 0 ? q[x + y * GRIDSIZESKYX] : q[x + (y - 1) * GRIDSIZESKYX];

			const float preconCenter = precon[x + y * GRIDSIZESKYX];
			const float Preconi = x - 1 < 0 ? preconCenter : precon[x - 1 + y * GRIDSIZESKYX];
			const float Preconj = y - 1 < 0 ? preconCenter : precon[x + (y - 1) * GRIDSIZESKYX];

			const float t = r[x + y * GRIDSIZESKYX] - (Aminxx * Preconi * qpi) - (Aminyy * Preconj * qpj);
			q[x + y * GRIDSIZESKYX] = t * preconCenter;
		}
	}


	//Solve L^Tp = q
	for (int y = GRIDSIZESKYY - 1; y >= 0; y--)
	{
		for (int x = GRIDSIZESKYX - 1; x >= 0; x--)
		{
			const int idx = x + y * GRIDSIZESKYX;

			const float zpi = x + 1 >= GRIDSIZESKYX ? output[x + y * GRIDSIZESKYX] : output[x + 1 + y * GRIDSIZESKYX];
			const float zpj = y + 1 >= GRIDSIZESKYY ? output[x + y * GRIDSIZESKYX] : output[x + (y + 1) * GRIDSIZESKYX];

			const float preconCenter = precon[x + y * GRIDSIZESKYX];
			const float Preconi = x + 1 >= GRIDSIZESKYX ? preconCenter : precon[x + 1 + y * GRIDSIZESKYX];
			const float Preconj = y + 1 >= GRIDSIZESKYY ? preconCenter : precon[x + (y + 1) * GRIDSIZESKYX];

			const float t = q[x + y * GRIDSIZESKYX] - (A[idx].x * Preconi * zpi) - (A[idx].y * Preconj * zpj);

			output[x + y * GRIDSIZESKYX] = t * preconCenter;	
		}
	}
	//setDebugArray(r);
	//setDebugArray(q, 1);
	//setDebugArray(output, 2);

}

void environment::applyA(std::vector<float>& s, std::vector<glm::ivec3>& A, std::vector<float>& z)
{
	z.assign(z.size(), 0.0f);

	for (int y = 0; y < GRIDSIZESKYY; y++)
	{
		for (int x = 0; x < GRIDSIZESKYX; x++)
		{
			const int idx = x + y * GRIDSIZESKYX;

			const int Aminx = x == 0 ? 0 : A[idx - 1].x; //Minus X looking at x
			const int Aminy = y == 0 ? 0 : A[idx - GRIDSIZESKYX].y; //Minus Y looking at y
			const int Aplusx = A[idx].x; //Plus x looking at x
			const int Aplusy = A[idx].y; //Plus Y looking at y
			const int Adiag = A[idx].z;

			const float Sminx = x == 0 ? 0 : s[idx - 1];
			const float Sminy = y == 0 ? 0 : s[idx - GRIDSIZESKYX];
			const float Splusx = x == GRIDSIZESKYX - 1 ? 0 : s[idx + 1];
			const float Splusy = y == GRIDSIZESKYY - 1 ? 0 : s[idx + GRIDSIZESKYX];

			z[idx] = Adiag * s[idx] +
				((Aminx * Sminx +
				Aminy * Sminy +
				Aplusx * Splusx +
				Aplusy * Splusy)
				/ Adiag);
		}
	}
}

//TODO: dt not used?
glm::vec3 environment::calculateFallingVelocity(const float , const int i, const float densAir)
{
	glm::vec3 outputVelocity{};

	//How many of this particle are in this region
	const float N0R = 8e-2f; 
	const float N0S = 3e-2f;
	const float N0I = 4e-4f;

	//Densities
	const float densW = 0.99f;
	const float densS = 0.11f;
	const float densI = 0.91f;
	const float _e = 0.25f; //E

	//constants
	const float a = 2115.0f; // TODO: b^1-b??
	const float b = 0.8f;
	const float c = 152.93f; //TODO d^1-d?
	const float d = 0.25f;
	const float CD = 0.6f;

	//Slope Parameters
	float SPR{0.0f};
	float SPS{0.0f};
	float SPI{0.0f};
	{
		//Check for division by 0
		float numerator = PI * densW * N0R;
		float denominator = densAir * static_cast<float>(m_envGrid.Qr[i]);
		SPR = (numerator == 0 || denominator == 0) ? 0.0f : pow((numerator / denominator), _e);

		numerator = PI * densW * N0S;
		denominator = densAir * static_cast<float>(m_envGrid.Qs[i]);
		SPS = (numerator == 0 || denominator == 0) ? 0.0f : pow((numerator / denominator), _e);

		numerator = PI * densW * N0I;
		denominator = densAir * static_cast<float>(m_envGrid.Qi[i]);
		SPI = (numerator == 0 || denominator == 0) ? 0.0f : pow((numerator / denominator), _e);
	}

	//Minimum and maximum particle sizes (could be tweaked)
	const float minDR = 0.01f;
	const float minDS = 0.01f;
	const float minDI = 0.1f;
	const float maxDR = 0.6f;
	const float maxDS = 0.3f;
	const float maxDI = 2.5f;
	//Stepsize (For now 10 steps hopefully works)
	const float stepR = (maxDR - minDR) * 0.1f;
	const float stepS = (maxDS - minDS) * 0.1f;
	const float stepI = (maxDI - minDI) * 0.1f;

	float weightedSumNum = 0.0;
	float weightedSumDenom = 0.0;

	//Loop over Diameters

	//Rain
	for (float D = minDR; D < maxDR; D += stepR)
	{
		const float nRD = N0R * exp(-SPR * D); //Exponential distribution (how many particles)
		const float UDR = a * pow(D, b) * sqrt(densW / densAir);
		weightedSumNum += UDR * nRD * D * stepR;
		weightedSumDenom += nRD * D * stepR;
	}
	outputVelocity.x = weightedSumNum / weightedSumDenom;
	weightedSumNum = 0;
	weightedSumDenom = 0;

	//Snow
	for (float D = minDS; D < maxDS; D += stepS)
	{
		const float nSD = N0S * exp(-SPS * D); //Exponential distribution (how many particles)
		const float UDS = c * pow(D, d) * sqrt(densS / densAir);
		weightedSumNum += UDS * nSD * D * stepS;
		weightedSumDenom += nSD * D * stepS;
	}
	outputVelocity.y = weightedSumNum / weightedSumDenom;
	weightedSumNum = 0;
	weightedSumDenom = 0;

	//Ice
	for (float D = minDI; D < maxDI; D += stepI)
	{
		const float nID = N0I * exp(-SPI * D); //Exponential distribution (how many particles)
		const float UDI = sqrt(4 / (3 * CD)) * pow(D, 0.5f) * sqrt(densI / densAir);
		weightedSumNum += UDI * nID * D * stepI;
		weightedSumDenom += nID * D * stepI;
	}
	outputVelocity.z = weightedSumNum / weightedSumDenom;

	if (outputVelocity.x != outputVelocity.x || outputVelocity.y != outputVelocity.y || outputVelocity.z != outputVelocity.z)
	{
		weightedSumNum = 0;
	}

	return outputVelocity;
}

//void environment::backTracing(const float dt, const int index, const float fallingVelocity)
//{
//	glm::vec2 currentPos = { std::floor(float(index) / float(GRIDSIZESKYX)), index % GRIDSIZESKYX };
//	glm::vec2 backPos = currentPos - glm::vec2()
//
//}
//
//float environment::trilinearSampling(const glm::vec2 position, half* array)
//{
//	glm::ivec2 pos = glm::floor(position);
//	glm::vec2 frac = position - glm::vec2(pos);
//	const int index = position.x + position.y * GRIDSIZESKYX;
//
//	//For now 2D, but can be easily expended to 3D.
//	float v00 = array[index];
//	float v10 = array[index + 1];
//	float v01 = array[index + 1 * GRIDSIZESKYX];
//	float v11 = array[index + 1 + 1 * GRIDSIZESKYX];
//
//	float v0 = bee::Lerp(v00, v10, frac.x);
//	float v1 = bee::Lerp(v01, v11, frac.x);
//
//	return bee::Lerp(v0, v1, frac.y);
//}

void environment::updateMicroPhysics(const float dt, const int i, const float* ps, const float T, const float D)
{
	//Formulas:
	// 𝐷𝑡𝑞𝑣 = 𝐸𝑊 + 𝑆𝐶 + 𝐸𝑅 + 𝐸𝐼 + 𝐸𝑆 + 𝐸𝐺 − 𝐶𝑊 − 𝐷𝐶, (9)
	// 𝐷𝑡𝑞𝑤 = 𝐶𝑊 + 𝑀𝐶 − 𝐸𝑊 − 𝐴𝑊 − 𝐾𝑊 − 𝑅𝑊 − 𝐹𝑊 − 𝐵𝑊, (10)
	// 𝐷𝑡𝑞𝑐 = 𝐷𝐶 + 𝐹𝑊 + 𝐵𝑊 − 𝑆𝐶 − 𝐴𝐶 − 𝐾𝐶 − 𝑀𝐶, (11)
	// 𝐷𝑡𝑞𝑟 = 𝐴𝑊 + 𝐾𝑊 + 𝑀𝑆 + 𝑀𝐼 − 𝐸𝑅 − 𝐹𝑅 − 𝐺𝑅, (12)
	// 𝐷𝑡𝑞𝑠 = 𝐴𝐶 + 𝐾𝐶 − 𝑀𝑆 − 𝐸𝑆 − 𝑅𝑆 − 𝐺𝑆, (13)
	// 𝐷𝑡𝑞𝑖 = 𝐹𝑅 + 𝑅𝑆 + 𝑅𝑊 − 𝐸𝐼 − 𝑀𝐼 − 𝐺𝐼, (14)

	float EW_min_CW = 0.0f; // Difference between eveperation of water vapor and condensation of water vapor
	float BW = 0.0f; // Ice growth at the cost of cloud water
	float FW = 0.0f; // Homogeneous freezing (instant freezing below -40)
	float MC = 0.0f; // Melting (ice to liquid)
	float DC_min_SC = 0.0f; // Difference between deposition of vapor to ice and sublimation of ice to vapor
	float AW = 0.0f; // Autoconversion to rain. droplets being big enough to fall
	float KW = 0.0f; // Collection of cloud water (droplets growing by taking other droplets)
	float AC = 0.0f; // Autoconversion to snow. ice forming snow
	float KC = 0.0f; // Gradually collection of cloud matter by snow
	float RW = 0.0f; // Growth of ice (precip) by hitting ice crystals and cloud liquid water
	float RS = 0.0f; // Growth of ice (precip) by hitting snow 
	float FR = 0.0f; // Growth of ice (precip) by freezing rain
	float MS = 0.0f; // Snow melting
	float MI = 0.0f; // Ice (precip) melting
	float ER = 0.0f; // Evaporation of rain
	float ES = 0.0f; // Evaporation of snow
	float EI = 0.0f; // Evaporation of ice (precip)
	float GR = 0.0f; // Rain hit ground
	float GS = 0.0f; // Snow hit ground
	float GI = 0.0f; // Ice (precip) hit ground
	float EG = 0.0f; // Evaporation of dry ground
	
	const float QWS = meteoformulas::ws((T - 273.15f), ps[i]); //Maximum water vapor air can hold
	const float QWI = meteoformulas::wi((T - 273.15f), ps[i]);

	if (T >= -40 + 273.15f)
	{
		EW_min_CW = std::min(QWS - static_cast<float>(m_envGrid.Qv[i]), static_cast<float>(m_envGrid.Qw[i]));

		if (T <= 0.0f + 273.15f)
		{
			const float a = 0.5f; // capacitance for hexagonal crystals
			const float quu = std::max(1e-12f * meteoformulas::Ni(T) / D, static_cast<float>(m_envGrid.Qi[i]));
			BW = std::min(static_cast<float>(m_envGrid.Qw[i]), pow((1 - a) * meteoformulas::cvd(T, ps[i], D) * dt + pow(quu, 1 - a), 1 / (1 - a)) - static_cast<float>(m_envGrid.Qi[i]));
		}
	}
	if (T < -40 + 273.15f)
	{
		FW = static_cast<float>(m_envGrid.Qw[i]);
	}
	if (T > 0 + 273.15f) //Melting
	{
		// 𝛿(𝑋𝑐 + 𝑋𝑠 + 𝑋𝑖) ≤ 𝑐𝑝air / 𝐿𝑓 * 𝑇, == First melt cloud ice, then snow, then precip ice, when the criteria is met between any step, stop melting.

		const float BMI{ 5e-4f };					  // Aggregation rate of ice to rain rate coefficient:

		const float check = Cpd / Lf * T;
		float heatSum = 0.0f;

		const float Qc = static_cast<float>(m_envGrid.Qc[i]);
		const float Qs = static_cast<float>(m_envGrid.Qs[i]);
		const float Qi = static_cast<float>(m_envGrid.Qi[i]);
		const float MIt = BMI * T;

		//Melt cloud ice if possible
		if (Qc > 0.0f && heatSum < check)
		{
			//Melt all or the maximum we can handle
			const float melting = std::min(Qc, check - heatSum);
			MC = melting;
			heatSum += melting;
		}
		if (Qs > 0.0f && heatSum < check)
		{
			const float melting = std::min(Qs, check - heatSum);
			MS = melting;
			heatSum += melting;
		}
		if (Qi > 0.0f && heatSum < check)
		{
			MI = std::min(MIt, std::min(Qi, check - heatSum));
		}
	}
	if (T <= 0 + 273.15f)
	{
		DC_min_SC = std::max(0.0f, std::min(QWI - static_cast<float>(m_envGrid.Qv[i]), static_cast<float>(m_envGrid.Qc[i])));
	}
	{
		// Rate coefficient found in https://journals.ametsoc.org/view/journals/apme/22/6/1520-0450_1983_022_1065_bpotsf_2_0_co_2.xml
		const float BAC = 1e-3f * exp(0.025f * T); // Aggregation rate of ice to snow rate coefficient:
		// All values below are tweakable.
		const float BAW{ 2e-3f };	//NotSure		  // Aggregation rate of liquid to rain rate coefficient:
		const float BKW{ 3e-3f };	//NotSure		  // Aggregation rate of rain hitting rain rate coefficient:
		const float BKC{ 4e-3f };	//NotSure		  // Aggregation rate of cold cloud to snow rate coefficient:
		const float BRW{ 2e-3f };	//NotSure		  // Aggregation rate of ice and cloud to ice (precip) rate coefficient:
		const float BRS{ 6e-3f };	//NotSure		  // Aggregation rate of ice and cloud to ice (precip) rate coefficient:
		const float BFR{ 1e-3f };	//NotSure		  // Aggregation rate of freezing rain to ice (precip) rate coefficient:
		const float BER{ 5e-4f };					  // Aggregation rate of rain to vapor rate coefficient:
		const float BES{ 5e-4f };					  // Aggregation rate of snow to vapor rate coefficient:
		const float BEI{ 5e-4f };					  // Aggregation rate of ice to vapor rate coefficient:
		const float BEG{ 1e-4f };					  // Evaporation rate of dry ground

		const float qwmin = 0.001f; // the minimum cloud water content required before rainmaking begins
		const float qcmin = 0.001f; // the minimum cloud ice content required before snowmaking begins

		AW = BAW * std::max(static_cast<float>(m_envGrid.Qw[i]) - qwmin, 0.0f);
		KW = BKW * static_cast<float>(m_envGrid.Qw[i]) * static_cast<float>(m_envGrid.Qr[i]);
		AC = BAC * std::max(static_cast<float>(m_envGrid.Qc[i]) - qcmin, 0.0f);
		KC = BKC * static_cast<float>(m_envGrid.Qc[i]) * static_cast<float>(m_envGrid.Qs[i]);
		RW = BRW * static_cast<float>(m_envGrid.Qi[i]) * static_cast<float>(m_envGrid.Qw[i]);
		RS = BRS * static_cast<float>(m_envGrid.Qs[i]) * static_cast<float>(m_envGrid.Qw[i]);
		if (T < -8 + 273.15f) FR = std::min(BFR * (T + 8) * (T + 8), static_cast<float>(m_envGrid.Qr[i])); //use max to not use rain if there is none

		//Constraint on how much can be evaporated (don't evap if air can't hold more)
		ER = BER * std::min(static_cast<float>(m_envGrid.Qw[i]) + static_cast<float>(m_envGrid.Qr[i]), std::max(QWS - static_cast<float>(m_envGrid.Qv[i]), 0.0f));
		ES = BES * std::min(static_cast<float>(m_envGrid.Qc[i]) + static_cast<float>(m_envGrid.Qs[i]) + static_cast<float>(m_envGrid.Qi[i]), std::max(QWI - static_cast<float>(m_envGrid.Qv[i]), 0.0f));
		EI = BEI * std::min(static_cast<float>(m_envGrid.Qc[i]) + static_cast<float>(m_envGrid.Qs[i]) + static_cast<float>(m_envGrid.Qi[i]), std::max(QWI - static_cast<float>(m_envGrid.Qv[i]), 0.0f));

		GR = static_cast<float>(m_envGrid.Qr[i]);
		GS = static_cast<float>(m_envGrid.Qs[i]);
		GI = static_cast<float>(m_envGrid.Qi[i]);
		const float D_ = 1.0f; // Weigthed mean diffusivity of the ground //TODO: add this
		const float O_ = 0.6f; // evaporative ground water storage coefficient // TODO: also this
		//Only if Qgj = 0 (Precip falling) and if we are at the ground
		if (int(float(i) / GRIDSIZEGROUND) == 0)
		{
			if (m_groundGrid.Qgr[i] == 0 && m_groundGrid.Qgs[i] == 0 && m_groundGrid.Qgi[i] == 0)
			{
				const float waterA = m_groundGrid.Qrs[i];
				const float time = m_groundGrid.t[i];
				EG = BEG * D_ * waterA * exp(-time / 86400 * O_);
			}
			else
			{
				m_groundGrid.t[i] = 0;
			}
		}
	}

	//𝐷𝑡𝑞𝑣 = 𝐸𝑊 + 𝑆𝐶 + 𝐸𝑅 + 𝐸𝐼 + 𝐸𝑆 + 𝐸𝐺 − 𝐶𝑊 − 𝐷𝐶, (9)
	//𝐷𝑡𝑞𝑤 = 𝐶𝑊 + 𝑀𝐶 − 𝐸𝑊 − 𝐴𝑊 − 𝐾𝑊 − 𝑅𝑊 − 𝐹𝑊 − 𝐵𝑊, (10)
	//𝐷𝑡𝑞𝑐 = 𝐷𝐶 + 𝐹𝑊 + 𝐵𝑊 − 𝑆𝐶 − 𝐴𝐶 − 𝐾𝐶 − 𝑀𝐶, (11)
	//𝐷𝑡𝑞𝑟 = 𝐴𝑊 + 𝐾𝑊 + 𝑀𝑆 + 𝑀𝐼 − 𝐸𝑅 − 𝐹𝑅 − 𝐺𝑅, (12)
	//𝐷𝑡𝑞𝑠 = 𝐴𝐶 + 𝐾𝐶 − 𝑀𝑆 − 𝐸𝑆 − 𝑅𝑆 − 𝐺𝑆, (13)
	//𝐷𝑡𝑞𝑖 = 𝐹𝑅 + 𝑅𝑆 + 𝑅𝑊 − 𝐸𝐼 − 𝑀𝐼 − 𝐺𝐼

	//TODO: check if everything is right
	m_envGrid.Qv[i] += dt * (EW_min_CW + DC_min_SC + ER + EI + ES + EG);
	m_envGrid.Qw[i] += dt * (-EW_min_CW + MC - AW - KW - RW - FW - BW);
	m_envGrid.Qc[i] += dt * (DC_min_SC + FW + BW - AC - KC - MC);
	m_envGrid.Qr[i] += dt * (AW + KW + MS + MI - ER - FR - GR);
	m_envGrid.Qs[i] += dt * (AC + KC - MS - ES - RS - GS);
	m_envGrid.Qi[i] += dt * (FR + RS + RW - EI - MI - GI);

	if (m_envGrid.Qv[i] < 0 || m_envGrid.Qw[i] < 0 || m_envGrid.Qc[i] < 0 || m_envGrid.Qr[i] < 0 || m_envGrid.Qs[i] < 0 || m_envGrid.Qi[i] < 0 || 
		m_envGrid.Qv[i] != m_envGrid.Qv[i] || m_envGrid.Qw[i] != m_envGrid.Qw[i] || m_envGrid.Qc[i] != m_envGrid.Qc[i] || m_envGrid.Qr[i] != m_envGrid.Qr[i] || m_envGrid.Qs[i] != m_envGrid.Qs[i] || m_envGrid.Qi[i] != m_envGrid.Qi[i])
	{
		AW = 0;
		//MI = 0.007 meaning Qi will become negative
	}

}

float environment::calculateSumPhaseHeat(const int i, const float T, const float pQv, const float pQw, const float pQc, const float pQr, const float pQs, const float pQi)
{
	float ratio = T / m_envGrid.potTemp[i];
	float sumPhaseheat = 0.0f;

	float dQv = static_cast<float>(m_envGrid.Qv[i]) - pQv;
	float dQw = static_cast<float>(m_envGrid.Qw[i]) - pQw;
	float dQc = static_cast<float>(m_envGrid.Qc[i]) - pQc;
	float dQr = static_cast<float>(m_envGrid.Qr[i]) - pQr;
	float dQs = static_cast<float>(m_envGrid.Qs[i]) - pQs;
	float dQi = static_cast<float>(m_envGrid.Qi[i]) - pQi;
	dQv = dQv == 0 ? 0 : dQv / (1 + dQv);
	dQw = dQw == 0 ? 0 : dQw / (1 + dQw);
	dQc = dQc == 0 ? 0 : dQc / (1 + dQc);
	dQr = dQr == 0 ? 0 : dQr / (1 + dQr);
	dQs = dQs == 0 ? 0 : dQs / (1 + dQs);
	dQi = dQi == 0 ? 0 : dQi / (1 + dQi);

	const float invCpdRatio = 1.0f / (Cpd * ratio);

	sumPhaseheat += E0v * invCpdRatio * dQv;
	sumPhaseheat += Lf * invCpdRatio * dQw;
	sumPhaseheat += Ls * invCpdRatio * dQc;
	sumPhaseheat += Ls * invCpdRatio * dQr;
	sumPhaseheat += Ls * invCpdRatio * dQs;
	sumPhaseheat += Lf * invCpdRatio * dQi;
	return sumPhaseheat;
}

void environment::computeHeatTransfer(const float dt, const int i, const float sumHeat)
{
	//𝜕𝜃 / 𝜕𝑡 = - (𝒖 · ∇)𝜃 + for(𝑎) {𝐿𝑎 /𝑐𝑝Π * 𝑋𝑗

	//const int pY = i - 1 * GRIDSIZESKYY;
	//const int nY = i + 1 * GRIDSIZESKYY;
	//const int pX = i - 1;
	//const int nX = i + 1; 
	const int pX = (i % GRIDSIZESKYX == 0) ? i : i - 1;
	const int nX = ((i + 1) % GRIDSIZESKYX == 0) ? i : i + 1;
	const int pY = (i / GRIDSIZESKYX == 0) ? i : i - GRIDSIZESKYX;
	const int nY = (i / GRIDSIZESKYX == GRIDSIZESKYY - 1) ? i : i + GRIDSIZESKYX;

	const glm::vec2 nabla0 = //∇0
	{
		(m_envGrid.potTemp[nX] - m_envGrid.potTemp[pX]) / (2.0f * float(VOXELSIZE)),
		(m_envGrid.potTemp[nY] - m_envGrid.potTemp[pY]) / (2.0f * float(VOXELSIZE))
	};

	//directional derivative
	const float DirDer = m_envGrid.velField[i].x * nabla0.x + m_envGrid.velField[i].y * nabla0.y;

	m_envGrid.potTemp[i] += dt * (-DirDer + sumHeat);

	if (m_envGrid.potTemp[i] != m_envGrid.potTemp[i] || m_envGrid.potTemp[i] < 0)
	{
		m_envGrid.potTemp[i] = 0;
	}
}

glm::vec2 environment::getUV(const int i)
{
	//Casual avaraging
	const int x = i % GRIDSIZESKYX;
	const float u = m_envGrid.velField[i].x;
	const float v = m_envGrid.velField[i].y;
	const float Pu = x == 0 ? m_envGrid.velField[i].x : m_envGrid.velField[i - 1].x;
	const float Pv = i - GRIDSIZESKYX < 0 ? 0.0f : m_envGrid.velField[i - GRIDSIZESKYX].y; //TODO: do we use 0 or Neumann?

	return glm::vec2((Pu + u) / 2, (Pv + v) / 2);
}

float environment::getIsentropicTemp(const int i)
{
	int x = i % GRIDSIZESKYX;
	//Map to closest valid value
	x = x < 0 ? 0 : x >= GRIDSIZESKYX ? GRIDSIZESKYX - 1 : x;


	//We just get the avarage from 2000 and 5000 meters.
	int index2 = x + int(2000 / VOXELSIZE) * GRIDSIZESKYX;
	int index5 = x + int(5000 / VOXELSIZE) * GRIDSIZESKYX;
	float difference = 3;
	//Except when that is invalid
	if (int(index5 / GRIDSIZESKYX) > GRIDSIZESKYY)
	{
		//Just use variable height based on current max height
		index2 = x + int(float(GRIDSIZESKYY) * 0.2f) * GRIDSIZESKYX;
		index5 = x + int(float(GRIDSIZESKYY) * 0.5f) * GRIDSIZESKYX;
		difference = float(index5) / GRIDSIZESKYX * VOXELSIZE - float(index2) / GRIDSIZESKYX * VOXELSIZE;
		difference *= 0.001f;
	}
	const float T2 = m_envGrid.potTemp[index2] * glm::pow(m_pressures[index2] / m_groundGrid.P[index2 % GRIDSIZESKYX], Rsd / Cpd);
	const float T5 = m_envGrid.potTemp[index5] * glm::pow(m_pressures[index5] / m_groundGrid.P[index5 % GRIDSIZESKYX], Rsd / Cpd);

	const float lapseRate = (T5 - T2) / difference;

	return m_groundGrid.T[x] + (int(float(i) / GRIDSIZESKYX) * VOXELSIZE) * 0.001f * lapseRate;
}

bool environment::outside(const int i)
{
	const int x = i % GRIDSIZESKYX;
	const int y = i / GRIDSIZESKYX;
	return (x < 0 || x >= GRIDSIZESKYX || y < 0 || y >= GRIDSIZESKYY);
}

void environment::setDebugArray(std::vector<float>& s, const int i)
{
	switch (i)
	{
	case 0:
		memcpy(m_debugArray0, s.data(), s.size() * sizeof(float));
		break;
	case 1:
		memcpy(m_debugArray1, s.data(), s.size() * sizeof(float));
		break;
	case 2:
		memcpy(m_debugArray2, s.data(), s.size() * sizeof(float));
		break;
	default:
		break;
	}
}

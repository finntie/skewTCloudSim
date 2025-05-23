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

	//Step forwards and step backwards (IMPORTANT: step backwards could cause inaccurancies)
	if (ImGui::Button(ICON_FA_ARROW_LEFT) || bee::Engine.Input().GetKeyboardKey(bee::Input::KeyboardKey::ArrowLeft))
	{
		simulationStep = -1;
	}
	if (ImGui::Button(ICON_FA_ARROW_RIGHT) || bee::Engine.Input().GetKeyboardKey(bee::Input::KeyboardKey::ArrowRight))
	{
		simulationStep = 1;
	}
	ImGui::SameLine();


	//Information
	ImGui::Text(std::string("X: " + std::to_string(mousePointingIndex % GRIDSIZESKYX) + ", Y: " + std::to_string(int(float(mousePointingIndex) / GRIDSIZESKYX))).c_str());
	ImGui::Text((std::string("Pot Temp: ") + std::to_string(m_envGrid.potTemp[mousePointingIndex])).c_str());
	ImGui::Text((std::string("Qv: ") + std::to_string(m_envGrid.Qv[mousePointingIndex])).c_str());
	ImGui::Text((std::string("Qw: ") + std::to_string(m_envGrid.Qw[mousePointingIndex])).c_str());
	ImGui::Text((std::string("Qc: ") + std::to_string(m_envGrid.Qc[mousePointingIndex])).c_str());
	ImGui::Text((std::string("Qr: ") + std::to_string(m_envGrid.Qr[mousePointingIndex])).c_str());
	ImGui::Text((std::string("Qs: ") + std::to_string(m_envGrid.Qs[mousePointingIndex])).c_str());
	ImGui::Text((std::string("Qi: ") + std::to_string(m_envGrid.Qi[mousePointingIndex])).c_str());
	ImGui::Text((std::string("Wind: ") + std::to_string(m_envGrid.velField[mousePointingIndex].x) + ", " + std::to_string(m_envGrid.velField[mousePointingIndex].y)).c_str());

	if (ImGui::TreeNode("Sky"))
	{
		ImGui::RadioButton("View Temps", &debugViewSky, 0);
		ImGui::RadioButton("View MR Qv", &debugViewSky, 1);
		ImGui::RadioButton("View MR Qw", &debugViewSky, 2);
		ImGui::RadioButton("View MR Qc", &debugViewSky, 3);
		ImGui::RadioButton("View MR Qr", &debugViewSky, 4);
		ImGui::RadioButton("View MR Qs", &debugViewSky, 5);
		ImGui::RadioButton("View MR Qi", &debugViewSky, 6);
		ImGui::RadioButton("View Wind Speed and Dir", &debugViewSky, 7);
		ImGui::TreePop();
	}
	if (ImGui::TreeNode("Ground"))
	{
		ImGui::RadioButton("View Temps", &debugViewGround, 0);
		ImGui::RadioButton("View Sub-surface water", &debugViewGround, 1);
		ImGui::RadioButton("View MR Qr", &debugViewGround, 2);
		ImGui::RadioButton("View MR Qs", &debugViewGround, 3);
		ImGui::RadioButton("View MR Qi", &debugViewGround, 4);
		ImGui::TreePop();
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
}

environment::~environment()
{

}

void environment::init(float* potTemps, glm::vec2* velField, half_float::half* Qv, float* groundTemp, half_float::half* groundPres)
{
	//TODO: memcpy broken for smart pointers?
	memcpy(m_envGrid.potTemp, potTemps, GRIDSIZESKY * sizeof(float));
	memcpy(m_envGrid.velField, velField, GRIDSIZESKY * sizeof(glm::vec2));
	memcpy(m_envGrid.Qv, Qv, GRIDSIZESKY * sizeof(half_float::half));

	memcpy(m_groundGrid.T, groundTemp, GRIDSIZEGROUND * sizeof(float));
	memcpy(m_groundGrid.P, groundPres, GRIDSIZEGROUND * sizeof(half_float::half));

}




//|-----------------------------------------------------------------------------------------------------------|
//|                                                  Code                                                     |
//|-----------------------------------------------------------------------------------------------------------|

using namespace half_float;
using namespace Constants;

void environment::DebugRender()
{
	auto& colorScheme = bee::Engine.DebugRenderer().GetColorScheme();

	for (float y = 0; y < GRIDSIZESKYY; y++)
	{
		for (float x = 0; x < GRIDSIZESKYX; x++)
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
				colorScheme.getColor("velField", glm::length(m_envGrid.velField[int(x) + int(y) * GRIDSIZESKYX]), color);
				bee::Engine.DebugRenderer().AddArrow(bee::DebugCategory::All, glm::vec3(x * 0.5f, y * 0.5f, 0.1f), glm::vec3(0.0f, 0.0f, 1.0f), m_envGrid.velField[int(x) + int(y) * GRIDSIZESKYX], 0.4f, bee::Colors::Black);
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

			bee::Engine.DebugRenderer().AddSquare(bee::DebugCategory::All, glm::vec3(x * 0.5f, y  * 0.5f, 0.0f), 1.0f, glm::vec3(0, 0, 1), bee::Colors::White);
			bee::Engine.DebugRenderer().AddFilledSquare(bee::DebugCategory::All, glm::vec3(x * 0.5f, y  * 0.5f, 0.01f), 0.9f * 0.95f, glm::vec3(0, 0, 1), { color, 1.0f });

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

		bee::Engine.DebugRenderer().AddSquare(bee::DebugCategory::All, glm::vec3(x * 0.5f, -0.5f, 0.0f), 1.0f, glm::vec3(0, 0, 1), bee::Colors::White);
		bee::Engine.DebugRenderer().AddFilledSquare(bee::DebugCategory::All, glm::vec3(x * 0.5f, -0.5f, 0.01f), 0.9f * 0.95f, glm::vec3(0, 0, 1), { color, 1.0f });

	}

	//Mouse pos debugging
	{
		glm::vec3 MousePos3D = bee::screenToGround(bee::Engine.Input().GetMousePosition());
		MousePos3D *= 2;
		if (int(MousePos3D.x) < GRIDSIZESKYX && int(MousePos3D.x) >= 0 && int(MousePos3D.y) < GRIDSIZESKYY && int(MousePos3D.y) >= 0)
		{
			mousePointingIndex = (int(MousePos3D.x) + int(MousePos3D.y) * GRIDSIZESKYX);
		}
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
	for (int loop = 0; loop < 2; loop++)
	{
		for (int i = 0; i < GRIDSIZESKY; i++)
		{
			// we want not to update neighbouring cells, so we do the Red-Black Gauss-Seidel tactic
			if ((i + loop + int(float(i) / GRIDSIZESKYY)) % 2 == 0) continue;

			// 1. Update Sky Temps
			// 2. Diffuse and Advect potential temp
			// 3. Diffuse, Advect and pressure project velocity field
			// 4. Diffuse and Advect water content qj
			// 5. water transfer in ground and vorticity confinement. 
			// 6. Update microphysics
			// 7. Compute heat transfer (form. 67)

			// 1.
			const float T = m_envGrid.potTemp[i] * glm::pow(m_pressures[i] / m_groundGrid.P[i % GRIDSIZESKYX], Rsd / Cpd);

			// 2.
			diffuseAndAdvect(dt, i, m_envGrid.potTemp);

			// 3.
			const float Tv = T * (0.608f * m_envGrid.Qv[i] + 1); //TODO: this becomes negative
			const float density = m_pressures[i] * 100 / (Rsd * Tv); //Convert Pha to Pa
			const float B = calculateBuoyancy(i, m_pressures, T);
			const glm::vec2 F = vorticityConfinement(i);
			updateVelocityField(dt, i, m_pressures, density, B, F);

			// 4.
			glm::vec3 fallVelocitiesPrecip = calculateFallingVelocity(dt, i, density);
			diffuseAndAdvect(dt, i, m_envGrid.Qv, true);
			diffuseAndAdvect(dt, i, m_envGrid.Qw);
			diffuseAndAdvect(dt, i, m_envGrid.Qc);
			diffuseAndAdvect(dt, i, m_envGrid.Qr, false, fallVelocitiesPrecip.x);
			diffuseAndAdvect(dt, i, m_envGrid.Qs, false, fallVelocitiesPrecip.y);
			diffuseAndAdvect(dt, i, m_envGrid.Qi, false, fallVelocitiesPrecip.z);

			// 6.
			const float pQv = m_envGrid.Qv[i], pQw = m_envGrid.Qw[i], pQc = m_envGrid.Qc[i], pQr = m_envGrid.Qr[i], pQs = m_envGrid.Qs[i], pQi = m_envGrid.Qi[i];
			updateMicroPhysics(dt, i, m_pressures, T, density);

			// 7.
			const float sumPhaseHeat = calculateSumPhaseHeat(i, T, pQv, pQw, pQc, pQr, pQs, pQi);
			computeHeatTransfer(dt, i, sumPhaseHeat);

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
	const float groundThickness = GRIDSIZEGROUND;
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

using namespace Constants;

void environment::updateVelocityField(const float dt, const int i, const float* ps, const float D, const float B, const glm::vec2 F)
{
	//u[t+1] = u + deltaTime * ( -(u * ∇)u - (1/ρ)∇p + v*(∇^2u) + b + f ); 
	// Or without pressure projection which we will do first:
	//u* = u + deltaTime * ( -(u * ∇)u + v*(∇^2u) + b + f ); 

	const float viscocity{ 1e-5f };
	//const int pY{ i - 1 * GRIDSIZESKYY };
	//const int nY{ i + 1 * GRIDSIZESKYY };
	//const int pX{ i - 1 };
	//const int nX{ i + 1 };
	const int pX = (i % GRIDSIZESKYX == 0) ? i : i - 1;
	const int nX = ((i + 1) % GRIDSIZESKYX == 0) ? i : i + 1; 
	const int pY = (i / GRIDSIZESKYX == 0) ? i : i - GRIDSIZESKYX;
	const int nY = (i / GRIDSIZESKYX == GRIDSIZESKYY - 1) ? i : i + GRIDSIZESKYX; 


	const glm::vec2 nablaUu = //∇Uu
	{
		(m_envGrid.velField[nX].x - m_envGrid.velField[pX].x) / (2.0f * float(VOXELSIZE)),
		(m_envGrid.velField[nY].x - m_envGrid.velField[pY].x) / (2.0f * float(VOXELSIZE))
	};
	const glm::vec2 nablaUv = //∇Uv
	{
		(m_envGrid.velField[nX].y - m_envGrid.velField[pX].y) / (2.0f * float(VOXELSIZE)),
		(m_envGrid.velField[nY].y - m_envGrid.velField[pY].y) / (2.0f * float(VOXELSIZE))
	};

	const glm::vec2 DirDer = //(u * ∇)u
	{
		m_envGrid.velField[i].x * nablaUu.x + m_envGrid.velField[i].x * nablaUu.y,
		m_envGrid.velField[i].y * nablaUv.x + m_envGrid.velField[i].y * nablaUv.y
	};

	const glm::vec2 nablaP = //∇p
	{
		(ps[nX] - ps[pX]) / (2.0f * float(VOXELSIZE)),
		(ps[nY] - ps[pY]) / (2.0f * float(VOXELSIZE))
	};

	const glm::vec2 nablaU2 = //∇u^2
	{
		(m_envGrid.velField[nX] - (2.0f * m_envGrid.velField[i]) + m_envGrid.velField[pX]) / (float(VOXELSIZE) * float(VOXELSIZE)) +
		(m_envGrid.velField[nY] - (2.0f * m_envGrid.velField[i]) + m_envGrid.velField[pY]) / (float(VOXELSIZE) * float(VOXELSIZE))
	};


	//	New value		=     Old Value		  +		timestep *	 movement of air	 -     pressure gradient    +	 smoothing	    + buoyancy + extra forces
	m_envGrid.velField[i] = m_envGrid.velField[i] +    dt    *   (  - DirDer		   -   (1 / D) * nablaP + viscocity * nablaU2 +	  B	   +     F);

	//TODO: ? Enforce incompressibility:
	//∇²p = (1/Δt) * ∇·u


	if (m_envGrid.velField[i] != m_envGrid.velField[i] || m_envGrid.velField[i].x > 10 || m_envGrid.velField[i].y > 10 || m_envGrid.velField[i].x < -10 || m_envGrid.velField[i].y < -10)
	{
		m_envGrid.velField[i].x = 0;
	}

}

void environment::diffuseAndAdvect(const float dt, const int i, float* array)
{
	// Diffuse
	const float viscocity = 2.2e-5f;

	//const int pY{ i - 1 * GRIDSIZESKYY };
	//const int nY{ i + 1 * GRIDSIZESKYY };
	//const int pX{ i - 1 };
	//const int nX{ i + 1 };
	const int pX = (i % GRIDSIZESKYX == 0) ? i : i - 1;
	const int nX = ((i + 1) % GRIDSIZESKYX == 0) ? i : i + 1;
	const int pY = (i / GRIDSIZESKYX == 0) ? i : i - GRIDSIZESKYX;
	const int nY = (i / GRIDSIZESKYX == GRIDSIZESKYY - 1) ? i : i + GRIDSIZESKYX;

	const float nabla02 = //∇θ^2
	{
		(array[nX] - (2.0f * array[i]) + array[pX]) / (float(VOXELSIZE) * float(VOXELSIZE)) +
		(array[nY] - (2.0f * array[i]) + array[pY]) / (float(VOXELSIZE) * float(VOXELSIZE))
	};

	array[i] = array[i] - dt * viscocity * nabla02;


	//Advect
	const glm::vec2 nabla0 = //∇0
	{
		(array[nX] - array[pX]) / (2.0f * float(VOXELSIZE)),
		(array[nY] - array[pY]) / (2.0f * float(VOXELSIZE))
	};

	const float dot = glm::dot(m_envGrid.velField[i], nabla0);
	array[i] += dt * dot;

	if (array[i] != array[i] || array[i] < 0)
	{
		array[i] = 0;
	}
}

void environment::diffuseAndAdvect(const float dt, const int i, half* array, bool vapor, const float falVel)
{
	// Diffuse
	const float viscocity = 2.2e-5f;

	const int pX = i - 1;
	const int nX = i + 1;
	const int pY = i - GRIDSIZESKYX;
	const int nY = i + GRIDSIZESKYX;

	const bool atLeft = i % GRIDSIZESKYX == 0;
	const bool atRight = (i + 1) % GRIDSIZESKYX == 0;
	const bool atTop = int(i / GRIDSIZESKYX) == 0;
	const bool atBottom = int(i / GRIDSIZESKYX) == GRIDSIZESKYY - 1;

	float left = 0.0f;
	float right = 0.0f;
	float up = 0.0f;
	float down = 0.0f;

	if (vapor)
	{
		left= atLeft ? 0 : static_cast<float>(array[pX]);
		right = atRight ? 0 : static_cast<float>(array[nX]);
		up= atTop ? 0 : static_cast<float>(array[pY]);
		down= atBottom ? 0 : static_cast<float>(array[nY]);
	}
	else //If vapor, we wrap the edges
	{
		left = atLeft ? static_cast<float>(array[i - 1 + GRIDSIZESKYX]) : static_cast<float>(array[pX]);
		right = atRight ? static_cast<float>(array[i + 1 - GRIDSIZESKYX]) : static_cast<float>(array[pX]);
		up = atTop ? 0 : static_cast<float>(array[pY]);
		down = atBottom ? 0 : static_cast<float>(array[nY]);
	}

	const float nabla02 = //∇θ^2
	{
		(right - (2.0f * array[i]) + left) / (float(VOXELSIZE) * float(VOXELSIZE)) +
		(down - (2.0f * array[i]) + up) / (float(VOXELSIZE) * float(VOXELSIZE))
	};

	array[i] += dt * viscocity * nabla02;


	//Advect
	const glm::vec2 nabla0 = //∇0
	{
		(right - left) / (2.0f * float(VOXELSIZE)),
		(down - up) / (2.0f * float(VOXELSIZE))
	};

	//TODO: - or + (is falVel positive or negative?)
	const float verticalVelocity = m_envGrid.velField[i].y - falVel;
	const float advection = m_envGrid.velField[i].x * nabla0.x + verticalVelocity * nabla0.y;
	//const float dot = glm::dot(m_envGrid.velField[i] - glm::vec2(0.0f, falVel), nabla0);

	array[i] += dt * advection;

	if (array[i] != array[i] || array[i] < 0)
	{
		array[i] = 0;
	}
}

glm::vec2 environment::vorticityConfinement(const int i)
{
	const int pX = (i % GRIDSIZESKYX == 0) ? i : i - 1;
	const int nX = ((i + 1) % GRIDSIZESKYX == 0) ? i : i + 1;
	const int pY = (i / GRIDSIZESKYX == 0) ? i : i - GRIDSIZESKYX;
	const int nY = (i / GRIDSIZESKYX == GRIDSIZESKYY - 1) ? i : i + GRIDSIZESKYX;

	//TODO: in 3D this part is different
	const glm::vec2 nablaVector = glm::normalize(glm::vec2((eta(nX) - eta(pX)) / (2.0f * float(VOXELSIZE)),
														   (eta(nY) - eta(pY)) / (2.0f * float(VOXELSIZE))));

	return -nablaVector * eta(i, true);
}

float environment::eta(const int i, bool raw)
{
	const int pX = (i % GRIDSIZESKYX == 0) ? i : i - 1;
	const int nX = ((i + 1) % GRIDSIZESKYX == 0) ? i : i + 1;
	const int pY = (i / GRIDSIZESKYX == 0) ? i : i - GRIDSIZESKYX;
	const int nY = (i / GRIDSIZESKYX == GRIDSIZESKYY - 1) ? i : i + GRIDSIZESKYX;

	const float numerator = (m_envGrid.velField[nX].y - m_envGrid.velField[pX].y);
	const float numerator2 = (m_envGrid.velField[nY].x - m_envGrid.velField[pY].x);
	const float denominator = 1.0f / (2.0f * float(VOXELSIZE));

	//TODO: in 3D this part is different
	const float nablaVelocity = (numerator - numerator2) * denominator;

	if (raw) return nablaVelocity;
	return -abs(nablaVelocity);
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

float environment::calculateBuoyancy(const int i, const float* ps, const float T)
{
	const float moleFrac = m_envGrid.Qv[i] / (m_envGrid.Qv[i] + 1);
	const float avMolarMass = moleFrac * Mw + (1 - moleFrac) * Mda;
	const float massFrac = moleFrac * (Mw / avMolarMass);

	const float isenExpAir = 1.4f;
	const float isenExpVapor = 1.33f;

	const float isenExponent = massFrac * isenExpVapor + (1 - massFrac) * isenExpAir;

	const float dT = m_groundGrid.T[i % GRIDSIZESKYX] * static_cast<float>(glm::pow(ps[i] / m_groundGrid.P[i % GRIDSIZESKYX], static_cast<float>(glm::pow(isenExponent, -2))));

	return g * ((Mda / avMolarMass) * (dT / T) - 1);
}

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
	
	const float QWS = meteoformulas::ws((T - 273.15f), ps[i]);
	const float QWI = meteoformulas::wi((T - 273.15f), ps[i]);

	if (T >= -40 + 273.15f)
	{
		EW_min_CW = std::max(0.0f, std::min(QWS - static_cast<float>(m_envGrid.Qv[i]), static_cast<float>(m_envGrid.Qw[i])));

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
		const float O_ = 1.0f; // evaporative ground water storage coefficient // TODO: also this
		//Only if Qgj = 0 (Precip falling) and if we are at the ground
		if (int(float(i) / GRIDSIZEGROUND) == 0)
		{
			if (m_groundGrid.Qgr[i] == 0 && m_groundGrid.Qgs[i] == 0 && m_groundGrid.Qgi[i] == 0)
			{
				EG = BEG * BEG * D_ * m_groundGrid.Qrs[i] * exp(-m_groundGrid.t[i] / 86400 * O_);
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
	m_envGrid.Qw[i] += dt * (EW_min_CW + MC - AW - KW - RW - FW - BW);
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

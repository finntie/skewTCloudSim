#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>


#include "environment.h"

#include "editor.h"
#include "skewTer.h"
#include "microPhys.h"
#include "math/meteoformulas.h"
#include "math/constants.hpp"
#include "core/engine.hpp"
#include "imgui/IconsFontAwesome.h"
#include "math/math.hpp"
#include "rendering/colors.hpp"

#include "math/geometry.hpp"
#include "rendering/debug_render.hpp"
#include "core/input.hpp"
#include "tools/log.hpp"
#include "imgui/imgui.h"
#include "game.h"


//|-----------------------------------------------------------------------------------------------------------|
//|                                                 ImGui                                                     |
//|-----------------------------------------------------------------------------------------------------------|


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
}

environment::~environment()
{

}

void environment::init(double* potTemps, glm::vec2* velField, float* Qv, double* groundTemp, float* groundPres)
{
	memcpy(m_envGrid.potTemp, potTemps, GRIDSIZESKY * sizeof(double));
	memcpy(m_envGrid.velField, velField, GRIDSIZESKY * sizeof(glm::vec2));
	memcpy(m_envGrid.Qv, Qv, GRIDSIZESKY * sizeof(float));

	memcpy(m_groundGrid.T, groundTemp, GRIDSIZEGROUND * sizeof(double));
	memcpy(m_groundGrid.P, groundPres, GRIDSIZEGROUND * sizeof(float));


	//Init Isentropic temps
	for (int i = 0; i < GRIDSIZESKYY; i++)
	{
		m_isenTropicTemps[i] = float(potTemps[i * GRIDSIZESKYX]);
		m_isenTropicVapor[i] = float(Qv[i * GRIDSIZESKYX]);
	}

	//Init ground
	float noise[GRIDSIZEGROUND];
	bee::PNoise1D(100, noise, GRIDSIZEGROUND, GRIDSIZEGROUND);
	for (int i = 0; i < GRIDSIZEGROUND; i++)
	{
		const float maxHeight = 0;
		m_GHeight[i] = static_cast<int>(std::round(noise[i] * maxHeight));
	}

	Game.Editor().setIsentropics(m_isenTropicTemps, m_isenTropicVapor);

	//Reset values that are in ground
	for (int x = 0; x < GRIDSIZEGROUND; x++)
	{
		for (int y = 0; y <= m_GHeight[x]; y++)
		{
			m_envGrid.potTemp[x + y * GRIDSIZESKYX] = 0.0f;
			m_envGrid.Qv[x + y * GRIDSIZESKYX] = 0.0f;
			m_envGrid.Qw[x + y * GRIDSIZESKYX] = 0.0f;
			m_envGrid.Qc[x + y * GRIDSIZESKYX] = 0.0f;
			m_envGrid.Qr[x + y * GRIDSIZESKYX] = 0.0f;
			m_envGrid.Qs[x + y * GRIDSIZESKYX] = 0.0f;
			m_envGrid.Qi[x + y * GRIDSIZESKYX] = 0.0f;
			m_envGrid.velField[x + y * GRIDSIZESKYX] = { 0.0f, 0.0f };
		}
	}

	computeNeighArray();
}




//|-----------------------------------------------------------------------------------------------------------|
//|                                                  Code                                                     |
//|-----------------------------------------------------------------------------------------------------------|

using namespace half_float;
using namespace Constants;

bool environment::EditorData()
{
	m_speed = Game.Editor().getSpeed();
	if (Game.Editor().changedGround())
	{
		computeNeighArray();
	}

	//Play data
	if (!Game.Editor().getSimulate())
	{
		int step = Game.Editor().getStep();
		if (step > 0) Game.Editor().setStep(--step);
		else if (step < 0)
		{
			Game.Editor().setStep(++step);
			m_speed *= -1;
		}
		else return false;
	}

	return true;
}

void environment::Update(float dt)
{
	if (!EditorData()) return;


	// 1. Update total incoming solar radiation 
	//Avarage solar irradiance: https://globalsolaratlas.info/map?c=51.793328,5.633017,9&r=NLD
	float avarageIrradiance = 1150.0f; // W/m2
	float Irradiance = avarageIrradiance * std::max(std::sin(PI / 2 * (1 - ((m_time - m_hourOfSunrise - m_dayLightDuration / 2) / (m_dayLightDuration / 2)))), 0.0f);
	
	
	for (int s = 0; s < 1; s++) //Speed in cost of fps.
	{

		// Fill Pressures
		for (int i = 0; i < GRIDSIZESKY; i++)
		{
			const float height = std::floorf(float(i) / GRIDSIZESKYX) * VOXELSIZE;
			m_pressures[i] = meteoformulas::getStandardPressureAtHeight(float(m_groundGrid.T[i % GRIDSIZESKYX] - 273.15f), height);
		}

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
			LC;
			Irradiance;
			// 2.
			updateGroundTemps(dt, i, Irradiance, LC);

			// 3. and 4.
			const float pQgr = (m_groundGrid.Qgr[i]), pQgs = (m_groundGrid.Qgs[i]), pQgi = (m_groundGrid.Qgi[i]);
			updateMicroPhysicsGround(dt, i);

			// 5.
			m_groundGrid.T[i] += dt * calculateSumPhaseHeatGround(i, pQgr, pQgs, pQgi);
		}

		// Update sky
		{
			// --	Own Loops	--
			// 1. Diffuse and Advect potential temp

			// --	Own Loops	--
			// 2. Add Forces, Diffuse, Advect and Pressure Project velocity field

			// --	Own Loops	--
			// 3. Diffuse and Advect water content of Qj
			
			// --	Loop	--
			// 4. Regain Sky Temps and density
			// 5. Update microphysics
			// 6. Compute heat transfer (form. 67)

			//------DEBUG--------
			std::vector<float> debugVector;
			std::vector<float> debugVector2;
			std::vector<float> debugVector3;
			debugVector.resize(GRIDSIZESKY);
			debugVector2.resize(GRIDSIZESKY);
			debugVector3.resize(GRIDSIZESKY);


			// 1.
			//pressureProjectVelField(dt);
			calculateDivergence(debugVector3);
			diffuseAndAdvectTemp(m_speed * dt, m_envGrid.potTemp);



			// 2.
			updateVelocityField(dt);

			calculateDivergence(debugVector2);


			// 3.
			diffuseAndAdvect(m_speed * dt, m_envGrid.Qv, true);
			diffuseAndAdvect(m_speed * dt, m_envGrid.Qw);
			diffuseAndAdvect(m_speed * dt, m_envGrid.Qc);
			diffuseAndAdvect(m_speed * dt, m_envGrid.Qr, false, 1);
			diffuseAndAdvect(m_speed * dt, m_envGrid.Qs, false, 2);
			diffuseAndAdvect(m_speed * dt, m_envGrid.Qi, false, 3);

			float totalMass = 0.0f;
			for (int i = 0; i < GRIDSIZESKY; i++)
			{
				totalMass += m_envGrid.Qw[i];
			}
			printf("Mass: %f\n", totalMass);
			totalMass = 0.0f;

			// --	Loop 2.	--
			for (int loop = 0; loop < 2; loop++)
			{
				for (int i = 0; i < GRIDSIZESKY; i++)
				{
					// we want not to update neighbouring cells, so we do the Red-Black Gauss-Seidel tactic
					if ((i + loop + int(float(i) / GRIDSIZESKYX)) % 2 == 0 || isGround(i)) continue;

					float T22 = 255;
					if (m_envGrid.Qi[i] > 0.0f && i / GRIDSIZESKYX > 16)
					{
						T22 = 273.15f - 31.0f; //Check
					}

					// 4.
					const float T = float(m_envGrid.potTemp[i]) * glm::pow(m_pressures[i] / m_groundGrid.P[i % GRIDSIZESKYX], Rsd / Cpd);
					const float Tv = T * (0.608f * m_envGrid.Qv[i] + 1);
					const float density = m_pressures[i] * 100 / (Rsd * Tv); //Convert Pha to Pa

					//Debug values
					//const float QWS = meteoformulas::ws((T - 273.15f), m_pressures[i]); //Maximum water vapor air can hold
					//debugVector[i] = QWS - m_envGrid.Qv[i];
					//
					//const float T2 = float(m_envGrid.potTemp[i + GRIDSIZESKYX]) * glm::pow(m_pressures[i + GRIDSIZESKYX] / m_groundGrid.P[i % GRIDSIZESKYX], Rsd / Cpd);
					//debugVector2[i] = (m_envGrid.Qv[i] - meteoformulas::ws((T2 - 273.15f), m_pressures[i + GRIDSIZESKYX]));

					// 5.
					updateMicroPhysics2(dt, i, m_pressures, T, density);

					debugVector[i] = m_condens * 1000;
					//debugVector2[i] = m_freeze * 1000;
					//debugVector3[i] = m_depos * 1000;

					// 6.
					const float sumPhaseHeat = calculateSumPhaseHeat(dt, i, T);
					computeHeatTransfer(i, sumPhaseHeat);

				}
			}
			//setDebugArray(debugVector, 0);
			//setDebugArray(debugVector2, 1);
			setDebugArray(debugVector3, 1);
		}

		m_time += m_speed * dt * 2.77778e-4f;
		if (m_time > 24.0f) m_time = 0.0f;
	}
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
	const float absorbedRadiationAlbedo = 0.25f;  //How much light is reflected back? 0 = absorbes all, 1 = reflects all
	const float groundThickness = 1.0f; //Just used 1 meter
	const float densityGround = 1500.0f;
	const double T4 = m_groundGrid.T[i] * m_groundGrid.T[i] * m_groundGrid.T[i] * m_groundGrid.T[i];
	
	m_groundGrid.T[i] += m_speed * dt * ((1 - LC) * (((1 - absorbedRadiationAlbedo) * Irradiance - Constants::ge * Constants::oo * T4) / (groundThickness * densityGround * Constants::Cpds)));
}

void environment::updateMicroPhysicsGround(const float dt, const int i)
{
	// Uses some of the variables used in updateMicroPhysics

	const int pX = (i % GRIDSIZEGROUND == 0) ? i : i - 1;
	const int nX = ((i + 1) % GRIDSIZEGROUND == 0) ? i : i + 1;

	float FR = 0.0f; // Growth of ice (precip) by freezing rain
	float MS = 0.0f; // Snow melting
	float MI = 0.0f; // Ice (precip) melting
	float ER = 0.0f; // Evaporation of rain
	float ES = 0.0f; // Evaporation of snow
	float EI = 0.0f; // Evaporation of ice (precip)
	//Done in the sky update due to advection taking place after this update
	//float GR = 0.0f; // Rain hit ground 
	//float GS = 0.0f; // Snow hit ground
	//float GI = 0.0f; // Ice (precip) hit ground
	float IR = 0.0f; //	Water flowing through the ground (through holes in ground)
	float EG = 0.0f; // Evaporation of dry ground
	float DG = 0.0f; // diffusion coefficient for ground rain water https://dtrx.de/od/diff/
	float DS = 0.0f; // diffusion coefficient for subsurface water https://www.researchgate.net/figure/Diffusion-coefficient-for-water-in-soils_tbl2_267235072

	//TODO: these values could be so wrong.
	const float BFR{ 5e-3f };					  // Aggregation rate of freezing rain to ice (precip) rate coefficient:
	const float BMI{ 5e-4f };					  // Aggregation rate of ice to rain rate coefficient:
	const float BER{ 5e-4f };					  // Aggregation rate of rain to vapor rate coefficient:
	const float BES{ 5e-4f };					  // Aggregation rate of snow to vapor rate coefficient:
	const float BEI{ 5e-4f };					  // Aggregation rate of ice to vapor rate coefficient:
	const float BIR{ 100.0f };					  // Evaporation rate of subsurface water (A constant)
	const float k{ 5e-5f }; //Sand				  // Hydraulic conductivity of the ground in m/s(How easy water flows through ground) https://structx.com/Soil_Properties_007.html
	const float BEG{ 200.0f };					  // Evaporation rate of dry ground

	float T = float(m_groundGrid.T[i]);
	float p = m_groundGrid.P[i];
	const float QWS = meteoformulas::ws((T - 273.15f), p); //Maximum water vapor air can hold
	const float QWI = meteoformulas::wi((T - 273.15f), p);

	const int envGIdx = i + (m_GHeight[i] + 1) * GRIDSIZEGROUND;


	if (T < -8 + 273.15f) FR = BFR * (T - 273.15f + 8) * (T - 273.15f + 8); 

	if (T > 0 + 273.15f) //Melting
	{
		// 𝛿(𝑋𝑐 + 𝑋𝑠 + 𝑋𝑖) ≤ 𝑐𝑝air / 𝐿𝑓 * 𝑇, == First melt cloud ice, then snow, then precip ice, when the criteria is met between any step, stop melting.

		const float check = Cpd / Lf * (T - 273.15f);
		float heatSum = 0.0f;

		const float Qs = m_groundGrid.Qgs[i];
		const float Qi = m_groundGrid.Qgi[i];
		const float MIt = BMI * (T - 273.15f);

		//Melt snow if possible
		if (Qs > 0.0f && heatSum < check)
		{
			const float melting = std::min(Qs, check - heatSum);
			MS = melting;
			heatSum += melting;
		}
		if (Qi > 0.0f && heatSum < check)
		{
			//Precip Ice can fall longer through air warmer than its melting point.
			MI = std::min(MIt, std::min(Qi, check - heatSum));
		}
	}
	//i represent the first sky voxel (the voxel above the ground)

	//Constraint on how much can be evaporated (don't evap if air can't hold more)
	//We first make sure that we are not adding more vapor than the air can hold, then we add up all the things we can evap, multiplied by dt.
	//TODO: Check if right
	ER = BER * std::min(m_groundGrid.Qgr[i], std::min(m_speed * dt * (m_envGrid.Qw[envGIdx] + m_groundGrid.Qgr[i]), std::max(QWS - m_envGrid.Qv[envGIdx], 0.0f)));
	ES = BES * std::min(m_groundGrid.Qgs[i], std::min(m_speed * dt * (m_envGrid.Qc[envGIdx] + m_groundGrid.Qgs[i] + m_groundGrid.Qgi[i]), std::max(QWI - m_envGrid.Qv[envGIdx], 0.0f)));
	EI = BEI * std::min(m_groundGrid.Qgi[i], std::min(m_speed * dt * (m_envGrid.Qc[envGIdx] + m_groundGrid.Qgs[i] + m_groundGrid.Qgi[i]), std::max(QWI - m_envGrid.Qv[envGIdx], 0.0f)));

	const float D_ = 1e-6f; // Weigthed mean diffusivity of the ground //TODO: hmm, could tripple check if right
	const float O_ = 0.1f; // evaporative ground water storage coefficient (i.e. only part of the soil can be evaporated) 
	IR = BIR * k * m_groundGrid.Qgr[i]; //TODO: this values is way too low, I have to increase with BIR, check with real life values.
	//Only if Qgj = 0 (Precip falling)
	if (m_groundGrid.Qgr[i] == 0 && m_groundGrid.Qgs[i] == 0 && m_groundGrid.Qgi[i] == 0)
	{
		EG = std::min(BEG * D_ * m_groundGrid.Qrs[i] * exp(-m_groundGrid.t[i] / 86400 * O_), m_groundGrid.Qrs[i]);
		m_groundGrid.t[i] += dt;
	}
	else
	{
		m_groundGrid.t[i] = 0;
	}
	
	//Check how many meters down
	float slopeFlowRain = 0.0f;
	float slopeFlowWater = 0.0f;
	{
		// Using Manning Formula: V = 1 / n * Rh^2/3 * S^1/2
		// We use for n = 0.030.
		// RH is weird for open way, so we use water depth, which is in our case is Qgr * VOXELSIZE (in meters)
		const float n = 1 / 0.03f;

		const float slope1 = float(m_GHeight[pX] - m_GHeight[i]) / 1; // Divide by 1 is useless but its to say that height changed with n by 1 meters.
		const float slope2 = float(m_GHeight[nX] - m_GHeight[i]) / 1;
		const float slope1FromRain = slope1 > 0 ? m_groundGrid.Qgr[pX] : -m_groundGrid.Qgr[i];
		const float slope2FromRain = slope2 > 0 ? m_groundGrid.Qgr[nX] : -m_groundGrid.Qgr[i];
		const float slope1FromWater = slope1 > 0 ? m_groundGrid.Qrs[pX] : m_groundGrid.Qrs[i];
		const float slope2FromWater = slope2 > 0 ? m_groundGrid.Qrs[nX] : m_groundGrid.Qrs[i];

		// Height of water
		const float RH1 = abs(slope1FromRain) * VOXELSIZE;
		const float RH2 = abs(slope2FromRain) * VOXELSIZE;
		const float speed1 = n * powf(RH1, 2.0f / 3.0f);
		const float speed2 = n * powf(RH2, 2.0f / 3.0f);

		// In m/s
		const float vel1 = speed1 * pow(abs(slope1), 0.5f);
		const float vel2 = speed2 * pow(abs(slope2), 0.5f);

		// Speed * time = distance, and use that to calculate how much water has made its way down already, so to say.
		// This makes the water kind of stick the less it is, which is okay
		const float change1 = vel1 * dt * slope1FromRain;
		const float change2 = vel2 * dt * slope2FromRain;

		slopeFlowRain = std::max(change1 + change2, -m_groundGrid.Qgr[i]);

		slopeFlowWater = std::max(g * k * (slope1FromWater * slope1 + slope2FromWater * slope2), -m_groundGrid.Qrs[i]);

	}

	const float lapR = (m_groundGrid.Qgr[pX] + m_groundGrid.Qgr[nX] - 2 * m_groundGrid.Qgr[i]) / (VOXELSIZE * VOXELSIZE);
	lapR;
	DG = 1.75e-3f;
	const float lapSR = (m_groundGrid.Qrs[pX] + m_groundGrid.Qrs[nX] - 2 * m_groundGrid.Qrs[i]) / (VOXELSIZE * VOXELSIZE);
	DS = 1.8e-5f;

	//Limit everything if speed is higher
	if (m_speed > 1.0f)
	{
		//Limit subsurface ground water
		EG = std::min(EG * m_speed, m_groundGrid.Qrs[i]);
		//Limit rain on ground
		ER = std::min(ER * m_speed, m_groundGrid.Qgr[i]);
		FR = std::min(FR * m_speed, m_groundGrid.Qgr[i]);
		IR = std::min(IR * m_speed, m_groundGrid.Qgr[i]);
		//Limit snow on ground
		MS = std::min(MS * m_speed, m_groundGrid.Qgs[i]);
		ES = std::min(ES * m_speed, m_groundGrid.Qgs[i]);
		//Limit hail on ground
		EI = std::min(EI * m_speed, m_groundGrid.Qgi[i]);
		MI = std::min(MI * m_speed, m_groundGrid.Qgi[i]);
	}

	m_groundGrid.Qgr[i] += ( dt * (DG * lapR + slopeFlowRain + MS + MI - ER - FR - IR));
	m_groundGrid.Qrs[i] += ( dt * (DS * lapSR + slopeFlowWater + IR - EG));
	m_groundGrid.Qgs[i] += ( dt * (-MS - ES));
	m_groundGrid.Qgi[i] += ( dt * (FR - EI - MI));

	if (m_groundGrid.Qrs[i] < 0|| m_groundGrid.Qgs[i] < 0 || m_groundGrid.Qgi[i] < 0 ||
		m_groundGrid.Qrs[i] != m_groundGrid.Qrs[i] || m_groundGrid.Qgs[i] != m_groundGrid.Qgs[i] || m_groundGrid.Qgi[i] != m_groundGrid.Qgi[i])
	{
		DS = 0;
	}
}

float environment::calculateSumPhaseHeatGround(const int i, const float pQgr, const float pQgs, const float pQgi)
{
	float sumPhaseheat = 0.0f;

	float dQgr = m_groundGrid.Qgr[i] - pQgr;
	float dQgs = m_groundGrid.Qgs[i] - pQgs;
	float dQgi = m_groundGrid.Qgi[i] - pQgi;
	dQgr = dQgr == 0 ? 0 : dQgr / (1 + dQgr);
	dQgs = dQgs == 0 ? 0 : dQgs / (1 + dQgs);
	dQgi = dQgi == 0 ? 0 : dQgi / (1 + dQgi);

	const float invCpdRatio = 1.0f / Cpws;

	sumPhaseheat += Ls * invCpdRatio * dQgr;
	sumPhaseheat += Ls * invCpdRatio * dQgs;
	sumPhaseheat += Lf * invCpdRatio * dQgi;
	return sumPhaseheat;
}

void environment::diffuseAndAdvectTemp(const float dt, double* array)
{
	// Diffuse
	// For temperature, we use ambient as boundary condition
	{
		std::vector<double> dif;
		dif.resize(GRIDSIZESKY);
		std::memcpy(dif.data(), m_envGrid.potTemp, GRIDSIZESKY * sizeof(double));

		const float k = 0.005f * dt / (VOXELSIZE * VOXELSIZE); //Viscosity value
		const int LOOPS = 20; //Total loops for the Gauss-Seidel method
		for (int loop = 0; loop < 2; loop++)
		{
			for (int L = 0; L < LOOPS; L++)
			{
				for (int j = 0; j < GRIDSIZESKY; j++)
				{
					if ((j + loop + int(float(j) / GRIDSIZESKYX)) % 2 == 0 || isGround(j)) continue;

					const float yPos = floor(float(j) / GRIDSIZESKYX);

					//TODO: could use a function inside this function which switches over the types, is faster.
					const double l = m_NeighData[j].left == OUTSIDE ? getIsentropicTemp(yPos) : m_NeighData[j].left  == GROUND ? m_groundGrid.T[(j - 1) % GRIDSIZESKYX] : dif[j - 1];
					const double r = m_NeighData[j].right == OUTSIDE ? getIsentropicTemp(yPos) : m_NeighData[j].right == GROUND ? m_groundGrid.T[(j + 1) % GRIDSIZESKYX] : dif[j + 1];
					const double d = m_NeighData[j].down == GROUND ? m_groundGrid.T[j % GRIDSIZESKYX] : dif[j - GRIDSIZESKYX];
					const double u = m_NeighData[j].up == OUTSIDE ? getIsentropicTemp(yPos) : dif[j + GRIDSIZESKYX];

					dif[j] = (m_envGrid.potTemp[j] + k * (l + r + u + d)) / (1 + 4 * k);
				}
			}
		}
		std::memcpy(m_envGrid.potTemp, dif.data(), GRIDSIZESKY * sizeof(double));
	}

	//Advection
	for (int loop = 0; loop < 2; loop++)
	{
		for (int i = 0; i < GRIDSIZESKY; i++)
		{
			// we want not to update neighbouring cells, so we do the Red-Black Gauss-Seidel tactic
			if ((i + loop + int(float(i) / GRIDSIZESKYX)) % 2 == 0 || isGround(i)) continue;

			//If at the ground, set to ground temp
			if (isGround(i - GRIDSIZESKYX))
			{

				const double dif = array[i] - m_groundGrid.T[i % GRIDSIZESKYX];
				array[i] -= dif * dt * 0.1f;
				continue;
			}

			// Current Position of value
			const glm::vec2 CPos = { i % GRIDSIZESKYX, int(i / GRIDSIZESKYX) };

			// Current Velocity of the exact cell, also applying falling velocity
			const glm::vec2 CVel = getUV(i);
			// Previous Position of value
			const glm::vec2 PPos = CPos - dt * CVel / VOXELSIZE; //Get previous position of particle

			double value = 0.0f;
			//Get current value of previous position
			getInterpolValueTemp(array, PPos, value);

			array[i] = value;

			if (array[i] != array[i] || array[i] < 0)
			{
				array[i] = 0;
			}
		}
	}



}

void environment::getInterpolValueTemp(double* array, const glm::vec2 Ppos, double& output)
{
	//Get surrounding values 
	glm::vec2 coord1 = { (std::floor(Ppos.x)), std::floor(Ppos.y) };

	const int index00 = int(coord1.x) + int(coord1.y) * GRIDSIZESKYX;
	const int index10 = int(coord1.x + 1) + int(coord1.y) * GRIDSIZESKYX;
	const int index01 = index00 + GRIDSIZESKYX;
	const int index11 = index10 + GRIDSIZESKYX;

	//Use Isentropic Temp if outside of grid and also check for ground
	const double value00 = outside(coord1.x, coord1.y) ? getIsentropicTemp(coord1.y) : isGround(index00) ? m_groundGrid.T[int(coord1.x)] : array[index00];
	const double value10 = outside(coord1.x + 1, coord1.y) ? getIsentropicTemp(coord1.y) : isGround(index10) ? m_groundGrid.T[int(coord1.x) + 1] : array[index10];
	const double value01 = outside(coord1.x, coord1.y + 1) ? getIsentropicTemp(coord1.y + 1) : isGround(index01) ? m_groundGrid.T[int(coord1.x)] : array[index01];
	const double value11 = outside(coord1.x + 1, coord1.y + 1) ? getIsentropicTemp(coord1.y + 1) : isGround(index11) ? m_groundGrid.T[int(coord1.x) + 1] : array[index11];
		
	//Interpolate using bilinear interpolation, could be compressed

	double interpX = ((coord1.x + 1) - Ppos.x) / ((coord1.x + 1) - coord1.x) * value00 +
		(Ppos.x - coord1.x) / ((coord1.x + 1) - coord1.x) * value10;
	double interpX2 = ((coord1.x + 1) - Ppos.x) / ((coord1.x + 1) - coord1.x) * value01 +
		(Ppos.x - coord1.x) / ((coord1.x + 1) - coord1.x) * value11;
	output = ((coord1.y + 1) - Ppos.y) / ((coord1.y + 1) - coord1.y) * interpX +
		(Ppos.y - coord1.y) / ((coord1.y + 1) - coord1.y) * interpX2;

	if (output != output || output < 0)
	{
		output = 0;
	}
}

void environment::diffuseAndAdvect(const float dt, float* array, bool vapor, const int fallVelType)
{

	// Diffuse
	{
		std::vector<float> dif;
		dif.resize(GRIDSIZESKY);
		std::memcpy(dif.data(), array, GRIDSIZESKY * sizeof(float));
	
		//TODO: how much diffusion?
		const float k = 0.05f * dt / (VOXELSIZE * VOXELSIZE); //Viscosity value
		const int LOOPS = 20; //Total loops for the Gauss-Seidel method
		for (int loop = 0; loop < 2; loop++)
		{
			for (int L = 0; L < LOOPS; L++)
			{
				for (int j = 0; j < GRIDSIZESKY; j++)
				{
					if ((j + loop + int(float(j) / GRIDSIZESKYX)) % 2 == 0 || isGround(j)) continue;
	
					const float yPos = floor(float(j) / GRIDSIZESKYX);
	
					//TODO: could use a function inside this function which switches over the types, is faster.
					if (vapor) //If vapor, we wrap the edges
					{
						if (m_NeighData[j].down == GROUND) { dif[j] = array[j]; continue; }
						const float left = m_NeighData[j].left == OUTSIDE ? getIsentropicVapor(yPos) : m_NeighData[j].left == GROUND ? dif[j] : dif[j - 1];
						const float right = m_NeighData[j].right == OUTSIDE ? getIsentropicVapor(yPos) : m_NeighData[j].right == GROUND ? dif[j] : dif[j + 1];
						const float down = m_NeighData[j].down == GROUND ? dif[j] : dif[j - GRIDSIZESKYX];
						const float up = m_NeighData[j].up == OUTSIDE ? getIsentropicVapor(yPos) : dif[j + GRIDSIZESKYX]; //Neumann due to being diffusion
					
						dif[j] = (array[j] + k * (left + right + up + down)) / (1 + 4 * k);
					}
					else // Else use nuemann
					{
						const float left = m_NeighData[j].left == OUTSIDE ? dif[j] : m_NeighData[j].left == GROUND ? dif[j] : dif[j - 1];
						const float right = m_NeighData[j].right == OUTSIDE ? dif[j] : m_NeighData[j].right == GROUND ? dif[j] : dif[j + 1];
						const float down = m_NeighData[j].down != SKY ? dif[j] : dif[j - GRIDSIZESKYX];
						const float up = m_NeighData[j].up == OUTSIDE ? 0.0f : dif[j + GRIDSIZESKYX];
	
						dif[j] = (array[j] + k * (left + right + up + down)) / (1 + 4 * k);
					}
	
				}
			}
		}
		std::memcpy(array, dif.data(), GRIDSIZESKY * sizeof(float));
	}
	//Check
	for (int i = 0; i < GRIDSIZESKY; i++)
	{
		if (array[i] != array[i] || array[i] < 0)
		{
			array[i] = 0;
		}
	}


	//Advection
	std::vector<float> debugVector;
	debugVector.resize(GRIDSIZESKY);

	//Advect falling velocity of precip
	if (fallVelType != 0)
	{
		interPolatePrecip(dt, array, fallVelType);
	}

	//Advect using wind field
	for (int loop = 0; loop < 2; loop++)
	{
		for (int i = 0; i < GRIDSIZESKY; i++)
		{
			// we want not to update neighbouring cells, so we do the Red-Black Gauss-Seidel tactic
			if ((i + loop + int(float(i) / GRIDSIZESKYX)) % 2 == 0 || isGround(i) || (m_NeighData[i].down == GROUND && vapor)) continue;

			//Advection
			if (!vapor) PPMWAdvect(array, i, dt);
			else
			{
				// Current Position of value
				const glm::vec2 CPos = { i % GRIDSIZESKYX, int(i / GRIDSIZESKYX) };
				// Current Velocity of the exact cell, also applying falling velocity
				const glm::vec2 CVel = getUV(i);
				// Previous Position of value
				const glm::vec2 PPos = CPos - dt * CVel / VOXELSIZE; //Get previous position of particle
				
				float value = 0.0f;
				//Get current value of previous position
				if (!getInterpolValue(array, PPos, vapor, value))
				{
					value = array[i];
				}
				
				array[i] = value;
			}

			if (array[i] != array[i] || array[i] < 0)
			{
				array[i] = 0;
			}
		}
	}
	//if (fallVelType == 3) setDebugArray(debugVector, 0);
}

void environment::interPolatePrecip(const float dt, float* array, const int fallVelType)
{
	for (int loop = 0; loop < 2; loop++)
	{
		for (int i = 0; i < GRIDSIZESKY; i++)
		{
			// we want not to update neighbouring cells, so we do the Red-Black Gauss-Seidel tactic
			if ((i + loop + int(float(i) / GRIDSIZESKYX)) % 2 == 0 || isGround(i)) continue;

			float fallVel = 0.0f;
			float fallVelUp = 0.0f;

			glm::vec3 fallVelocitiesPrecip{ 0.0f };
			glm::vec3 fallVelocitiesPrecipUP{0.0f};

			{
				const float T = float(m_envGrid.potTemp[i]) * glm::pow(m_pressures[i] / m_groundGrid.P[i % GRIDSIZESKYX], Rsd / Cpd);
				const float Tv = T * (0.608f * m_envGrid.Qv[i] + 1);
				const float density = m_pressures[i] * 100 / (Rsd * Tv); //Convert Pha to Pa
				fallVelocitiesPrecip = calculateFallingVelocity(dt, i, density);
			}
			if (m_NeighData[i].up == SKY)
			{
				const int iUP = i + GRIDSIZESKYX;
				const float T = float(m_envGrid.potTemp[iUP]) * glm::pow(m_pressures[iUP] / m_groundGrid.P[iUP % GRIDSIZESKYX], Rsd / Cpd);
				const float Tv = T * (0.608f * m_envGrid.Qv[iUP] + 1);
				const float density = m_pressures[iUP] * 100 / (Rsd * Tv); //Convert Pha to Pa
				fallVelocitiesPrecipUP = calculateFallingVelocity(dt, iUP, density);
			}
			switch (fallVelType)
			{
			case 1:
				fallVel = fallVelocitiesPrecip.x;
				fallVelUp = fallVelocitiesPrecipUP.x;
				break;
			case 2:
				fallVel = fallVelocitiesPrecip.y;
				fallVelUp = fallVelocitiesPrecipUP.y;
				break;
			case 3:
				fallVel = fallVelocitiesPrecip.z;
				fallVelUp = fallVelocitiesPrecipUP.z;
				break;
			default:
				break;
			}

			if (m_NeighData[i].up == SKY)
			{
				array[i] += (dt * fallVelUp / VOXELSIZE) * array[i + GRIDSIZESKYX]; //Grab % of precip above
			}
			array[i] -= (dt * fallVel / VOXELSIZE) * array[i]; //Remove % of precip

			if (array[i] < 0.0f)
			{
				fallVel = 0.0f;
			}
		}
	}
}

bool environment::getInterpolValue(float* array, const glm::vec2 Ppos, const bool vapor, float& output)
{
	//Get surrounding values 
	glm::vec2 coord1 = { (std::floor(Ppos.x)), std::floor(Ppos.y) };
	//const bool left = coord1.x < 0;
	//const bool right = coord1.x >= GRIDSIZESKYX - 1;

	//Using - 1 on x and y because we are taking the bottom left grid cell, meaning that right and top will be one further away.
	//if (((left || right) && !vapor) || coord1.y < 0 || coord1.y >= GRIDSIZESKYY - 1)
	//{
	//	return false;
	//}

	//We can just use the changeIndex by, because if it was not a vapor, it would already have returned false
	const int index00 = int(coord1.x) + int(coord1.y) * GRIDSIZESKYX;
	const int index10 = int(coord1.x + 1) + int(coord1.y) * GRIDSIZESKYX;
	const int index01 = index00 + GRIDSIZESKYX;
	const int index11 = index10 + GRIDSIZESKYX;

	float value00 = 0.0f;
	float value10 = 0.0f;
	float value01 = 0.0f;
	float value11 = 0.0f;

	if (vapor)
	{
		value00 = outside(coord1.x, coord1.y) ? getIsentropicVapor(coord1.y) : isGround(index00) ? -1.0f : array[index00];
		value10 = outside(coord1.x + 1, coord1.y) ? getIsentropicVapor(coord1.y) : isGround(index10) ? -1.0f : array[index10];
		value01 = outside(coord1.x, coord1.y + 1) ? getIsentropicVapor(coord1.y + 1) : isGround(index01) ? -1.0f : array[index01];
		value11 = outside(coord1.x + 1, coord1.y + 1) ? getIsentropicVapor(coord1.y + 1) : isGround(index11) ? -1.0f : array[index11];
	}
	else
	{
		value00 = outside(coord1.x, coord1.y) ? 0.0f : isGround(index00) ? 0.0f : array[index00];
		value10 = outside(coord1.x + 1, coord1.y) ? 0.0f : isGround(index10) ? 0.0f : array[index10];
		value01 = outside(coord1.x, coord1.y + 1) ? 0.0f : isGround(index01) ? 0.0f : array[index01];
		value11 = outside(coord1.x + 1, coord1.y + 1) ? 0.0f : isGround(index11) ? 0.0f : array[index11];
	}

	if (value00 < 0 || value10 < 0 || value01 < 0 || value11 < 0) return false; //Value could never on its own be lower than 0.


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

void environment::PPMWAdvect(float* array, const int i, const float dt)
{
	//Using PPM (Piecewise Parabolic Method) advection with Walcek. 
	//Formula based on https://gmd.copernicus.org/preprints/gmd-2023-78/gmd-2023-78-manuscript-version5.pdf and https://gmd.copernicus.org/articles/13/5707/2020
	//https://mom6.readthedocs.io/en/main/api/generated/pages/PPM.html and help from chat-GPT 

	//Second-order in time in 2D
	PPMWAdvectLR(array, i, dt / 2, true);
	PPMWAdvectLR(array, i, dt, false);
	PPMWAdvectLR(array, i, dt / 2, true);
}

void environment::PPMWAdvectLR(float* array, const int idx, const float dt, const bool x)
{
	const float fluxRight = PPMWAdvectFlux(array, idx, dt, x, true);
	const float fluxLeft = PPMWAdvectFlux(array, idx, dt, x, false);

	array[idx] = array[idx] - dt / VOXELSIZE * (fluxRight - fluxLeft);
}

float environment::PPMWAdvectFlux(float* array, const int idx, const float dt, const bool x, const bool right)
{
	//Down/up = dirichlet, otherwise is Neumann
	//Can't check left face or down face if it is none existing. 

	const int dirX = right ? 1 : -1;
	const int dirY = right ? GRIDSIZESKYX : -GRIDSIZESKYX;
	float veli = 0.0f;
	if (!right && ((x && m_NeighData[idx].left != SKY) || (!x && m_NeighData[idx].down != SKY))) veli = (x ? m_envGrid.velField[idx].x : m_envGrid.velField[idx].y);
	else veli = right ? (x ? m_envGrid.velField[idx].x : m_envGrid.velField[idx].y) : (x ? m_envGrid.velField[idx - 1].x : m_envGrid.velField[idx - GRIDSIZESKYX].y);

	//Calculate C
	float C = veli * dt / VOXELSIZE;
	C = C > 1.0f ? 1.0f : (C < -1.0f ? -1.0f : C);

	//Depending on upwind/downwind, we use i or i + 1
	//Downwind with right face = coming from right
	//Downwind with left face = coming from left
	bool downWind = right ? C < 0.0f : C > 0.0f;
	const int i = downWind ? idx + dirX : idx;
	const int iy = downWind ? idx + dirY : idx;
	float qi{ 0.0f };
	float qRight{ 0.0f };
	float qLeft{ 0.0f };

	//Winds from outside the grid, we already returned if left faced. 
	if (right && downWind && ((x && m_NeighData[idx].right != SKY) || (!x && m_NeighData[idx].up != SKY)) ||
		!right && downWind && ((x && m_NeighData[idx].left != SKY) || (!x && m_NeighData[idx].down != SKY)))
	{
		qi = qRight = qLeft = array[idx]; // Set to 0 if dirichlet
	}
	else if (x)
	{
		qi = array[i];
		qRight = m_NeighData[i].right != SKY ? array[i] : array[i + 1];
		qLeft = m_NeighData[i].left != SKY ? array[i] : array[i - 1];
	}
	else
	{
		qi = array[iy];
		qRight = m_NeighData[iy].up != SKY ? array[iy] : array[iy + GRIDSIZESKYX];
		qLeft = m_NeighData[iy].down != SKY ? array[iy] : array[iy - GRIDSIZESKYX];
	}

	const float qMin = std::min(qLeft, std::min(qi, qRight));
	const float qMax = std::max(qLeft, std::max(qi, qRight));

	//Slope limiter: van Leer
	float s = 0.0f;
	if ((qi - qLeft) * (qRight - qi) > 0.0f) s = glm::sign(qRight - qi) * std::min(0.5f * fabs(qRight - qLeft), std::min(2 * fabs(qRight - qi), 2 * fabs(qi - qLeft)));
	const float qLS = qi - 0.5f * s;
	const float qRS = qi + 0.5f * s;

	float B = 1.0f;

	if (qRS > qMax)
	{
		const float denominator = qRS - qi;
		if (denominator > 1e-16f)
		{
			B = std::min(B, (qMax - qi) / denominator);
		}
	}
	if (qLS < qMin)
	{
		const float denominator = qi - qLS;
		if (denominator > 1e-16f)
		{
			B = std::min(B, (qi - qMin) / denominator);
		}
	}

	//Scale to be in between min and max
	float qR = qi + B * (qRS - qi);
	float qL = qi + B * (qLS - qi);

	//Fix extremum
	if ((qR - qi) * (qi - qL) <= 0.0f)
	{
		qR = qL = qi;
	}


	float flux = 0.0f;
	if (C > 0.0f)
	{
		const float qq = qR + C * (-qL - 2 * qR + 3 * qi) + C * C * (qL + qR - 2 * qi);
		flux = veli * qq;
	}
	else
	{
		const float qq = qR + C * (-2 * qL - qR + 3 * qi) + C * C * (qL + qR - 2 * qi);
		flux = veli * qq;
	}
	return flux;
}

using namespace Constants;

void environment::updateVelocityField(const float dt)
{
	//--------DEBUG--------
	std::vector<float> debugVector;
	std::vector<float> debugVector2;
	std::vector<float> debugVector3;
	debugVector.resize(GRIDSIZESKY);
	debugVector2.resize(GRIDSIZESKY);
	debugVector3.resize(GRIDSIZESKY);


	//Full Navier-Stroke formula
	//u[t+1] = u + deltaTime * ( -(u * ∇)u - (1/ρ)∇p + v*(∇^2u) + b + f ); 
	//We advect and add forces based on the Eulerian fluid solver of Hädrich et al. [2020]:
	//• Set uA = advect(un, ∆t, un)
	//• Add uB = uA + ∆t*g
	

	//Our updating of the velocity goes as follows: 
	// 
	//1. Add Forces
	//2. Diffuse the field
	//3. Advect the field
	//4. Use projection to make the fluid incompressible

	//1.
	
	for (int i = 0; i < GRIDSIZESKY; i++)
	{
		if (isGround(i)) continue;
		
		//TODO: this is not really a good solution.
		if (m_NeighData[i].left == OUTSIDE || m_NeighData[i].right == OUTSIDE || m_NeighData[i].up == OUTSIDE || m_NeighData[i].down == OUTSIDE)
		{
			continue;
		}
	
		float B = calculateBuoyancy(i, m_pressures);
		glm::vec2 F = vorticityConfinement(i);
		
		debugVector[i] = F.x;
		debugVector2[i] = F.y;
		debugVector3[i] = B;
		//F.x = 0.0f;
		//F.y = 0.0f;
		//B = 0.0f;
		m_envGrid.velField[i].x += m_speed * dt * F.x;
		m_envGrid.velField[i].y += m_speed * dt * (B + F.y);
	}
	
	
	//setDebugArray(debugVector);
	//setDebugArray(debugVector2, 1);
	setDebugArray(debugVector3, 2);
	
	
	//2. TODO: could make use of same matrix projection() uses.
	
	{
		std::vector<glm::vec2> dif;
		dif.resize(GRIDSIZESKY);
		std::memcpy(dif.data(), m_envGrid.velField, GRIDSIZESKY * sizeof(glm::vec2));
	
		const float k = 0.01f / (VOXELSIZE * VOXELSIZE); //Viscosity value
		const int LOOPS = 20; //Total loops for the Gauss-Seidel method
		for (int loop = 0; loop < 2; loop++)
		{
			for (int L = 0; L < LOOPS; L++)
			{
				for (int i = 0; i < GRIDSIZESKY; i++)
				{
					if ((i + loop + (i / GRIDSIZESKYX)) % 2 == 0 || isGround(i)) continue;
	
					const glm::vec2 left  = m_NeighData[i].left  == OUTSIDE ? dif[i] : m_NeighData[i].left == GROUND ? glm::vec2(0.0f) : dif[i - 1];	// Neumann (Or no slip)
					const glm::vec2 right = m_NeighData[i].right == OUTSIDE ? dif[i] : m_NeighData[i].right == GROUND ? glm::vec2(0.0f) : dif[i + 1];	// Neumann (Or no slip)
					const glm::vec2 up    = m_NeighData[i].up    != SKY ? glm::vec2(dif[i].x, 0.0f) : dif[i + GRIDSIZESKYX]; // free slip
					const glm::vec2 down  = m_NeighData[i].down  != SKY ? glm::vec2(0.0f) : dif[i - GRIDSIZESKYX]; // no slip
	
					dif[i] = (m_envGrid.velField[i] + k * (left + right + up + down)) / (1 + 4 * k);
				}
			}
		}
		std::memcpy(m_envGrid.velField, dif.data(), GRIDSIZESKY * sizeof(glm::vec2));
	}
	
	
	//3.
	
	//---Advection of velocity using Semi-Langrasian Advection---
	//We use the old value to simulate where the particle was, we get the velocity from that exact location and use that velocity for our next step
	//If our value lies outside the grid, we just use our old velocity.
	for (int loop = 0; loop < 2; loop++)
	{
		for (int i = 0; i < GRIDSIZESKY; i++)
		{
			const int y = i / GRIDSIZESKYX;
			if ((i + loop + y) % 2 == 0 || isGround(i)) continue;
	
	
			const glm::vec2 center = m_envGrid.velField[i];
			const glm::vec2 left  = m_NeighData[i].left  == OUTSIDE ? center : m_NeighData[i].left == GROUND ? glm::vec2(0.0f) : m_envGrid.velField[i - 1];	// Neumann (Or no slip)
			const glm::vec2 right = m_NeighData[i].right == OUTSIDE ? center : m_NeighData[i].right == GROUND ? glm::vec2(0.0f) : m_envGrid.velField[i + 1];	// Neumann (Or no slip)
			const glm::vec2 up    = m_NeighData[i].up    != SKY ? glm::vec2(m_envGrid.velField[i].x, 0.0f) : m_envGrid.velField[i + GRIDSIZESKYX]; // Free slip
			const glm::vec2 down  = m_NeighData[i].down  != SKY ? glm::vec2(0.0f) : m_envGrid.velField[i - GRIDSIZESKYX]; // no slip
	
			// Current Position of U and V 
			const glm::vec2 CposU = { i % GRIDSIZESKYX + 0.5f, int(i / GRIDSIZESKYX) };
			const glm::vec2 CposV = { i % GRIDSIZESKYX, int(i / GRIDSIZESKYX) + 0.5f };
			// Current Velocity of U and V
			const glm::vec2 CVelU = { center.x, (left.y + right.y + up.y + down.y) / 4 };
			const glm::vec2 CVelV = { (left.x + right.x + up.x + down.x) / 4, center.y };
			// Previous Position of U and V
			const glm::vec2 PposU = CposU - m_speed * dt * CVelU / VOXELSIZE; //Get previous position of particle
			const glm::vec2 PposV = CposV - m_speed * dt * CVelV / VOXELSIZE; //Get previous position of particle
	
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
				if (y == GRIDSIZESKYY - 1)
				{
					valueV = 0.0f; //Set to 0 at ceiling, because of free-slip
					//TODO: make a solid solution by implementing it into the interpolation
				}
				else valueV = m_envGrid.velField[i].y;
			}
	
			//----New Value------ = ----------Advection------ 
			m_envGrid.velField[i] = glm::vec2(valueU, valueV);
	
			if (m_envGrid.velField[i] != m_envGrid.velField[i] || m_envGrid.velField[i].x > 100 || m_envGrid.velField[i].y > 100 || m_envGrid.velField[i].x < -100 || m_envGrid.velField[i].y < -100)
			{
				m_envGrid.velField[i].x = 0;
			}
		}
	}

	// 4.
	pressureProjectVelField(dt);
}

float environment::calculateBuoyancy(const int i, const float* ps)
{
	const float T = static_cast<float>(m_envGrid.potTemp[i]) * glm::pow(ps[i] / m_groundGrid.P[i % GRIDSIZESKYX], Rsd / Cpd);
	const float T2 = static_cast<float>(m_envGrid.potTemp[i + GRIDSIZESKYX]) * glm::pow(ps[i + GRIDSIZESKYX] / m_groundGrid.P[i % GRIDSIZESKYX], Rsd / Cpd);

	const float mDistance = 4096.0f / VOXELSIZE;
	const int oX = i % GRIDSIZESKYX;
	const int oY = int(float(i) / GRIDSIZESKYX);

	//Calculate min and maxY for our calculations.
	const int checkSize = 1; //0 or lower is not suppported

	//Check if we are going through the floor or ceiling
	int outsideUp = std::max(0, oY + checkSize - (GRIDSIZESKYY - 1));
	int outsideDown = std::max(0, (oY - checkSize) * -1);
	//Also check for ground below (ceiling has no ground, duh)
	for (int y = oY - 1; y >= std::max(0, oY - checkSize); y--)
	{
		if (y <= m_GHeight[oX])
		{
			outsideDown = oY - y; //No need to max by outsideBy, since we account for the floor by maxing
			break;
		}
	}
	int outsideMin = std::min(outsideUp, outsideDown);
	if ((checkSize - outsideMin) <= 0)
	{
		perror("Time to fix this...");//TODO: fix if ground is up to the ceiling causing for 0
	}

	const int minY = oY - (checkSize - outsideDown);
	const int maxY = oY + (checkSize - outsideUp);
	const int layerChecks = (checkSize - outsideMin) * 2 + 1;

	float* buoyancies = new float[3];
	memset(buoyancies, 0, 3 * sizeof(int)); //Set to 0

	float BuoyancyFinal = 0.0f;

	//Environment vapor (Using average vapor of whole width)
	float* QvEnv = new float[layerChecks];
	float* QvP = new float[layerChecks];
	QvEnv[0] = m_envGrid.Qv[i];
	QvP[0] = m_envGrid.Qv[i];
	{
		float Qaverage = 0.0f;
		int QeIdx = 0;

		for (int y = oY - checkSize; y <= oY + checkSize; y++)
		{
			//Boundary check
			if ((y < oY && y < minY) || (y > oY && y > maxY))
			{
				QvP[QeIdx] = 0.0f;
				QvEnv[QeIdx++] = 0.0f; 
				continue;
			}

			Qaverage = averageEnvironment(oX + y * GRIDSIZESKYX, oX + oY * GRIDSIZESKYX, mDistance, false);
			if (QeIdx >= layerChecks) continue;
			if (Qaverage == 0)
			{
				QvEnv[QeIdx++] = 0.0f;
				continue;
			}
			//For Qv our 'parcel' is the one we are currently working with
			QvP[QeIdx] = m_envGrid.Qv[oX + y * GRIDSIZESKYX];
			QvEnv[QeIdx] = Qaverage;
			QeIdx++;
		}
	}

	//Environment Temp (Using average temp of whole until ground)
	float Taverage = 0.0f;
	float Baverage = 0.0f;
	int Bamount = 0;
	{
		//First calculate adiabatic temp up and down.
		float* PTemps = new float[layerChecks];
		int PTempIdx = 0;


		//First we calculate the parcel temps for each different heights
		if (m_envGrid.Qv[i] >= meteoformulas::ws((T2 - 273.15f), ps[i + GRIDSIZESKYX])) //Air is saturated, thus use moist adiabatic
		{
			for (int y = oY - checkSize; y <= oY + checkSize; y++)
			{
				//Boundary check
				if ((y < oY && y < minY) || (y > oY && y > maxY))
				{
					PTemps[PTempIdx++] = T;
					continue;
				}

				const int idx = oX + y * GRIDSIZESKYX;

				if (y < oY) //Remove temp below
				{
					///meteoformulas::getMoistTemp()
					//TODO: wrong and not very good if P change > 1
					PTemps[PTempIdx++] = T - meteoformulas::MLR(T - 273.15f, ps[idx]) * (ps[i] - ps[idx]);
				}
				else if (y > oY) //Add temp above
				{
					PTemps[PTempIdx++] = meteoformulas::MLR(T - 273.15f, ps[idx]) * (ps[idx] - ps[i]) + T;
				}
				else //Is temp at
				{
					PTemps[PTempIdx++] = T;
				}
			}
		}
		else //Use dry adiabatic
		{
			for (int y = oY - checkSize; y <= oY + checkSize; y++)
			{
				//Boundary check
				if ((y < oY && y < minY) || (y > oY && y > maxY))
				{
					PTemps[PTempIdx++] = T;
					continue;
				}

				const int idx = oX + y * GRIDSIZESKYX;

				if (y < oY) //Remove temp below
				{
					//TODO: I guess this is better then DLR?
					PTemps[PTempIdx++] = T * glm::pow(ps[idx] / ps[i], Constants::Rsd / Constants::Cpd);//T - meteoformulas::DLR(T - 273.15f, ps[idx]) * (ps[i] - ps[idx]);
				}
				else if (y > oY) //Add temp above
				{
					PTemps[PTempIdx++] = T * glm::pow(ps[idx] / ps[i], Constants::Rsd / Constants::Cpd);//meteoformulas::DLR(T - 273.15f, ps[idx]) * (ps[idx] - ps[i]) + T;
				}
				else //Is temp at
				{
					PTemps[PTempIdx++] = T;
				}
			}
		}
		PTempIdx = 0;

		//Now we calculate the environement temps and use them in the buoyancy formula to calculate buoyancy at each layer
		for (int y = oY - checkSize; y <= oY + checkSize; y++)
		{
			//Boundary check
			if ((y < oY && y < minY) || (y > oY && y > maxY))
			{
				buoyancies[Bamount] = 0.0f;
				if (PTempIdx == checkSize - 1 || PTempIdx == checkSize)Bamount++;
				PTempIdx++;
				continue;
			}

			Taverage = averageEnvironment(oX + y * GRIDSIZESKYX, oX + oY * GRIDSIZESKYX, mDistance, true);
			if (Taverage == 0 || PTempIdx >= layerChecks) continue;

			//TODO: this is totally wrong.
			//Temp parcel
			const float VTP = PTemps[PTempIdx] * (1.0f + 0.608f * QvP[PTempIdx]);
			//Temp environment
			const float VTE = Taverage * (1.0f + 0.608f * QvEnv[PTempIdx]);

			//Buoyancy on this layer
			// g * (1.0f - (Mda / avMolarMass) * (Tp / Te))
			const float currB = g * ((VTP - VTE) / VTE);
			buoyancies[Bamount] += currB;
			Baverage += currB;
			//Increase so that we have below, at and above parcel
			if (PTempIdx == checkSize - 1 || PTempIdx == checkSize)
			{
				Bamount++;
			}
			PTempIdx++;

			//Reset
			Taverage = 0.0f;
		}
		BuoyancyFinal = buoyancies[1];


		//=---------------------------Check state of buoyancy--------------------------------=

		int divideBy = 1;

		//Fully calculated Buoyancy without checks
		buoyancies[0] = checkSize - outsideDown == 0 ? 0.0f : buoyancies[0] / (checkSize - outsideDown);
		buoyancies[2] = checkSize - outsideUp == 0 ? 0.0f : buoyancies[2] / (checkSize - outsideUp);
		
		//TODO: if at ground or ?ceiling? no buoyancy when going into it
		//Include down and/or up depending on state at level of parcel
		if constexpr (checkSize > 0)
		{
			if (buoyancies[1] < 0.0f) //If falling
			{
				BuoyancyFinal += buoyancies[0];
				divideBy++;
				if (BuoyancyFinal > 0.0f) //Rising due to falling into colder air
				{
					//BuoyancyFinal = 0.0f;
					BuoyancyFinal -= buoyancies[0];
					divideBy--;
				}
			}
			else if (buoyancies[1] > 0.0f) //If going up
			{
				BuoyancyFinal += buoyancies[2];
				divideBy++;
				if (BuoyancyFinal < 0.0f) //Falling due to rising into warmer air
				{
					//BuoyancyFinal = 0.0f;
					BuoyancyFinal -= buoyancies[2];
					divideBy--;
				}
			}
			else //If not moving based on temperature difference to the left or right
			{
				BuoyancyFinal += buoyancies[0];
				BuoyancyFinal += buoyancies[2];
				divideBy++;
				divideBy++;
			}
		}

		//Cleanup
		delete[] PTemps;
		delete[] QvEnv;
		delete[] buoyancies;
	}
	return BuoyancyFinal / 3; //Take the average
}

float environment::averageEnvironment(const int i, const int distanceFromidx, const float maxDistance, const bool temp)
{
	//Loop to the right then to the left to get average
	if (isGround(i)) return (0.0f);


	const int oX = distanceFromidx % GRIDSIZESKYX;
	const int oY = int(float(distanceFromidx) / GRIDSIZESKYX);
	const int y = i / GRIDSIZESKYX;
	const int maxRight = oX + int(ceil(maxDistance)) > GRIDSIZEGROUND ? GRIDSIZEGROUND : oX + int(ceil(maxDistance));
	const int minLeft = oX - int(ceil(maxDistance)) < 0 ? 0 - 1 : oX - int(ceil(maxDistance));
	const float MDistanceSqr = maxDistance * maxDistance;

	float amount = 0.0f;
	float average = 0.0f;

	for (int x = oX + 1; x < maxRight; x++)
	{
		if (isGround(x, y) || x >= GRIDSIZESKYX) break;

		const float distance = float((oX - x) * (oX - x) + (oY - y) * (oY - y));
		float cAmount = -(distance / MDistanceSqr - 1);
		if (abs(cAmount) > 1 || cAmount <= 0.0f) continue;
		//Smoothing
		cAmount = std::pow(cAmount, 5.0f);
		amount += cAmount;
		if (temp) average += (static_cast<float>(m_envGrid.potTemp[x + y * GRIDSIZESKYX]) * glm::pow(m_pressures[x + y * GRIDSIZESKYX] / m_groundGrid.P[x], Rsd / Cpd)) * cAmount;
		else average += m_envGrid.Qv[x + y * GRIDSIZESKYX] * cAmount;
	}
	for (int x = oX - 1; x > minLeft; x--)
	{
		if (isGround(x, y) || x < 0) break;

		const float distance = float((oX - x) * (oX - x) + (oY - y) * (oY - y));
		float cAmount = -(distance / MDistanceSqr - 1);
		if (abs(cAmount) > 1 || cAmount <= 0.0f) continue;
		//Smoothing
		cAmount = std::pow(cAmount, 5.0f);
		amount += cAmount;
		if (temp) average += (static_cast<float>(m_envGrid.potTemp[x + y * GRIDSIZESKYX]) * glm::pow(m_pressures[x + y * GRIDSIZESKYX] / m_groundGrid.P[x], Rsd / Cpd)) * cAmount;
		else average += m_envGrid.Qv[x + y * GRIDSIZESKYX] * cAmount;
	}
	if (amount == 0) return 0.0f;

	return average / amount;
}

glm::vec2 environment::vorticityConfinement(const int i)
{
	//Not anymore (Using) https://www.researchgate.net/publication/239547604_Modification_of_the_Euler_equations_for_vorticity_confinement''_Application_to_the_computation_of_interacting_vortex_rings
	//Now using https://sci-hub.se/downloads/2020-12-24/50/10.1080@10618562.2020.1856822.pdf

	const float epsilon = 0.08f; //to change the strength of the vorticity confinement
	const float dynVisc = 1.8e-5f;
	const float density = 1.225f; //Air density

	const float center = curl(i, true);
	const float left  = m_NeighData[i].left  == OUTSIDE ? center : m_NeighData[i].left == GROUND ? center : curl(i - 1);	// Neumann
	const float right = m_NeighData[i].right == OUTSIDE ? center : m_NeighData[i].right == GROUND ? center : curl(i + 1);	// Neumann
	const float up    = m_NeighData[i].up    != SKY ? center : curl(i + GRIDSIZESKYX); // Neumann
	const float down  = m_NeighData[i].down  != SKY ? center : curl(i - GRIDSIZESKYX); // Neumann

	// stress tensor (incompressible) http://www.astro.yale.edu/vdbosch/astro320_summary6.pdf
	const glm::vec2 stressTensor = dynVisc * lap(i);

	const glm::vec2 gradient = {  (right - left) / (2.0f * VOXELSIZE),
								 (up - down) / (2.0f * VOXELSIZE) };


	//TODO: in 3D this part is different, could look at the fluid paper.
	const float magnitude = glm::length(gradient);
	if (magnitude < 1e-6f)
	{
		return stressTensor;
	}

	const glm::vec2 nablaVector = gradient / magnitude;	

	//we rotate it just 90 degrees to get the perpindicular vector
	const glm::vec2 s = glm::vec2(-nablaVector.y, nablaVector.x); //Use cross(?) in 3D

	const float magGradient = glm::sqrt(gradient.x * gradient.x + gradient.y * gradient.y);

	const float confinementParam = epsilon * glm::sqrt((VOXELSIZE * VOXELSIZE) + (VOXELSIZE * VOXELSIZE)) * magGradient;


	glm::vec2 result = stressTensor - density * confinementParam * s;
	if (result != result)
	{
		result = glm::vec2{ 0,0 };
	}
	return result;
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

	//If mainpoint is outside the barrier, we just use Neumann on the old value, thus returned false here already.

	const int index00 = int(indexFull.x) + int(indexFull.y) * GRIDSIZESKYX;
	const int index10 = index00 + 1;
	const int index01 = index00 + GRIDSIZESKYX; 
	const int index11 = index01 + 1;

	const glm::vec2 v00 = outside(index00) || isGround(int(indexFull.x), int(indexFull.y))     ? glm::vec2(0.0f) : m_envGrid.velField[index00];
	const glm::vec2 v10 = outside(index10) || isGround(int(indexFull.x) + 1, int(indexFull.y)) ? glm::vec2(0.0f) : m_envGrid.velField[index10];
	const glm::vec2 v01 = outside(index01) ? glm::vec2(m_envGrid.velField[index01].x, 0.0f) : isGround(int(indexFull.x), int(indexFull.y + 1)) ? glm::vec2(0.0f) : m_envGrid.velField[index01];
	const glm::vec2 v11 = outside(index11) ? glm::vec2(m_envGrid.velField[index11].x, 0.0f) : isGround(int(indexFull.x) + 1, int(indexFull.y + 1)) ? glm::vec2(0.0f) : m_envGrid.velField[index11];

	//Interpolate using bilinear interpolation, could be compressed
	if (U)
	{
		//----U----
		float interpX = ((coord1.x + 1) - Ppos.x) / ((coord1.x + 1) - coord1.x) * v00.x +
			(Ppos.x - coord1.x) / ((coord1.x + 1) - coord1.x) * v10.x;
		float interpX2 = ((coord1.x + 1) - Ppos.x) / ((coord1.x + 1) - coord1.x) * v01.x +
			(Ppos.x - coord1.x) / ((coord1.x + 1) - coord1.x) * v11.x;

		output = ((coord1.y + 1) - Ppos.y) / ((coord1.y + 1) - coord1.y) * interpX +
			(Ppos.y - coord1.y) / ((coord1.y + 1) - coord1.y) * interpX2;
		return true;
	}
	else
	{
		//----V----
		float interpX = ((coord1.x + 1) - Ppos.x) / ((coord1.x + 1) - coord1.x) * v00.y +
			(Ppos.x - coord1.x) / ((coord1.x + 1) - coord1.x) * v10.y;
		float interpX2 = ((coord1.x + 1) - Ppos.x) / ((coord1.x + 1) - coord1.x) * v01.y +
			(Ppos.x - coord1.x) / ((coord1.x + 1) - coord1.x) * v11.y;

		output = ((coord1.y + 1) - Ppos.y) / ((coord1.y + 1) - coord1.y) * interpX +
			(Ppos.y - coord1.y) / ((coord1.y + 1) - coord1.y) * interpX2;
		return true;
	}
}

void environment::pressureProjectVelField(const float )
{
	std::vector<float> presProjections;
	presProjections.resize(GRIDSIZESKY);
	calculatePresProj(presProjections);

	//Debug
	std::vector<float> presProjectionsDebug;
	presProjectionsDebug.resize(GRIDSIZESKY);


	for (int i = 0; i < GRIDSIZESKY; i++)
	{
		if (isGround(i)) continue;

		const float NxPresProj = m_NeighData[i].right == OUTSIDE ? 0.0f : m_NeighData[i].right == GROUND ? presProjections[i] : presProjections[i + 1];
		const float NyPresProj = m_NeighData[i].up != SKY ? presProjections[i] : presProjections[i + GRIDSIZESKYX];

		const float scale = 1.0f;

		m_envGrid.velField[i].x += scale * (NxPresProj - presProjections[i]) ;
		m_envGrid.velField[i].y += scale * (NyPresProj - presProjections[i]) ;
		presProjectionsDebug[i] = scale * (NyPresProj - presProjections[i]) ; //Debug
		presProjections[i] = scale * (NxPresProj - presProjections[i]) ; //Debug
	}
	//setDebugArray(presProjections, 1); // X
	//setDebugArray(presProjectionsDebug, 0); // Y
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
	float tolValue = 1e-6f;
	const int MAXITERATION = 100;

	divergence.resize(GRIDSIZESKY);
	precon.resize(GRIDSIZESKY);
	q.resize(GRIDSIZESKY);
	z.resize(GRIDSIZESKY);
	s.resize(GRIDSIZESKY);
	p.assign(p.size(), 0.0f);
	r.resize(GRIDSIZESKY);
	A.resize(GRIDSIZESKY);

	std::vector<float> debugVector;
	std::vector<float> debugVector2;
	debugVector.resize(GRIDSIZESKY);
	debugVector2.resize(GRIDSIZESKY);



	//Fill matrix A	
	for (int y = 0; y < GRIDSIZESKYY; y++) 
	{
		for (int x = 0; x < GRIDSIZESKYX; x++) 
		{
			if (isGround(x, y)) continue; 
			//Set -1 for all x direction for neumann
			A[x + y * GRIDSIZESKYX].x = x == GRIDSIZESKYX - 1 || !isGround(x + 1, y) ? -1 : 0; //If outside or is not ground, set -1. 
			A[x + y * GRIDSIZESKYX].y = y < GRIDSIZESKYY - 1 && !isGround(x, y + 1) ? -1 : 0;
			A[x + y * GRIDSIZESKYX].z = 0; 

			if (x == GRIDSIZESKYX - 1 || !isGround(x + 1, y)) A[x + y * GRIDSIZESKYX].z++;
			if (y < GRIDSIZESKYY - 1 && !isGround(x, y + 1)) A[x + y * GRIDSIZESKYX].z++;
			if (x == 0 || !isGround(x - 1, y)) A[x + y * GRIDSIZESKYX].z++;
			if (y > 0 && !isGround(x, y - 1)) A[x + y * GRIDSIZESKYX].z++;
		}
	}


	calculateDivergence(divergence);
	r = divergence; 

	setDebugArray(divergence, 0);

	//Checking max residual 
	{
		for (int i = 0; i < GRIDSIZESKY; i++)
		{
			if (isGround(i)) continue;
			maxr = std::max(maxr, fabsf(r[i]));
		}
		if (maxr == 0) return;
	}

	calculatePrecon(precon, A);
	applyPreconditioner(precon, r, A, q, z);

	//setDebugArray(s, 1);
	s = z;

	//Dotproduct
	{
		for (int i = 0; i < GRIDSIZESKY; i++)
		{
			if (isGround(i)) continue;
			sigma += z[i] * r[i];
		}
	}

	//Loop until done or max iteration reached
	MAXITERATION;
	for (int i = 0; i < MAXITERATION; i++)
	{
		applyA(s, A, z);
		//for (int j = 0; j < GRIDSIZESKY; j++) debugVector[j] = z[j] * 1e8f;
		//setDebugArray(debugVector, 0);

		//Dotproduct
		sigmaNew = 0;
		for (int j = 0; j < GRIDSIZESKY; j++)
		{
			if (isGround(j)) continue;
			sigmaNew += z[j] * s[j];
		}
		sigmaNew = sigma / sigmaNew;

		float totalP = 0.0f;
		//Adding up pressure value and reducing residual value
		for (int j = 0; j < GRIDSIZESKY; j++)
		{
			if (isGround(j)) continue;
			p[j] += sigmaNew * s[j];
			totalP += p[j];
			r[j] -= sigmaNew * z[j];
		}

		//Checking max residual 
		maxr = 0;
		for (int j = 0; j < GRIDSIZESKY; j++)
		{
			if (isGround(j)) continue;
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
			if (isGround(j)) continue;
			sigmaNew += z[j] * r[j];
		}
		B = sigmaNew / sigma;


		//Setting search vector s
		for (int j= 0; j < GRIDSIZESKY; j++)
		{
			if (isGround(j)) continue;
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
		if (isGround(i)) continue;
		float LRout = 1.0f;
		const float Ucurr = m_NeighData[i].right == OUTSIDE ? m_envGrid.velField[i].x : m_NeighData[i].right == GROUND ? 0.0f : m_envGrid.velField[i].x; //Using Neumann boundary condition (or no-slip if ground)
		const float Umin1 = m_NeighData[i].left == OUTSIDE || i == 0 ? LRout = 0.0f : m_NeighData[i].left == GROUND ? 0.0f : m_envGrid.velField[i - 1].x; //Using Neumann boundary condition (or no-slip if ground)
		const float Vcurr = m_envGrid.velField[i].y; //Using free-slip boundary condition for Sky (this will be 0, because this velocity should be already set to 0)
		const float Vmin1 = (m_NeighData[i].down == SKY && i - GRIDSIZESKYX >= 0) ? m_envGrid.velField[i - GRIDSIZESKYX].y : 0.0f; //Using no-slip boundary condition for ground

		//Note that we set divergence to 0 if on the left or right boundary, this essentially just says: if divergence on boundary, add or subtract flux from the boundary so that divergence will be none.
		//Essentially just making the program decide how much it needs to pull from the left or right side
		output[i] = ((Ucurr - Umin1) + (Vcurr - Vmin1)) * LRout;
	}
}

void environment::calculatePrecon(std::vector<float>& output, std::vector<glm::ivec3>& A)
{
	const float Tune = 0.97f;

	for (int y = 0; y < GRIDSIZESKYY; y++)
	{
		for (int x = 0; x < GRIDSIZESKYX; x++)
		{
			const int idx = x + y * GRIDSIZESKYX;
			if (isGround(x, y))
			{
				output[idx] = 0.0f;
				continue;
			}
			//TODO: for 3D, look at formula 4.34		
			int Aminxx = x == 0 ? 0 : A[idx - 1].x; //Minus X looking at x
			int Aminxy = x == 0 ? 0 : A[idx - 1].y; //Minus X looking at y
			int Aminyy = y == 0 ? 0 : A[idx - GRIDSIZESKYX].y; //Minus Y looking at y
			int Aminyx = y == 0 ? 0 : A[idx - GRIDSIZESKYX].x; //Minus Y looking at x
			
			const float Preconi = x == 0 ? output[idx] : output[idx - 1];
			const float Preconj = y == 0 ? output[idx] : output[idx - GRIDSIZESKYX];
			
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
			if (isGround(x, y)) continue;

			const int idx = x + y * GRIDSIZESKYX;

			const int Aminxx = x == 0 ? 0 : A[idx - 1].x; //Minus X looking at x
			const int Aminyy = y == 0 ? 0 : A[idx - GRIDSIZESKYX].y; //Minus Y looking at y

			const float qpi = x == 0 ? q[idx] : q[x - 1 + y * GRIDSIZESKYX];
			const float qpj = y == 0 ? q[idx] : q[x + (y - 1) * GRIDSIZESKYX];

			const float preconCenter = precon[x + y * GRIDSIZESKYX];
			const float Preconi = x == 0 ? preconCenter : precon[x - 1 + y * GRIDSIZESKYX];
			const float Preconj = y == 0 ? preconCenter : precon[x + (y - 1) * GRIDSIZESKYX];

			const float t = r[x + y * GRIDSIZESKYX] - (Aminxx * Preconi * qpi) - (Aminyy * Preconj * qpj);
			q[x + y * GRIDSIZESKYX] = t * preconCenter;
		}
	}


	//Solve L^Tp = q
	for (int y = GRIDSIZESKYY - 1; y >= 0; y--)
	{
		for (int x = GRIDSIZESKYX - 1; x >= 0; x--)
		{
			if (isGround(x, y)) continue;

			const int idx = x + y * GRIDSIZESKYX;

			const float zpi = x + 1 >= GRIDSIZESKYX ? output[idx] : output[x + 1 + y * GRIDSIZESKYX];
			const float zpj = y + 1 >= GRIDSIZESKYY ? output[idx] : output[x + (y + 1) * GRIDSIZESKYX];

			const float preconCenter = precon[x + y * GRIDSIZESKYX];
			const float Preconi = x + 1 >= GRIDSIZESKYX ? preconCenter : precon[x + 1 + y * GRIDSIZESKYX];
			const float Preconj = y + 1 >= GRIDSIZESKYY ? preconCenter : precon[x + (y + 1) * GRIDSIZESKYX];

			const float t = q[x + y * GRIDSIZESKYX] - (A[idx].x * Preconi * zpi) - (A[idx].y * Preconj * zpj);

			output[x + y * GRIDSIZESKYX] = t * preconCenter;	
		}
	}
	//setDebugArray(output);
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
			if (isGround(x, y)) continue;

			const int idx = x + y * GRIDSIZESKYX;

			const int Aminx = x == 0 ? 0 : A[idx - 1].x; //Minus X looking at x
			const int Aminy = y == 0 ? 0 : A[idx - GRIDSIZESKYX].y; //Minus Y looking at y
			const int Aplusx = x == GRIDSIZESKYX - 1 ? 0 : A[idx].x; //looking at x (Why 0?)
			const int Aplusy = y == GRIDSIZESKYY - 1 ? 0 : A[idx].y; //looking at y
			int Adiag =  A[idx].z;

			const float Sminx = x == 0 ? s[idx] : s[idx - 1]; 
			const float Sminy = y == 0 ? s[idx] : s[idx - GRIDSIZESKYX];
			const float Splusx = x == GRIDSIZESKYX - 1 ? s[idx] : s[idx + 1];//Why does this need to be 0?
			const float Splusy = y == GRIDSIZESKYY - 1 ? s[idx] : s[idx + GRIDSIZESKYX];

			z[idx] = (Adiag * s[idx] +
					((Aminx * Sminx +
					  Aminy * Sminy +
					  Aplusx * Splusx +
					  Aplusy * Splusy)
					));
		}
	}
}

glm::vec3 environment::calculateFallingVelocity(const float , const int i, const float densAir)
{
	//TODO: could make switch function to make precip specific (since its cost heavier now)
	
	//Densities in g/cm3
	const float densI = 0.91f;

	//constants
	const float b = 0.8f;
	const float a = 2115.0f;
	const float d = 0.25f;
	const float c = 152.93f;
	const float CD = 0.6f; //Drag coefficient

	float slopeR = meteoformulas::slopePrecip(densAir, m_envGrid.Qr[i], 0);
	float slopeS = meteoformulas::slopePrecip(densAir, m_envGrid.Qs[i], 1);
	float slopeI = meteoformulas::slopePrecip(densAir, m_envGrid.Qi[i], 2);

	const static float gammaR = meteoformulas::gamma(4.0f + b); //Save it, since it is a constant
	const static float gammaS = meteoformulas::gamma(4.0f + d); //Save it, since it is a constant
	const static float gammaI = meteoformulas::gamma(4.0f + 0.25f); //Save it, since it is a constant

	float UR = a * (gammaR / ( 6 * std::pow(slopeR, b))) * sqrt(1.225f / densAir);
	float US = c * (gammaS / (6 * std::pow(slopeS, d))) * sqrt(1.225f /  densAir);
	float UI = (gammaI / (6 * std::pow(slopeI, 0.5f))) * pow(4 * g * 100 * densI / (3 * CD * densAir * 0.001f), 0.5f); //Converting g to cm/s2 and densAir to g/cm3

	UR *= 0.01f; //Convert cm to m
	US *= 0.01f; //Convert cm to m
	UI *= 0.01f; //Convert cm to m

	return glm::vec3(UR, US, UI);
}

void environment::updateMicroPhysics2(const float dt, const int i, const float* ps, const float T, const float density)
{
	const int idxX = i % GRIDSIZESKYX;

	glm::vec3 fallVel{ 0.0f };
	if (m_envGrid.Qr[i] > 0.0f || m_envGrid.Qs[i] > 0.0f || m_envGrid.Qi[i] > 0.0f) fallVel = calculateFallingVelocity(dt, i, density);

	microPhys::microPhysResult MResult = microPhys::microPhysResult(m_envGrid.Qv[i], m_envGrid.Qw[i], m_envGrid.Qc[i], m_envGrid.Qr[i], m_envGrid.Qs[i], m_envGrid.Qi[i],
		dt, m_speed, i, T, ps[i], density, m_GHeight[idxX], fallVel);

	Game.mPhys().calculateEnvMicroPhysics(MResult);


	//Ground addition
	{
		const bool atGround = i / GRIDSIZEGROUND - 1 == m_GHeight[idxX];
		if (atGround)
		{
			float GR{ 0.0f };
			float GS{ 0.0f };
			float GI{ 0.0f };
			float EG{ 0.0f };

			GR = m_envGrid.Qr[i];
			GS = m_envGrid.Qs[i];
			GI = m_envGrid.Qi[i];

			float BEG{ 200.0f };	//NotSure		  // Evaporation rate of dry ground
			const float D_ = 1e-6f; // Weigthed mean diffusivity of the ground //TODO: hmm, could tripple check if right
			const float secsInDay = 60 * 60 * 24;
			const float O_ = 0.21f * secsInDay; // evaporative ground water storage coefficient (i.e. only part of the soil can be evaporated) https://en.wikipedia.org/wiki/Specific_storage

			//Note that this is just picked from water storage coefficient which may not is the same as the evaporative soil water storage coefficient.
			//Only if Qgj = 0 (Precip falling) and if we are at the ground

			if (m_groundGrid.Qgr[idxX] == 0 && m_groundGrid.Qgs[idxX] == 0 && m_groundGrid.Qgi[idxX] == 0)
			{
				//https://agupubs.onlinelibrary.wiley.com/doi/full/10.1002/2013WR014872
				const float waterA = m_groundGrid.Qrs[idxX];
				const float time = m_groundGrid.t[idxX];
				EG = BEG * D_ * waterA * exp(-time / O_);
			}
			else
			{
				m_groundGrid.t[idxX] = 0;
			}


			//Limit
			GR = dt * std::min(GR * m_speed, m_envGrid.Qr[i]);
			GS = dt * std::min(GS * m_speed, m_envGrid.Qs[i]);
			GI = dt * std::min(GI * m_speed, m_envGrid.Qi[i]);
			MResult.condens -= EG = dt * EG * m_speed; //No need to limit (yet)

			//Add to ground
			m_groundGrid.Qgr[idxX] += GR;
			m_groundGrid.Qgs[idxX] += GS;
			m_groundGrid.Qgi[idxX] += GI;

			//Add to data
			MResult.Qr -= GR;
			MResult.Qs -= GS;
			MResult.Qi -= GI;
			MResult.Qv += EG;
		}
	}

	//Set data
	m_envGrid.Qv[i] += MResult.Qv;
	m_envGrid.Qw[i] += MResult.Qw;
	m_envGrid.Qc[i] += MResult.Qc;
	m_envGrid.Qr[i] += MResult.Qr;
	m_envGrid.Qs[i] += MResult.Qs;
	m_envGrid.Qi[i] += MResult.Qi;

	m_condens = MResult.condens;
	m_freeze = MResult.freeze;
	m_depos = MResult.depos;

	//Check for certainty
	if (m_envGrid.Qv[i] < 0.0f || m_envGrid.Qw[i] < 0.0f || m_envGrid.Qc[i] < 0.0f || m_envGrid.Qr[i] < 0.0f || m_envGrid.Qs[i] < 0.0f || m_envGrid.Qi[i] < 0.0f)
	{
		m_freeze = 0.0f;
	}
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

	float EW_min_CW = 0.0f; // Difference between evaporation of water vapor and condensation of water vapor
	float BW = 0.0f; // Ice growth at the cost of cloud water
	float FW = 0.0f; // Homogeneous freezing (instant freezing below -40)
	float MC = 0.0f; // Melting (ice to liquid)
	float DC_min_SC = 0.0f; // Difference between sublimation of ice to vapor and deposition of vapor to ice
	float AW = 0.0f; // Autoconversion to rain. droplets being big enough to fall
	float KW = 0.0f; // Collection of cloud water (droplets growing by taking other droplets)
	float AC = 0.0f; // Autoconversion to snow. ice forming snow
	float KCW = 0.0f; // Gradually collection of cloud matter by snow
	float KCI = 0.0f; // Gradually collection of Ice matter by snow
	float RW = 0.0f; // TODO: WRONG. Growth of ice (precip) by hitting ice crystals and cloud liquid water
	float FKI = 0.0f; // Growth of ice (precip) or snow by rain hitting ice clouds (decrease cloud ice).
	float FKR = 0.0f; // Growth of ice (precip) or snow by rain hitting ice clouds (decrease rain).
	float RS = 0.0f; // Growth of ice (precip) by hitting snow 
	float RRS = 0.0f; // Accretion rate of rain by snow (takes away snow)
	float RSR = 0.0f; // Accretion rate of snow by rain (takes away rain)
	float FR = 0.0f; // Growth of ice (precip) by freezing rain
	float MS = 0.0f; // Snow melting
	float MI = 0.0f; // Ice (precip) melting
	float ER = 0.0f; // Evaporation of rain
	float ES = 0.0f; // Evaporation(Sublimation) of snow
	float DS = 0.0f; // Deposition to snow (Almost same term as ES)
	float EI = 0.0f; // Evaporation of ice (precip)
	float GR = 0.0f; // Rain hit ground
	float GS = 0.0f; // Snow hit ground
	float GI = 0.0f; // Ice (precip) hit ground
	float EG = 0.0f; // Evaporation of dry ground

	const float QWS = meteoformulas::ws((T - 273.15f), ps[i]); //Maximum water vapor air can hold
	const float QWI = meteoformulas::wi((T - 273.15f), ps[i]); //Maximum water vapor cold air can hold

	//Production terms, used to sometimes create snow or ice or other stuff (formula 20): https://research.csiro.au/ccam/wp-content/uploads/sites/520/2024/01/1377337420.pdf 
	const float PTerm1 = T < 273.15f && m_envGrid.Qw[i] + m_envGrid.Qc[i] > 0 + 1e-11f ? 1.0f : 0.0f;
	const float PTerm2 = T < 273.15f && m_envGrid.Qr[i] < 1e-4f && m_envGrid.Qs[i] < 1e-4f ? 1.0f : 0.0f;
	const float PTerm3 = T < 273.15f && m_envGrid.Qr[i] < 1e-4f ? 1.0f : 0.0f;

	const int idxX = i % GRIDSIZESKYX;

	const bool atGround = i / GRIDSIZEGROUND - 1 == m_GHeight[idxX];

	if (T >= -40 + 273.15f)
	{
		//Don't convert if vapor is not overflowing, max by vapor.
		//EW_min_CW = std::max(std::min(QWS - m_envGrid.Qv[i], 0.0f), -m_envGrid.Qv[i]);

		//Condensation rate https://www.ecmwf.int/sites/default/files/elibrary/2002/16952-parametrization-non-convective-condensation-processes.pdf
		{
			const float _ES = meteoformulas::es(T - 273.15f) * 10.0f;
			const float Tc = T - 273.15f;
			const float derivative = ((E * ps[i]) / ((ps[i] - _ES) * (ps[i] - _ES))) * (_ES * (17.27f * 237.3f / ((Tc + 237.3f) * (Tc + 237.3f))));
			const float Cr = 1 / (dt * m_speed) * (m_envGrid.Qv[i] - QWS) / (1 + Constants::E0v / Constants::Cpd * derivative);
			EW_min_CW = -std::min(std::max(Cr, -m_envGrid.Qw[i]), m_envGrid.Qv[i]);
			
		}


		if (T <= 0.0f + 273.15f)
		{
			//Formula from https://journals.ametsoc.org/view/journals/mwre/128/4/1520-0493_2000_128_1070_asfcot_2.0.co_2.xml and WeatherScapes
			const float a = 0.5f; // capacitance for hexagonal crystals
			const float quu = std::max(1e-12f * meteoformulas::Ni(T - 273.15f) / D, m_envGrid.Qc[i]);
			BW = std::min(m_envGrid.Qw[i], pow((1 - a) * meteoformulas::cvd(T - 273.15f, ps[i], D) * dt + pow(quu, 1 - a), 1 / (1 - a)) - m_envGrid.Qc[i]);
		}
	}
	if (T < -40 + 273.15f)
	{
		FW = m_envGrid.Qw[i];
	}
	if (T > 0 + 273.15f) //Melting
	{
		// 𝛿(𝑋𝑐 + 𝑋𝑠 + 𝑋𝑖) ≤ 𝑐𝑝air / 𝐿𝑓 * 𝑇, == First melt cloud ice, then snow, then precip ice, when the criteria is met between any step, stop melting.

		const float BMI{ 5e-4f };					  // Aggregation rate of ice to rain rate coefficient:

		const float check = Cpd / Lf * (T - 273.15f);
		float heatSum = 0.0f;

		const float Qc = m_envGrid.Qc[i];
		const float Qs = m_envGrid.Qs[i];
		const float Qi = m_envGrid.Qi[i];
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
			//PSMLT by https://research.csiro.au/ccam/wp-content/uploads/sites/520/2024/01/1377337420.pdf
						//How many of this particle
			const float N0S = 3e-2f;
			const float slopeS = meteoformulas::slopePrecip(D, m_envGrid.Qs[i], 1);
			//constants
			const float c = 152.93f;
			const float d = 0.25f;

			const float dQv = meteoformulas::DQVair(T - 273.15f, ps[i]); //Diffusivity of water vapor in air
			const float kv = meteoformulas::ViscAir(T - 273.15f); //Kinematic viscosity of air
			float Sc = kv / dQv; // Schmidt number (kv /dQv)
			const static float gammaDS = meteoformulas::gamma((d + 5.0f) / 2); //Save it, since it is a constant
			const float Tc = T - 273.15f;
			const float Drs = meteoformulas::ws(0.0f, ps[i]) - m_envGrid.Qv[i];

			const float density = pow(D / 1.225f, 0.25f);

			float PSMLT = 0.0f;

			PSMLT = -(2 * PI / (density * 0.001f * Lf)) * (Ka * Tc - E0s * dQv * density * 0.001f * Drs) * N0S *
				(0.78f * powf(slopeS, -2) + 0.31f * powf(Sc, 1.0f / 3.0f) * gammaDS * powf(c, 0.5f) * density * powf(kv, -0.5f) * powf(slopeS, -(d + 5) / 2))
				- (Constants::Cvl * Tc / Lf); //TODO: Adding PSACW + PSACR later will result in heatSum not being right.

			MS = std::max(0.0f, PSMLT);
			heatSum += PSMLT;
		}
		if (Qi > 0.0f && heatSum < check)
		{
			MI = std::min(MIt, std::min(Qi, check - heatSum));
		}
	}
	if (T <= 0 + 273.15f)
	{
		//Don't convert if vapor is not overflowing, max by vapor.
		//DC_min_SC = std::max(std::min(QWI - m_envGrid.Qv[i], 0.0f), -m_envGrid.Qv[i]);

		//Deposition rate based on https://www.ecmwf.int/sites/default/files/elibrary/2002/16952-parametrization-non-convective-condensation-processes.pdf
		{
			const float _EI = meteoformulas::ei(T - 273.15f) * 10.0f;
			const float Tc = T - 273.15f;
			const float derivative = ((E * ps[i]) / ((ps[i] - _EI) * (ps[i] - _EI))) * (_EI * (21.875f * 265.5f / ((Tc + 265.5f) * (Tc + 265.5f))));
			const float Cr = 1 / (dt * m_speed) * (m_envGrid.Qv[i] - QWI) / (1 + Constants::Ls / Constants::Cpd * derivative);
			DC_min_SC = -std::min(std::max(Cr, -m_envGrid.Qc[i]), m_envGrid.Qv[i]);

		}
	}
	{
		//Aggration rates
		float BAC{ 0.0f }; // Aggregation rate of ice to snow rate coefficient:
		float BAW{ 0.0f };	// Aggregation rate of autoconversion of rain rate coefficient:
		float BKW{ 0.0f };	// Aggregation rate of rain hitting cloud rate coefficient:
		float BFKI{ 0.0f };	// Accretion rate of rain hitting ice cloud rate coefficient: (take away ice cloud)
		float BFKR{ 0.0f };	// Accretion rate of rain hitting ice cloud rate coefficient: (take away rain)
		float BKCW{ 0.0f };	// Aggregation rate of warm cloud to snow rate coefficient:
		float BKCI{ 0.0f };	// Aggregation rate of cold cloud to snow rate coefficient:
		float BRW{ 2e-3f };	//NotSure		  // Aggregation rate of ice cloud to ice (precip) rate coefficient:
		float BRS{ 0.0f };	// Aggregation rate of ice hitting snow rate coefficient:
		float BFR{ 1e-3f };	//NotSure		  // Aggregation rate of freezing rain to ice (precip) rate coefficient:
		float BER{ 0.0f };	// Aggregation rate of rain to vapor rate coefficient:
		float BES{ 0.0f };	// Aggregation rate of snow to vapor rate coefficient:
		float BDS{ 0.0f };	// Deposition rate of vapor to snow rate coefficient: (almost same as BES term)
		float BEI{ 5e-4f };	//NotSure		  // Aggregation rate of ice to vapor rate coefficient:
		float BEG{ 200.0f };	//NotSure		  // Evaporation rate of dry ground
		float BRRS{ 0.0f }; // Accretion of rain by snow (takes away snow)
		float BRSR{ 0.0f }; // Accrestion of snow by rain (takes away rain)

		const float qwmin = 0.001f; // the minimum cloud water content required before rainmaking begins
		const float qcmin = 0.001f; // the minimum cloud ice content required before snowmaking begins

		glm::vec3 fallVel{ 0.0f };
		if (m_envGrid.Qr[i] > 0.0f || m_envGrid.Qs[i] > 0.0f || m_envGrid.Qi[i] > 0.0f) fallVel = calculateFallingVelocity(dt, i, D);

		//Rate coefficients
		const float a1 = 1e-3f * exp(0.025f * (T - 273.15f));
		const float a2 = 1e-3f * exp(0.09f * (T - 273.15f));

		//Ice to Snow
		if (m_envGrid.Qw[i] >= qcmin)
		{
			// Rate coefficient found in https://journals.ametsoc.org/view/journals/apme/22/6/1520-0450_1983_022_1065_bpotsf_2_0_co_2.xml
			BAC = 1e-3f * exp(0.025f * (T - 273.15f)); // Aggregation rate of ice to snow rate coefficient:
		}

		//Cloud to rain due to autoconversion
		if (m_envGrid.Qw[i] >= qwmin)
		{
			const float rc = 10e-6f; // Estimated cloud droplet size, averages between 4 and 12 μm;
			float Nc = (m_envGrid.Qw[i] / (4 / 3 * PI * meteoformulas::pwater(T - 273.15f) * rc * rc * rc)) * 1.225f; //Cloud number concentation in m3
			Nc /= 1e6f; //Convert to how many droplets would be in cm3 instead of m3
			const float disE = 0.5f * 0.5f; //following Liu et al. (2006)
			//Normally its ^ 1 / 6, but in the formula we will be using its ^6, so it cancells out.
			const float dispersion = ((1 + 3 * disE) * (1 + 4 * disE) * (1 + 5 * disE)) / ((1 + disE) * (1 + 2 * disE)); //the relative dispersion ε of cloud droplets, 
			const float kBAW = 1.1e10f; //Constant k in g-2/cm3/s-1
			const float shapeParam = 1.0f; //Shape of tail of drop, μ
			const float L = m_envGrid.Qw[i] * 1.225f * 1e-3f; //From kg/kg to g/cm3

			// Aggregation rate of liquid to rain rate coefficient: // Liu–Daum–McGraw–Wood (LD) scheme: https://journals.ametsoc.org/view/journals/atsc/63/3/jas3675.1.xml
			BAW = kBAW * dispersion * (L * L * L) * std::powf(Nc, -1) *
				static_cast<float>(1 - std::exp(-std::pow(1.03e16f * std::pow(Nc, -2.0f / 3.0f) * (L * L), shapeParam)));
			BAW *= 1e3f; //To kg/m3
		}

		//Cloud to rain due to collection and rain to snow/hail due to 3 component freezing
		if (m_envGrid.Qr[i] > 0 && (m_envGrid.Qw[i] > 0 || m_envGrid.Qc[i] > 0.0f))
		{
			//Formula from https://journals.ametsoc.org/view/journals/apme/22/6/1520-0450_1983_022_1065_bpotsf_2_0_co_2.xml
			//How many of this particle are in this region
			const float N0R = 8e-2f;
			//Densities in g/cm3
			const float densW = 0.99f;
			//constants
			const float b = 0.8f;
			const float a = 2115.0f;
			const float MassI = 4.19e-10f; //Mass ice in gram
			//collection efficiency rain from cloud ice and cloud water
			const float ERI = 0.3f;
			const float ERW = 1.0f;

			const float slopeR = meteoformulas::slopePrecip(D, m_envGrid.Qr[i], 0);
			const static float gammaR = meteoformulas::gamma(3.0f + b); //Save it, since it is a constant
			const static float gammaRC = meteoformulas::gamma(6.0f + b); //Save it, since it is a constant

			float PIACR = 0.0f;
			float PRACI = 0.0f;
			float PRACW = 0.0f;

			const float density = pow(D / 1.225f, 0.5f);

			//Collection from cloud ice to form snow/hail
			if (m_envGrid.Qc[i] > 0.0f)
			{
				PRACI = (PI * ERI * N0R * a * m_envGrid.Qc[i] * gammaR) / (4 * pow(slopeR, 3 + b)) * density;
				PIACR = (PI * PI * ERI * N0R * a * m_envGrid.Qc[i] * densW * gammaRC) / (24 * MassI * pow(slopeR, 6 + b)) * density;
			}

			//Collection from cloud water
			if (m_envGrid.Qw[i] > 0.0f) PRACW = (PI * ERW * N0R * a * m_envGrid.Qw[i] * gammaR) / (4 * pow(slopeR, 3 + b)) * density;

			BKW = PRACW;
			BFKI = PRACI;
			BFKR = PIACR;
		}

		//Cloud to snow due to collection or autoconversion
		if (m_envGrid.Qs[i] > 0 && (m_envGrid.Qw[i] > 0 || m_envGrid.Qc[i] > 0))
		{
			//Formula from https://journals.ametsoc.org/view/journals/apme/22/6/1520-0450_1983_022_1065_bpotsf_2_0_co_2.xml
			//Correction from https://ntrs.nasa.gov/api/citations/19990100647/downloads/19990100647.pdf
			//How many of this particle are in this region
			const float N0S = 3e-2f;
			//constants
			const float c = 152.93f;
			const float d = 0.25f;
			//collection efficiency snow from cloud ice and cloud water
			const float ESI = a1 * 1e3f;
			const float ESW = 1.0f;
			//const float EIW = 1.0f;

			//radius, mass and terminal velocity of 50 or 40 micrometer size ice crystal to cm or gram
			//const float RI50 = 5e-3f;
			const float mI50 = 4.8e-7f;
			const float mI40 = 2.46e-7f;
			//const float UI50 = 100.0f;

			const float slopeS = meteoformulas::slopePrecip(D, m_envGrid.Qs[i], 1);
			const static float gammaS = meteoformulas::gamma(3.0f + d); //Save it, since it is a constant

			//Formula for A and B https://journals.ametsoc.org/view/journals/atsc/40/5/1520-0469_1983_040_1185_tmamsa_2_0_co_2.xml?tab_body=pdf
			const float density = pow(D / 1.225f, 0.5f);
			float Si = m_envGrid.Qv[i] / QWI; // Saturation ratio over ice
			const float X = meteoformulas::DQVair(T - 273.15f, ps[i]);
			const float A = Constants::E0v / (Constants::Ka * T) * (Constants::Ls * Constants::Mw * 0.001f / (Constants::R * T) - 1); //Mw to kg/mol
			const float B = Constants::R * T * X * Constants::Mw * 0.001f * meteoformulas::ei(T - 273.15f) * 1000; //kPa to Pa

			float PSACI = 0.0f;
			float PSACW = 0.0f;
			//Based on https://journals.ametsoc.org/view/journals/apme/19/8/1520-0450_1980_019_0950_nsoipc_2_0_co_2.xml?tab_body=pdf
			float PSFW = 0.0f; //Growth of snow by transforming water cloud to snow by deposition and riming
			float PSFI = 0.0f; //Growth of snow by transforming ice cloud to snow by deposition and riming

			const float a1g = 206.2f * (Si - 1) / (A + B);
			const float a2g = exp(0.09f * (T - 273.15f));

			//const float dt1 = 1 / (a1g * (1 - a2g)) * (pow(mI50, 1 - a2g) - pow(mI40, 1 - a2g));

			//Collection from cloud ice
			if (m_envGrid.Qc[i] > 0.0f)
			{
				PSACI = (PI * ESI * N0S * c * m_envGrid.Qc[i] * gammaS) / (4 * pow(slopeS, 3 + d)) * density;
				if (a1g > 0) PSFI = a2g * a1g * m_envGrid.Qi[i] / (pow(mI50, 0.5f) - pow(mI40, 0.5f));
			}

			//Collection from cloud water
			if (m_envGrid.Qw[i] > 0.0f)
			{
				PSACW = (PI * ESW * N0S * c * m_envGrid.Qw[i] * gammaS) / (4 * pow(slopeS, 3 + d)) * density;
				//const float Qc50 = m_envGrid.Qc[i] * (dt / dt1) / 100; //Should vary between 0.5 and 10%
				//const float NI50 = Qc50 / mI50;
				//PSFW = NI50 * (a1g * pow(mI50, a2g) + PI * EIW * (D * 0.001f) * m_envGrid.Qw[i] * RI50 * RI50 * UI50);
			}

			BKCI = PSACI + PSFI;
			BKCW = PSACW + PSFW; //TODO: Do we need PSFW, (possibly the same as PSACW
		}

		//Hail hitting snow and snow forming hail
		if (m_envGrid.Qs[i] > 0.0f || m_envGrid.Qi[i] > 0.0f)
		{
			//Densities in g/cm3
			const float densS = 0.11f;
			//How many of this particle
			const float N0S = 3e-2f;
			const float N0I = 4e-4f;

			const float slopeS = meteoformulas::slopePrecip(D, m_envGrid.Qs[i], 1);
			const float slopeI = meteoformulas::slopePrecip(D, m_envGrid.Qi[i], 2);

			const float EGS = T >= 273.15f ? 1.0f : exp(0.09f * (T - 273.15f));
			float PGACS = 0.0f; //Hail hitting snow
			float PGAUT = 0.0f; //Snow autoconverting to form hail

			if (m_envGrid.Qs[i] > 0.0f && m_envGrid.Qi[i] > 0.0f) PGACS = PI * PI * EGS * N0S * N0I * fabs(fallVel.z - fallVel.y) * (densS / D) *
				(5 / (powf(slopeS, 6) * slopeI) + (2 / (powf(slopeS, 5) * powf(slopeI, 2))) + (0.5f / (powf(slopeS, 4) * powf(slopeI, 3))));

			if (m_envGrid.Qs[i] > 0.0f)
			{
				const float QS0 = 6e-4f; //Minimum snow needed to create ice.
				PGAUT = a2 * std::max(m_envGrid.Qs[i] - QS0, 0.0f);
			}

			BRS = PGACS + PGAUT;
		}

		// Snow creating rain and visa versa
		if (m_envGrid.Qr[i] > 0.0f && m_envGrid.Qs[i] > 0.0f)
		{
			//Densities in g/cm3
			const float densS = 0.11f;
			const float densW = 0.99f;
			//Intercept parameter size distribution in cm-4
			const float N0S = 3e-2f;
			const float N0R = 8e-2f;

			const float ESR = 1.0f;

			const float slopeR = meteoformulas::slopePrecip(D, m_envGrid.Qr[i], 0);
			const float slopeS = meteoformulas::slopePrecip(D, m_envGrid.Qs[i], 1);

			//If PTerm2 = 0, 3-component freezing to increase hail.
			//Else 2-component freezing, snow grows in expens of rain, only PSACR is used.
			//But if T > 0, PSACR is used to enhance PSMLT.

			float PRACS = 0.0f;
			float PSACR = 0.0f;

			const float size = PI * PI * ESR * N0R * N0S;

			if (PTerm2 == 0) PRACS = size * fabs(fallVel.x - fallVel.y) * (densS / D) *
				(5 / (powf(slopeS, 6) * slopeR) + (2 / (powf(slopeS, 5) * powf(slopeR, 2))) + (0.5f / (powf(slopeS, 4) * powf(slopeR, 3))));

			PSACR = size * fabs(fallVel.y - fallVel.x) * (densW / D) *
				(5 / (powf(slopeR, 6) * slopeS) + (2 / (powf(slopeR, 5) * powf(slopeS, 2))) + (0.5f / (powf(slopeR, 4) * powf(slopeS, 3))));
			

			BRRS = PRACS; //Used for 3 component freezing snow to hail.
			BRSR = PSACR; //Used in PSMLT or decreases rain
		}

		//Deposition/Sublimation of snow
		if (T < 273.15f)
		{
			//How many of this particle
			const float N0S = 3e-2f;
			const float slopeS = meteoformulas::slopePrecip(D, m_envGrid.Qs[i], 1);
			//constants
			const float c = 152.93f;
			const float d = 0.25f;

			const float dQv = meteoformulas::DQVair(T - 273.15f, ps[i]); //Diffusivity of water vapor in air
			const float kv = meteoformulas::ViscAir(T - 273.15f); //Kinematic viscosity of air
			float Sc = kv / dQv; // Schmidt number (kv /dQv)
			float Si = m_envGrid.Qv[i] / QWI; // Saturation ratio over ice
			const static float gammaDS = meteoformulas::gamma((d + 5.0f) / 2); //Save it, since it is a constant

			const float A = Constants::Ls * Constants::Ls / (Constants::Ka * Constants::Rsw * T * T);
			const float B = 1 / (D * 0.001f * meteoformulas::wi(T - 273.15f, ps[i]) * dQv); //TODO dQv is probably wrong

			const float density = pow(D / 1.225f, 0.25f);

			float PSDEP = 0.0f;
			float PSSUB = 0.0f;

			PSSUB = PSDEP = PI * PI * (Si - 1) / (D * 0.001f * (A + B)) * N0S * 
				(0.78f * powf(slopeS, -2) + 0.31f * powf(Sc, 1.0f / 3.0f) * gammaDS * powf(c, 0.5f) * density * powf(kv, -0.5f) * powf(slopeS, -(d + 5) / 2));

			//TODO: test if positive/negative is correct
			BES = -1 * PSSUB * (1 - PTerm1); //Only occurs outside of cloud
			BDS = -1 * PSDEP * PTerm1; //Only occurs within cloud
		}


		// Evaporation rate of rain
		if (m_envGrid.Qr[i] > 0.0f && m_envGrid.Qr[i] + m_envGrid.Qw[i] < QWS - m_envGrid.Qv[i]) //Only if there is vapor dificit in relation to saturation mixing ratio
		{
			//TODO: fix (g/g not g/m3???)
			const float Qs = std::max(QWS - m_envGrid.Qv[i], 0.0f) * D * 1000; //Vapor dificit to g/m3
			const float Qr = m_envGrid.Qr[i] * D * 1000; //To g/m3
			const float N0 = 1.93e-6f * std::pow(8e6f, 7.0f / 20.0f); //Controls shape and density of rain
			BER = N0 * Qr * std::pow(Qs, 13.0f / 20.0f);
			BER = BER * 0.001f / D; //g/m3 to kg/kg
		}


		AW = std::max(m_envGrid.Qw[i] - qwmin, 0.0f) == 0.0f ? 0.0f : BAW;
		KW = BKW;
		AC = BAC * std::max(m_envGrid.Qc[i] - qcmin, 0.0f);
		FKI = BFKI;
		FKR = BFKR;
		KCI = BKCI;
		KCW = BKCW;
		RW = BRW * m_envGrid.Qi[i] * m_envGrid.Qw[i];
		RS = BRS;
		RRS = BRRS;
		RSR = BRSR;
		if (T < -8 + 273.15f) FR = std::min(BFR * (T - 273.15f + 8) * (T - 273.15f + 8), m_envGrid.Qr[i]); //use min to not use rain if there is none

		//Constraint on how much can be evaporated (don't evap if air can't hold more)
		//We first make sure that we are not adding more vapor than the air can hold, then we add up all the things we can evap, multiplied by dt.
		ER = std::min(m_envGrid.Qr[i], BER);
		ES = std::max(0.0f, BES);
		DS = std::max(0.0f, BDS); //Using DS and ES for consistancy with paper, but only need 1 to add to latent heat.
		EI = BEI * std::min(m_envGrid.Qi[i], std::min(m_speed * dt * (m_envGrid.Qc[i] + m_envGrid.Qs[i] + m_envGrid.Qi[i]), std::max(QWI - m_envGrid.Qv[i], 0.0f)));

		if (atGround)
		{
			GR = m_envGrid.Qr[i];
			GS = m_envGrid.Qs[i];
			GI = m_envGrid.Qi[i];
		}
		const float D_ = 1e-6f; // Weigthed mean diffusivity of the ground //TODO: hmm, could tripple check if right
		const float secsInDay = 60 * 60 * 24;
		const float O_ = 0.21f * secsInDay; // evaporative ground water storage coefficient (i.e. only part of the soil can be evaporated) https://en.wikipedia.org/wiki/Specific_storage
		//Note that this is just picked from water storage coefficient which may not is the same as the evaporative soil water storage coefficient.
		//Only if Qgj = 0 (Precip falling) and if we are at the ground
		if (atGround)
		{
			if (m_groundGrid.Qgr[idxX] == 0 && m_groundGrid.Qgs[idxX] == 0 && m_groundGrid.Qgi[idxX] == 0)
			{
				//https://agupubs.onlinelibrary.wiley.com/doi/full/10.1002/2013WR014872
				const float waterA = m_groundGrid.Qrs[idxX];
				const float time = m_groundGrid.t[idxX];
				EG = BEG * D_ * waterA * exp(-time / O_);
			}
			else
			{
				m_groundGrid.t[idxX] = 0;
			}
		}
	}

	//Limit by speed and add latent heat
	{
		m_condens = 0.0f;
		m_freeze = 0.0f;
		m_depos = 0.0f;
		//TODO: DELTATIME IS NOT USED
		//Limit Vapor
		m_condens -= EW_min_CW = dt * std::min(EW_min_CW * m_speed, m_envGrid.Qw[i]);
		m_depos -= DC_min_SC = dt * std::min(DC_min_SC * m_speed, m_envGrid.Qc[i]); //TODO: wrong latent heat add/subtact, jus remove from Qv or something.
		m_depos += DS = dt * std::min(DS * m_speed, m_envGrid.Qv[i]);
		//Limit cloud matter
		AW = dt * std::min(AW * m_speed, m_envGrid.Qw[i]);
		KW = dt * std::min(KW * m_speed, m_envGrid.Qw[i]);
		m_freeze += RW = dt * std::min(RW * m_speed, m_envGrid.Qw[i]);
		m_freeze += FW = dt * std::min(FW * m_speed, m_envGrid.Qw[i]);
		m_freeze += BW = dt * std::min(BW * m_speed, m_envGrid.Qw[i]);
		m_freeze += KCW = dt * std::min(KCW * m_speed, m_envGrid.Qw[i]);
		//Limit cloud ice
		AC = dt * std::min(AC * m_speed, m_envGrid.Qc[i]);
		KCI = dt * std::min(KCI * m_speed, m_envGrid.Qc[i]);
		FKI = dt * std::min(FKI * m_speed, m_envGrid.Qc[i]);
		m_freeze -= MC = dt * std::min(MC * m_speed, m_envGrid.Qc[i]);
		//Limit rain
		m_condens -= ER = dt * std::min(ER * m_speed, m_envGrid.Qr[i]);
		m_freeze += FR = dt * std::min(FR * m_speed, m_envGrid.Qr[i]);
		m_freeze += FKR = dt * std::min(FKR * m_speed, m_envGrid.Qr[i]);
		GR = dt * std::min(GR * m_speed, m_envGrid.Qr[i]);
		m_freeze += RSR = dt * std::min(RSR * m_speed, m_envGrid.Qr[i]);
		//Limit snow
		m_freeze -= MS = dt * std::min(MS * m_speed, m_envGrid.Qs[i]);
		m_depos -= ES = dt * std::min(ES * m_speed, m_envGrid.Qs[i]);
		RS = dt * std::min(RS * m_speed, m_envGrid.Qs[i]);
		GS = dt * std::min(GS * m_speed, m_envGrid.Qs[i]);
		RRS = dt * std::min(RRS * m_speed, m_envGrid.Qs[i]);
		//Limit hail
		m_depos -= EI = dt * std::min(EI * m_speed, m_envGrid.Qi[i]);
		m_freeze -= MI = dt * std::min(MI * m_speed, m_envGrid.Qi[i]);
		GI = dt * std::min(GI * m_speed, m_envGrid.Qi[i]);

		m_condens -= EG = dt * EG * m_speed; //No need to limit 
	}
	

	//𝐷𝑡𝑞𝑣 = 𝐸𝑊 + 𝑆𝐶 + 𝐸𝑅 + 𝐸𝐼 + 𝐸𝑆 + 𝐸𝐺 − 𝐶𝑊 − 𝐷𝐶, (9)
	//𝐷𝑡𝑞𝑤 = 𝐶𝑊 + 𝑀𝐶 − 𝐸𝑊 − 𝐴𝑊 − 𝐾𝑊 − 𝑅𝑊 − 𝐹𝑊 − 𝐵𝑊, (10)
	//𝐷𝑡𝑞𝑐 = 𝐷𝐶 + 𝐹𝑊 + 𝐵𝑊 − 𝑆𝐶 − 𝐴𝐶 − 𝐾𝐶 − 𝑀𝐶, (11)
	//𝐷𝑡𝑞𝑟 = 𝐴𝑊 + 𝐾𝑊 + 𝑀𝑆 + 𝑀𝐼 − 𝐸𝑅 − 𝐹𝑅 − 𝐺𝑅, (12)
	//𝐷𝑡𝑞𝑠 = 𝐴𝐶 + 𝐾𝐶 − 𝑀𝑆 − 𝐸𝑆 − 𝑅𝑆 − 𝐺𝑆, (13)
	//𝐷𝑡𝑞𝑖 = 𝐹𝑅 + 𝑅𝑆 + 𝑅𝑊 − 𝐸𝐼 − 𝑀𝐼 − 𝐺𝐼


	m_envGrid.Qw[i] += (-EW_min_CW + MC - AW - KW - KCW - RW - FW - BW);
	m_envGrid.Qc[i] += (-DC_min_SC + FW + BW - AC - KCI - MC - FKI);
	if (T < 0 + 273.15f)
	{
		m_envGrid.Qv[i] += (EW_min_CW + DC_min_SC + ER + EI + EG + ES - DS);
		m_envGrid.Qr[i] += (AW + KW + MS + MI - ER - FR - GR - FKR - RSR);
		//m_debugArray0[i] = std::max(ES - 1e-11f, 0.0f) / (AC + KCW + KCI + FKI * (PTerm3)+FKR * (PTerm3)+RSR * (PTerm2)-RRS * (1 - PTerm2) - ES + DS - MS - RS - GS);
		m_debugArray1[i] = (AC + KCW + KCI + FKI * (PTerm3)+FKR * (PTerm3)+RSR * (PTerm2)-RRS * (1 - PTerm2) - ES + DS - MS - RS - GS) * 100.0f;

		m_envGrid.Qs[i] += (AC + KCW + KCI + FKI * (PTerm3) + FKR * (PTerm3) + RSR * (PTerm2) - RRS * (1 - PTerm2) - ES + DS - MS - RS - GS);
		m_envGrid.Qi[i] += (FR + RS + RW + FKI * (1 - PTerm3) + FKR * (1 - PTerm3) + RSR * (1 - PTerm2) + RRS * (1 - PTerm2) - EI - MI - GI);
	}
	else
	{
		m_envGrid.Qv[i] += (EW_min_CW + DC_min_SC + ER + EI + EG);
		m_envGrid.Qr[i] += (AW + KW + MS + MI - ER - FR - GR - FKR); //TODO: watch out for PSMLT
		m_envGrid.Qs[i] += (-MS - RS - GS);
		m_envGrid.Qi[i] += (FR + RS + RW + FKI * (1 - PTerm3) + FKR * (1 - PTerm3) - EI - MI - GI);
	}
	//Add to the ground if needed
	if (atGround)
	{
		m_groundGrid.Qgr[idxX] += GR;
		m_groundGrid.Qgs[idxX] += GS;
		m_groundGrid.Qgi[idxX] += GI;
	}

	if (m_envGrid.Qv[i] < 0 || m_envGrid.Qw[i] < 0 || m_envGrid.Qc[i] < 0 || m_envGrid.Qr[i] < 0 || m_envGrid.Qs[i] < 0 || m_envGrid.Qi[i] < 0 ||
		m_envGrid.Qv[i] != m_envGrid.Qv[i] || m_envGrid.Qw[i] != m_envGrid.Qw[i] || m_envGrid.Qc[i] != m_envGrid.Qc[i] || m_envGrid.Qr[i] != m_envGrid.Qr[i] || m_envGrid.Qs[i] != m_envGrid.Qs[i] || m_envGrid.Qi[i] != m_envGrid.Qi[i])
	{
 		AW = 0;
	}

}

float environment::calculateSumPhaseHeat(const float , const int i, const float T)
{
	const float Qv = m_envGrid.Qv[i];
	const float Mair = 0.02896f; //In kg/mol
	const float Mwater = 0.01802f; //In kg/mol
	const float XV = (Qv / Mwater) / ((Qv / Mwater) + (1 - Qv) / Mair);
	const float Mth = XV * Mwater + (1 - XV) * Mair; 
	const float Yair = 1.4f, yV = 1.33f;
	const float YV = XV * (Mwater / Mth); //Mass fraction of vapor
	const float yth = YV * yV + (1 - YV) * Yair; //Weighted average

	const float cpth = yth * R / (Mth * (yth - 1)); // Get specific gas constant

	float sumPhaseheat = 0.0f;

	sumPhaseheat += meteoformulas::Lwater(T - 273.15f) / cpth * m_condens;
	sumPhaseheat += meteoformulas::Lice(T - 273.15f) / cpth * m_depos;
	sumPhaseheat += Lf / cpth * m_freeze;

	m_condens = 0.0f;
	m_freeze = 0.0f;
	m_depos = 0.0f;

	return sumPhaseheat;
}

void environment::computeHeatTransfer(const int i, const float sumHeat)
{
	m_envGrid.potTemp[i] += sumHeat;

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
	const float Pv = i - GRIDSIZESKYX < 0 ? 0.0f : m_envGrid.velField[i - GRIDSIZESKYX].y;

	return glm::vec2((Pu + u) / 2, (Pv + v) / 2);
}

float environment::getIsentropicTemp(const float coordy)
{
	//Clamp
	const int index = coordy < 0.0f ? 0 : coordy >= GRIDSIZESKYY ? GRIDSIZESKYY - 1 : int(coordy);
	return m_isenTropicTemps[index];
}

float environment::getIsentropicVapor(const float coordy)
{
	//Clamp
	const int index = coordy < 0.0f ? 0 : coordy >= GRIDSIZESKYY ? GRIDSIZESKYY - 1 : int(coordy);
	return m_isenTropicVapor[index];
}

//Curl: https://en.wikipedia.org/wiki/Del
float environment::curl(const int i, bool raw)
{
	//TODO: in 3D this part is different, could look at the fluid paper. (appendix A1.3)

	const float left  = m_NeighData[i].left  != SKY ? getUV(i).x : getUV(i - 1).x;	// Neumann
	const float right = m_NeighData[i].right != SKY ? getUV(i).x : getUV(i + 1).x;	// Neumann
	const float up    = m_NeighData[i].up    != SKY ? getUV(i).y : getUV(i + GRIDSIZESKYX).y; // Neumann
	const float down  = m_NeighData[i].down  != SKY ? getUV(i).y : getUV(i - GRIDSIZESKYX).y; // Neumann

	const float deltaU = (right - left) / (2.0f * VOXELSIZE);
	const float deltaV = (up - down) / (2.0f * VOXELSIZE);

	//TODO: in 3D not just a abs anymore (due to magnitude)
	return raw ? deltaU - deltaV : fabs(deltaU - deltaV);
}

//Divergence: https://en.wikipedia.org/wiki/Del
float environment::div(const int i)
{
	//TODO: in 3D this part is different, could look at the fluid paper.
	const float left  = m_NeighData[i].left  != SKY ? getUV(i).x : getUV(i - 1).x;	// Neumann
	const float right = m_NeighData[i].right != SKY ? getUV(i).x : getUV(i + 1).x;	// Neumann
	const float up    = m_NeighData[i].up    != SKY ? getUV(i).y : getUV(i + GRIDSIZESKYX).y; // Neumann
	const float down  = m_NeighData[i].down  != SKY ? getUV(i).y : getUV(i - GRIDSIZESKYX).y; // Neumann

	const float deltaU = (right - left) / (2.0f * VOXELSIZE);
	const float deltaV = (up - down) / (2.0f * VOXELSIZE);

	return deltaU + deltaV;
}

//Laplacian: https://en.wikipedia.org/wiki/Del
glm::vec2 environment::lap(const int i)
{
	//TODO: in 3D this part is different, could look at the fluid paper.
	const glm::vec2 center = getUV(i);
	const  glm::vec2 left  = m_NeighData[i].left !=  SKY ? center : getUV(i - 1);	// Neumann 
	const  glm::vec2 right = m_NeighData[i].right != SKY ? center : getUV(i + 1);	// Neumann 
	const  glm::vec2 up    = m_NeighData[i].up !=    SKY ? getUV(i) : getUV(i + GRIDSIZESKYX); // Neumann 
	const  glm::vec2 down  = m_NeighData[i].down !=  SKY ? getUV(i) : getUV(i - GRIDSIZESKYX); // Neumann 

	return { (right.x - 2 * center.x + left.x) / (VOXELSIZE * VOXELSIZE) +
			 (up.x - 2 * center.x + down.x) / (VOXELSIZE * VOXELSIZE),
			 (right.y - 2 * center.y + left.y) / (VOXELSIZE * VOXELSIZE) +
			 (up.y - 2 * center.y + down.y) / (VOXELSIZE * VOXELSIZE) };
}

bool environment::outside(const int i)
{
	return (i - GRIDSIZESKYX < 0 || i + GRIDSIZESKYX >= GRIDSIZESKY);
}

bool environment::outside(const float x, const float y)
{
	return (x < 0 || x >= GRIDSIZESKYX || y < 0 || y >= GRIDSIZESKYY);
}

bool environment::isGround(int i)
{
	int x = i % GRIDSIZESKYX;
	int y = i / GRIDSIZESKYX;
	return y <= m_GHeight[x];
}

bool environment::isGround(int x, int y)
{
	return y <= m_GHeight[x];
}

void environment::computeNeighArray()
{
	//Set Neighbour array
	for (int y = 0; y < GRIDSIZESKYY; y++)
	{
		for (int x = 0; x < GRIDSIZESKYX; x++)
		{
			const int i = x + y * GRIDSIZESKYX;
			m_NeighData[i].left = x == 0 ? OUTSIDE : isGround(x - 1, y) ? GROUND : SKY;
			m_NeighData[i].right = x == GRIDSIZESKYX - 1 ? OUTSIDE : isGround(x + 1, y) ? GROUND : SKY;
			m_NeighData[i].down = y == 0 ? GROUND : isGround(x, y - 1) ? GROUND : SKY; // Down is always ground (at y == 0)
			m_NeighData[i].up = y == GRIDSIZESKYY - 1 ? OUTSIDE : isGround(x, y + 1) ? GROUND : SKY;
		}
	}

	//Set correct ground temp for height using isentropic
	for (int x = 0; x < GRIDSIZEGROUND; x++)
	{
		m_groundGrid.T[x] = getIsentropicTemp(float(m_GHeight[x]));
	}
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

envDebugData* environment::getDebugData()
{
	return new envDebugData(m_envGrid, m_groundGrid, m_GHeight, m_debugArray0, m_debugArray1, m_debugArray2);
};

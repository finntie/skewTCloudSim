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

void environment::init(double* potTemps, glm::vec2* velField, float* Qv, double* groundTemp, float* groundPres, float* pressures)
{
	memcpy(m_envGrid.potTemp, potTemps, GRIDSIZESKY * sizeof(double));
	memcpy(m_envGrid.velField, velField, GRIDSIZESKY * sizeof(glm::vec2));
	memcpy(m_envGrid.Qv, Qv, GRIDSIZESKY * sizeof(float));

	memcpy(m_groundGrid.T, groundTemp, GRIDSIZEGROUND * sizeof(double));
	memcpy(m_groundGrid.P, groundPres, GRIDSIZEGROUND * sizeof(float));
	memcpy(m_pressures, pressures, GRIDSIZESKYY * sizeof(float));
		
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

	//Init Editor
	Game.Editor().setIsentropics(m_isenTropicTemps, m_isenTropicVapor, m_pressures);
	Game.Editor().setTime(m_time);
	Game.Editor().setLongitude(m_longitude);
	Game.Editor().setDay(m_day);

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

	m_pauseDiurnal = Game.Editor().getDiurnalCyclePaused();
	m_longitude = Game.Editor().getLongitude();
	m_day = Game.Editor().getDay();
	m_sunStrength = Game.Editor().getSunStrength();
	Game.Editor().getTime(m_time);
	Game.Editor().setTime(m_time);

	return true;
}

void environment::Update(float dt)
{
	if (!EditorData()) return;


	// 1. Update total incoming solar radiation 
	float Irradiance = irridiance();
	
	for (int s = 0; s < 1; s++) //Speed in cost of fps.
	{
		// Update Ground
		for (int i = 0; i < GRIDSIZEGROUND; i++)
		{
			// 1. Compute Cloud Covering Fraction 
			// 2. Update ground temperature
			// 3. Advect microphysics ground
			// 4. Update microphysic process ground
			// 5. Compute heat transfer at ground level due to phase transition

			// 1.
			float LC = groundCoverageFactor(i);
			// 2.
			updateGroundTemps(dt, i, Irradiance, LC);

			// 3.
			advectMicroPhysicsGround(dt, i);

			// 4.
			const int envGIdx = i + (m_GHeight[i] + 1) * GRIDSIZEGROUND;
			const int y = i / GRIDSIZESKYX;
			const float T = float(m_envGrid.potTemp[envGIdx]) * glm::pow(m_pressures[y] / m_groundGrid.P[i % GRIDSIZESKYX], Rsd / Cpd);
			const float Tv = T * (0.608f * m_envGrid.Qv[envGIdx] + 1);
			const float density = m_pressures[y] * 100 / (Rsd * Tv); //Convert Pha to Pa
			updateMicroPhysicsGround(dt, i, T, Irradiance * (1 - LC), LC, density);

			// 5.
			m_groundGrid.T[i] += dt * calculateSumPhaseHeatGround(i); 
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

			// --	Loop 	--
			for (int loop = 0; loop < 2; loop++)
			{
				for (int i = 0; i < GRIDSIZESKY; i++)
				{
					// we want not to update neighbouring cells, so we do the Red-Black Gauss-Seidel tactic
					if ((i + loop + int(float(i) / GRIDSIZESKYX)) % 2 == 0 || isGround(i)) continue;
					const int y = i / GRIDSIZESKYX;

					// 4.
					const float T = float(m_envGrid.potTemp[i]) * glm::pow(m_pressures[y] / m_groundGrid.P[i % GRIDSIZESKYX], Rsd / Cpd);
					const float Tv = T * (0.608f * m_envGrid.Qv[i] + 1);
					const float density = m_pressures[y] * 100 / (Rsd * Tv); //Convert Pha to Pa

					//Debug values
					//const float QWS = meteoformulas::ws((T - 273.15f), m_pressures[y]); //Maximum water vapor air can hold
					//debugVector[i] = QWS - m_envGrid.Qv[i];
					//
					//const float T2 = float(m_envGrid.potTemp[i + GRIDSIZESKYX]) * glm::pow(m_pressures[y + 1] / m_groundGrid.P[i % GRIDSIZESKYX], Rsd / Cpd);
					//debugVector2[i] = (m_envGrid.Qv[i] - meteoformulas::ws((T2 - 273.15f), m_pressures[y + 1]));

					// 5.
					updateMicroPhysics(dt, i, T, density);

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

		m_time += m_speed * dt * int(!m_pauseDiurnal);
		if (m_time > 86400.0f) m_time = 0.0f;
	}
}

float environment::irridiance()
{
	//Calculates the total energy from the sun at a specific spot
	//Using formulas from https://tc.copernicus.org/articles/17/211/2023/
	float Gs = 1361.0f * m_sunStrength; // Solar constant in W/m-2
	float rd = 1 + 0.034f * cos(2 * PI * m_day / 365); //Relative distance to the sun
	float sd = 0.409f * sin(2 * PI / 365 * (m_day - 81)); //Solar diclenation with spring equinox on day 81
	const float timeHour = m_time / 3600;
	const float longitudeRad = glm::radians(m_longitude);

	float solarRad = (timeHour - (m_hourOfSunrise + m_dayLightDuration / 2.0f)) * (PI / 12.0f); //Convert time to noon to radians.
	printf("cos solarRad: %f\n", cos(solarRad));
	//Get amount of W/m-2 at this time of the day.
	return std::max(0.0f, Gs * rd * (sin(longitudeRad) * sin(sd) + cos(longitudeRad) * cos(sd) * cos(solarRad)));
}

float environment::groundCoverageFactor(const int i)
{
	float totalCloudContent = 0.0f;
	const float qfull = 1.2f; // a threshold value where all incoming radiation is reflected by cloud matter: http://meto.umd.edu/~zli/PDF_papers/Li%20Nature%20Article.pdf
	for (int y = 0; y < GRIDSIZESKYY; y++) totalCloudContent += (m_envGrid.Qc[i + y * GRIDSIZESKYX] + m_envGrid.Qw[i + y * GRIDSIZESKYX]) * VOXELSIZE;
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

void environment::advectMicroPhysicsGround(const float dt, const int i)
{
	const int pX = (i % GRIDSIZEGROUND == 0) ? i : i - 1;
	const int nX = ((i + 1) % GRIDSIZEGROUND == 0) ? i : i + 1;


	//Check how many meters down
	float slopeFlowRain = 0.0f;
	float slopeFlowWater = 0.0f;
	{
		// Using Manning Formula: V = 1 / n * Rh^2/3 * S^1/2
		// We use for n = 0.030.
		// RH is weird for open way, so we use water depth, which is in our case is Qgr * VOXELSIZE (in meters)
		const float n = 1 / 0.03f;

		// Hydraulic conductivity of the ground in m/s (How easy water flows through ground) https://structx.com/Soil_Properties_007.html
		const float k{ 5e-5f }; //Sand	

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
	float DG = 1.75e-3f; // diffusion coefficient for ground rain water https://dtrx.de/od/diff/
	const float lapSR = (m_groundGrid.Qrs[pX] + m_groundGrid.Qrs[nX] - 2 * m_groundGrid.Qrs[i]) / (VOXELSIZE * VOXELSIZE);
	float DS = 1.8e-5f; // diffusion coefficient for subsurface water https://www.researchgate.net/figure/Diffusion-coefficient-for-water-in-soils_tbl2_267235072

	//TODO: Limit on speed (Watch out: if only limiting 1 side and not the left or right, it could lead to inconsistencies)
	if (m_speed > 1.0f)
	{
		
	}

	m_groundGrid.Qgr[i] += ( m_speed * dt * (DG * lapR + slopeFlowRain));
	m_groundGrid.Qrs[i] += ( m_speed * dt * (DS * lapSR + slopeFlowWater));
}

void environment::updateMicroPhysicsGround(const float dt, const int i, const float Tair, const float irr, const float c, const float density)
{
	const int iH = i + (m_GHeight[i] + 1) * GRIDSIZEGROUND;

	glm::vec3 fallVel{ 0.0f };
	if (m_envGrid.Qr[iH] > 0.0f || m_envGrid.Qs[iH] > 0.0f || m_envGrid.Qi[iH] > 0.0f) fallVel = calculateFallingVelocity(dt, iH, density);

	//Calculate precip falling on ground
	microPhys::microPhysHittingGroundResult MResultHitting = microPhys::microPhysHittingGroundResult(m_envGrid.Qv[iH], m_envGrid.Qr[iH], m_envGrid.Qs[iH], m_envGrid.Qi[iH],
		m_groundGrid.Qgr[i], m_groundGrid.Qgs[i], m_groundGrid.Qgi[i],
		dt, m_speed, float(m_groundGrid.T[i]), Tair, m_groundGrid.P[i], density, fallVel);

	Game.mPhys().calculateEnvMicroPhysicsHittingGround(MResultHitting);

	m_groundGrid.Qgr[i] += MResultHitting.Qgr;
	m_groundGrid.Qgs[i] += MResultHitting.Qgs;
	m_groundGrid.Qgi[i] += MResultHitting.Qgi;
	m_envGrid.Qr[iH] += MResultHitting.Qr;
	m_envGrid.Qs[iH] += MResultHitting.Qs;
	m_envGrid.Qi[iH] += MResultHitting.Qi;

	m_condens = MResultHitting.condens;
	m_freeze = MResultHitting.freeze;
	m_depos = MResultHitting.depos;

	//Calculate ground microphysics
	microPhys::microPhysGroundResult MResult = microPhys::microPhysGroundResult(m_groundGrid.Qgr[i], m_envGrid.Qv[iH], m_groundGrid.Qgr[i], m_groundGrid.Qgs[i], m_groundGrid.Qgi[i],
		dt, m_speed, float(m_groundGrid.T[i]), Tair, m_groundGrid.P[i], density, m_groundGrid.t[i], irr, getUV(iH).x, c);

	Game.mPhys().calculateMicroPhysicsGround(MResult);

	m_groundGrid.Qrs[i] += MResult.Qrs;
	m_envGrid.Qv[iH] += MResult.Qv;
	m_groundGrid.Qgr[i] += MResult.Qr;
	m_groundGrid.Qgs[i] += MResult.Qs;
	m_groundGrid.Qgi[i] += MResult.Qi;
	m_groundGrid.t[i] = MResult.time;
	
	//Adding, not setting, due to precip hitting ground also creating heat
	m_condens += MResult.condens;
	m_freeze += MResult.freeze;
	m_depos += MResult.depos;
}

float environment::calculateSumPhaseHeatGround(const int i)
{
	const float Qv = m_envGrid.Qv[i];
	const float Mair = 0.02896f; //In kg/mol
	const float Mwater = 0.01802f; //In kg/mol
	const float XV = (Qv / Mwater) / ((Qv / Mwater) + (1 - Qv) / Mair);
	const float Mth = XV * Mwater + (1 - XV) * Mair;
	const float Yair = 1.4f, yV = 1.33f;
	const float YV = XV * (Mwater / Mth); //Mass fraction of vapor
	const float yth = YV * yV + (1 - YV) * Yair; //Weighted average

	// Get specific gas constant
	const float cpth = R / (Mth * (yth - 1)); //Not multiplied by yth due to not needing pottemp but normal temp

	float sumPhaseheat = 0.0f;

	sumPhaseheat += meteoformulas::Lwater(float(m_groundGrid.T[i]) - 273.15f) / cpth * m_condens;
	sumPhaseheat += meteoformulas::Lice(float(m_groundGrid.T[i]) - 273.15f) / cpth * m_depos;
	sumPhaseheat += Lf / cpth * m_freeze;

	m_condens = 0.0f;
	m_freeze = 0.0f;
	m_depos = 0.0f;

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
					const int idxGL = (j - 1) % GRIDSIZESKYX;
					const int idxGR = (j + 1) % GRIDSIZESKYX;
					const int idxGD = j % GRIDSIZESKYX;

					//TODO: could use a function inside this function which switches over the types, is faster.
					const double l = m_NeighData[j].left == OUTSIDE ? getIsentropicTemp(yPos) : m_NeighData[j].left  == GROUND ? meteoformulas::potentialTemp(float(m_groundGrid.T[idxGL]) - 273.25f,  m_pressures[int(yPos)], m_groundGrid.P[idxGL]) + 273.15f : dif[j - 1];
					const double r = m_NeighData[j].right == OUTSIDE ? getIsentropicTemp(yPos) : m_NeighData[j].right == GROUND ? meteoformulas::potentialTemp(float(m_groundGrid.T[idxGR]) - 273.25f, m_pressures[int(yPos)], m_groundGrid.P[idxGR]) + 273.15f : dif[j + 1];
					const double d = m_NeighData[j].down == GROUND ? meteoformulas::potentialTemp(float(m_groundGrid.T[idxGD]) - 273.25f, m_pressures[int(yPos)], m_groundGrid.P[idxGD]) + 273.15f : dif[j - GRIDSIZESKYX];
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
				int y = i / GRIDSIZESKYX;
				const float T = meteoformulas::potentialTemp(float(m_groundGrid.T[i % GRIDSIZESKYX]) - 273.15f, m_pressures[y], m_groundGrid.P[i % GRIDSIZESKYX]) + 273.15f;
				const double dif = array[i] - T;
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

	const int idxG0 = int(coord1.x);
	const int idxG1 = int(coord1.x + 1);

	//Use Isentropic Temp if outside of grid and also check for ground which we need to make potential temp
	const double value00 = outside(coord1.x, coord1.y) ? getIsentropicTemp(coord1.y) : isGround(index00) ? meteoformulas::potentialTemp(float(m_groundGrid.T[idxG0]) - 273.15f, m_pressures[index00 / GRIDSIZESKYX], m_groundGrid.P[idxG0]) + 273.15f : array[index00];
	const double value10 = outside(coord1.x + 1, coord1.y) ? getIsentropicTemp(coord1.y) : isGround(index10) ? meteoformulas::potentialTemp(float(m_groundGrid.T[idxG1]) - 273.15f, m_pressures[index10 / GRIDSIZESKYX], m_groundGrid.P[idxG1]) + 273.15f : array[index10];
	const double value01 = outside(coord1.x, coord1.y + 1) ? getIsentropicTemp(coord1.y + 1) : isGround(index01) ? meteoformulas::potentialTemp(float(m_groundGrid.T[idxG0]) - 273.15f, m_pressures[index01 / GRIDSIZESKYX], m_groundGrid.P[idxG0]) + 273.15f : array[index01];
	const double value11 = outside(coord1.x + 1, coord1.y + 1) ? getIsentropicTemp(coord1.y + 1) : isGround(index11) ? meteoformulas::potentialTemp(float(m_groundGrid.T[idxG1]) - 273.15f, m_pressures[index11 / GRIDSIZESKYX], m_groundGrid.P[idxG1]) + 273.15f : array[index11];
		
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
		const float k = 0.2f * dt / (VOXELSIZE * VOXELSIZE); //Viscosity value
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
			if ((i + loop + int(float(i) / GRIDSIZESKYX)) % 2 == 0 || isGround(i)) continue;

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
			int y = i / GRIDSIZESKYX;

			float fallVel = 0.0f;
			float fallVelUp = 0.0f;

			glm::vec3 fallVelocitiesPrecip{ 0.0f };
			glm::vec3 fallVelocitiesPrecipUP{0.0f};

			{
				const float T = float(m_envGrid.potTemp[i]) * glm::pow(m_pressures[y] / m_groundGrid.P[i % GRIDSIZESKYX], Rsd / Cpd);
				const float Tv = T * (0.608f * m_envGrid.Qv[i] + 1);
				const float density = m_pressures[y] * 100 / (Rsd * Tv); //Convert Pha to Pa
				fallVelocitiesPrecip = calculateFallingVelocity(dt, i, density);
			}
			if (m_NeighData[i].up == SKY)
			{
				const int iUP = i + GRIDSIZESKYX;
				const int yUP = iUP / GRIDSIZESKYX;
				const float T = float(m_envGrid.potTemp[iUP]) * glm::pow(m_pressures[yUP] / m_groundGrid.P[iUP % GRIDSIZESKYX], Rsd / Cpd);
				const float Tv = T * (0.608f * m_envGrid.Qv[iUP] + 1);
				const float density = m_pressures[yUP] * 100 / (Rsd * Tv); //Convert Pha to Pa
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
	
		float B = calculateBuoyancy(i);
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
	pressureProjectVelField();
}

float environment::calculateBuoyancy(const int i)
{
	const float mDistance = 4096.0f / VOXELSIZE;
	const int oX = i % GRIDSIZESKYX;
	const int oY = int(float(i) / GRIDSIZESKYX);

	const float T = static_cast<float>(m_envGrid.potTemp[i]) * glm::pow(m_pressures[oY] / m_groundGrid.P[oX], Rsd / Cpd);
	const float T2 = static_cast<float>(m_envGrid.potTemp[i + GRIDSIZESKYX]) * glm::pow(m_pressures[oY + 1] / m_groundGrid.P[oX], Rsd / Cpd);


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
		if (m_envGrid.Qv[i] >= meteoformulas::ws((T2 - 273.15f), m_pressures[oY + 1])) //Air is saturated, thus use moist adiabatic
		{
			for (int y = oY - checkSize; y <= oY + checkSize; y++)
			{
				//Boundary check
				if ((y < oY && y < minY) || (y > oY && y > maxY))
				{
					PTemps[PTempIdx++] = T;
					continue;
				}

				if (y < oY) //Remove temp below
				{
					///meteoformulas::getMoistTemp()
					//TODO: wrong and not very good if P change > 1
					PTemps[PTempIdx++] = T - meteoformulas::MLR(T - 273.15f, m_pressures[y]) * (m_pressures[oY] - m_pressures[y]);
				}
				else if (y > oY) //Add temp above
				{
					PTemps[PTempIdx++] = meteoformulas::MLR(T - 273.15f, m_pressures[y]) * (m_pressures[y] - m_pressures[oY]) + T;
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

				if (y < oY) //Remove temp below
				{
					//TODO: I guess this is better then DLR?
					PTemps[PTempIdx++] = T * glm::pow(m_pressures[y] / m_pressures[oY], Constants::Rsd / Constants::Cpd);//T - meteoformulas::DLR(T - 273.15f, ps[idx]) * (ps[i] - ps[idx]);
				}
				else if (y > oY) //Add temp above
				{
					PTemps[PTempIdx++] = T * glm::pow(m_pressures[y] / m_pressures[oY], Constants::Rsd / Constants::Cpd);//meteoformulas::DLR(T - 273.15f, ps[idx]) * (ps[idx] - ps[i]) + T;
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
		if (temp) average += (static_cast<float>(m_envGrid.potTemp[x + y * GRIDSIZESKYX]) * glm::pow(m_pressures[y] / m_groundGrid.P[x], Rsd / Cpd)) * cAmount;
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
		if (temp) average += (static_cast<float>(m_envGrid.potTemp[x + y * GRIDSIZESKYX]) * glm::pow(m_pressures[y] / m_groundGrid.P[x], Rsd / Cpd)) * cAmount;
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

void environment::pressureProjectVelField()
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

void environment::updateMicroPhysics(const float dt, const int i, const float T, const float density)
{
	const int idxX = i % GRIDSIZESKYX;
	const int idxY = i / GRIDSIZESKYX;

	glm::vec3 fallVel{ 0.0f };
	if (m_envGrid.Qr[i] > 0.0f || m_envGrid.Qs[i] > 0.0f || m_envGrid.Qi[i] > 0.0f) fallVel = calculateFallingVelocity(dt, i, density);

	microPhys::microPhysResult MResult = microPhys::microPhysResult(m_envGrid.Qv[i], m_envGrid.Qw[i], m_envGrid.Qc[i], m_envGrid.Qr[i], m_envGrid.Qs[i], m_envGrid.Qi[i],
		dt, m_speed, T, m_pressures[idxY], density, m_GHeight[idxX], fallVel);

	Game.mPhys().calculateEnvMicroPhysics(MResult);

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
		const float T = meteoformulas::potentialTemp(getIsentropicTemp(float(m_GHeight[x])) - 273.15f, m_groundGrid.P[x], m_pressures[m_GHeight[x] + 1]);
		m_groundGrid.T[x] = T + 273.15f;
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

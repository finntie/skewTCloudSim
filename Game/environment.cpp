#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>


#include "environment.h"

#include "editor.h"
#include "math/meteoformulas.h"
#include "math/constants.hpp"
#include "core/engine.hpp"
#include "imgui/IconsFontAwesome.h"
#include "math/math.hpp"
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
	auto& editObj = bee::Engine.ECS().Registry.ctx().get<editor>();
	editObj.update();
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
	}

	//Init ground
	float noise[GRIDSIZEGROUND];
	bee::PNoise1D(100, noise, GRIDSIZEGROUND, GRIDSIZEGROUND);
	for (int i = 0; i < GRIDSIZEGROUND; i++)
	{
		const float maxHeight = 6;
		m_GHeight[i] = static_cast<int>(std::round(noise[i] * maxHeight));
	}

	//Init editor
	bee::Engine.ECS().Registry.ctx().emplace<editor>(m_envGrid, m_groundGrid, m_GHeight, m_debugArray0, m_debugArray1, m_debugArray2);

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

void environment::DebugRender(float dt)
{
	//TODO: remove or use as calling of debug functions in editor
	auto& editorObj = bee::Engine.ECS().Registry.ctx().get<editor>();
	editorObj.viewData();
	m_speed = editorObj.getSpeed();
	editorObj.setDeltaTime(dt);

	if (editorObj.changedGround())
	{
		computeNeighArray();
	}

}

void environment::Update(float dt)
{
	DebugRender(dt);
	{
		auto& editorObj = bee::Engine.ECS().Registry.ctx().get<editor>();
		if (!editorObj.getSimulate())
		{
			int step = editorObj.getStep();
			if (step > 0) editorObj.setStep(--step);
			else if (step < 0)
			{
				editorObj.setStep(++step);
				dt *= -1;
			}
			else return;
		}

	}

	// 1. Update total incoming solar radiation 
	//Avarage solar irradiance: https://globalsolaratlas.info/map?c=51.793328,5.633017,9&r=NLD
	float avarageIrradiance = 1150.0f; // W/m2
	float Irradiance = avarageIrradiance * std::max(std::sin(PI / 2 * (1 - ((m_time - m_hourOfSunrise - m_dayLightDuration / 2) / (m_dayLightDuration / 2)))), 0.0f);
	
	
	for (int s = 0; s < 1; s++) //Speed in cost of fps.
	{

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
			const float pQgr = (m_groundGrid.Qgr[i]), pQgs = (m_groundGrid.Qgs[i]), pQgi = (m_groundGrid.Qgi[i]);
			updateMicroPhysicsGround(dt, i);

			// 5.
			m_groundGrid.T[i] += m_speed * dt * calculateSumPhaseHeatGround(i, pQgr, pQgs, pQgi);
		}


		// Fill Pressures
		for (int i = 0; i < GRIDSIZESKY; i++)
		{
			const float height = std::floorf(float(i) / GRIDSIZESKYX) * VOXELSIZE;
			m_pressures[i] = meteoformulas::getStandardPressureAtHeight(float(m_groundGrid.T[i % GRIDSIZESKYX] - 273.15f), height);
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
			diffuseAndAdvectTemp(m_speed * dt, m_envGrid.potTemp);

			// 2.
			updateVelocityField(dt);

			// 3.
			diffuseAndAdvect(m_speed * dt, m_envGrid.Qv, true);
			diffuseAndAdvect(m_speed * dt, m_envGrid.Qw);
			diffuseAndAdvect(m_speed * dt, m_envGrid.Qc);
			diffuseAndAdvect(m_speed * dt, m_envGrid.Qr, false, 1);
			diffuseAndAdvect(m_speed * dt, m_envGrid.Qs, false, 2);
			diffuseAndAdvect(m_speed * dt, m_envGrid.Qi, false, 3);

			// --	Loop 2.	--
			for (int loop = 0; loop < 2; loop++)
			{
				for (int i = 0; i < GRIDSIZESKY; i++)
				{
					// we want not to update neighbouring cells, so we do the Red-Black Gauss-Seidel tactic
					if ((i + loop + int(float(i) / GRIDSIZESKYX)) % 2 == 0 || isGround(i)) continue;

					// 4.
					const float T = float(m_envGrid.potTemp[i]) * glm::pow(m_pressures[i] / m_groundGrid.P[i % GRIDSIZESKYX], Rsd / Cpd);
					const float Tv = T * (0.608f * m_envGrid.Qv[i] + 1);
					const float density = m_pressures[i] * 100 / (Rsd * Tv); //Convert Pha to Pa

					// 5.
					const float pQv = m_envGrid.Qv[i], pQw = m_envGrid.Qw[i], pQc = m_envGrid.Qc[i], pQr = m_envGrid.Qr[i], pQs = m_envGrid.Qs[i], pQi = m_envGrid.Qi[i];
					updateMicroPhysics(dt, i, m_pressures, T, density);

					// 6.
					const float sumPhaseHeat = calculateSumPhaseHeat(i, T, pQv, pQw, pQc, pQr, pQs, pQi);
					computeHeatTransfer(dt, i, sumPhaseHeat);

				}
			}
			//setDebugArray(debugVector3, 2);
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
	//const float surfaceArea = GRIDSIZEGROUND; //TODO: not used?
	const float absorbedRadiationAlbedo = 0.25f;  //How much light is reflected back? 0 = absorbes all, 1 = reflects all
	const float groundThickness = 1.0f; //TODO: just 1 meter or??
	const float densityGround = 1500.0f;
	const double T4 = m_groundGrid.T[i] * m_groundGrid.T[i] * m_groundGrid.T[i] * m_groundGrid.T[i];

	m_groundGrid.T[i] += m_speed * dt * ((1 - LC) * (((1 - absorbedRadiationAlbedo) * Irradiance - Constants::ge * Constants::oo * T4) / (groundThickness * densityGround * Constants::Cpds)));
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
	float DG = 0.0f; // diffusion coefficient for ground rain water https://dtrx.de/od/diff/
	float DS = 0.0f; // diffusion coefficient for subsurface water https://www.researchgate.net/figure/Diffusion-coefficient-for-water-in-soils_tbl2_267235072

	//TODO: these values could be so wrong.
	const float BFR{ 5e-3f };					  // Aggregation rate of freezing rain to ice (precip) rate coefficient:
	const float BMI{ 5e-4f };					  // Aggregation rate of ice to rain rate coefficient:
	const float BER{ 5e-4f };					  // Aggregation rate of rain to vapor rate coefficient:
	const float BES{ 5e-4f };					  // Aggregation rate of snow to vapor rate coefficient:
	const float BEI{ 5e-4f };					  // Aggregation rate of ice to vapor rate coefficient:
	const float BIR{ 100.0f };					  // Evaporation rate of subsurface water (A constant)
	const float k{ 5e-5f }; //Sand				  // Hydraulic conductivity of the ground in m/s(How easy water flows through ground) https://structx.com/Soil_Properties_007.html?utm_source=chatgpt.com
	const float BEG{ 200.0f };					  // Evaporation rate of dry ground

	float T = float(m_groundGrid.T[i]);
	float p = m_groundGrid.P[i];
	const float QWS = meteoformulas::ws((T - 273.15f), p); //Maximum water vapor air can hold
	const float QWI = meteoformulas::wi((T - 273.15f), p);

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
	ER = BER * std::min(m_groundGrid.Qgr[i], std::min(m_speed * dt * (m_envGrid.Qw[i] + m_groundGrid.Qgr[i]), std::max(QWS - m_envGrid.Qv[i], 0.0f)));
	ES = BES * std::min(m_groundGrid.Qgs[i], std::min(m_speed * dt * (m_envGrid.Qc[i] + m_groundGrid.Qgs[i] + m_groundGrid.Qgi[i]), std::max(QWI - m_envGrid.Qv[i], 0.0f)));
	EI = BEI * std::min(m_groundGrid.Qgi[i], std::min(m_speed * dt * (m_envGrid.Qc[i] + m_groundGrid.Qgs[i] + m_groundGrid.Qgi[i]), std::max(QWI - m_envGrid.Qv[i], 0.0f)));

	GR = m_envGrid.Qr[i];
	GS = m_envGrid.Qs[i];
	GI = m_envGrid.Qi[i];
	const float D_ = 1e-6f; // Weigthed mean diffusivity of the ground //TODO: hmm, could tripple check if right
	const float O_ = 0.1f; // evaporative ground water storage coefficient (i.e. only part of the soil can be evaporated) 
	IR = BIR * k * m_groundGrid.Qgr[i]; //TODO: this values is way too low, I have to increase with BIR, check with real life values.
	//Only if Qgj = 0 (Precip falling)
	if (m_groundGrid.Qgr[i] == 0 && m_groundGrid.Qgs[i] == 0 && m_groundGrid.Qgi[i] == 0)
	{
		EG = BEG * D_ * m_groundGrid.Qrs[i] * exp(-m_groundGrid.t[i] / 86400 * O_);
		m_groundGrid.t[i] += dt;
	}
	else
	{
		m_groundGrid.t[i] = 0;
	}

	const float nablaSlope = dt * ((m_groundGrid.Qgr[nX] - m_groundGrid.Qgr[pX]) / (m_GHeight[nX] - m_GHeight[pX] + 1e-32f));

	const float lapR = (m_groundGrid.Qgr[pX] + m_groundGrid.Qgr[nX] - 2 * m_groundGrid.Qgr[i]) / (VOXELSIZE * VOXELSIZE);
	DG = 1.75e-3f;
	const float lapSR = (m_groundGrid.Qrs[pX] + m_groundGrid.Qrs[nX] - 2 * m_groundGrid.Qrs[i]) / (VOXELSIZE * VOXELSIZE);
	DS = 1.8e-5f;

	m_groundGrid.Qgr[i] += ( m_speed * dt * (DG * lapR + GR + MS + MI - ER - FR - IR));
	m_groundGrid.Qrs[i] += ( m_speed * dt * (DS * lapSR + nablaSlope + IR - EG));
	m_groundGrid.Qgs[i] += ( m_speed * dt * (GS - MS - ES));
	m_groundGrid.Qgi[i] += ( m_speed * dt * (GI + FR - EI - MI));

	if (m_groundGrid.Qrs[i] < 0|| m_groundGrid.Qgs[i] < 0 || m_groundGrid.Qgi[i] < 0 ||
		m_groundGrid.Qrs[i] != m_groundGrid.Qrs[i] || m_groundGrid.Qgs[i] != m_groundGrid.Qgs[i] || m_groundGrid.Qgi[i] != m_groundGrid.Qgi[i])
	{
		GS = 0;
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

		const float k = 0.5f * dt / (VOXELSIZE * VOXELSIZE); //Viscosity value
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

		const float k = 0.5f * dt / (VOXELSIZE * VOXELSIZE); //Viscosity value
		const int LOOPS = 20; //Total loops for the Gauss-Seidel method
		for (int loop = 0; loop < 2; loop++)
		{
			for (int L = 0; L < LOOPS; L++)
			{
				for (int j = 0; j < GRIDSIZESKY; j++)
				{
					if ((j + loop + int(float(j) / GRIDSIZESKYX)) % 2 == 0 || isGround(j)) continue;

					//TODO: could use a function inside this function which switches over the types, is faster.
					if (vapor) //If vapor, we wrap the edges
					{
						const float left = m_NeighData[j].left == OUTSIDE ? dif[j - 1 + GRIDSIZESKYX] : m_NeighData[j].left == GROUND ? dif[j] : dif[j - 1];
						const float right = m_NeighData[j].right == OUTSIDE ? dif[j + 1 - GRIDSIZESKYX] : m_NeighData[j].right == GROUND ? dif[j] : dif[j + 1];
						const float down = m_NeighData[j].down != SKY ? dif[j] : dif[j - GRIDSIZESKYX];
						const float up = m_NeighData[j].up == OUTSIDE ? dif[j] : dif[j + GRIDSIZESKYX]; //Neumann due to being diffusion

						dif[j] = (array[j] + k * (left + right + up + down)) / (1 + 4 * k);
					}
					else // Else use nuemann
					{
						const float left = m_NeighData[j].left == OUTSIDE ? dif[j] : m_NeighData[j].left == GROUND ? dif[j] : dif[j - 1];
						const float right = m_NeighData[j].right == OUTSIDE ? dif[j] : m_NeighData[j].right == GROUND ? dif[j] : dif[j + 1];
						const float down = m_NeighData[j].down != SKY ? dif[j] : dif[j - GRIDSIZESKYX]; //TODO: this is kind of nuemann for ground, is that ok?
						const float up = m_NeighData[j].up == OUTSIDE ? 0.0f : dif[j + GRIDSIZESKYX];

						dif[j] = (array[j] + k * (left + right + up + down)) / (1 + 4 * k);
					}

				}
			}
		}
		std::memcpy(array, dif.data(), GRIDSIZESKY * sizeof(float));
	}


	//Advection
	for (int loop = 0; loop < 2; loop++)
	{
		for (int i = 0; i < GRIDSIZESKY; i++)
		{
			// we want not to update neighbouring cells, so we do the Red-Black Gauss-Seidel tactic
			if ((i + loop + int(float(i) / GRIDSIZESKYX)) % 2 == 0 || isGround(i)) continue;

			float fallVel = 0.0f;

			if (fallVelType != 0)
			{
				const float T = float(m_envGrid.potTemp[i]) * glm::pow(m_pressures[i] / m_groundGrid.P[i % GRIDSIZESKYX], Rsd / Cpd);
				const float Tv = T * (0.608f * m_envGrid.Qv[i] + 1);
				const float density = m_pressures[i] * 100 / (Rsd * Tv); //Convert Pha to Pa
				glm::vec3 fallVelocitiesPrecip = calculateFallingVelocity(dt, i, density); //TODO: do we need to calculate density or just density of standard air?
				switch (fallVelType)
				{
				case 1:
					fallVel = fallVelocitiesPrecip.x;
					break;
				case 2:
					fallVel = fallVelocitiesPrecip.y;
					break;
				case 3:
					fallVel = fallVelocitiesPrecip.z;
					break;
				default:
					break;
				}
			}

			//Advection
			// Current Position of value
			const glm::vec2 CPos = { i % GRIDSIZESKYX, int(i / GRIDSIZESKYX) };
			// Current Velocity of the exact cell, also applying falling velocity
			//TODO: - or + (is falVel positive or negative?)
			const glm::vec2 CVel = getUV(i) - glm::vec2(0.0f, fallVel);
			// Previous Position of value
			const glm::vec2 PPos = CPos - dt * CVel / VOXELSIZE; //Get previous position of particle (TODO: division by VOXELSIZE?)

			float value = 0.0f;
			//Get current value of previous position
			if (!getInterpolValue(array, PPos, vapor, value))
			{
				value = array[i];
			}

			array[i] = value;

			if (array[i] != array[i] || array[i] < 0)
			{
				array[i] = 0;
			}
		}
	}
}

bool environment::getInterpolValue(float* array, const glm::vec2 Ppos, const bool vapor, float& output)
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

	//TODO: if ground, do we set to 0 or use nuemann-ish?
	const float value00 = !isGround(index00) ? array[index00] : vapor ? -1.0f : 0.0f;
	const float value10 = !isGround(index10) ? array[index10] : vapor ? -1.0f : 0.0f;
	const float value01 = !isGround(index01) ? array[index01] : vapor ? -1.0f : 0.0f;
	const float value11 = !isGround(index11) ? array[index11] : vapor ? -1.0f : 0.0f;

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

		float B = calculateBuoyancy(i, m_pressures);
		glm::vec2 F = vorticityConfinement(i);

		debugVector[i] = F.x;
		debugVector2[i] = F.y;
		debugVector3[i] = B;

		m_envGrid.velField[i].x += m_speed * dt * F.x;
		m_envGrid.velField[i].y += m_speed * dt * (B + F.y); //TODO: Buoyance makes the most right edge go up since y will never update here.
	}


	setDebugArray(debugVector);
	setDebugArray(debugVector2, 1);
	setDebugArray(debugVector3, 2);


	//2. TODO: could make use of same matrix projection() uses.

	{
		std::vector<glm::vec2> dif;
		dif.resize(GRIDSIZESKY);
		std::memcpy(dif.data(), m_envGrid.velField, GRIDSIZESKY * sizeof(glm::vec2));

		const float k = 0.5f * dt / (VOXELSIZE * VOXELSIZE); //Viscosity value
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
			if ((i + loop + int(float(i) / GRIDSIZESKYX)) % 2 == 0 || isGround(i)) continue;

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
				valueV = m_envGrid.velField[i].y;
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
	//TODO: base on distance, not only whole width.
	const float T = static_cast<float>(m_envGrid.potTemp[i]) * glm::pow(ps[i] / m_groundGrid.P[i % GRIDSIZESKYX], Rsd / Cpd);

	const float mDistance = 16.0f;
	const float maxDistance = mDistance;
	const int oX = i % GRIDSIZESKYX;
	const int oY = int(float(i) / GRIDSIZESKYX);

	//Calculate min and maxY for our calculations.
	const int checkSize = 1;

	const int outsideBy = std::max(std::max(0, oY + checkSize - (GRIDSIZESKYY - 1)), std::max(0, (oY - checkSize) * -1));

	const int minY = oY - (checkSize - outsideBy);
	const int maxY = oY + (checkSize - outsideBy);

	const int maxRight = oX + int(ceil(mDistance)) > GRIDSIZEGROUND ? GRIDSIZEGROUND : oX + int(ceil(mDistance));
	const int minLeft = oX - int(ceil(mDistance)) < 0 ? 0 : oX - int(ceil(mDistance));

	//Environment vapor (Using average vapor of whole width)
	float Qaverage = 0.0f;
	float amount = 0.0f;
	float* Qe = new float[(checkSize - outsideBy) * 2 + 1];
	Qe[0] = m_envGrid.Qv[i];
	{
		int QeIdx = 0;

		for (int y = minY; y <= maxY; y++)
		{

			//Loop to right then left to get environement temp
			for (int x = oX + 1; x < maxRight; x++)
			{
				if (isGround(x, y)) break;

				const float distance = float((oX - x) * (oX - x) + (oY - y) * (oY - y));
				const float cAmount = -(distance / maxDistance - 1);
				if (abs(cAmount) > 1 || cAmount <= 0.0f) continue;
				amount += cAmount;
				Qaverage += m_envGrid.Qv[x + y * GRIDSIZESKYX] * cAmount;
			}
			for (int x = oX - 1; x > minLeft; x--)
			{
				if (isGround(x, y)) break;

				const float distance = float((oX - x) * (oX - x) + (oY - y) * (oY - y));
				const float cAmount = -(distance / maxDistance - 1);
				if (abs(cAmount) > 1 || cAmount <= 0.0f) continue;
				amount += cAmount;
				Qaverage += m_envGrid.Qv[x + y * GRIDSIZESKYX] * cAmount;;
			}
			//If nothing got added continue, no need to reset.
			if (amount == 0) 
			{
				Qe[QeIdx++] = m_envGrid.Qv[oX + y * GRIDSIZESKYX];
				continue;
			}

			Qaverage /= amount;
			amount = 0.0f;
			//For Qv, we just pick as 'parcel' Qv the one above or below.
			Qe[QeIdx++] = m_envGrid.Qv[oX + y * GRIDSIZESKYX] - Qaverage;
		}
	}

	//Environment Temp (Using average temp of whole until ground)
	float Taverage = 0.0f;
	float Baverage = 0.0f;
	int Bamount = 0;
	{
		//First calculate adiabatic temp up and down.
		float* PTemps = new float[(checkSize - outsideBy) * 2 + 1];
		int PTempIdx = 0;

		if (checkSize - outsideBy > 0)
		{
			//First we calculate the parcel temps for each different heights
			if (m_envGrid.Qv[i] >= meteoformulas::ws((T - 273.15f), ps[i])) //Air is saturated, thus use moist adiabatic
			{
				for (int y = minY; y <= maxY; y++)
				{
					const int idx = oX + y * GRIDSIZESKYX;

					if (y < oY) //Remove temp below
					{
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
				for (int y = minY; y <= maxY; y++)
				{
					const int idx = oX + y * GRIDSIZESKYX;

					if (y < oY) //Remove temp below
					{
						PTemps[PTempIdx++] = T - meteoformulas::DLR(T - 273.15f, ps[idx]) * (ps[i] - ps[idx]);
					}
					else if (y > oY) //Add temp above
					{
						PTemps[PTempIdx++] = meteoformulas::DLR(T - 273.15f, ps[idx]) * (ps[idx] - ps[i]) + T;
					}
					else //Is temp at
					{
						PTemps[PTempIdx++] = T;
					}
				}
			}
		}
		else PTemps[0] = T;
		PTempIdx = 0;

		//Now we calculate the environement temps and use them in the buoyancy formula to calculate buoyancy at each layer
		for (int y = minY; y <= maxY; y++)
		{
			//Loop to right then left to get environement temp
			for (int x = oX + 1; x < maxRight; x++)
			{
				if (isGround(x, y)) break;

				const float distance = float((oX - x) * (oX - x) + (oY - y) * (oY - y));
				const float cAmount = -(distance / maxDistance - 1);
				if (abs(cAmount) > 1 || cAmount <= 0.0f) continue;
				amount += cAmount;
				Taverage += (static_cast<float>(m_envGrid.potTemp[x + y * GRIDSIZESKYX]) * glm::pow(ps[x + y * GRIDSIZESKYX] / m_groundGrid.P[x], Rsd / Cpd)) * cAmount;
			}
			for (int x = oX - 1; x > minLeft; x--)
			{
				if (isGround(x, y)) break;

				const float distance = float((oX - x) * (oX - x) + (oY - y) * (oY - y));
				const float cAmount = -(distance / maxDistance - 1);
				if (abs(cAmount) > 1 || cAmount <= 0.0f) continue;
				amount += cAmount;
				Taverage += (static_cast<float>(m_envGrid.potTemp[x + y * GRIDSIZESKYX]) * glm::pow(ps[x + y * GRIDSIZESKYX] / m_groundGrid.P[x], Rsd / Cpd)) * cAmount;
			}
			//If nothing got added continue, no need to reset.
			if (amount == 0) continue;

			Taverage /= amount + 1e-32f;
			//Temp environment
			const float Te = T - Taverage;
			//Temp parcel
			const float Tp = PTemps[PTempIdx];
			
			//Buoyancy on this layer
			// g * (1.0f - (Mda / avMolarMass) * (Tp / Te))
			Baverage += g * (Te / Tp + 0.608f * Qe[PTempIdx]);
			Bamount++;
			PTempIdx++;

			//Reset
			Taverage = 0.0f;
			amount = 0.0f;

		}

		//Cleanup
		delete[] PTemps;
		delete[] Qe;

		//TODO: delete if above works
		//for (int y = minY; y <= maxY; y++)
		//{
		//	for (int x = 0; x < GRIDSIZESKYX; x++)
		//	{
		//		if (isGround(x, y)) continue;
		//
		//		const float distance = float((oX - x) * (oX - x) + (oY - y) * (oY - y));
		//		const float cAmount = -(distance / maxDistance - 1);
		//		if (abs(cAmount) > 1 || cAmount <= 0.0f) continue;
		//		amount += cAmount;
		//		Taverage += (static_cast<float>(m_envGrid.potTemp[x + y * GRIDSIZESKYX]) * glm::pow(ps[x + y * GRIDSIZESKYX] / m_groundGrid.P[x], Rsd / Cpd)) * cAmount;
		//	}
		//}
		//Taverage /= amount;
	}
	//const float Te = T - Taverage;
	//const float Tp = T;
	//return g * (Te / Tp + 0.608f * Qe);


	return Baverage / Bamount; //Take the average
}

glm::vec2 environment::vorticityConfinement(const int i)
{
	//Not anymore (Using) https://www.researchgate.net/publication/239547604_Modification_of_the_Euler_equations_for_vorticity_confinement''_Application_to_the_computation_of_interacting_vortex_rings
	//Now using https://sci-hub.se/downloads/2020-12-24/50/10.1080@10618562.2020.1856822.pdf

	//TODO: this is not really a good solution, but it also does not prevent a lot.
	if (m_NeighData[i].left == OUTSIDE || m_NeighData[i].right == OUTSIDE || m_NeighData[i].up == OUTSIDE || m_NeighData[i].down == OUTSIDE)
	{
		return { 0,0 };
	}





	const float epsilon = 0.08f; //to change the strength of the vorticity confinement
	const float dynVisc = 1.8e-5f;
	const float density = 1.225f; //Air density

	const float center = curl(i, true);
	const float left  = m_NeighData[i].left  == OUTSIDE ? center : m_NeighData[i].left  == GROUND ? center : curl(i - 1);	// Neumann
	const float right = m_NeighData[i].right == OUTSIDE ? center : m_NeighData[i].right == GROUND ? center : curl(i + 1);	// Neumann
	const float up    = m_NeighData[i].up    != SKY ? center : curl(i + GRIDSIZESKYX); // Neumann
	const float down  = m_NeighData[i].down  != SKY ? center : curl(i - GRIDSIZESKYX); // Neumann

	// stress tensor (incompressible) http://www.astro.yale.edu/vdbosch/astro320_summary6.pdf
	const glm::vec2 stressTensor = dynVisc * lap(i); //TODO: just a laplacian?

	const glm::vec2 gradient = {  (right - left) / (2.0f * VOXELSIZE),
								 (up - down) / (2.0f * VOXELSIZE) };


	//TODO: in 3D this part is different, could look at the fluid paper.
	const float magnitude = glm::length(gradient);
	if (magnitude < 1e-6f)
	{
		return stressTensor;
	}

	const glm::vec2 nablaVector = gradient / magnitude;	

	//TODO: we rotate it just 90 degrees to get the perpindicular vector
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


	const float density = 2;// 1.225f; //Air density

	for (int i = 0; i < GRIDSIZESKY; i++)
	{
		if (isGround(i)) continue;

		const float NxPresProj = m_NeighData[i].right != SKY ? presProjections[i] : presProjections[i + 1];
		const float NyPresProj = m_NeighData[i].up != SKY ? presProjections[i] : presProjections[i + GRIDSIZESKYX];

		//TODO: not sure which boundary condition to use
		m_envGrid.velField[i].x += ((0.5f * density) * ((NxPresProj - presProjections[i])));
		m_envGrid.velField[i].y += ((0.5f * density) * ((NyPresProj - presProjections[i])));
		presProjectionsDebug[i] = (0.5f * density) * ((NyPresProj - presProjections[i])); //Debug
		presProjections[i] = (0.5f * density) * ((NxPresProj - presProjections[i])); //Debug
	}
	//setDebugArray(presProjections, 1); // X
	//setDebugArray(presProjectionsDebug, 1); // Y
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
			//if (isGround(x, y)) continue;

			A[x + y * GRIDSIZESKYX].x = isGround(x + 1, y) ? 0 : -1;
			A[x + y * GRIDSIZESKYX].y = y < GRIDSIZESKYY - 1 && !isGround(x, y + 1) ? -1 : 0;
			A[x + y * GRIDSIZESKYX].z = 0;

			//TODO: Nuemann is using sides, so they are fluids
			if (!isGround(x + 1, y)) A[x + y * GRIDSIZESKYX].z++;
			if (y < GRIDSIZESKYY - 1 && !isGround(x, y + 1)) A[x + y * GRIDSIZESKYX].z++;
			if (!isGround(x - 1, y)) A[x + y * GRIDSIZESKYX].z++;
			if (y > 0 && !isGround(x, y - 1)) A[x + y * GRIDSIZESKYX].z++;
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
			if (isGround(i)) continue;
			maxr = std::max(maxr, fabsf(r[i]));
		}
		if (maxr == 0) return;
	}

	calculatePrecon(precon, A);
	
	applyPreconditioner(precon, divergence, A, q, z);
	s = z;
	//setDebugArray(r, 2);

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
		const float Ucurr = m_envGrid.velField[i].x;
		const float Umin1 = m_NeighData[i].left == OUTSIDE ? m_envGrid.velField[i].x : m_NeighData[i].left == GROUND ? 0.0f : m_envGrid.velField[i - 1].x; //Using Neumann boundary condition (or no-slip if ground)
		const float Vcurr = m_envGrid.velField[i].y;
		const float Vmin1 = m_NeighData[i].down != SKY ? 0.0f : m_envGrid.velField[i - GRIDSIZESKYX].y; //Using no-slip boundary condition for ground

		output[i] = (Ucurr - Umin1) / VOXELSIZE + (Vcurr - Vmin1) / VOXELSIZE; //TODO: Division by voxelsize?
	}
}

void environment::calculatePrecon(std::vector<float>& output, std::vector<glm::ivec3>& A)
{
	const float Tune = 0.97f;

	for (int y = 0; y < GRIDSIZESKYY; y++)
	{
		for (int x = 0; x < GRIDSIZESKYX; x++)
		{
			if (isGround(x, y)) continue;
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
			if (isGround(x, y)) continue;

			const int idx = x + y * GRIDSIZESKYX;

			const int Aminxx = x == 0 ? -1 : A[idx - 1].x; //Minus X looking at x
			const int Aminyy = y == 0 ? 0 : A[idx - GRIDSIZESKYX].y; //Minus Y looking at y

			const float qpi = x - 1 < 0 ? q[x + y * GRIDSIZESKYX] : q[x - 1 + y * GRIDSIZESKYX];
			const float qpj = y - 1 < 0 ? 0.0f : q[x + (y - 1) * GRIDSIZESKYX];

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
			const float zpj = y + 1 >= GRIDSIZESKYY ? 0.0f : output[x + (y + 1) * GRIDSIZESKYX];

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
			if (isGround(x, y)) continue;

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
	const float b = 0.8f;
	const float a = glm::pow(2115.0f, 1 - b); // b^1-b??
	const float d = 0.25f;
	const float c = glm::pow(152.93f, 1 - d); //TODO d^1-d?
	const float CD = 0.6f;

	//Slope Parameters
	float SPR{0.0f};
	float SPS{0.0f};
	float SPI{0.0f};
	{
		//Check for division by 0
		float numerator = PI * densW * N0R;
		float denominator = std::max(densAir * m_envGrid.Qr[i], 1e-14f);
		SPR = (numerator == 0 || denominator == 0) ? 0.0f : pow((numerator / denominator), _e);

		numerator = PI * densW * N0S;
		denominator = std::max(densAir * m_envGrid.Qs[i], 1e-14f);
		SPS = (numerator == 0 || denominator == 0) ? 0.0f : pow((numerator / denominator), _e);

		numerator = PI * densW * N0I;
		denominator = std::max(densAir * m_envGrid.Qi[i], 1e-14f);
		SPI = (numerator == 0 || denominator == 0) ? 0.0f : pow((numerator / denominator), _e);
	}

	//Minimum and maximum particle sizes in cm (could be tweaked)
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
		const float D3 = D * D * D;
		weightedSumNum += UDR * nRD * D3 * stepR;
		weightedSumDenom += nRD * D3 * stepR;
	}
	outputVelocity.x = weightedSumNum / weightedSumDenom;
	weightedSumNum = 0;
	weightedSumDenom = 0;

	//Snow
	for (float D = minDS; D < maxDS; D += stepS)
	{
		const float nSD = N0S * exp(-SPS * D); //Exponential distribution (how many particles)
		const float UDS = c * pow(D, d) * sqrt(densS / densAir);
		const float D3 = D * D * D;
		weightedSumNum += UDS * nSD * D3 * stepS;
		weightedSumDenom += nSD * D3 * stepS;
	}
	outputVelocity.y = weightedSumDenom == 0.0f ? 0.0f : weightedSumNum / weightedSumDenom;
	weightedSumNum = 0;
	weightedSumDenom = 0;

	//Ice
	for (float D = minDI; D < maxDI; D += stepI)
	{
		const float nID = N0I * exp(-SPI * D); //Exponential distribution (how many particles)
		const float UDI = sqrt(4 / (3 * CD)) * pow(D, 0.5f) * sqrt(densI / densAir);
		const float D3 = D * D * D;
		weightedSumNum += UDI * nID * D3 * stepI;
		weightedSumDenom += nID * D3 * stepI;
	}
	outputVelocity.z = weightedSumDenom == 0.0f ? 0.0f : weightedSumNum / weightedSumDenom;

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
		EW_min_CW = std::min(QWS - m_envGrid.Qv[i], m_envGrid.Qw[i]);

		if (T <= 0.0f + 273.15f)
		{
			const float a = 0.5f; // capacitance for hexagonal crystals
			const float quu = std::max(1e-12f * meteoformulas::Ni(T - 273.15f) / D, m_envGrid.Qi[i]);
			BW = std::min(m_envGrid.Qw[i], pow((1 - a) * meteoformulas::cvd(T - 273.15f, ps[i], D) * dt + pow(quu, 1 - a), 1 / (1 - a)) - m_envGrid.Qi[i]);
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

		const float check = Cpd / Lf * T;
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
		DC_min_SC = std::max(0.0f, std::min(QWI - m_envGrid.Qv[i], m_envGrid.Qc[i]));
	}
	{
		// Rate coefficient found in https://journals.ametsoc.org/view/journals/apme/22/6/1520-0450_1983_022_1065_bpotsf_2_0_co_2.xml
		const float BAC = 1e-3f * exp(0.025f * (T - 273.15f)); // Aggregation rate of ice to snow rate coefficient:
		// All values below are tweakable.
		const float BAW{ 2e-3f };	//NotSure		  // Aggregation rate of liquid to rain rate coefficient:
		const float BKW{ 3e-3f };	//NotSure		  // Aggregation rate of rain hitting rain rate coefficient:
		const float BKC{ 4e-3f };	//NotSure		  // Aggregation rate of cold cloud to snow rate coefficient:
		const float BRW{ 2e-3f };	//NotSure		  // Aggregation rate of ice and cloud to ice (precip) rate coefficient:
		const float BRS{ 6e-3f };	//NotSure		  // Aggregation rate of ice and cloud to ice (precip) rate coefficient:
		const float BFR{ 1e-3f };	//NotSure		  // Aggregation rate of freezing rain to ice (precip) rate coefficient:
		const float BER{ 5e-4f };	//NotSure		  // Aggregation rate of rain to vapor rate coefficient:
		const float BES{ 5e-4f };	//NotSure		  // Aggregation rate of snow to vapor rate coefficient:
		const float BEI{ 5e-4f };	//NotSure		  // Aggregation rate of ice to vapor rate coefficient:
		const float BEG{ 100.0f };	//NotSure		  // Evaporation rate of dry ground

		const float qwmin = 0.001f; // the minimum cloud water content required before rainmaking begins
		const float qcmin = 0.001f; // the minimum cloud ice content required before snowmaking begins

		AW = BAW * std::max(m_envGrid.Qw[i] - qwmin, 0.0f);
		KW = BKW * m_envGrid.Qw[i] * m_envGrid.Qr[i];
		AC = BAC * std::max(m_envGrid.Qc[i] - qcmin, 0.0f);
		KC = BKC * m_envGrid.Qc[i] * m_envGrid.Qs[i];
		RW = BRW * m_envGrid.Qi[i] * m_envGrid.Qw[i];
		RS = BRS * m_envGrid.Qs[i] * m_envGrid.Qw[i];
		if (T < -8 + 273.15f) FR = std::min(BFR * (T - 273.15f + 8) * (T - 273.15f + 8), m_envGrid.Qr[i]); //use min to not use rain if there is none

		//Constraint on how much can be evaporated (don't evap if air can't hold more)
		//We first make sure that we are not adding more vapor than the air can hold, then we add up all the things we can evap, multiplied by dt.
		ER = BER * std::min(m_envGrid.Qr[i], std::min(m_speed * dt * (m_envGrid.Qw[i] + m_envGrid.Qr[i]), std::max(QWS - m_envGrid.Qv[i], 0.0f)));
		ES = BES * std::min(m_envGrid.Qs[i], std::min(m_speed * dt * (m_envGrid.Qc[i] +m_envGrid.Qs[i] + m_envGrid.Qi[i]), std::max(QWI - m_envGrid.Qv[i], 0.0f)));
		EI = BEI * std::min(m_envGrid.Qi[i], std::min(m_speed * dt * (m_envGrid.Qc[i] +m_envGrid.Qs[i] + m_envGrid.Qi[i]), std::max(QWI - m_envGrid.Qv[i], 0.0f)));

		GR = m_envGrid.Qr[i];
		GS = m_envGrid.Qs[i];
		GI = m_envGrid.Qi[i];
		const float D_ = 1e-6f; // Weigthed mean diffusivity of the ground //TODO: hmm, could tripple check if right
		const float O_ = 0.01f; // evaporative ground water storage coefficient (i.e. only part of the soil can be evaporated) 
		//Only if Qgj = 0 (Precip falling) and if we are at the ground
		if (int(float(i) / GRIDSIZEGROUND) == m_GHeight[i % GRIDSIZESKYX])
		{
			if (m_groundGrid.Qgr[i] == 0 && m_groundGrid.Qgs[i] == 0 && m_groundGrid.Qgi[i] == 0)
			{
				const float waterA = m_groundGrid.Qrs[i];
				const float time = m_groundGrid.t[i];
				EG = BEG * D_ * waterA * exp(-time / (86400 * O_));
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
	m_envGrid.Qv[i] += m_speed * dt * (EW_min_CW + DC_min_SC + ER + EI + ES + EG);
	m_envGrid.Qw[i] += m_speed * dt * (-EW_min_CW + MC - AW - KW - RW - FW - BW);
	m_envGrid.Qc[i] += m_speed * dt * (DC_min_SC + FW + BW - AC - KC - MC);
	m_envGrid.Qr[i] += m_speed * dt * (AW + KW + MS + MI - ER - FR - GR);
	m_envGrid.Qs[i] += m_speed * dt * (AC + KC - MS - ES - RS - GS);
	m_envGrid.Qi[i] += m_speed * dt * (FR + RS + RW - EI - MI - GI);

	if (m_envGrid.Qw[i] > 0.0f)
	{
		AW = 0;
	}

	if (m_envGrid.Qv[i] < 0 || m_envGrid.Qw[i] < 0 || m_envGrid.Qc[i] < 0 || m_envGrid.Qr[i] < 0 || m_envGrid.Qs[i] < 0 || m_envGrid.Qi[i] < 0 || 
		m_envGrid.Qv[i] != m_envGrid.Qv[i] || m_envGrid.Qw[i] != m_envGrid.Qw[i] || m_envGrid.Qc[i] != m_envGrid.Qc[i] || m_envGrid.Qr[i] != m_envGrid.Qr[i] || m_envGrid.Qs[i] != m_envGrid.Qs[i] || m_envGrid.Qi[i] != m_envGrid.Qi[i])
	{
		AW = 0;
	}

}

float environment::calculateSumPhaseHeat(const int i, const float T, const float pQv, const float pQw, const float pQc, const float pQr, const float pQs, const float pQi)
{
	float ratio = float(T / m_envGrid.potTemp[i]);
	float sumPhaseheat = 0.0f;

	float dQv = m_envGrid.Qv[i] - pQv;
	float dQw = m_envGrid.Qw[i] - pQw;
	float dQc = m_envGrid.Qc[i] - pQc;
	float dQr = m_envGrid.Qr[i] - pQr;
	float dQs = m_envGrid.Qs[i] - pQs;
	float dQi = m_envGrid.Qi[i] - pQi;
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

	const int pX = m_NeighData[i].left != SKY ? i : i - 1;
	const int nX = m_NeighData[i].right != SKY ? i : i + 1;
	const int pY = m_NeighData[i].down != SKY ? i : i - GRIDSIZESKYX;
	const int nY = m_NeighData[i].up != SKY ? i : i + GRIDSIZESKYX;

	const glm::vec2 nabla0 = //∇0
	{
		(m_envGrid.potTemp[nX] - m_envGrid.potTemp[pX]) / (2.0f * float(VOXELSIZE)),
		(m_envGrid.potTemp[nY] - m_envGrid.potTemp[pY]) / (2.0f * float(VOXELSIZE))
	};

	//directional derivative
	const float DirDer = m_envGrid.velField[i].x * nabla0.x + m_envGrid.velField[i].y * nabla0.y;

	m_envGrid.potTemp[i] += (dt * -DirDer + sumHeat);

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

float environment::getIsentropicTemp(const float coordy)
{
	//Clamp
	const int index = coordy < 0.0f ? 0 : coordy >= GRIDSIZESKYY ? GRIDSIZESKYY - 1 : int(coordy);

	return m_isenTropicTemps[index];




	//OLD WAY USING CALCULATIONS, WAS NOT REALLY VALID

	////Map to closest valid value
	//int x = coordx < 0 ? 0 : coordx >= GRIDSIZESKYX ? GRIDSIZESKYX - 1 : int(coordx);

	////We just get the avarage from 2000 and 5000 meters.
	//int index2 = x + int(2000 / VOXELSIZE) * GRIDSIZESKYX;
	//int index5 = x + int(5000 / VOXELSIZE) * GRIDSIZESKYX;
	//float difference = 3;
	////Except when that is invalid
	//if (int(index5 / GRIDSIZESKYX) > GRIDSIZESKYY)
	//{
	//	//Just use variable height based on current max height
	//	index2 = x + int(float(GRIDSIZESKYY) * 0.2f) * GRIDSIZESKYX;
	//	index5 = x + int(float(GRIDSIZESKYY) * 0.5f) * GRIDSIZESKYX;
	//	difference = float(index5) / GRIDSIZESKYX * VOXELSIZE - float(index2) / GRIDSIZESKYX * VOXELSIZE;
	//	difference *= 0.001f;
	//}
	//const float T2 = m_envGrid.potTemp[index2] * glm::pow(m_pressures[index2] / m_groundGrid.P[index2 % GRIDSIZESKYX], Rsd / Cpd);
	//const float T5 = m_envGrid.potTemp[index5] * glm::pow(m_pressures[index5] / m_groundGrid.P[index5 % GRIDSIZESKYX], Rsd / Cpd);

	//const float lapseRate = (T5 - T2) / difference;

	//return m_groundGrid.T[int(x)] + (int(coordy / GRIDSIZESKYX) * VOXELSIZE) * 0.001f * lapseRate;
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

#include "environment.h"

#include "math/meteoformulas.h"
#include "math/constants.hpp"
#include "math/math.hpp"
#include "core/engine.hpp"
#include "imgui/IconsFontAwesome.h"
#include "rendering/colors.hpp"

#include "rendering/debug_render.hpp"


//|-----------------------------------------------------------------------------------------------------------|
//|                                                 ImGui                                                     |
//|-----------------------------------------------------------------------------------------------------------|

#ifdef BEE_INSPECTOR

// ImGui integration.
std::string environment::GetName() const { return Title; }
std::string environment::GetIcon() const { return ICON_FA_TERMINAL; }

environment::environment()
{
	for (int i = 0; i < GRIDSIZESKY; i++)
	{
		envGrid.Qv[i] = 0.01f;
		envGrid.Qw[i] = 0.01f;
		envGrid.Qc[i] = 0.01f;
		envGrid.Qr[i] = 0.01f;
		envGrid.Qs[i] = 0.01f;
		envGrid.Qi[i] = 0.01f;
		envGrid.potTemp[i] = 30.0f;
	}
	for (int i = 0; i < GRIDSIZEGROUND; i++)
	{
		groundGrid.P[i] = 1000.0f;
		groundGrid.T[i] = 15.0f;
	}
	auto& colorScheme = bee::Engine.DebugRenderer().GetColorScheme();
	colorScheme.createColorScheme("TemperatureSky", -40, bee::Colors::Blue, 40, bee::Colors::Red);
	colorScheme.addColor("TemperatureSky", -10, bee::Colors::Cyan);
	colorScheme.addColor("TemperatureSky", 10, bee::Colors::Green);
	colorScheme.addColor("TemperatureSky", 20, bee::Colors::Yellow);
}

void environment::OnPanel()
{
	auto& colorScheme = bee::Engine.DebugRenderer().GetColorScheme();

	for (float y = 0; y < GRIDSIZESKYY; y++)
	{
		for (float x = 0; x < GRIDSIZESKYX; x++)
		{
			//Get temp
			const float height = y * VOXELSIZE;
			const float pressure = meteoformulas::getStandardPressureAtHeight(groundGrid.T[int(x)], height);
			const float T = envGrid.potTemp[int(x) + int(y) * GRIDSIZESKYX] * glm::pow(pressure / groundGrid.P[int(x)], Constants::Rsd / Constants::Cpd);


			bee::Engine.DebugRenderer().AddSquare(bee::DebugCategory::All, glm::vec3(x + VOXELSIZE * 0.5f, y + VOXELSIZE * 0.5f, 0.0f), 1.0f, glm::vec3(0, 0, 1), bee::Colors::White);
			glm::vec3 color{};
			colorScheme.getColor("TemperatureSky", T, color);
			bee::Engine.DebugRenderer().AddFilledSquare(bee::DebugCategory::All, glm::vec3(x + VOXELSIZE * 0.5f, y + VOXELSIZE * 0.5f, 0.0f), 0.9f, glm::vec3(0, 0, 1), { color, 1.0f });

		}
	}
	for (int x = 0; x < GRIDSIZEGROUND; x++)
	{
		bee::Engine.DebugRenderer().AddSquare(bee::DebugCategory::All, glm::vec3(x + VOXELSIZE * 0.5f, -0.5f, 0.0f), 1.0f, glm::vec3(0, 0, 1), bee::Colors::White);
	}
	std::unordered_map<const char*, std::pair<int, glm::vec3>> colorSchemes;

}

#endif


//|-----------------------------------------------------------------------------------------------------------|
//|                                                  Code                                                     |
//|-----------------------------------------------------------------------------------------------------------|

using namespace half_float;
using namespace Constants;

void environment::Update(float dt)
{
	// Update Ground
	for (int i = 0; i < GRIDSIZEGROUND; i++)
	{

	}

	// Fill Pressures
	float* pressures = new float[GRIDSIZESKY];
	for (int i = 0; i < GRIDSIZESKY; i++)
	{
		const float height = std::floorf(float(i) / GRIDSIZESKYY) * VOXELSIZE;
		pressures[i] = meteoformulas::getStandardPressureAtHeight(groundGrid.T[i], height);
	}

	// Update sky
	for (int i = 0; i < GRIDSIZESKY; i++)
	{
		// 1. Update Sky Temps
		// 2. Diffuse and Advect potential temp
		// 3. Diffuse, Advect and pressure project velocity field
		// 4. Diffuse and Advect water content qj
		// 5. water transfer in ground and vorticity confinement. 
		// 6. Update microphysics
		// 7. Compute heat transfer (form. 67)

		// 1.
		const float T = envGrid.potTemp[i] * glm::pow(pressures[i] / groundGrid.P[i % GRIDSIZESKYX], Rsd / Cpd);

		// 2.
		diffuseAndAdvect(dt, i, envGrid.potTemp);

		// 3.
		const float Tv = T * (0.608f * envGrid.Qv[i] + 1);
		const float density = pressures[i] / (Rsd * Tv);
		const float B = calculateBuoyancy(i, pressures, T);
		updateVelocityField(dt, i, pressures, density, B);

		// 4.
		glm::vec3 fallVelocitiesPrecip = calculateFallingVelocity(dt, i, density);
		diffuseAndAdvect(dt, i, envGrid.Qv);
		diffuseAndAdvect(dt, i, envGrid.Qw);
		diffuseAndAdvect(dt, i, envGrid.Qc);
		diffuseAndAdvect(dt, i, envGrid.Qr, fallVelocitiesPrecip.x);
		diffuseAndAdvect(dt, i, envGrid.Qs, fallVelocitiesPrecip.y);
		diffuseAndAdvect(dt, i, envGrid.Qi, fallVelocitiesPrecip.z);

		// 5.

		// 6.
		const float pQv = envGrid.Qv[i], pQw = envGrid.Qw[i], pQc = envGrid.Qc[i], pQr = envGrid.Qr[i], pQs = envGrid.Qs[i], pQi = envGrid.Qi[i];
		updateMicroPhysics(dt, i, pressures, T, density);

		// 7.
		const float sumPhaseHeat = calculateSumPhaseHeat(i, T, pQv, pQw, pQc, pQr, pQs, pQi);
		computeHeatTransfer(dt, i, sumPhaseHeat);
	}



}

using namespace Constants;

void environment::updateVelocityField(const float dt, const int i, const float* ps, const float D, const float B)
{
	//u[t+1] = u + deltaTime * -dot(u, ∇u) - (1/ρ)∇p + v*(∇^2u) + b + f; 
	
	const float viscocity{ 1e-5f };
	//const int pY{ i - 1 * GRIDSIZESKYY };
	//const int nY{ i + 1 * GRIDSIZESKYY };
	//const int pX{ i - 1 };
	//const int nX{ i + 1 };
	const int pX = (i % GRIDSIZESKYX == 0) ? i : i - 1;
	const int nX = ((i + 1) % GRIDSIZESKYX == 0) ? i : i + 1; 
	const int pY = (i / GRIDSIZESKYX == 0) ? i : i - GRIDSIZESKYX;
	const int nY = (i / GRIDSIZESKYX == GRIDSIZESKYY - 1) ? i : i + GRIDSIZESKYX; 


	const glm::vec2 nablaUu = //∇u.x
	{
		(envGrid.velField[nX].x - envGrid.velField[pX].x) / (2.0f * float(GRIDSIZESKYX)),
		(envGrid.velField[nY].x - envGrid.velField[pY].x) / (2.0f * float(GRIDSIZESKYY))
	};
	const glm::vec2 nablaUv = //∇u.y
	{
		(envGrid.velField[nX].y - envGrid.velField[pX].y) / (2.0f * float(GRIDSIZESKYX)),
		(envGrid.velField[nY].y - envGrid.velField[pY].y) / (2.0f * float(GRIDSIZESKYY))
	};

	const glm::vec2 dot = { -glm::dot(envGrid.velField[i], nablaUu),
							-glm::dot(envGrid.velField[i], nablaUv) };

	const glm::vec2 nablaP = //∇p
	{
		(ps[nX] - ps[pX]) / (2.0f * float(GRIDSIZESKYX)),
		(ps[nY] - ps[pY]) / (2.0f * float(GRIDSIZESKYY))
	};

	const glm::vec2 nablaU2 = //∇u^2
	{
		(envGrid.velField[nX] - (2.0f * envGrid.velField[i]) + envGrid.velField[pX]) / (float(GRIDSIZESKYX) * float(GRIDSIZESKYX)) +
		(envGrid.velField[nY] - (2.0f * envGrid.velField[i]) + envGrid.velField[pY]) / (float(GRIDSIZESKYY) * float(GRIDSIZESKYY))
	};


	//	New value		=     Old Value		  + timestep * movement of air -     pressure gradient    +	     smoothing	    + buoyancy + extra forces
	envGrid.velField[i] = envGrid.velField[i] +    dt    *      dot		   -   (1 / D) * nablaP + viscocity * nablaU2 +	  B	   +     0.0f;

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
		(array[nX] - (2.0f * array[i]) + array[pX]) / (float(GRIDSIZESKYX) * float(GRIDSIZESKYX)) +
		(array[nY] - (2.0f * array[i]) + array[pY]) / (float(GRIDSIZESKYY) * float(GRIDSIZESKYY))
	};

	array[i] = array[i] - dt * viscocity * nabla02;


	//Advect
	const glm::vec2 nabla0 = //∇0
	{
		(array[nX] - array[pX]) / (2.0f * float(GRIDSIZESKYX)),
		(array[nY] - array[pY]) / (2.0f * float(GRIDSIZESKYY))
	};

	const float dot = glm::dot(envGrid.velField[i], nabla0);

	array[i] = array[i] - dt * dot;
}

void environment::diffuseAndAdvect(const float dt, const int i, half* array, const float falVel)
{
	// Diffuse
	const float viscocity = 2.2e-5f;

	//const int pY = i - 1 * GRIDSIZESKYY;
	//const int nY = i + 1 * GRIDSIZESKYY;
	//const int pX = i - 1;
	//const int nX = i + 1;
	const int pX = (i % GRIDSIZESKYX == 0) ? i : i - 1;
	const int nX = ((i + 1) % GRIDSIZESKYX == 0) ? i : i + 1;
	const int pY = (i / GRIDSIZESKYX == 0) ? i : i - GRIDSIZESKYX;
	const int nY = (i / GRIDSIZESKYX == GRIDSIZESKYY - 1) ? i : i + GRIDSIZESKYX;

	const float nabla02 = //∇θ^2
	{
		(array[nX] - (2.0f * array[i]) + array[pX]) / (float(GRIDSIZESKYX) * float(GRIDSIZESKYX)) +
		(array[nY] - (2.0f * array[i]) + array[pY]) / (float(GRIDSIZESKYY) * float(GRIDSIZESKYY))
	};

	array[i] = array[i] - dt * viscocity * nabla02;


	//Advect
	const glm::vec2 nabla0 = //∇0
	{
		(array[nX] - array[pX]) / (2.0f * float(GRIDSIZESKYX)),
		(array[nY] - array[pY]) / (2.0f * float(GRIDSIZESKYY))
	};

	//TODO: - or + (is falVel positive or negative?)
	const float dot = glm::dot(envGrid.velField[i] + glm::vec2(0.0f, falVel), nabla0);

	array[i] = array[i] - dt * dot;
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
	const float SPR = pow((PI * densW * N0R) / (densAir * static_cast<float>(envGrid.Qr[i])), _e);
	const float SPS = pow((PI * densS * N0S) / (densAir * static_cast<float>(envGrid.Qs[i])), _e);
	const float SPI = pow((PI * densI * N0I) / (densAir * static_cast<float>(envGrid.Qi[i])), _e);

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

float environment::calculateBuoyancy(const int , const float* , const float )
{
	return 1.0f;
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
	float IR = 0.0f; //	Water flowwing through the ground (through holes in ground)
	float EG = 0.0f; // Evaporation of dry ground
	
	const float QWS = meteoformulas::ws(T, ps[i]);
	const float QWI = meteoformulas::wi(T, ps[i]);

	if (T >= -40)
	{
		EW_min_CW = std::min(QWS - static_cast<float>(envGrid.Qv[i]), static_cast<float>(envGrid.Qw[i]));

		if (T <= 0.0f)
		{
			const float a = 0.5f; // capacitance for hexagonal crystals
			const float quu = std::max(1e-12f * meteoformulas::Ni(T) / D, static_cast<float>(envGrid.Qi[i]));
			BW = std::min(static_cast<float>(envGrid.Qw[i]), pow((1 - a) * meteoformulas::cvd(T, ps[i], D) * dt + pow(quu, 1 - a), 1 / (1 - a)) - static_cast<float>(envGrid.Qi[i]));
		}
	}
	if (T < -40)
	{
		FW = static_cast<float>(envGrid.Qw[i]);
	}
	if (T > 0)
	{
		MC = static_cast<float>(envGrid.Qc[i]); //Part of melting (below = info)
	}
	if (T <= 0)
	{
		DC_min_SC = std::min(QWI - static_cast<float>(envGrid.Qv[i]), static_cast<float>(envGrid.Qc[i]));
	}
	{
		// Rate coefficient found in https://journals.ametsoc.org/view/journals/apme/22/6/1520-0450_1983_022_1065_bpotsf_2_0_co_2.xml
		const float BAC = 1e-3f * exp(0.025f * T); // Aggregation rate of ice to snow rate coefficient:
		// All values below are tweakable.
		const float BAW{ 1.0f };					  // Aggregation rate of liquid to rain rate coefficient:
		const float BKW{ 1.0f };					  // Aggregation rate of rain hitting rain rate coefficient:
		const float BKC{ 1.0f };					  // Aggregation rate of cold cloud to snow rate coefficient:
		const float BRW{ 1.0f };					  // Aggregation rate of ice and cloud to ice (precip) rate coefficient:
		const float BRS{ 1.0f };					  // Aggregation rate of ice and cloud to ice (precip) rate coefficient:
		const float BFR{ 1.5f };					  // Aggregation rate of freezing rain to ice (precip) rate coefficient:
		const float BMI{ 1.0f };					  // Aggregation rate of ice to rain rate coefficient:
		const float BER{ 1.0f };					  // Aggregation rate of rain to vapor rate coefficient:
		const float BES{ 1.0f };					  // Aggregation rate of snow to vapor rate coefficient:
		const float BEI{ 1.0f };					  // Aggregation rate of ice to vapor rate coefficient:
		const float BIR{ 1.0f };					  // Evaporation rate of subsurface water
		const float k{ 1.0f };						  // How easy water flows through ground
		const float BEG{ 1.0f };					  // Evaporation rate of dry ground

		const float qwmin = 0.001f; // the minimum cloud water content required before rainmaking begins
		const float qcmin = 0.001f; // the minimum cloud ice content required before snowmaking begins

		AW = BAW * std::max(static_cast<float>(envGrid.Qw[i]) - qwmin, 0.0f);
		KW = BKW * static_cast<float>(envGrid.Qw[i]) * static_cast<float>(envGrid.Qr[i]);
		AC = BAC * std::max(static_cast<float>(envGrid.Qc[i]) - qcmin, 0.0f);
		KC = BKC * static_cast<float>(envGrid.Qc[i]) * static_cast<float>(envGrid.Qs[i]);
		RW = BRW * static_cast<float>(envGrid.Qi[i]) * static_cast<float>(envGrid.Qw[i]);
		RS = BRS * static_cast<float>(envGrid.Qs[i]) * static_cast<float>(envGrid.Qw[i]);
		if (T < -8) FR = BFR * (T + 8) * (T + 8);
		// 𝛿(𝑋𝑐 + 𝑋𝑠 + 𝑋𝑖) ≤ 𝑐𝑝air / 𝐿𝑓 * 𝑇, == First melt cloud ice, then snow, then precip ice, when the criteria is met between any step, stop melting.
		if (T > 0) MS = static_cast<float>(envGrid.Qs[i]);
		if (T > 0) MI = BMI * T;

		//Constraint on how much can be evaporated (don't evap if air can't hold more)
		ER = BER * std::min(static_cast<float>(envGrid.Qw[i]) + static_cast<float>(envGrid.Qr[i]), std::max(QWS - static_cast<float>(envGrid.Qv[i]), 0.0f));
		ES = BES * std::min(static_cast<float>(envGrid.Qc[i]) + static_cast<float>(envGrid.Qs[i]) + static_cast<float>(envGrid.Qi[i]), std::max(QWI - static_cast<float>(envGrid.Qv[i]), 0.0f));
		EI = BEI * std::min(static_cast<float>(envGrid.Qc[i]) + static_cast<float>(envGrid.Qs[i]) + static_cast<float>(envGrid.Qi[i]), std::max(QWI - static_cast<float>(envGrid.Qv[i]), 0.0f));

		GR = static_cast<float>(envGrid.Qr[i]);
		GS = static_cast<float>(envGrid.Qs[i]);
		GI = static_cast<float>(envGrid.Qi[i]);
		const float Qgr = 1.0f; //Ground rain water, TODO: add formula for this
		const float D_ = 1.0f; // Weigthed mean diffusivity of the ground //TODO: add this
		const float O_ = 1.0f; // evaporative ground water storage coefficient // TODO: also this
		IR = BIR * k * Qgr;
		//Only if Qgj = 0 (Precip falling)
		EG = BEG * BEG * D_ * Qgr * exp(-dt / 24 * O_);
	}

	//𝐷𝑡𝑞𝑣 = 𝐸𝑊 + 𝑆𝐶 + 𝐸𝑅 + 𝐸𝐼 + 𝐸𝑆 + 𝐸𝐺 − 𝐶𝑊 − 𝐷𝐶, (9)
	//𝐷𝑡𝑞𝑤 = 𝐶𝑊 + 𝑀𝐶 − 𝐸𝑊 − 𝐴𝑊 − 𝐾𝑊 − 𝑅𝑊 − 𝐹𝑊 − 𝐵𝑊, (10)
	//𝐷𝑡𝑞𝑐 = 𝐷𝐶 + 𝐹𝑊 + 𝐵𝑊 − 𝑆𝐶 − 𝐴𝐶 − 𝐾𝐶 − 𝑀𝐶, (11)
	//𝐷𝑡𝑞𝑟 = 𝐴𝑊 + 𝐾𝑊 + 𝑀𝑆 + 𝑀𝐼 − 𝐸𝑅 − 𝐹𝑅 − 𝐺𝑅, (12)
	//𝐷𝑡𝑞𝑠 = 𝐴𝐶 + 𝐾𝐶 − 𝑀𝑆 − 𝐸𝑆 − 𝑅𝑆 − 𝐺𝑆, (13)
	//𝐷𝑡𝑞𝑖 = 𝐹𝑅 + 𝑅𝑆 + 𝑅𝑊 − 𝐸𝐼 − 𝑀𝐼 − 𝐺𝐼

	//TODO: check if everything is right
	envGrid.Qv[i] = dt * (EW_min_CW + DC_min_SC + ER + EI + ES + EG);
	envGrid.Qw[i] = dt * (EW_min_CW + MC - AW - KW - RW - FW - BW);
	envGrid.Qc[i] = dt * (DC_min_SC + FW + BW - AC - KC - MC);
	envGrid.Qr[i] = dt * (AW + KW + MS + MI - ER - FR - GR);
	envGrid.Qs[i] = dt * (AC + KC - MS - ES - RS - GS);
	envGrid.Qi[i] = dt * (FR + RS + RW - EI - MI - GI);
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
		(envGrid.potTemp[nX] - envGrid.potTemp[pX]) / (2.0f * float(GRIDSIZESKYX)),
		(envGrid.potTemp[nY] - envGrid.potTemp[pY]) / (2.0f * float(GRIDSIZESKYY))
	};
	const float dot = glm::dot(envGrid.velField[i], nabla0);

	envGrid.potTemp[i] += dt * (-dot + sumHeat);
}

float environment::calculateSumPhaseHeat(const int i, const float T, const float pQv, const float pQw, const float pQc, const float pQr, const float pQs, const float pQi)
{
	float ratio = T / envGrid.potTemp[i];
	float sumPhaseheat = 0.0f;

	const float dQv = static_cast<float>(envGrid.Qv[i]) - pQv;
	const float dQw = static_cast<float>(envGrid.Qw[i]) - pQw;
	const float dQc = static_cast<float>(envGrid.Qc[i]) - pQc;
	const float dQr = static_cast<float>(envGrid.Qr[i]) - pQr;
	const float dQs = static_cast<float>(envGrid.Qs[i]) - pQs;
	const float dQi = static_cast<float>(envGrid.Qi[i]) - pQi;

	const float invCpdRatio = 1.0f / (Cpd * ratio);

	sumPhaseheat += E0v * invCpdRatio * dQv;
	sumPhaseheat += Lf * invCpdRatio * dQw;
	sumPhaseheat += Ls * invCpdRatio * dQc;
	sumPhaseheat += Ls * invCpdRatio * dQr;
	sumPhaseheat += Ls * invCpdRatio * dQs;
	sumPhaseheat += Lf * invCpdRatio * dQi;
	return sumPhaseheat;
}

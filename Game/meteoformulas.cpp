#include "meteoformulas.h"

#include <boost/math/special_functions/lambert_w.hpp>
#include "constants.hpp"


using namespace Constants;

float meteoformulas::es(const float T)
{
	return ptrip * 0.01f * float(exp((17.67f * T) / (T + 243.5f)));
}

float meteoformulas::ws(const float T, const float P)
{
	float Es = es(T) * 100; //hPa to Pa 
	return (E * Es) / (P - Es);
}

float meteoformulas::rs(const float T, const float P)
{
	float Es = es(T);
	return (E * Es) / (P - Es);
}

float meteoformulas::qv(const float T, const float P)
{
	float Es = es(T); 
	return (E * Es) / (P - (1 - E) * Es);
}

float meteoformulas::Rm(const float T, const float P)
{
	const float Qv = ws(T, P);
	return (1 - Qv) * Rsd + Qv * Rsw;
}

float meteoformulas::Cpm(const float T, const float P)
{
	const float Qv = ws(T, P);
	return (1 - Qv) * Cpa + Qv * Cpvw;
}

float meteoformulas::MLR(const float T, const float P)
{
	float TKelvin = T + 273.15f;
	float Ws = rs(T, P);

	float Tw = ((Rsd * TKelvin + Hv * Ws) / (Cpd + (Hv * Hv * Ws * E / (Rsd * TKelvin * TKelvin))));
	return Tw / P;
}

std::vector<float> meteoformulas::getDryLapseRate(const float T0, const std::vector<float>& heights)
{
	std::vector<float> temps;

	for (float height : heights)
	{
		temps.push_back(T0 * height * 0.001f);
	}
	return temps;
}

std::vector<float> meteoformulas::getPotentialTemp(const float T0, const float P0, const std::vector<float>& pressures)
{
	std::vector<float> temps;
	float T = T0;

	for (float P : pressures)
	{
		T = (T0 + 273.15f) * glm::pow((P / P0), Rsd / Cpd);
		temps.push_back(T - 273.15f);
	}
	return temps;
}

float meteoformulas::getStandardHeightAtPressure(const float T0, const float P, const float P0)
{
	const float TKelvin = T0 + 273.15f;
	//float Tv = TKelvin * (1.0f + 0.611f * qv(TKelvin, P * 100));
	//return Rsd * Tv / g * logf(P0 / P);

	return TKelvin / Lb * (glm::pow(P / P0, -R * Lb / (g * (Mda * 0.001f))) - 1);

}

float meteoformulas::getStandardPressureAtHeight(const float T0, const float h, const float h0, const float P0)
{
	const float TKelvin = T0 + 273.15f;

	return P0 * glm::pow(1 + (Lb / TKelvin) * (h - h0), (-g * (Mda * 0.001f)) / (R * Lb));
}

float meteoformulas::getLogPHeight(const float V, const bool Pressure, const float H0)
{
	float log = 0.0f;

	if (Pressure) log = log10(V);
	else log = log10(getStandardPressureAtHeight(0, V)); //TODO: should this be T0 = 0  and others at default?
	
	//log should be a value between 2 and 3 (if value given between 100 and 1000)
	//i.e. 500 ~ 2.7 | 250 ~ 2.4
	
	//Range between 1 and 2
	log -= 1;
	log = log * -1 + 3;

	if (Pressure) return getStandardHeightAtPressure(0,V) * log + H0;
	else return V * log + H0;
}

std::vector<float> meteoformulas::getMoistTemp(const float T0, const float Pref, const std::vector<float>& pressures)
{
	std::vector<float> temps;

	//TODO: check if valid using celsius
	float T = T0; 
	float P = Pref;
	float h = 1.0f;

	for (float Pnext : pressures)
	{
		float dP = Pnext - P;
		// Runge-Kutta 4th Order Method to solve the ODE
		float k1 = h * MLR(T, P);
		float k2 = h * MLR(T + 0.5f * k1, P + 0.5f * dP * h);
		float k3 = h * MLR(T + 0.5f * k2, P + 0.5f * dP * h);
		float k4 = h * MLR(T + k3, P + dP + h);

		T = (1.0f / (6.0f * h)) * (k1 + 2 * k2 + 2 * k3 + k4) * dP + T;
		temps.push_back(T);
		P = Pnext;
	}

	return temps;
}

glm::vec3 meteoformulas::getLCL(const float T0, const float P0, const float Z0, const float D0)
{
	//Formula from https://en.wikipedia.org/wiki/Lifting_condensation_level
	const float TKelvin = T0 + 273.15f;
	const float DKelvin = D0 + 273.15f;
	const float Pa0 = P0 * 100.0f;

	const float CPM = Cpm(TKelvin, Pa0);
	const float RM = Rm(TKelvin, Pa0);
	const float RHl = es(DKelvin) / es(TKelvin);

	const float a = CPM / RM + (Cvl - Cpvw) / Rsw;
	const float b = -1 * ((E0v - (Cvv - Cvl) * Ttrip) / (Rsw * TKelvin));
	const float c = b / a;
	
	const float Wmin1 = boost::math::lambert_wm1(glm::pow(RHl, 1/a) * c * glm::pow(euler, c));
	const float TLCL = c * (1 / Wmin1) * TKelvin;
	const float pLCL = P0 * glm::pow(TLCL / TKelvin, CPM / RM);
	const float zLCL = Z0 + CPM / g * (TKelvin - TLCL);

	return { TLCL - 273.15f, pLCL, zLCL };

}

float meteoformulas::getEL(const float T0, const float P0, const float Z0, const float D0, const skewTInfo& OData)
{
	glm::vec3 LCL = getLCL(T0, P0, Z0, D0);

	std::vector<float> MLRtemps = {};
	 
	//Cut down our needs of calculations (we begin from the LCL height)
	int count = 0;
	while (OData.data[count].pressure > LCL.y) count++;
	const int startCount = count;

	std::vector<float> OPressures(OData.data.size() - startCount);

	std::transform(OData.data.begin() + startCount, OData.data.end(), OPressures.begin(), [](const auto& elem) { return elem.pressure; });


	MLRtemps = getMoistTemp(LCL.x, LCL.y, OPressures);

	//Now check for intersections
	bool outside = false;
	float lastOutsideHeight = -1.0f;

	while (count < int(OData.data.size() - 1))
	{
		if (!outside)
		{
			if (MLRtemps[count - startCount] > OData.data[count].temperature)
			{
				outside = true;
			}
		}
		else
		{
			if (MLRtemps[count - startCount] < OData.data[count].temperature)
			{
				outside = false;
				lastOutsideHeight = OData.data[count].altitude;
			}
		}
		//Note: we don't do anything when the two temps are the same.

		count++;
	}


	return lastOutsideHeight;
}

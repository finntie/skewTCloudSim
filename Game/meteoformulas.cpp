#include "meteoformulas.h"

#include <boost/math/special_functions/lambert_w.hpp>
#include "constants.hpp"


using namespace Constants;

float meteoformulas::es(const float T)
{
	//return (0.61094f * 0.01f) * expf((17.67f * T) / (T + 243.5f)); //Worse accuraccy of around 0.2%
	return (0.61078f * 0.01f) * expf((17.27f * T) / (T + 237.3f));

}

float meteoformulas::ws(const float T, const float P)
{
	float Es = es(T) * 1000;
	return  (E * Es) / (P - Es);
}

float meteoformulas::qv(const float T, const float P)
{
	float Rs = ws(T, P);
	return Rs / (1 + Rs);
}

float meteoformulas::Rm(const float T, const float P)
{
	const float Qv = qv(T, P);
	return (1 - Qv) * Rsd + Qv * Rsw;
}

float meteoformulas::Cpm(const float T, const float P)
{
	const float Qv = qv(T, P);
	return (1 - Qv) * Cpa + Qv * Cpvw;
}

float meteoformulas::MLR(const float T, const float P)
{
	float TKelvin = T + 273.15f;
	float Ws = ws(T, P);
	float Tw = ((Rsd * TKelvin + Hv * Ws) / (Cpd + (Hv * Hv * Ws * E / (Rsd * TKelvin * TKelvin))));
	return Tw / P;
}

std::vector<float> meteoformulas::getTempAtWs(const float Ws, const std::vector<float>& pressures)
{
	std::vector<float> output(pressures.size());
	int i = 0;

	for (auto P : pressures)
	{
		float EsT = (Ws * P) / (E + Ws);

		//TODO wrong formula check es()
		output[i] = (-(log(EsT) - log(6.1078f)) * 243.5f) / (log(EsT) - log(6.1078f) - 17.67f);
		i++;
	}
	return output;
}

std::vector<float> meteoformulas::getDryLapseRate(const float T0, const std::vector<float>& heights)
{
	//???????
	std::vector<float> temps;
	//?????
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
	//TODO: make it so that if values with spacing larger than 1.0 gets calculated correctly
	//TODO: if difference between Pref and pressures[0] is too big, fix it
	std::vector<float> temps;

	float T = T0; 
	float P = Pref;

	for (float Pnext : pressures)
	{
		float dP = Pnext - P;

		T = MLR(T, P) * dP + T;

		temps.push_back(T);
		P = Pnext;
	}

	return temps;
}

glm::vec3 meteoformulas::getCCL(const float P0, const float D0, const skewTInfo& OData)
{
	int count = 0;
	while (OData.data[count].pressure > 100.0f) count++;
	if (count >= OData.data.size()) return { D0, P0, D0 };

	std::vector<float> Pressures(count);
	std::transform(OData.data.begin(), OData.data.begin() + count, Pressures.begin(), [](const auto& elem) { return elem.pressure; });

	const float WS = ws(D0, P0);
	std::vector<float> temps = getTempAtWs(WS, Pressures);

	//Loop until the mixing ratio temp >= observed temp
	count = 0;
	while (count < temps.size() && OData.data[count].temperature > temps[count])
	{
		count++;
	}

	const float PotTemp = (OData.data[count].temperature + 273.15f) / glm::pow((OData.data[count].pressure / P0), Rsd / Cpd) - 273.15f;

	return glm::vec3(OData.data[count].temperature, OData.data[count].pressure, PotTemp);
}


glm::vec3 meteoformulas::getLCL(const float T0, const float P0, const float Z0, const float D0)
{
	//Formula from https://en.wikipedia.org/wiki/Lifting_condensation_level
	const float TKelvin = T0 + 273.15f;
	//const float Pa0 = P0 * 100.0f;

	const float CPM = Cpm(T0, P0);
	const float RM = Rm(T0, P0);
	const float RHl = es(D0) / es(T0);

	const float a = (CPM / RM) + ((Cvl - Cpvw) / Rsw);
	const float b = -(E0v - (Cvv - Cvl) * Ttrip) / (Rsw * TKelvin);
	const float c = b / a;
	
	const float Wmin1 = boost::math::lambert_wm1(glm::pow(RHl, 1/a) * c * glm::exp(c));
	const float TLCL = (c / Wmin1) * TKelvin;
	const float pLCL = P0 * glm::pow(TLCL / TKelvin, CPM / RM);
	const float zLCL = Z0 + CPM / g * (TKelvin - TLCL);

	return { TLCL - 273.15f, pLCL, zLCL };

}

glm::vec3 meteoformulas::getLFC(const float T0, const float P0, const float Z0, const float D0, const skewTInfo& OData)
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
	bool outside = (MLRtemps[count - startCount] > OData.data[count].temperature);

	if (outside) //LFC is already at LCL
	{
		return LCL;
	}

	while (count < int(OData.data.size() - 1))
	{
		if (MLRtemps[count - startCount] > OData.data[count].temperature)
		{
			outside = true;
			return { OData.data[count].temperature, OData.data[count].pressure, OData.data[count].altitude };
		}
		count++;
	}
	//Nothing found :(
	return { -1.0f, -1.0f, -1.0f };
}

glm::vec3 meteoformulas::getEL(const float T0, const float P0, const float Z0, const float D0, const skewTInfo& OData)
{
	glm::vec3 LCL = getLCL(T0, P0, Z0, D0);

	std::vector<float> MLRtemps = {};
	 
	//Cut down our needs of calculations (we begin from the LCL height)
	int count = 0;
	while (OData.data[count].pressure > LCL.y) count++;
	if (count == 0) count = 1;
	const int startCount = count;
	std::vector<float> OPressures(OData.data.size() - startCount);
	std::transform(OData.data.begin() + startCount, OData.data.end(), OPressures.begin(), [](const auto& elem) { return elem.pressure; });


	MLRtemps = getMoistTemp(LCL.x, LCL.y, OPressures);

	//Now check for intersections
	bool outside = false;
	int lastOutsideCount = 0;

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
				lastOutsideCount = count;
			}
		}
		//Note: we don't do anything when the two temps are the same.

		count++;
	}


	return { OData.data[lastOutsideCount].temperature, OData.data[lastOutsideCount].pressure, OData.data[lastOutsideCount].altitude };
}

float meteoformulas::calculateCAPE(const float T0, const float P0, const float Z0, const float D0, const skewTInfo& OData)
{
	//Get LCL, EL and LFC
	const glm::vec3 LCL = getLCL(T0, P0, Z0, D0);
	const float ELz = getEL(T0, P0, Z0, D0, OData).z;
	const float LFCz = getLFC(T0, P0, Z0, D0, OData).z;
	std::vector<float> MLRtemps = {};

	//count starting from LCL
	int count = 0;
	while (OData.data[count].pressure > LCL.y) count++;
	if (count == 0) count = 1;
	const int startCount = count;
	std::vector<float> OPressures(OData.data.size() - startCount);
	std::transform(OData.data.begin() + startCount, OData.data.end(), OPressures.begin(), [](const auto& elem) { return elem.pressure; });
	while (OData.data[count].altitude < LFCz) count++; //Increase count up to LFC

	MLRtemps = getMoistTemp(LCL.x, LCL.y, OPressures);

	float Cape = 0.0f;

	//Loop over all data until reached EL
	while (OData.data[count].altitude <= ELz)
	{	
		//Specific humidity
		const float QVe = qv(OData.data[count].temperature, OData.data[count].pressure); //TODO: observed or standard pressure?
		const float QVp = qv(MLRtemps[count - startCount], OData.data[count].pressure); //TODO: observed or standard pressure?

		const float Tve = (OData.data[count].temperature + 273.15f) * (1 + E * QVe);
		const float Tvp = (MLRtemps[count - startCount] + 273.15f) * (1 + E * QVp);

		const float Dz = OData.data[count].altitude - OData.data[count - 1].altitude;

		if (Tve < Tvp)
		{
			Cape += ((Tvp - Tve) / Tve) * Dz; //Accumulate Cape
		}
		count++;
	}
	Cape *= g;

	return Cape;
}

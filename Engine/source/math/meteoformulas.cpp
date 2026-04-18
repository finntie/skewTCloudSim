#include "math/meteoformulas.h"

#include <boost/math/special_functions/lambert_w.hpp>
#include <boost/math/special_functions/gamma.hpp>
#include "math/constants.hpp"


float meteoformulas::es(const float T)
{
	//return (0.61094f) * expf((17.67f * T) / (T + 243.5f)); //Worse accuraccy of around 0.2%
	return 0.61078f * expf((17.27f * T) / (T + 237.3f));
}

float meteoformulas::ei(const float T) 
{ 
    return 0.61078f * expf((21.875f * T) / (T + 265.5f)); 
}

float meteoformulas::ws(const float T, const float P)
{
	float Es = es(T) * 10.0f; // kPa to hPa
	return  (Constants::E * Es) / (P - Es);
}

float meteoformulas::wi(const float T, const float P)
{
    float Es = ei(T) * 10.0f; // kPa to hPa
    return (Constants::E * Es) / (P - Es);
}

float meteoformulas::qs(const float T, const float P) 
{
    float Es = ei(T) * 10.0f; // kPa to hPa
    return (Constants::E * Es) / (P - (1 - Constants::E) * Es);
}

float meteoformulas::qv(const float T, const float P)
{
	float Rs = ws(T, P);
	return Rs / (1 + Rs);
}

float meteoformulas::Ni(const float T) 
{ 
    const float EI = ei(T) * 1000; //from kPa to Pa
    const float ES = es(T) * 1000; //from kPa to Pa
    return 10000 * exp((12.96f * (ES - EI)) / (EI - 0.639f)); 
}

float meteoformulas::pwater(const float T) 
{ 
    const float p0 = 999.83311f;
    const float a1 = 0.0752f;
    const float a2 = 0.0089f;
    const float a3 = 7.36413e-5f;
    const float a4 = 4.74639e-7f;
    const float a5 = 1.34888e-9f;

    return p0 + (a1 * T) - (a2 * T * T) + (a3 * T * T * T) - (a4 * T * T * T * T) + (a5 * T * T * T * T * T);
}

float meteoformulas::Lwater(const float T) 
{ 
    return (2500.8f - 2.36f * T + 0.0016f * T * T - 0.00006f * T * T * T) * 1e3f; 
}

float meteoformulas::Lice(const float T) 
{ 
    return (2834.1f - 0.29f * T - 0.004f * T * T) * 1e3f; 
}

float meteoformulas::Rm(const float T, const float P)
{
	const float Qv = qv(T, P);
    return (1 - Qv) * Constants::Rsd + Qv * Constants::Rsw;
}

float meteoformulas::Cpm(const float T, const float P)
{
	const float Qv = qv(T, P);
    return (1 - Qv) * Constants::Cpa + Qv * Constants::Cpvw;
}

float meteoformulas::DQVair(const float T, const float P)
{
    return 2.26e-5f * pow((T + 273.15f) / 273.15f, 1.94f) * (1013.25f / P);
}

float meteoformulas::ViscAir(const float T) 
{ 
    const float V0 = 1.716e-5f;
    const float Sv = 111.0f;
    return pow((T + 273.15f) / 273.15f, 3.0f / 2.0f) * ((273.15f + Sv) / (T + Sv)) * V0;
}

float meteoformulas::slopePrecip(const float D, const float Qj, const int precipType) 
{
    // constants
    const float _e = 0.25f;  // E

    switch (precipType)
    {
        case 0:  // Rain
        {
            // How many of this particle are in this region in cm-4
            const float N0R = 8e-2f;
            // Densities in g/cm3
            const float densW = 0.99f;
            // Check for division by 0
            const float numerator = Constants::PI * densW * N0R;
            const float denominator = std::max(D * Qj * 0.001f, 1e-14f);  // Convert kg/kg to g/cm3
            return (numerator == 0 || denominator == 0) ? 0.0f : pow((numerator / denominator), _e);
            break;
        }
        case 1:  // Snow
        {
            const float N0S = 3e-2f;
            const float densS = 0.11f;

            const float numerator = Constants::PI * densS * N0S;
            const float denominator = std::max(D * Qj * 0.001f, 1e-14f);  // Convert kg/kg to g/cm3
            return (numerator == 0 || denominator == 0) ? 0.0f : pow((numerator / denominator), _e);
            break;
        }
        case 2:  // Ice
        {
            const float N0I = 4e-4f;
            const float densI = 0.91f;

            const float numerator = Constants::PI * densI * N0I;
            const float denominator = std::max(D * Qj * 0.001f, 1e-14f);  // Convert kg/kg to g/cm3
            return (numerator == 0 || denominator == 0) ? 0.0f : pow((numerator / denominator), _e);
            break;
        }
        default:
            break;
    }
    return 0.0f;
}

float meteoformulas::gamma(const float x, const float y) 
{ 
    if (y != 0.0f)
    {
        return boost::math::tgamma(x, y);
    }
    return std::tgammaf(x);
}

float meteoformulas::MLR(const float T, const float P)
{
	float TKelvin = T + 273.15f;
	float Ws = ws(T, P);
    float Tw = ((Constants::Rsd * TKelvin + Constants::Hv * Ws) /
             (Constants::Cpd + (Constants::Hv * Constants::Hv * Ws * Constants::E / (Constants::Rsd * TKelvin * TKelvin))));
	return Tw / P;
}

float meteoformulas::cvd(const float T, const float P, const float dens) 
{ 
    //Vapor pressures are in Pa
    const float EI = ei(T) * 1000;
    const float ES = es(T) * 1000;
    //TODO: is T below really in kelvin?
    const float A = (Constants::Ls / (Constants::Ka * (T + 273.15f))) * (Constants::Ls / (Constants::Rsw * (T + 273.15f)) - 1); 
    const float B = (Constants::Rsw * (T + 273.15f) * P) / (2.21f * EI);
    const float Ni = 10000 * exp((12.96f * (ES - EI)) / (EI - 0.639f)); //Could use function, but than we have to again calculate EI and ES

    return 65.2f * ((pow(Ni, 0.5f) * (ES - EI)) / (pow(dens, 0.5f) * (A + B) * EI));
}

void meteoformulas::getTempAtWs(const float Ws, const float* pressures, float* output, const size_t size)
{
	for (int i = 0; i < size; i++)
	{
        float P = pressures[i];
            float EsT = (Ws * P) / (Constants::E + Ws);

		//TODO wrong formula check es()
		output[i] = (-(log(EsT) - log(6.1078f)) * 243.5f) / (log(EsT) - log(6.1078f) - 17.67f);
	}
}

float meteoformulas::potentialTemp(const float T, const float Pk, const float Pt) 
{ 
    return (T + 273.15f) * glm::pow((Pt / Pk), Constants::Rsd / Constants::Cpd) - 273.15f;
}

float meteoformulas::dryLapseTemp(const float T, const float Pk, const float Pt)
{
    return (T + 273.15f) * glm::pow((Pt / Pk), Constants::Rsd / Constants::Cpd) - 273.15f;
}

void meteoformulas::getPotentialTempArray(const float* temps, const float Pt, const float* pressures, float* output, const size_t size)
{
    for (int i = 0; i < size; i++)
    {
        float P = pressures[i];
        float T = temps[i];
        output[i] = potentialTemp(T, P, Pt);
    }
}

void meteoformulas::getDryAdiabatic(const float T0, const float P0, const float* pressures, float* output, const size_t size)
{
    const int difference = 1;
    int offset = 0;

    while (pressures[offset] - difference > P0 || pressures[offset] == 0)
    {
        output[offset] = 0;
        offset++;
        if (offset >= size)
        {
            offset = -1;
            return;
        }
    }

    for (int i = offset; i < size; i++)
    {
        float P = pressures[i];
        output[i] = dryLapseTemp(T0, P0, P);
    }
}


float meteoformulas::getStandardHeightAtPressure(const float T0, const float P, const float P0)
{
	const float TKelvin = T0 + 273.15f;
	//float Tv = TKelvin * (1.0f + 0.611f * qv(TKelvin, P * 100));
	//return Rsd * Tv / g * logf(P0 / P);

	return TKelvin / Constants::Lb *
               (glm::pow(P / P0, -Constants::R * Constants::Lb / (Constants::g * (Constants::Mda * 0.001f))) - 1);

}

float meteoformulas::getStandardPressureAtHeight(const float T0, const float h, const float h0, const float P0)
{
	const float TKelvin = T0 + 273.15f;

	return P0 * glm::pow(1 + (Constants::Lb / TKelvin) * (h - h0),
                             (-Constants::g * (Constants::Mda * 0.001f)) / (Constants::R * Constants::Lb));
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

void meteoformulas::getMoistTemp(const float T0, const float Pref, const float* pressures, float* output, const size_t size, int& offset)
{
    float T = T0;
    float P = Pref;
    const int difference = 1;

    while (pressures[offset] - difference > Pref || pressures[offset] == 0)
    {
        output[offset] = 0;
        offset++;
        if (offset >= size)
        {
            offset = -1;
            return;
        }
    }

    for (int i = offset; i < size; i++)
    {
        const float Pnext = pressures[i];
        float dP = Pnext - P;

        if (dP < -1.0f)
        {
            float _P = P + 1.0f;
            while (--_P > Pnext + 1.0f)
            {
                T = MLR(T, _P) * -1.0f + T;
            }
            P = _P;
            dP = Pnext - P;
        }
        T = MLR(T, P) * dP + T;

        output[i] = T;
        P = Pnext;
    }
}

glm::vec3 meteoformulas::getCCL(const float P0, const float D0, const float* pressures, const float* temperatures, const size_t size)
{
	int count = 0;
    while (pressures[count] > 100.0f) count++;
	if (count >= size) return { D0, P0, D0 };

	float* temps = new float[size];
	const float WS = ws(D0, P0);
    getTempAtWs(WS, pressures, temps, size);

	//Loop until the mixing ratio temp >= observed temp
	count = 0;
    while (count < size && temperatures[count] > temps[count])
	{
		count++;
	}
	delete[] temps;

	const float PotTemp =
            (temperatures[count] + 273.15f) / glm::pow((pressures[count] / P0), Constants::Rsd / Constants::Cpd) - 273.15f;
	return glm::vec3(temperatures[count], pressures[count], PotTemp);
}


glm::vec3 meteoformulas::getLCL(const float T0, const float P0, const float Z0, const float D0)
{
	//Formula from https://en.wikipedia.org/wiki/Lifting_condensation_level
	const float TKelvin = T0 + 273.15f;
	//const float Pa0 = P0 * 100.0f;

	const float CPM = Cpm(T0, P0);
	const float RM = Rm(T0, P0);
	const float RHl = es(D0) / es(T0);

	const float a = (CPM / RM) + ((Constants::Cvl - Constants::Cpvw) / Constants::Rsw);
        const float b = -(Constants::E0v - (Constants::Cvv - Constants::Cvl) * Constants::Ttrip) / (Constants::Rsw * TKelvin);
	const float c = b / a;
	
	const float Wmin1 = boost::math::lambert_wm1(glm::pow(RHl, 1/a) * c * glm::exp(c));
	const float TLCL = (c / Wmin1) * TKelvin;
	const float pLCL = P0 * glm::pow(TLCL / TKelvin, CPM / RM);
        const float zLCL = Z0 + CPM / Constants::g * (TKelvin - TLCL);

	return { TLCL - 273.15f, pLCL, zLCL };

}

glm::vec3 meteoformulas::getLFC(const float T0,
                                const float P0,
                                const float Z0,
                                const float D0,
                                const float* pressures,
                                const float* temperatures,
                                const float* altitudes,
                                const size_t size)
{


    glm::vec3 LCL = getLCL(T0, P0, Z0, D0);
    if (size <= 1) return LCL;
    float* MLRtemps = new float[size];

    // Cut down our needs of calculations (we begin from the LCL height)
    int count = 0;
    while (pressures[count] > LCL.y) count++;

    int MLRtempOffset = 0;
    getMoistTemp(LCL.x, LCL.y, pressures, MLRtemps, size, MLRtempOffset);
    if (MLRtempOffset == -1)
    {
        delete[] MLRtemps;
        return LCL;
    }



    // Now check for intersections
    bool outside = (MLRtemps[count] > temperatures[count]);

    if (outside)  // LFC is already at LCL
    {
        delete[] MLRtemps;
        return LCL;
    }

    while (count < size - 1)
    {
        if (MLRtemps[count] > temperatures[count])
        {
            outside = true;
            delete[] MLRtemps;
            return {temperatures[count], pressures[count], altitudes[count]};
        }
        count++;
    }
    // Nothing found :(
    delete[] MLRtemps;
    return {-1.0f, -1.0f, -1.0f};
}

glm::vec3 meteoformulas::getEL(const float T0,
                               const float P0,
                               const float Z0,
                               const float D0,
                               const float* pressures,
                               const float* temperatures,
                               const float* altitudes,
                               const size_t size)
{
	glm::vec3 LCL = getLCL(T0, P0, Z0, D0);
    float* MLRtemps = new float[size];
	 
	//Cut down our needs of calculations (we begin from the LCL height)
	int count = 0;
    while (pressures[count] > LCL.y) count++;
	if (count == 0) count = 1;

    int MLRtempOffset = 0;
	getMoistTemp(LCL.x, LCL.y, pressures, MLRtemps, size, MLRtempOffset);
    if (MLRtempOffset == -1)
    {
        delete[] MLRtemps;
        return LCL;
    }


	//Now check for intersections
	bool outside = false;
	int lastOutsideCount = 0;

	while (count < size - 1)
	{
		if (!outside)
		{
			if (MLRtemps[count] > temperatures[count])
			{
				outside = true;
			}
		}
		else
		{
			if (MLRtemps[count] < temperatures[count])
			{
				outside = false;
				lastOutsideCount = count;
			}
		}
		//Note: we don't do anything when the two temps are the same.

		count++;
	}
    delete[] MLRtemps;

	return {temperatures[lastOutsideCount], pressures[lastOutsideCount], altitudes[lastOutsideCount]};
}

float meteoformulas::calculateCAPE(const float T0,
                                   const float P0,
                                   const float Z0,
                                   const float D0,
                                   const float* pressures,
                                   const float* temperatures,
                                   const float* altitudes,
                                   const size_t size)
{
    if (size <= 1) return 0;

    // Get LCL, EL and LFC
    const glm::vec3 LCL = getLCL(T0, P0, Z0, D0);
    const float ELz = getEL(T0, P0, Z0, D0, pressures, temperatures, altitudes, size).z;
    const float LFCz = getLFC(T0, P0, Z0, D0, pressures, temperatures, altitudes, size).z;
    float* MLRtemps = new float[size];
    int MLRtempOffset = 0;

    // count starting from LCL
    int count = 0;
    while (pressures[count] > LCL.y) count++;
    while (altitudes[count] < LFCz) count++;  // Increase count up to LFC
    if (count == 0) count = 1;

    getMoistTemp(LCL.x, LCL.y, pressures, MLRtemps, size, MLRtempOffset);
    if (MLRtempOffset == -1)
    {
        delete[] MLRtemps;
        return -1;
    }
    count = MLRtempOffset > count ? MLRtempOffset : count;

    float Cape = 0.0f;

    // Loop over all data until reached EL
    while (altitudes[count] <= ELz)
    {
        // Specific humidity
        const float QVe = qv(temperatures[count], pressures[count]);           // TODO: observed or standard pressure?
        const float QVp = qv(MLRtemps[count], pressures[count]);  // TODO: observed or standard pressure?

        const float Tve = (temperatures[count] + 273.15f) * (1 + Constants::E * QVe);
        const float Tvp = (MLRtemps[count] + 273.15f) * (1 + Constants::E * QVp);

        const float Dz = altitudes[count] - altitudes[count - 1];

        if (Tve < Tvp)
        {
            Cape += ((Tvp - Tve) / Tve) * Dz;  // Accumulate Cape
        }
        count++;
    }
    Cape *= Constants::g;

    delete[] MLRtemps;

    return Cape;
}

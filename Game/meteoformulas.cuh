#pragma once

#include "meteoconstants.cuh"
#include <CUDA/cmath>
#include <cuda_runtime.h>
#include <cuda.h>

__constant__ float* gammaR{nullptr};
__constant__ float* gammaS{nullptr};
__constant__ float* gammaI{nullptr};

// <summary>Get (Saturated) Water Vapor </summary>
/// https://www.weather.gov/media/epz/wxcalc/vaporPressure.pdf
/// https://en.wikipedia.org/wiki/Vapour_pressure_of_water (Better)
/// <param name="T">(Dewpoint) Temperature in C</param>
/// <returns>(saturation) vapor pressure in kPa (= 10 hPa)</returns>
__device__ inline float esGPU(const float T)
{
    //return (0.61094f) * expf((17.67f * T) / (T + 243.5f)); //Worse accuraccy of around 0.2%
    return 0.61078f * expf((17.27f * T) / (T + 237.3f));
}


/// <summary>Get ice Vapor </summary>
/// https://en.wikipedia.org/wiki/Tetens_equation
/// <param name="T">(Dewpoint) Temperature in C</param>
/// <returns> ice vapor pressure in kPa (= 10 hPa)</returns>
__device__ inline float eiGPU(const float T)
{
    return 0.61078f * expf((21.875f * T) / (T + 265.5f));
}


/// <summary>Calculates the (saturated) mixing ratio (also called r(s)).
/// Filling in dewpoint for T will result in the mixing ratio,
/// Filling in temperature for T will result in the saturated mixing ratio.</summary>
/// https://vortex.plymouth.edu/~stmiller/stmiller_content/Publications/AtmosRH_Equations_Rev.pdf
/// https://journals.ametsoc.org/view/journals/mwre/108/7/1520-0493_1980_108_1046_tcoept_2_0_co_2.xml
/// <param name="T">(Dewpoint) Temperature in C</param>
/// <param name="P">Pressure in hPa</param>
/// <returns>(saturated) mixing ratio in (kg(vapor)/kg(air))</returns>
__device__ inline float wsGPU(const float T, const float P)
{
    float Es = esGPU(T) * 10.0f; // kPa to hPa
    return  (ConstantsGPU::E * Es) / (P - Es);
}


/// <summary>Mostly the same as the saturated mixing ratio, but now using ice</summary>
/// <param name="T">Temperature in C</param>
/// <param name="P">Pressure in hPa</param>
/// <returns>mixing ratio in (kg(ice)/kg(air))</returns>
__device__ inline float wiGPU(const float T, const float P)
{
    float Es = eiGPU(T) * 10.0f; // kPa to hPa
    return (ConstantsGPU::E * Es) / (P - Es);
}

/// <summary>Calculate the specific humidity of air (same as mixing ratio but using moist air instead of dry)</summary>
	/// <param name="T">Temperature in C</param>
	/// <param name="P">Pressure in hPa</param>
/// <returns>Specific humidity in kg/kg</returns>
__device__ inline float qsGPU(const float T, const float P)
{
    float Es = eiGPU(T) * 10.0f; // kPa to hPa
    return (ConstantsGPU::E * Es) / (P - (1 - ConstantsGPU::E) * Es);
}


/// <summary>Calculates the mass fraction of water vapor </summary>
/// Page 80 chapter 3.5.1 https://www.gnss-x.ac.cn/docs/Atmospheric%20Science%20An%20Introductory%20Survey%20(John%20M.%20Wallace,%20Peter%20V.%20Hobbs)%20(z-lib.org).pdf 
/// <param name="T">Temperature in °C</param>
/// <param name="P">Pressure in hPa</param>
/// <returns>% / 100 of water vapor in air</returns>
__device__ inline float qvGPU(const float T, const float P)
{
    float Rs = wsGPU(T, P);
    return Rs / (1 + Rs);
}

/// <summary>The concentration of available ice crystals for the nucleation process</summary>
/// <param name="T">Temperature in °C</param>
/// <returns>Value in m-3</returns>
__device__ inline float NiGPU(const float T)
{
    const float EI = eiGPU(T) * 1000; //from kPa to Pa
    const float ES = esGPU(T) * 1000; //from kPa to Pa
    return 10000 * exp((12.96f * (ES - EI)) / (EI - 0.639f));
}

/// <summary>Estimates the density of water in Kg/m3 </summary> https://www.omnicalculator.com/physics/water-density
/// <param name="T">Temperatuer in celcius</param>
__device__ inline float pwaterGPU(const float T)
{
    const float p0 = 999.83311f;
    const float a1 = 0.0752f;
    const float a2 = 0.0089f;
    const float a3 = 7.36413e-5f;
    const float a4 = 4.74639e-7f;
    const float a5 = 1.34888e-9f;

    return p0 + (a1 * T) - (a2 * T * T) + (a3 * T * T * T) - (a4 * T * T * T * T) + (a5 * T * T * T * T * T);
}

/// <summary>Specific latent heat for condensation at different temperatures</summary>
/// https://en.wikipedia.org/wiki/Latent_heat#cite_note-RYfit-26
/// <param name="T">Temperature in celcius</param>
/// <returns>Latent Heat in J/kg</returns>
__device__ inline float LwaterGPU(const float T)
{
    return (2500.8f - 2.36f * T + 0.0016f * T * T - 0.00006f * T * T * T) * 1e3f;
}

/// <summary>Specific latent heat for Deposition at different temperatures</summary>
/// https://en.wikipedia.org/wiki/Latent_heat#cite_note-RYfit-26
/// <param name="T">Temperature in celcius</param>
/// <returns>Latent Heat in J/kg</returns>
__device__ inline float LiceGPU(const float T)
{
    return (2834.1f - 0.29f * T - 0.004f * T * T) * 1e3f;
}

/// <summary>Calculates the specific gas constant for moist air</summary>
/// <param name="T">Temperature in Kelvin</param>
/// <param name="P">Pressure in Pa</param>
/// <returns>gas constant in J/kg</returns>
__device__ inline float RmGPU(const float T, const float P)
{
    const float Qv = qvGPU(T, P);
    return (1 - Qv) * ConstantsGPU::Rsd + Qv * ConstantsGPU::Rsw;
}

/// <summary>Calculates the specific heat capacity at constant pressure for moist air</summary>
/// <param name="T">Temperature in Kelvin</param>
/// <param name="P">Pressure in Pa</param>
/// <returns>heat capacity in J/kg</returns>
__device__ inline float CpmGPU(const float T, const float P)
{
    const float Qv = qvGPU(T, P);
    return (1 - Qv) * ConstantsGPU::Cpa + Qv * ConstantsGPU::Cpvw;
}

/// <summary>Calculates the diffusity of water vapor in air </summary>
/// <param name="T">Temp in celcius</param>
/// <param name="P">Pressure in hPa</param>
/// <returns>Diffusity in m2/s-1</returns>
__device__ inline float DQVairGPU(const float T, const float P)
{
    return 2.26e-5f * powf((T + 273.15f) / 273.15f, 1.94f) * (1013.25f / P);
}

/// <summary>Calculates the viscosity of air using Sutherland's law</summary>
/// https://doc.comsol.com/5.5/doc/com.comsol.help.cfd/cfd_ug_fluidflow_high_mach.08.27.html
/// <param name="T">Temp in celcius</param>
/// <returns>Viscosity in m2/s-1</returns>
__device__ inline float ViscAirGPU(const float T)
{
    const float V0 = 1.716e-5f;
    const float Sv = 111.0f;
    return powf((T + 273.15f) / 273.15f, 3.0f / 2.0f) * ((273.15f + Sv) / (T + Sv)) * V0;
}

/// <summary>Calculates the slope parameter of chosen precip</summary>
/// <param name="pAir">Density of air</param>
/// <param name="Qj">Mixing ratio at specific point</param>
/// <param name="precipType">Type of precip: 0 = Rain, 1 = Snow, 2 = ice</param>
/// <returns>Slope parameter in g/cm3</returns>
__device__ inline float slopePrecipGPU(const float D, const float Qj, const int precipType)
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
        const float numerator = ConstantsGPU::PI * densW * N0R;
        const float denominator = fmaxf(D * Qj * 0.001f, 1e-14f);  // Convert kg/kg to g/cm3
        return (numerator == 0 || denominator == 0) ? 0.0f : powf((numerator / denominator), _e);
        break;
    }
    case 1:  // Snow
    {
        const float N0S = 3e-2f;
        const float densS = 0.11f;

        const float numerator = ConstantsGPU::PI * densS * N0S;
        const float denominator = fmaxf(D * Qj * 0.001f, 1e-14f);  // Convert kg/kg to g/cm3
        return (numerator == 0 || denominator == 0) ? 0.0f : powf((numerator / denominator), _e);
        break;
    }
    case 2:  // Ice
    {
        const float N0I = 4e-4f;
        const float densI = 0.91f;

        const float numerator = ConstantsGPU::PI * densI * N0I;
        const float denominator = fmaxf(D * Qj * 0.001f, 1e-14f);  // Convert kg/kg to g/cm3
        return (numerator == 0 || denominator == 0) ? 0.0f : powf((numerator / denominator), _e);
        break;
    }
    default:
        break;
    }
    return 0.0f;
}


/// <summary>Gammas should be initialized, since calculating them could be expensive</summary>
inline void initGammas()
{
    const float b = 0.8f;
    const float d = 0.25f;
    static float GammaR = tgammaf(4.0f + b);
    static float GammaS = tgammaf(4.0f + d);
    static float GammaI = GammaS;
    cudaMemcpyToSymbol(gammaR, &GammaR, sizeof(float));
    cudaMemcpyToSymbol(gammaS, &GammaS, sizeof(float));
    cudaMemcpyToSymbol(gammaI, &GammaI, sizeof(float));
}

/// <summary>Calculates the falling velocity of each different precip type</summary>
/// <param name = "Qr"> Mixing ratio of rain in kg/kg</param>
/// <param name = "Qs"> Mixing ratio of snow in kg/kg</param>
/// <param name = "Qi"> Mixing ratio of hail in kg/kg</param>
/// <param name = "densAir"> Density of air in Pa</param>
/// <param name = "type"> 1 = rain, 2 = snow, 3 = hail, 4 = all</param>

/// <returns>x: rain, y: snow, z: ice</returns>
__device__ inline float3 calculateFallingVelocityGPU(const float Qr, const float Qs, const float Qi, const float densAir, const int type)
{
    bool all{ (type == 4) };
    float UR = 0.0f;
    float US = 0.0f;
    float UI = 0.0f;

    //Check if gammas are valid
    if (gammaR == nullptr)
    {
        return { -1,-1,-1 };
    }

    if (type == 1 || all)
    {
        const float a = 2115.0f;
        const float b = 0.8f;
        float slopeR = slopePrecipGPU(densAir, Qr, 0);
        UR = a * (*gammaR / (6 * std::powf(slopeR, b))) * sqrt(1.225f / densAir);
        UR *= 0.01f; //Convert cm to m
    }
    else if (type == 2 || all)
    {
        const float c = 152.93f;
        const float d = 0.25f;
        float slopeS = slopePrecipGPU(densAir, Qs, 1);
        US = c * (*gammaS / (6 * std::powf(slopeS, d))) * sqrt(1.225f / densAir);
        US *= 0.01f; //Convert cm to m
    }
    else if (type == 3 || all)
    {
        //Densities in g/cm3
        const float densI = 0.91f;
        //constants
        const float CD = 0.6f; //Drag coefficient
        float slopeI = slopePrecipGPU(densAir, Qi, 2);
        UI = (*gammaI / (6 * std::pow(slopeI, 0.5f))) * powf(4 * ConstantsGPU::g * 100 * densI / (3 * CD * densAir * 0.001f), 0.5f); //Converting g to cm/s2 and densAir to g/cm3
        UI *= 0.01f; //Convert cm to m
    }
    return float3{ UR, US, UI };
}


/// <summary>Calculates the gamma function of x</summary>
/// <param name="x">Value for gamma</param>
/// <returns>Result</returns>
__device__ inline float gammaGPU(const float x)
{
    return tgammaf(x);
}


/// <summary>The rate constant for vapor deposition on hexagonal crystals (Rate of evaporation causing ice growth?)</summary>
/// Formula from https://research.csiro.au/ccam/wp-content/uploads/sites/520/2024/01/1377337418.pdf (A.2)
/// <param name="T">Temperature in C</param>
/// <param name="P">Pressure in hPa </param>
__device__ inline float cvdGPU(const float T, const float P, const float densityAir)
{
    //Vapor pressures are in Pa
    const float EI = eiGPU(T) * 1000;
    const float ES = esGPU(T) * 1000;
    //TODO: is T below really in kelvin?
    const float A = (ConstantsGPU::Ls / (ConstantsGPU::Ka * (T + 273.15f))) * (ConstantsGPU::Ls / (ConstantsGPU::Rsw * (T + 273.15f)) - 1);
    const float B = (ConstantsGPU::Rsw * (T + 273.15f) * P) / (2.21f * EI);
    const float Ni = 10000 * exp((12.96f * (ES - EI)) / (EI - 0.639f)); //Could use function, but than we have to again calculate EI and ES

    return 65.2f * ((pow(Ni, 0.5f) * (ES - EI)) / (pow(densityAir, 0.5f) * (A + B) * EI));
}


/// <summary>The potential temperature of a parcel of fluid at pressure P 
/// is the temperature that the parcel would attain if adiabatically brought to a standard reference pressure P0,
/// usually 1,000 hPa(1,000 mb). 
/// The difference between this and the dry lapse rate is that for the dry lapse rate we cool the temperature down by moving it upwards,
/// while for this we move the temperature back down to the reference temperature.(Thus in essential the same formula, but we use it differently)</summary>
/// <param name="T">Temperature in °C</param>
/// <param name="PKnown">Known Pressure at Temp in hPa</param>
/// <param name="PTarget">Target Pressure at height in hPa</param>
/// <returns>Potential temp in °C</returns>
__device__ inline float potentialTempGPU(const float T, const float Pk, const float Pt)
{
	return (T + 273.15f) * powf((Pt / Pk), ConstantsGPU::Rsd / ConstantsGPU::Cpd) - 273.15f;
}


/// <summary>Calculates the moist lapse rate</summary>
/// <param name="T">Temperature in °C</param>
/// <param name="P">Pressure in hPa</param>
/// <returns>Lapse rate in dK/dP (delta Kelving per delta hectoPascal)</returns>
__device__ inline float MLRGPU(const float T, const float P)
{
    float TKelvin = T + 273.15f;
    float Ws = wsGPU(T, P);
    float Tw = ((ConstantsGPU::Rsd * TKelvin + ConstantsGPU::Hv * Ws) /
        (ConstantsGPU::Cpd + (ConstantsGPU::Hv * ConstantsGPU::Hv * Ws * ConstantsGPU::E / (ConstantsGPU::Rsd * TKelvin * TKelvin))));
    return Tw / P;
}



//------------------------------------------------------------------------------------------

//                                  Other Functions

//------------------------------------------------------------------------------------------

__global__ inline void setToValue(float* array, const float value, const int width)
{
    const int tX = threadIdx.x;
    const int tY = blockIdx.x;
    const int idx = tX + tY * width;

    array[idx] = value;
}

__global__ inline void multiplyValues(float* array1, const float* array2, const int width)
{
    const int tX = threadIdx.x;
    const int tY = blockIdx.x;
    const int idx = tX + tY * width;

    array1[idx] *= array2[idx];
}
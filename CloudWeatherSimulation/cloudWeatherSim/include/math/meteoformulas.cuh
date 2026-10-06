#pragma once

#include "meteoconstants.cuh"
#include <cuda_runtime.h>
#include <stdio.h>

#include <glm/glm.hpp>

namespace MForms
{

// <summary>Get (Saturated) Water Vapor </summary>
/// https://www.weather.gov/media/epz/wxcalc/vaporPressure.pdf
/// https://en.wikipedia.org/wiki/Vapour_pressure_of_water (Better)
/// <param name="T">(Dewpoint) Temperature in C</param>
/// <returns>(saturation) vapor pressure in kPa (= 10 hPa)</returns>
__host__ __device__ inline float es(const float T)
{
    // return (0.61094f) * expf((17.67f * T) / (T + 243.5f)); //Worse accuraccy of around 0.2%
    return 0.61078f * expf((17.27f * T) / (T + 237.3f));
}

/// <summary>Get ice Vapor </summary>
/// https://en.wikipedia.org/wiki/Tetens_equation
/// <param name="T">(Dewpoint) Temperature in C</param>
/// <returns> ice vapor pressure in kPa (= 10 hPa)</returns>
__host__ __device__ inline float ei(const float T) { return 0.61078f * expf((21.875f * T) / (T + 265.5f)); }

/// <summary>Calculates the (saturated) mixing ratio (also called r(s)).
/// Filling in dewpoint for T will result in the mixing ratio,
/// Filling in temperature for T will result in the saturated mixing ratio.</summary>
/// https://vortex.plymouth.edu/~stmiller/stmiller_content/Publications/AtmosRH_Equations_Rev.pdf
/// https://journals.ametsoc.org/view/journals/mwre/108/7/1520-0493_1980_108_1046_tcoept_2_0_co_2.xml
/// <param name="T">(Dewpoint) Temperature in C</param>
/// <param name="P">Pressure in hPa</param>
/// <returns>(saturated) mixing ratio in (kg(vapor)/kg(air))</returns>
__host__ __device__ inline float ws(const float T, const float P)
{
    float Es = es(T) * 10.0f;  // kPa to hPa
#ifdef __CUDA_ARCH__
    return (ConstantsGPU::E * Es) / (P - Es);
#else
    return (ConstantsCPU::E * Es) / (P - Es);
#endif
}

/// <summary>Mostly the same as the saturated mixing ratio, but now using ice</summary>
/// <param name="T">Temperature in C</param>
/// <param name="P">Pressure in hPa</param>
/// <returns>mixing ratio in (kg(ice)/kg(air))</returns>
__host__ __device__ inline float wi(const float T, const float P)
{
    float Es = ei(T) * 10.0f;  // kPa to hPa
#ifdef __CUDA_ARCH__
    return (ConstantsGPU::E * Es) / (P - Es);
#else
    return (ConstantsCPU::E * Es) / (P - Es);
#endif
}

/// <summary>Calculate the specific humidity of air (same as mixing ratio but using moist air instead of dry)</summary>
/// <param name="T">Temperature in C</param>
/// <param name="P">Pressure in hPa</param>
/// <returns>Specific humidity in kg/kg</returns>
__host__ __device__ inline float qs(const float T, const float P)
{
    float Es = ei(T) * 10.0f;  // kPa to hPa
#ifdef __CUDA_ARCH__
    return (ConstantsGPU::E * Es) / (P - (1 - ConstantsGPU::E) * Es);
#else
    return (ConstantsCPU::E * Es) / (P - (1 - ConstantsCPU::E) * Es);
#endif
}

/// <summary>Calculates the mass fraction of water vapor </summary>
/// Page 80 chapter 3.5.1
/// https://www.gnss-x.ac.cn/docs/Atmospheric%20Science%20An%20Introductory%20Survey%20(John%20M.%20Wallace,%20Peter%20V.%20Hobbs)%20(z-lib.org).pdf
/// <param name="T">Temperature in °C</param>
/// <param name="P">Pressure in hPa</param>
/// <returns>% / 100 of water vapor in air</returns>
__host__ __device__ inline float qv(const float T, const float P)
{
    float Rs = ws(T, P);
    return Rs / (1 + Rs);
}

/// <summary>The concentration of available ice crystals for the nucleation process</summary>
/// <param name="T">Temperature in °C</param>
/// <returns>Value in m-3</returns>
__host__ __device__ inline float Ni(const float T)
{
    const float EI = ei(T) * 1000;  // from kPa to Pa
    const float ES = es(T) * 1000;  // from kPa to Pa
    return 10000 * exp((12.96f * (ES - EI)) / (EI - 0.639f));
}

/// <summary>Estimates the density of water in Kg/m3 </summary> https://www.omnicalculator.com/physics/water-density
/// <param name="T">Temperatuer in celcius</param>
__host__ __device__ inline float pwater(const float T)
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
__host__ __device__ inline float Lwater(const float T)
{
    return (2500.8f - 2.36f * T + 0.0016f * T * T - 0.00006f * T * T * T) * 1e3f;
}

/// <summary>Specific latent heat for Deposition at different temperatures</summary>
/// https://en.wikipedia.org/wiki/Latent_heat#cite_note-RYfit-26
/// <param name="T">Temperature in celcius</param>
/// <returns>Latent Heat in J/kg</returns>
__host__ __device__ inline float Lice(const float T) { return (2834.1f - 0.29f * T - 0.004f * T * T) * 1e3f; }

/// <summary>Calculates the specific gas constant for moist air</summary>
/// <param name="T">Temperature in Kelvin</param>
/// <param name="P">Pressure in Pa</param>
/// <returns>gas constant in J/kg</returns>
__host__ __device__ inline float Rm(const float T, const float P)
{
    const float Qv = qv(T, P);
#ifdef __CUDA_ARCH__
    return (1 - Qv) * ConstantsGPU::Rsd + Qv * ConstantsGPU::Rsw;
#else
    return (1 - Qv) * ConstantsCPU::Rsd + Qv * ConstantsCPU::Rsw;
#endif
}

/// <summary>Calculates the specific heat capacity at constant pressure for moist air</summary>
/// <param name="T">Temperature in Kelvin</param>
/// <param name="P">Pressure in Pa</param>
/// <returns>heat capacity in J/kg</returns>
__host__ __device__ inline float Cpm(const float T, const float P)
{
    const float Qv = qv(T, P);
#ifdef __CUDA_ARCH__
    return (1 - Qv) * ConstantsGPU::Cpa + Qv * ConstantsGPU::Cpvw;
#else
    return (1 - Qv) * ConstantsCPU::Cpa + Qv * ConstantsCPU::Cpvw;
#endif
}

/// <summary>Calculates the diffusity of water vapor in air </summary>
/// <param name="T">Temp in celcius</param>
/// <param name="P">Pressure in hPa</param>
/// <returns>Diffusity in m2/s-1</returns>
__host__ __device__ inline float DQVair(const float T, const float P)
{
    return 2.26e-5f * powf((T + 273.15f) / 273.15f, 1.94f) * (1013.25f / P);
}

/// <summary>Calculates the viscosity of air using Sutherland's law</summary>
/// https://doc.comsol.com/5.5/doc/com.comsol.help.cfd/cfd_ug_fluidflow_high_mach.08.27.html
/// <param name="T">Temp in celcius</param>
/// <returns>Viscosity in m2/s-1</returns>
__host__ __device__ inline float ViscAir(const float T)
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
__host__ __device__ inline float slopePrecip(const float D, const float Qj, const int precipType)
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
#ifdef __CUDA_ARCH__
            const float numerator = ConstantsGPU::PI * densW * N0R;
#else
            const float numerator = ConstantsCPU::PI * densW * N0R;
#endif
            const float denominator = fmaxf(D * Qj * 0.001f, 1e-14f);  // Convert kg/kg to g/cm3
            return powf((numerator / denominator), _e);
            break;
        }
        case 1:  // Snow
        {
            const float N0S = 3e-2f;
            const float densS = 0.11f;

#ifdef __CUDA_ARCH__
            const float numerator = ConstantsGPU::PI * densS * N0S;
#else
            const float numerator = ConstantsCPU::PI * densS * N0S;
#endif
            const float denominator = fmaxf(D * Qj * 0.001f, 1e-14f);  // Convert kg/kg to g/cm3
            return powf((numerator / denominator), _e);
            break;
        }
        case 2:  // Ice
        {
            const float N0I = 4e-4f;
            const float densI = 0.91f;

#ifdef __CUDA_ARCH__
            const float numerator = ConstantsGPU::PI * densI * N0I;
#else
            const float numerator = ConstantsCPU::PI * densI * N0I;
#endif
            const float denominator = fmaxf(D * Qj * 0.001f, 1e-14f);  // Convert kg/kg to g/cm3
            return powf((numerator / denominator), _e);
            break;
        }
        default:
            break;
    }
    return 0.0f;
}

/// <summary>Calculates the falling velocity of each different precip type</summary>
/// <param name = "Qr"> Mixing ratio of rain in kg/kg</param>
/// <param name = "Qs"> Mixing ratio of snow in kg/kg</param>
/// <param name = "Qi"> Mixing ratio of hail in kg/kg</param>
/// <param name = "densAir"> Density of air in Pa</param>
/// <param name = "type"> 0 = rain, 1 = snow, 2 = hail, 3 = all</param>
/// <returns>x: rain, y: snow, z: ice</returns>
__host__ __device__ inline float3 calculateFallingVelocity(const float Qr,
                                                           const float Qs,
                                                           const float Qi,
                                                           const float densAir,
                                                           const int type,
                                                           const float GammaR,
                                                           const float GammaS,
                                                           const float GammaI)
{
    bool all{(type == 3)};
    float UR = 0.0f;
    float US = 0.0f;
    float UI = 0.0f;

    // Check if gammas are valid
    if (GammaR == 0.0f)
    {
        printf("Gamma is invalid %f %f %f!\n", GammaR, GammaS, GammaI);
        return {-1, -1, -1};
    }

    if (type == 0 || all)
    {
        const float a = 2115.0f;
        const float b = 0.8f;
        float slopeR = slopePrecip(densAir, Qr, 0);
        UR = a * (GammaR / (6 * powf(slopeR, b))) * sqrtf(1.225f / densAir);
        UR *= 0.01f;  // Convert cm to m
    }
    else if (type == 1 || all)
    {
        const float c = 152.93f;
        const float d = 0.25f;
        float slopeS = slopePrecip(densAir, Qs, 1);
        US = c * (GammaS / (6 * powf(slopeS, d))) * sqrtf(1.225f / densAir);
        US *= 0.01f;  // Convert cm to m
    }
    else if (type == 2 || all)
    {
        // Densities in g/cm3
        const float densI = 0.91f;
        // constants
        const float CD = 0.6f;  // Drag coefficient
        float slopeI = slopePrecip(densAir, Qi, 2);
#ifdef __CUDA_ARCH__
        UI = (GammaI / (6 * powf(slopeI, 0.5f))) * powf(4 * ConstantsGPU::g * 100 * densI / (3 * CD * densAir * 0.001f),
                                                        0.5f);  // Converting g to cm/s2 and densAir to g/cm3
#else
        UI = (GammaI / (6 * powf(slopeI, 0.5f))) * powf(4 * ConstantsCPU::g * 100 * densI / (3 * CD * densAir * 0.001f),
                                                        0.5f);  // Converting g to cm/s2 and densAir to g/cm3
#endif
        UI *= 0.01f;  // Convert cm to m
    }
    return float3{UR, US, UI};
}

/// <summary>Calculates the gamma function of x</summary>
/// <param name="x">Value for gamma</param>
/// <returns>Result</returns>
__host__ __device__ inline float gamma(const float x) { return tgammaf(x); }

/// <summary>The rate constant for vapor deposition on hexagonal crystals (Rate of evaporation causing ice growth?)</summary>
/// Formula from https://research.csiro.au/ccam/wp-content/uploads/sites/520/2024/01/1377337418.pdf (A.2)
/// <param name="T">Temperature in C</param>
/// <param name="P">Pressure in hPa </param>
__host__ __device__ inline float cvd(const float T, const float P, const float densityAir)
{
    // Vapor pressures are in Pa
    const float EI = ei(T) * 1000;
    const float ES = es(T) * 1000;
    // TODO: is T below really in kelvin?
#ifdef __CUDA_ARCH__
    const float A =
        (ConstantsGPU::Ls / (ConstantsGPU::Ka * (T + 273.15f))) * (ConstantsGPU::Ls / (ConstantsGPU::Rsw * (T + 273.15f)) - 1);
    const float B = (ConstantsGPU::Rsw * (T + 273.15f) * P) / (2.21f * EI);
#else
    const float A =
        (ConstantsCPU::Ls / (ConstantsCPU::Ka * (T + 273.15f))) * (ConstantsCPU::Ls / (ConstantsCPU::Rsw * (T + 273.15f)) - 1);
    const float B = (ConstantsCPU::Rsw * (T + 273.15f) * P) / (2.21f * EI);
#endif
    const float Ni =
        10000 * exp((12.96f * (ES - EI)) / (EI - 0.639f));  // Could use function, but than we have to again calculate EI and ES

    return 65.2f * ((pow(Ni, 0.5f) * (ES - EI)) / (pow(densityAir, 0.5f) * (A + B) * EI));
}

/// <summary>The potential temperature of a parcel of fluid at pressure P
/// is the temperature that the parcel would attain if adiabatically brought to a standard reference pressure P0,
/// usually 1,000 hPa(1,000 mb).
/// The difference between this and the dry lapse rate is that for the dry lapse rate we cool the temperature down by moving it
/// upwards, while for this we move the temperature back down to the reference temperature.(Thus in essential the same formula,
/// but we use it differently)</summary> <param name="T">Temperature in °C</param> <param name="PKnown">Known Pressure at Temp
/// in hPa</param> <param name="PTarget">Target Pressure at height in hPa</param> <returns>Potential temp in °C</returns>
__host__ __device__ inline float potentialTemp(const float T, const float Pk, const float Pt)
{
#ifdef __CUDA_ARCH__
    return (T + 273.15f) * powf((Pt / Pk), ConstantsGPU::Rsd / ConstantsGPU::Cpd) - 273.15f;
#else
    return (T + 273.15f) * powf((Pt / Pk), ConstantsCPU::Rsd / ConstantsCPU::Cpd) - 273.15f;
#endif
}


/// <summary>The dry adiabatic temperature is the temperature a dry parcel decreases with adiabatically,
/// you could assume its 9.8 in a hydrostatic balanced atmosphere, but we don't assume that here, thus we use poissons' asq
/// equation The difference between this and the dry lapse rate is that for the dry lapse rate we cool the temperature down by
/// moving it upwards, while for this we move the temperature back down to the reference temperature. (Thus in essential the
/// same formula, but we use it differently)</summary>
/// <param name="T">Temperature in °C</param>
/// <param name="PKnown">Known Pressure at Temp in hPa</param>
/// <param name="PTarget">Target Pressure at height in hPa</param>
/// <returns>Temp in °C</returns>
__host__ __device__ inline float dryLapseTemp(const float T, const float Pk, const float Pt)
{
#ifdef __CUDA_ARCH__
    return (T + 273.15f) * powf((Pt / Pk), ConstantsGPU::Rsd / ConstantsGPU::Cpd) - 273.15f;
#else
    return (T + 273.15f) * powf((Pt / Pk), ConstantsCPU::Rsd / ConstantsCPU::Cpd) - 273.15f;
#endif
}

/// <summary>Calculates the moist lapse rate</summary>
/// <param name="T">Temperature in °C</param>
/// <param name="P">Pressure in hPa</param>
/// <returns>Lapse rate in dK/dP (delta Kelving per delta hectoPascal)</returns>
__host__ __device__ inline float MLR(const float T, const float P)
{
    float TKelvin = T + 273.15f;
    float Ws = ws(T, P);
#ifdef __CUDA_ARCH__
    float Tw = ((ConstantsGPU::Rsd * TKelvin + ConstantsGPU::Hv * Ws) /
                (ConstantsGPU::Cpd +
                 (ConstantsGPU::Hv * ConstantsGPU::Hv * Ws * ConstantsGPU::E / (ConstantsGPU::Rsd * TKelvin * TKelvin))));
#else
    float Tw = ((ConstantsCPU::Rsd * TKelvin + ConstantsCPU::Hv * Ws) /
                (ConstantsCPU::Cpd +
                 (ConstantsCPU::Hv * ConstantsCPU::Hv * Ws * ConstantsCPU::E / (ConstantsCPU::Rsd * TKelvin * TKelvin))));
#endif
    return Tw / P;
}


/// <summary> Calculates the temperature of the (saturated) mixing ratio for every pressure</summary>
/// <param name="Ws">(Saturated) Mixing ratio</param>
/// <param name="pressures">Array of all pressures to calculate</param>
/// <param name="output">Array with the same size as pressures that will be filled by the function</param>
/// <param name="size">Size of the array</param>
// Only available on host, no use for GPU
__host__ inline void getTempAtWs(const float Ws, const float* pressures, float* output, const size_t size)
{
    for (int i = 0; i < size; i++)
    {
        float P = pressures[i];
        float EsT = (Ws * P) / (ConstantsCPU::E + Ws);

        // TODO wrong formula check es()
        output[i] = (-(log(EsT) - log(6.1078f)) * 243.5f) / (log(EsT) - log(6.1078f) - 17.67f);
    }
}

__host__ __device__ inline float getStandardHeightAtPressure(const float T0, const float P, const float P0)
{
    const float TKelvin = T0 + 273.15f;
    // float Tv = TKelvin * (1.0f + 0.611f * qv(TKelvin, P * 100));
    // return Rsd * Tv / g * logf(P0 / P);

    #ifdef __CUDA_ARCH__
    return TKelvin / ConstantsGPU::Lb *
           (powf(P / P0, -ConstantsGPU::R * ConstantsGPU::Lb / (ConstantsGPU::g * (ConstantsGPU::Mda * 0.001f))) - 1);
    #else
    return TKelvin / ConstantsCPU::Lb *
           (powf(P / P0, -ConstantsCPU::R * ConstantsCPU::Lb / (ConstantsCPU::g * (ConstantsCPU::Mda * 0.001f))) - 1);
    #endif

}

__host__ __device__ inline float getStandardPressureAtHeight(const float T0, const float h, const float h0, const float P0)
{
    const float TKelvin = T0 + 273.15f;

    #ifdef __CUDA_ARCH__
    return P0 * powf(1 + (ConstantsGPU::Lb / TKelvin) * (h - h0),
                     (-ConstantsGPU::g * (ConstantsGPU::Mda * 0.001f)) / (ConstantsGPU::R * ConstantsGPU::Lb));
    #else
    return P0 * powf(1 + (ConstantsCPU::Lb / TKelvin) * (h - h0),
                         (-ConstantsCPU::g * (ConstantsCPU::Mda * 0.001f)) / (ConstantsCPU::R * ConstantsCPU::Lb));
    #endif
}


/// <summary>The potential temperature of a parcel of fluid at pressure P
/// is the temperature that the parcel would attain if adiabatically brought to a standard reference pressure P0,
/// usually 1,000 hPa(1,000 mb).
/// https://en.wikipedia.org/wiki/Potential_temperature
/// while for this we move the temperature back down to the reference temperature.</summary>
/// <param name="temps">Array of all Temperatures in °C</param>
/// <param name="PTarget">Target Pressure in hPa</param>
/// <param name="PsKnown">Array of all pressures at all temps to calculate</param>
/// <param name="output">Array with the same size as pressures that will be filled by the function</param>
// Only available on host, no need for GPU
__host__ inline void getPotentialTempArray(const float* temps,
                                          const float Pt,
                                          const float* pressures,
                                          float* output,
                                          const size_t size)
{
    for (int i = 0; i < size; i++)
    {
        float P = pressures[i];
        float T = temps[i];
        output[i] = potentialTemp(T, P, Pt);
    }
}

/// <summary>The dry adiabatic is the temperature which the not saturated parcel decreases with in height.
/// https://en.wikipedia.org/wiki/Potential_temperature</summary>
/// <param name="T0">Reference Temperature in °C</param>
/// <param name="P0">Reference Pressure in hPa</param>
/// <param name="pressures">Array of all pressures to calculate</param>
/// <param name="output">Array with the same size as pressures that will be filled by the function</param>
/// <param name="size">Size of the array</param>
// Only available on host, no need for GPU
__host__ inline void getDryAdiabatic(const float T0, const float P0, const float* pressures, float* output, const size_t size)
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


/// <summary>Calculate moist lapse rate for given temp at given pressures</summary>
/// <param name="T0">Reference Temperature in °C</param>
/// <param name="P0">Reference Pressure in hPa</param>
/// <param name="pressures">Array of all pressures to calculate</param>
/// <param name="output">Array with the same size as pressures that will be filled by the function</param>
/// <param name="size">Size of the array</param>
// Only available on host, no need for GPU
__host__ inline void getMoistTemp(const float T0,
                           const float Pref,
                           const float* pressures,
                           float* output,
                           const size_t size,
                           int& offset)
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



//--------------------------------------------------------------------------------------------------
//                                                                                                 |
//                                          Extra's (mostly host)                                  |
//                                                                                                 |
//--------------------------------------------------------------------------------------------------




/// <summary>Calculates CCL with given parameters</summary>
/// <param name="P0">Initial pressure in hPa</param>
/// <param name="D0">Initial dew point in °C</param>
/// <param name="pressures">Array of all pressures to calculate</param>
/// <param name="temperatures">Array of all temperatures to calculate</param>
/// <param name="size">Size of the array</param>
/// <returns>.x = Temp at the CCL, <para>
/// .y = pressure at CCL, </para> <para>
/// .z = Potential Temp at the P0 </para></returns>
__host__ inline glm::vec3 getCCL(const float P0,
                                const float D0,
                                const float* pressures,
                                const float* temperatures,
                                const size_t size)
{
    int count = 0;
    while (pressures[count] > 100.0f) count++;
    if (count >= size) return {D0, P0, D0};

    float* temps = new float[size];
    const float WS = ws(D0, P0);
    getTempAtWs(WS, pressures, temps, size);

    // Loop until the mixing ratio temp >= observed temp
    count = 0;
    while (count < size && temperatures[count] > temps[count])
    {
        count++;
    }
    delete[] temps;

    const float PotTemp =
        (temperatures[count] + 273.15f) / powf((pressures[count] / P0), ConstantsCPU::Rsd / ConstantsCPU::Cpd) - 273.15f;
    return glm::vec3(temperatures[count], pressures[count], PotTemp);
}

/// <summary>
/// Lambert function approximation, mathmatical trick to solve formulas of type: w·e^w = x
/// Code made from Claude, as source: https://arxiv.org/pdf/1209.0735
/// </summary>
/// <param name="x">Value between -1/e (-0.367) and 0</param>
/// <returns></returns>
__host__ __device__ inline float lambertWm1(float x)
{
    constexpr float invE = -0.36787944f;  // -1 / e
    constexpr float E = 2.7182818f;

    if (x <= invE) return -1.0f;
    if (x >= 0.0f) return -1e30f;

    float w = 0.0f;

    // Initial guess
    if (x > -0.2f)
    {
        float L1 = logf(-x);
        float L2 = logf(-L1);
        w = L1 - L2 + L2 / L1;
    }
    else
    {
        float p = -sqrtf(2.0f * (1.0f + E * x));
        w = -1.0f + p - (p * p) / 3.0f + (11.0f * p * p * p) / 72.0f;
    }

    // Halley's iteration
    for (int i = 0; i < 8; i++)
    {
        float ew = expf(w);
        float wew = w * ew;
        float wewx = wew - x;
        float w1 = w + 1.0f;
        float denom = ew * w1 - (w + 2.0f) * wewx / (2.0f * w1);
        float dw = wewx / denom;
        w -= dw;
        if (fabsf(dw) < 1e-6f * fabsf(w)) break;
    }
    return w;
}

/// <summary>Calculates LCL with given parameters, formula from https://en.wikipedia.org/wiki/Lifting_condensation_level</summary>
/// <param name="T0">Initial temperature in °C</param>
/// <param name="P0">Initial pressure in hPa</param>
/// <param name="Z0">Initial height in m</param>
/// <param name="D0">Initial dew point in °C</param>
/// <returns>.x = Temp at LCL, <para>
/// .y = pressure at LCL, </para> <para>
/// .z = height at LCL </para></returns>
__host__ __device__ inline glm::vec3 getLCL(const float T0, const float P0, const float Z0, const float D0)
{
    // Formula from https://en.wikipedia.org/wiki/Lifting_condensation_level
    const float TKelvin = T0 + 273.15f;

    const float CPM = Cpm(T0, P0);
    const float RM = Rm(T0, P0);
    const float RHl = es(D0) / es(T0);

#ifdef __CUDA_ARCH__
    const float a = (CPM / RM) + ((ConstantsGPU::Cvl - ConstantsGPU::Cpvw) / ConstantsGPU::Rsw);
    const float b = -(ConstantsGPU::E0v - (ConstantsGPU::Cvv - ConstantsGPU::Cvl) * ConstantsGPU::Ttrip) / (ConstantsGPU::Rsw * TKelvin);
    float g = ConstantsGPU::g;
#else
    const float a = (CPM / RM) + ((ConstantsCPU::Cvl - ConstantsCPU::Cpvw) / ConstantsCPU::Rsw);
    const float b = -(ConstantsCPU::E0v - (ConstantsCPU::Cvv - ConstantsCPU::Cvl) * ConstantsCPU::Ttrip) / (ConstantsCPU::Rsw * TKelvin);
    float g = ConstantsCPU::g;
#endif
    const float c = b / a;

    const float Wmin1 = lambertWm1(powf(RHl, 1 / a) * c * expf(c));
    const float TLCL = (c / Wmin1) * TKelvin;
    const float pLCL = P0 * powf(TLCL / TKelvin, CPM / RM);
    const float zLCL = Z0 + CPM / g * (TKelvin - TLCL);

    return {TLCL - 273.15f, pLCL, zLCL};
}


/// <summary>Calculates the Level of Free Convection (LFC) or: height at which parcel is free to rise</summary>
/// <param name="T0">Initial temperature in °C</param>
/// <param name="P0">Initial pressure in hPa</param>
/// <param name="Z0">Initial height in m</param>
/// <param name="D0">Initial dew point in °C</param>
/// <param name="pressures">Array of all pressures to calculate</param>
/// <param name="temperatures">Array of all temperatures to calculate</param>
/// <param name="temperatures">Array of all altitude to calculate</param>
/// <param name="size">Size of the array</param>
/// <returns>Height of which an ideal air parcel is free to rise. (first time air parcel is warming than its surrounding)
/// <para>Returns -1 if no LFC was found</para></returns>
__host__ inline glm::vec3 getLFC(const float T0,
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

/// <summary>Calculates the Equilibrium Level (EL) or: Top of clouds</summary>
/// <param name="T0">Initial temperature in °C</param>
/// <param name="P0">Initial pressure in hPa</param>
/// <param name="Z0">Initial height in m</param>
/// <param name="D0">Initial dew point in °C</param>
/// <param name="pressures">Array of all pressures to calculate</param>
/// <param name="temperatures">Array of all temperatures to calculate</param>
/// <param name="temperatures">Array of all altitude to calculate</param>
/// <param name="size">Size of the array</param>
/// <returns>Height of tops (last time parcel was warmer than observed temp)
/// <para>Returns -1 if no EL was found</para></returns>
__host__ inline glm::vec3 getEL(const float T0,
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

    // Cut down our needs of calculations (we begin from the LCL height)
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

    // Now check for intersections
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
        // Note: we don't do anything when the two temps are the same.

        count++;
    }
    delete[] MLRtemps;

    return {temperatures[lastOutsideCount], pressures[lastOutsideCount], altitudes[lastOutsideCount]};
}


/// <summary>Calculates the CAPE (Convective Available Potential Energy)</summary>
/// <param name="T0">Initial temperature in °C</param>
/// <param name="P0">Initial pressure in hPa</param>
/// <param name="Z0">Initial height in m</param>
/// <param name="D0">Initial dew point in °C</param>
/// <param name="pressures">Array of all pressures to calculate</param>
/// <param name="temperatures">Array of all temperatures to calculate</param>
/// <param name="temperatures">Array of all altitude to calculate</param>
/// <param name="size">Size of the array</param>
/// <returns>CAPE in J/kg</returns>
__host__ inline float calculateCAPE(const float T0,
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
        const float QVe = qv(temperatures[count], pressures[count]);  // TODO: observed or standard pressure?
        const float QVp = qv(MLRtemps[count], pressures[count]);      // TODO: observed or standard pressure?

        const float Tve = (temperatures[count] + 273.15f) * (1 + ConstantsCPU::E * QVe);
        const float Tvp = (MLRtemps[count] + 273.15f) * (1 + ConstantsCPU::E * QVp);

        const float Dz = altitudes[count] - altitudes[count - 1];

        if (Tve < Tvp)
        {
            Cape += ((Tvp - Tve) / Tve) * Dz;  // Accumulate Cape
        }
        count++;
    }
    Cape *= ConstantsCPU::g;

    delete[] MLRtemps;

    return Cape;
}


}  // namespace MForms



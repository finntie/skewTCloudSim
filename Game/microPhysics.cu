#include "microPhysics.cuh"

//Game includes
#include "environment.h"

#include "meteoconstants.cuh"
#include "meteoformulas.cuh"
#include "game.h"
#include "dataClass.cuh"

#include <CUDA/include/cuda_runtime.h>
#include <CUDA/include/cuda.h>
#include <CUDA/cmath>



//Not same as the Gammas in #meteoformulas.cuh
__constant__ float m_gammaSS; //Smelting snow; (d + 5) / 2
__constant__ float m_gammaR; //Rain; 3 + b
__constant__ float m_gammaRC; //Rain to ice; 6 + b
__constant__ float m_gammaER; //Evaporating rain: (b + 5) / 2
__constant__ float m_gammaS; //Snow; 3 + d
__constant__ float m_gammaI; //Ice; 3.5f
__constant__ float m_gammaRI; //Smelting Ice; 2.75f

__constant__ float m_gammaQr; //Rain falling; 4 + b
__constant__ float m_gammaQs; //Snow falling; 4.0f + d
__constant__ float m_gammaQi; //Ice falling; 4.0f + d


/// <summary> Initializes all the gammas</summary>
void initGammasMicroPhysics()
{
    float GammaR, GammaER, GammaS, GammaRC, GammaSS, GammaI, GammaRI;
    float GammaQr, GammaQs, gammaQi; //For falling vel
    
    //Init constant gamma's
    const float b = 0.8f;
    const float d = 0.25f;
    GammaR = tgammaf(3 + b);
    GammaER = tgammaf((b + 5.0f) / 2.0f);
    GammaS = tgammaf(3 + d);
    GammaRC = tgammaf(6 + b);
    GammaSS = tgammaf((d + 5.0f) / 2.0f);
    GammaI = tgammaf(3.5f);
    GammaRI = tgammaf(2.75f);
    
    GammaQr = tgammaf(4.0f + b);
    GammaQs = tgammaf(4.0f + d);
    gammaQi = GammaQs;

    cudaMemcpyToSymbol(m_gammaR,  &GammaR, sizeof(float));
    cudaMemcpyToSymbol(m_gammaER, &GammaER, sizeof(float));
    cudaMemcpyToSymbol(m_gammaS,  &GammaS, sizeof(float));
    cudaMemcpyToSymbol(m_gammaRC, &GammaRC, sizeof(float));
    cudaMemcpyToSymbol(m_gammaSS, &GammaSS, sizeof(float));
    cudaMemcpyToSymbol(m_gammaI,  &GammaI, sizeof(float));
    cudaMemcpyToSymbol(m_gammaRI, &GammaRI, sizeof(float));
    cudaMemcpyToSymbol(m_gammaQr, &GammaQr, sizeof(float));
    cudaMemcpyToSymbol(m_gammaQs, &GammaQs, sizeof(float));
    cudaMemcpyToSymbol(m_gammaQi, &gammaQi, sizeof(float));
}



using namespace ConstantsGPU;

__device__ float FPVCON(const float temp, const float ps, const float Qv, const float QWS, const float dt, const float speed)
{
    //Condensation rate https://www.ecmwf.int/sites/default/files/elibrary/2002/16952-parametrization-non-convective-condensation-processes.pdf
    {
        const float _ES = esGPU(temp) * 10.0f;
        const float derivative = ((E * ps) / ((ps - _ES) * (ps - _ES))) * (_ES * (17.27f * 237.3f / ((temp + 237.3f) * (temp + 237.3f))));
        return 1 / (dt * speed) * (Qv - QWS) / (1 + E0v / Cpd * derivative);
    }
}

__device__ float FPVDEP(const float temp, const float ps, const float Qv, const float QWI, const float dt, const float speed)
{
    //Deposition rate based on https://www.ecmwf.int/sites/default/files/elibrary/2002/16952-parametrization-non-convective-condensation-processes.pdf
    {
        const float _EI = eiGPU(temp) * 10.0f;
        const float derivative = ((E * ps) / ((ps - _EI) * (ps - _EI))) * (_EI * (21.875f * 265.5f / ((temp + 265.5f) * (temp + 265.5f))));
        return 1 / (dt * speed) * (Qv - QWI) / (1 + Ls / Cpd * derivative); //TODO: check pos/neg
        //Already using dt * speed to already limit condensation/deposation correctly
    }
}

__device__ float FPIMLT(const float temp, const float Qc)
{
    //Melt cloud ice if possible
    //Melt all or the maximum we can handle
    if (temp >= 0.0f)
    {
        return Qc;
    }
    return 0.0f;
}

__device__ float FPIDW(const float dt, const float temp, const float Qc, const float Qw, const float Dair, const float ps)
{
    if (temp < 0.0f && Qw > 0.0f && Qc > 0.0f)
    {
        //Formula from https://journals.ametsoc.org/view/journals/mwre/128/4/1520-0493_2000_128_1070_asfcot_2.0.co_2.xml and WeatherScapes
        const float a = 0.5f; // capacitance for hexagonal crystals
        const float quu = fmax(1e-12f * NiGPU(temp) / (Dair * 0.001f), Qc);
        return powf((1 - a) * cvdGPU(temp, ps, Dair * 0.001f) * dt + powf(quu, 1 - a), 1 / (1 - a)) - Qc;
    }
    return 0.0f;
}

__device__ float FPIHOM(const float temp, const float Qw)
{
    if (temp < -40 && Qw > 0.0f)
    {
        return Qw;
    }
    return 0.0f;
}

__device__ float FPIACR(const float Qr, const float Qc, const float Dair, const float slopeR)
{
    if (Qr > 0 && Qc > 0.0f)
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

        const float density = powf(1.225f / Dair, 0.5f);

        return (PI * PI * ERI * N0R * a * Qc * densW * m_gammaRC) / (24 * MassI * powf(slopeR, 6 + b)) * density;
    }
    return 0.0f;
}

__device__ float FPRACI(const float Qr, const float Qc, const float Dair, const float slopeR)
{
    if (Qr > 0 && Qc > 0.0f)
    {
        //Formula from https://journals.ametsoc.org/view/journals/apme/22/6/1520-0450_1983_022_1065_bpotsf_2_0_co_2.xml
        //How many of this particle are in this region
        const float N0R = 8e-2f;
        //constants
        const float b = 0.8f;
        const float a = 2115.0f;
        //collection efficiency rain from cloud ice and cloud water
        const float ERI = 0.3f;

        const float density = powf(1.225f / Dair, 0.5f);

        return (PI * ERI * N0R * a * Qc * m_gammaR) / (4 * powf(slopeR, 3 + b)) * density;
    }
    return 0.0f;
}

__device__ float FPRAUT(const float Qw, const float QwMin, const float temp)
{
    //Cloud to rain due to autoconversion
    if (Qw >= QwMin)
    {
        const float rc = 10e-6f; // Estimated cloud droplet size, averages between 4 and 12 micrometer;
        float Nc = (Qw / (4 / 3 * PI * pwaterGPU(temp) * rc * rc * rc)) * 1.225f; //Cloud number concentation in m3
        Nc /= 1e6f; //Convert to how many droplets would be in cm3 instead of m3
        const float disE = 0.5f * 0.5f; //following Liu et al. (2006)
        //Normally its ^ 1 / 6, but in the formula we will be using its ^6, so it cancells out.
        const float dispersion = ((1 + 3 * disE) * (1 + 4 * disE) * (1 + 5 * disE)) / ((1 + disE) * (1 + 2 * disE)); //the relative dispersion of cloud droplets, 
        const float kBAW = 1.1e10f; //Constant k in g-2/cm3/s-1
        const float shapeParam = 1.0f; //Shape of tail of drop, in micrometer
        const float L = Qw * 1.225f * 1e-3f; //From kg/kg to g/cm3

        // Aggregation rate of liquid to rain rate coefficient: // Liu–Daum–McGraw–Wood (LD) scheme: https://journals.ametsoc.org/view/journals/atsc/63/3/jas3675.1.xml
        return 1e3f * kBAW * dispersion * (L * L * L) * std::powf(Nc, -1) *
            static_cast<float>(1 - std::exp(-std::powf(1.03e16f * std::powf(Nc, -2.0f / 3.0f) * (L * L), shapeParam))); //First 1e3 is conversion to kg/m3
    }
    return 0.0f;
}

__device__ float FPRACW(const float Qr, const float Qw, const float Dair, const float slopeR)
{
    if (Qr > 0 && Qw > 0.0f)
    {
        //Formula from https://journals.ametsoc.org/view/journals/apme/22/6/1520-0450_1983_022_1065_bpotsf_2_0_co_2.xml
        //How many of this particle are in this region
        const float N0R = 8e-2f;
        //constants
        const float b = 0.8f;
        const float a = 2115.0f;
        //collection efficiency rain from cloud ice and cloud water
        const float ERW = 1.0f;

        const float density = powf(1.225f / Dair, 0.5f);

        //Collection from cloud water
        return (PI * ERW * N0R * a * Qw * m_gammaR) / (4 * powf(slopeR, 3 + b)) * density;
    }
    return 0.0f;
}

__device__ float FPREVP(const float temp, const float Qr, const float Qv, const float QWS, const float Dair, const float ps, const float slopeR, const float PTerm1)
{
    //TODO: will be changed?
    // Evaporation rate of rain
    if (Qr > 0.0f && Qv / QWS < 1.0f && (1 - PTerm1)) //Only if there is vapor dificit in relation to saturation mixing ratio
    {
        const float tempK = temp + 273.15f;
        //Intercept parameter size distribution in cm-4
        const float N0R = 8e-2f;
        const float S = Qv / QWS; //Saturation ratio
        //constants
        const float b = 0.8f;
        const float a = 2115.0f;

        const float dQv = DQVairGPU(temp, ps); //Diffusivity of water vapor in air
        const float kv = ViscAirGPU(temp); //Kinematic viscosity of air
        float Sc = kv / dQv; // Schmidt number (kv /dQv)

        const float density = powf(1.225f / (Dair * 0.001f), 0.25f);

        //Added a -1 multiplier to make it positive instead of negative (Due to S - 1)
        return fmax(0.0f, -1 * 2 * PI * (S - 1) * N0R * (0.78f * powf(slopeR, -2) + 0.31f * powf(Sc, 1.0f / 3.0f) *
            m_gammaER * powf(a, 0.5f) * powf(kv, -0.5f) * density * powf(slopeR, -((b + 5) / 2))) *
            1 / (Dair * 0.001f) * powf(E0v * E0v / (Ka * Rsw * tempK * tempK) + (1 / (Dair * 0.001f * QWS * dQv)), -1.0f));
    }
    return 0.0f;
}

__device__ float FPRACS(const float Qr, const float Qs, const float PTerm2, const float3 fallVel, const float Dair, const float slopeS, const float slopeR)
{
    //Used for 3 component freezing snow to hail.
    if (Qr > 0.0f && Qs > 0.0f && PTerm2 == 0)
    {
        //Densities in g/cm3
        const float densS = 0.11f;
        //Intercept parameter size distribution in cm-4
        const float N0S = 3e-2f;
        const float N0R = 8e-2f;

        const float ESR = 1.0f;
        //If PTerm2 = 0, 3-component freezing to increase hail.
        //Else 2-component freezing, snow grows in expens of rain, only PSACR is used.
        //But if T > 0, PSACR is used to enhance PSMLT.

        const float size = PI * PI * ESR * N0R * N0S;

        //TODO: check densS / m_Dair
        return size * fabs(fallVel.x - fallVel.y) * (densS / (Dair * 0.001f)) *
            (5 / (powf(slopeS, 6) * slopeR) + (2 / (powf(slopeS, 5) * powf(slopeR, 2))) + (0.5f / (powf(slopeS, 4) * powf(slopeR, 3))));
    }
    return 0.0f;
}

__device__ float FPSACW(const float Qs, const float Qw, const float Dair, const float slopeS)
{
    if (Qs > 0 && Qw > 0)
    {
        //Formula from https://journals.ametsoc.org/view/journals/apme/22/6/1520-0450_1983_022_1065_bpotsf_2_0_co_2.xml
        //How many of this particle are in this region
        const float N0S = 3e-2f;
        //constants
        const float c = 152.93f;
        const float d = 0.25f;
        //collection efficiency snow from cloud ice and cloud water
        const float ESW = 1.0f;

        const float density = powf(1.225f / Dair, 0.5f);

        return (PI * ESW * N0S * c * Qw * m_gammaS) / (4 * powf(slopeS, 3 + d)) * density;
    }
    return 0.0f;
}

__device__ float FPSACR(const float Qr, const float Qs, const float3 fallVel, const float Dair, const float slopeR, const float slopeS)
{
    //Used in PSMLT or decreases rain
    if (Qr > 0.0f && Qs > 0.0f)
    {
        //Densities in g/cm3
        const float densW = 0.99f;
        //Intercept parameter size distribution in cm-4
        const float N0S = 3e-2f;
        const float N0R = 8e-2f;

        const float ESR = 1.0f;
        //If PTerm2 = 0, 3-component freezing to increase hail.
        //Else 2-component freezing, snow grows in expens of rain, only PSACR is used.
        //But if T > 0, PSACR is used to enhance PSMLT.

        const float size = PI * PI * ESR * N0R * N0S;

        return size * fabsf(fallVel.y - fallVel.x) * (densW / (Dair * 0.001f)) *
            (5 / (powf(slopeR, 6) * slopeS) + (2 / (powf(slopeR, 5) * powf(slopeS, 2))) + (0.5f / (powf(slopeR, 4) * powf(slopeS, 3))));
    }
    return 0.0f;
}

__device__ float FPSACI(const float temp, const float Qs, const float Qc, const float Dair, const float slopeS)
{
    if (Qs > 0 && Qc > 0)
    {
        //Formula from https://journals.ametsoc.org/view/journals/apme/22/6/1520-0450_1983_022_1065_bpotsf_2_0_co_2.xml
        //How many of this particle are in this region
        const float N0S = 3e-2f;
        //constants
        const float c = 152.93f;
        const float d = 0.25f;
        //collection efficiency snow from cloud ice and cloud water
        const float ESI = expf(0.025f * (temp));

        const float density = powf(1.225f / Dair, 0.5f);

        return (PI * ESI * N0S * c * Qc * m_gammaS) / (4 * powf(slopeS, 3 + d)) * density;
    }
    return 0.0f;
}

__device__ float FPSAUT(const float temp, const float Qc, const float QcMin)
{
    if (Qc - QcMin > 0.0f && temp < 0.0f)
    {
        return 1e-3f * exp(0.025f * (temp)) * (Qc - QcMin);
    }
    return 0.0f;
}

__device__ float FPSFW(const float dt, const float temp, const float ps, const float Qs, const float Qw, const float Qc, const float Qv, const float QWI, const float Dair)
{
    if (Qs > 0 && Qw > 0)
    {
        const float tempK = temp + 273.15f;
        //Formula from https://journals.ametsoc.org/view/journals/apme/22/6/1520-0450_1983_022_1065_bpotsf_2_0_co_2.xml
        //collection efficiency snow from cloud ice and cloud water
        const float EIW = 1.0f;

        //radius, mass and terminal velocity of 50 or 40 micrometer size ice crystal to cm or gram
        const float RI50 = 5e-3f;
        const float mI50 = 4.8e-7f;
        const float mI40 = 2.46e-7f;
        const float UI50 = 100.0f;

        //Formula for A and B https://journals.ametsoc.org/view/journals/atsc/40/5/1520-0469_1983_040_1185_tmamsa_2_0_co_2.xml?tab_body=pdf
        //const float density = powf(1.225f / m_Dair, 0.5f);
        float Si = Qv / QWI; // Saturation ratio over ice
        const float X = DQVairGPU(temp, ps);
        const float A = E0v / (Ka * tempK) * (Ls * Mw * 0.001f / (R * tempK) - 1); //Mw to kg/mol
        const float B = R * tempK * X * Mw * 0.001f * eiGPU(temp) * 1000; //kPa to Pa

        const float a1g = 206.2f * (Si - 1) / (A + B);
        const float a2g = expf(0.09f * temp);

        const float dt1 = 1 / (a1g * (1 - a2g)) * (powf(mI50, 1 - a2g) - powf(mI40, 1 - a2g));

        const float Qc50 = Qc * (dt / dt1) / 100; //Should vary between 0.5 and 10%
        const float NI50 = Qc50 / mI50;
        return NI50 * (a1g * powf(mI50, a2g) + PI * EIW * (Dair * 0.001f) * Qw * RI50 * RI50 * UI50);

    }
    return 0.0f;
}

__device__ float FPSFI(const float dt, const float temp, const float ps, const float Qs, const float Qw, const float Qc, const float Qv, const float QWI, const float Dair)
{
    if (Qs > 0 && Qc > 0)
    {
        const float tempK = temp + 273.15f;
        //Formula from https://journals.ametsoc.org/view/journals/apme/22/6/1520-0450_1983_022_1065_bpotsf_2_0_co_2.xml
        //Correction from https://ntrs.nasa.gov/api/citations/19990100647/downloads/19990100647.pdf

        //radius, mass and terminal velocity of 50 or 40 micrometer size ice crystal to cm or gram
        const float mI50 = 4.8e-7f;
        const float mI40 = 2.46e-7f;

        //Formula for A and B https://journals.ametsoc.org/view/journals/atsc/40/5/1520-0469_1983_040_1185_tmamsa_2_0_co_2.xml?tab_body=pdf
        float Si = Qv / QWI; // Saturation ratio over ice
        const float X = DQVairGPU(temp, ps);
        const float A = E0v / (Ka * tempK) * (Ls * Mw * 0.001f / (R * tempK) - 1); //Mw to kg/mol
        const float B = R * tempK * X * Mw * 0.001f * eiGPU(temp) * 1000; //kPa to Pa

        const float a1g = 206.2f * (Si - 1) / (A + B);
        const float a2g = expf(0.09f * temp);

        if (a1g > 0) return a2g * a1g * Qc / (powf(mI50, 0.5f) - powf(mI40, 0.5f));
    }
    return 0.0f;
}

__device__ float FPSDEP(const float temp, const float ps, const float Qv, const float QWI, const float Dair, const float slopeS)
{
    if (temp < 0.0f)
    {
        const float tempK = temp + 273.15f;
        //How many of this particle
        const float N0S = 3e-2f;
        //constants
        const float c = 152.93f;
        const float d = 0.25f;

        const float dQv = DQVairGPU(temp, ps); //Diffusivity of water vapor in air
        const float kv = ViscAirGPU(temp); //Kinematic viscosity of air
        float Sc = kv / dQv; // Schmidt number (kv /dQv)
        float Si = Qv / QWI; // Saturation ratio over ice

        const float A = Ls * Ls / (Ka * Rsw * tempK * tempK);
        const float B = 1 / (Dair * 0.001f * wiGPU(temp, ps) * dQv);

        const float density = powf(1.225f / Dair, 0.25f);

        return PI * PI * (Si - 1) / (Dair * 0.001f * (A + B)) * N0S *
            (0.78f * powf(slopeS, -2) + 0.31f * powf(Sc, 1.0f / 3.0f) * m_gammaSS * powf(c, 0.5f) * density * powf(kv, -0.5f) * powf(slopeS, -(d + 5) / 2));
    }
    return 0.0f;
}

__device__ float FPSSUB(const float PSDEP)
{
    return fmax(0.0f, -1 * PSDEP); //Inverse of depos and can't be negative
}

__device__ float FPSMLT(const float temp, const float ps, const float Qw, const float Qr, const float Qs, const float Qv, const float Dair, const float slopeS, const float PSACW, const float PSACR, const float dtSpeed)
{
    if (temp >= 0.0f && Qs > 0.0f)
    {
        //Limit variables
        const float LPSACW = fminf(Qw, PSACW * dtSpeed);
        const float LPSACR = fminf(Qr, PSACR * dtSpeed);

        //TODO: introduce limit?
        //const float limit = Cpd / Lf * m_Tc;

        //PSMLT by https://research.csiro.au/ccam/wp-content/uploads/sites/520/2024/01/1377337420.pdf
        //How many of this particle
        const float N0S = 3e-2f;
        //constants
        const float c = 152.93f;
        const float d = 0.25f;

        const float dQv = DQVairGPU(temp, ps); //Diffusivity of water vapor in air
        const float kv = ViscAirGPU(temp); //Kinematic viscosity of air
        float Sc = kv / dQv; // Schmidt number (kv /dQv)
        const float Drs = wsGPU(0.0f, ps) - Qv;

        const float density = powf(1.225f / Dair, 0.25f);

        //Multiply by -1 to make value turned around (we want to make it positive)
        return fmax(0.0f, -1 * (-(2 * PI / (density * 0.001f * Lf)) * (Ka * temp - E0v * dQv * density * 0.001f * Drs) * N0S *
            (0.78f * powf(slopeS, -2) + 0.31f * powf(Sc, 1.0f / 3.0f) * m_gammaSS * powf(c, 0.5f) * density * powf(kv, -0.5f) * powf(slopeS, -(d + 5) / 2))
            - (Cvl * temp / Lf) * (PSACW + PSACR)));
    }
    return 0.0f;
}

__device__ float FPGAUT(const float temp, const float Qs, const float QiMin)
{
    const float surplus = Qs - QiMin;
    if (surplus > 0.0f && temp < 0.0f)
    {
        return (1e-3f * exp(0.09f * temp)) * (surplus);
    }
    return 0.0f;
}

__device__ float FPGFR(const float temp, const float Qr, const float Dair, const float slopeR)
{
    if (Qr > 0.0f && temp < 0.0f)
    {
        //How many of this particle
        const float N0R = 8e-2f;
        //Densities in g/cm3
        const float densW = 0.99f;
        //Constants
        const float A = 0.66f; // Kelvin
        const float B = 100.0f; // m3/s

        return 20 * PI * PI * B * N0R * (densW / (Dair * 0.001f)) * (exp(A * (-temp)) - 1) * powf(slopeR, -7);
    }
    return 0.0f;
}

__device__ float FPGACW(const float Qi, const float Qw, const float slopeI, const float Dair)
{
    if (Qi > 0.0f && Qw > 0.0f)
    {
        //How many of this particle
        const float N0I = 4e-4f;
        //Density in g/cm3
        const float densI = 0.91f;
        //Constants
        const float EGW = 1.0f;
        const float CD = 0.6f; //Drag coefficient

        return PI * EGW * N0I * Qw * m_gammaI / (4 * powf(slopeI, 3.5f)) * powf(4 * g * 100 * densI / (3 * CD * Dair * 0.001f), 0.5f); //Converting g to cm/s2 and densAir to g/cm3
    }
    return 0.0f;
}

__device__ float FPGACI(const float Qi, const float Qc, const float slopeI, const float Dair)
{
    if (Qi > 0.0f && Qc > 0.0f)
    {
        //How many of this particle
        const float N0I = 4e-4f;
        //Density in g/cm3
        const float densI = 0.91f;
        //Constants
        const float EGI = 1.0f;
        const float CD = 0.6f; //Drag coefficient

        return PI * EGI * N0I * Qc * m_gammaI / (4 * powf(slopeI, 3.5f)) * powf(4 * g * 100 * densI / (3 * CD * Dair * 0.001f), 0.5f); //Converting g to cm/s2 and densAir to g/cm3
    }
    return 0.0f;
}

__device__ float FPGACR(const float Qi, const float Qr, const float Dair, const float3 fallVel, const float slopeR, const float slopeI)
{
    if (Qi > 0.0f && Qr > 0.0f)
    {
        //How many of this particle
        const float N0R = 8e-2f;
        const float N0I = 4e-4f;
        //Density in g/cm3
        const float densW = 0.99f;
        //Constants
        const float EGR = 0.1f; //0.1 since this is for dry growth

        return PI * PI * EGR * N0R * N0I * fabs(fallVel.z - fallVel.x) * (densW / (Dair * 0.001f)) *
            (5 / (powf(slopeR, 6) * slopeI) + (2 / (powf(slopeR, 5) * powf(slopeI, 2))) + (0.5f / (powf(slopeR, 4) * powf(slopeI, 3))));
    }
    return 0.0f;
}

__device__ float FPGACS(const float temp, const float Qs, const float Qi, const float Dair, const float3 fallVel, const float slopeS, const float slopeI, const bool EGS1)
{
    if (Qs > 0.0f && Qi > 0.0f)
    {
        //Densities in g/cm3
        const float densS = 0.11f;
        //How many of this particle
        const float N0S = 3e-2f;
        const float N0I = 4e-4f;

        const float EGS = temp >= 0.0f || EGS1 ? 1.0f : exp(0.09f * temp);

        return PI * PI * EGS * N0S * N0I * fabs(fallVel.z - fallVel.y) * (densS / (Dair * 0.001f)) *
            (5 / (powf(slopeS, 6) * slopeI) + (2 / (powf(slopeS, 5) * powf(slopeI, 2))) + (0.5f / (powf(slopeS, 4) * powf(slopeI, 3))));

    }
    return 0.0f;
}

__device__ float FPGDRY(const float Qw, const float Qc, const float Qr, const float Qs, 
    const float PGACW, const float PGACI, const float PGACR, const float PGACS, const float dtSpeed)
{
    //Limit all values
    const float LPGACW = fminf(Qw, PGACW * dtSpeed);
    const float LPGACI = fminf(Qc, PGACI * dtSpeed);
    const float LPGACR = fminf(Qr, PGACR * dtSpeed);
    const float LPGACS = fminf(Qs, PGACS * dtSpeed);

    return LPGACW + LPGACI + LPGACR + LPGACS;
}

__device__ float FPGSUB(const float temp, const float ps, const float Qv, const float QWI, const float Qi, const float Dair, const float slopeI)
{
    float Si = Qv / QWI; // Saturation ratio over ice

    if (Qi > 0.0f && Si < 1.0f)
    {
        const float tempK = temp + 273.15f;
        //How many of this particle
        const float N0I = 4e-4f;
        //Density in g/cm3
        const float densI = 0.91f;
        //constants
        const float CD = 0.6f; //Drag coefficient

        const float dQv = DQVairGPU(temp, ps); //Diffusivity of water vapor in air
        const float kv = ViscAirGPU(temp); //Kinematic viscosity of air
        float Sc = kv / dQv; // Schmidt number (kv /dQv)

        const float A = Ls * Ls / (Ka * Rsw * tempK * tempK);
        const float B = 1 / (Dair * 0.001f * wiGPU(temp, ps) * dQv);

        const float density = powf(4 * g * 100 * densI / (3 * CD * Dair * 0.001f), 0.25f);

        //We multiply by -1, since Si - 1 should be negative for sublimation to occur. Hail deposition could not occur, since this would convert to snow!
        return fmax(0.0f, -1 * PI * PI * (Si - 1) / (Dair * 0.001f * (A + B)) * N0I *
            (0.78f * powf(slopeI, -2) + 0.31f * powf(Sc, 1.0f / 3.0f) * m_gammaRI * density * powf(kv, -0.5f) * powf(slopeI, -2.75f)));
    }
    return 0.0f;
}

__device__ float FPGMLT(const float temp, const float ps, const float Qw, const float Qr, const float Qi, const float Qv, const float Dair, const float slopeI, const float PGACW, const float PGACR, const float dtSpeed)
{
    if (Qi > 0.0f)
    {
        //Limit variables
        const float LPGACW = fminf(Qw, PGACW * dtSpeed);
        const float LPGACR = fminf(Qr, PGACR * dtSpeed);
        //How many of this particle
        const float N0I = 4e-4f;
        //Density in g/cm3
        const float densI = 0.91f;
        //Constants
        const float CD = 0.6f; //Drag coefficient

        const float dQv = DQVairGPU(temp, ps); //Diffusivity of water vapor in air
        const float kv = ViscAirGPU(temp); //Kinematic viscosity of air
        float Sc = kv / dQv; // Schmidt number (kv /dQv)
        const float Drs = wsGPU(0.0f, ps) - Qv;

        const float density = powf(4 * g * 100 * densI / (3 * CD), 0.25f);

        //Multiply by -1 to make value turned around (we want to make it positive)
        return fmax(0.0f, -1 * (-2 * PI / (Dair * 0.001f * Lf) * (Ka * temp - E0v * dQv * Dair * 0.001f * Drs) * N0I *
            (0.78f * powf(slopeI, -2) + 0.31f * powf(Sc, 1.0f / 3.0f) * m_gammaRI * density * powf(kv, -0.5f) * powf(slopeI, -2.75f)) -
            Cvl * temp / Lf * (LPGACW + LPGACR)));
    }
    return 0.0f;
}


__device__ float FPGWET(const float temp, const float ps, const float Qc, const float Qs, const float Qi, const float Qv, 
    const float3 fallVel, const float Dair, const float slopeS, const float slopeI, const float PGACI, const float PGACS, const float dtSpeed)
{
    if (Qi > 0.0f)
    {
        const float PGACI1 = PGACI * 10.0f; //Multiplying by 10 is the same as re-calculating PGACI with EGI as 1.0f
        //Re-calcuating PGACS if Temp is lower then 0.0 to ensure EGS is 1.0f TODO: could otherwise use division by exp(0.09f * temp) to regain value
        const float PGACS1 = temp >= 0.0f ? PGACS : FPGACS(temp, Qs, Qi, Dair, fallVel, slopeS, slopeI, true);

        //Limit them
        const float LPGACI1 = fminf(Qc, PGACI1 * dtSpeed);
        const float LPGACS1 = fminf(Qs, PGACS1 * dtSpeed);

        //How many of this particle
        const float N0I = 4e-4f;
        //Density in g/cm3
        const float densI = 0.91f;
        //Constants
        const float CD = 0.6f; //Drag coefficient

        const float dQv = DQVairGPU(temp, ps); //Diffusivity of water vapor in air
        const float kv = ViscAirGPU(temp); //Kinematic viscosity of air
        float Sc = kv / dQv; // Schmidt number (kv /dQv)
        const float Drs = wsGPU(0.0f, ps) - Qv; //In kg/kg or g/g

        const float density = powf(4 * g * 100.0f * densI / (3 * CD), 0.25f);

        return 2 * PI * N0I * (Dair * 0.001f * E0v * dQv * Drs - Ka * temp) / (Dair * 0.001f * (Lf + Cvl * temp)) *
            (0.78f * powf(slopeI, -2) + 0.31f * powf(Sc, 1.0f / 3.0f) * m_gammaRI * density * powf(kv, -0.5f) * powf(slopeI, -2.75f)) +
            (LPGACI1 + LPGACS1) * (1 - Cpi * temp / (Lf + Cvl * temp));
    }

    return 0.0f;
}

__device__ float FPGACR1(const float temp, const float Qw, const float Qc, const float Qs, const float Qi, const float Dair, const float3 fallVel, const float slopeS, const float slopeI, const float PGWET, const float PGACW, const float PGACI, const float PGACS, const float dtSpeed)
{
    if (Qi > 0.0f)
    {
        const float PGACI1 = PGACI * 10.0f; //Multiplying by 10 is the same as re-calculating PGACI with EGI as 1.0f
        //Re-calcuating PGACS if Temp is lower then 0.0 to ensure EGS is 1.0f TODO: could otherwise use division by exp(0.09f * temp) to regain value
        const float PGACS1 = temp >= 0.0f ? PGACS : FPGACS(temp, Qs, Qi, Dair, fallVel, slopeS, slopeI, true);

        //Limit them
        const float LPGACI1 = fminf(Qc, PGACI1 * dtSpeed);
        const float LPGACS1 = fminf(Qs, PGACS1 * dtSpeed);
        const float LPGACW = fminf(Qw, PGACW * dtSpeed);

        //If PGACW < (PGWET - PGACI1 - PGACS1), then PGACR1 is positive, meaning, we have freezing, else the wetness collected on the hail will melt off. (PGACR1 < 0)
        return PGWET - LPGACW - LPGACI1 - LPGACS1;
    }

    return 0.0f;
}

__device__ float FPGFLW(const float Qr)
{
    if (Qr > 0.0f)
    {
        const float BIR = 100.0f; // Rate of water flowing through ground
        const float k{ 5e-5f }; //Sand. Hydraulic conductivity of the ground in m/s(How easy water flows through ground) https://structx.com/Soil_Properties_007.html
        return BIR * k * Qr; //TODO: this values is way too low, I have to increase with BIR, check with real life values.
    }
    return 0.0f;
}

__device__ float FPGGMLT(const float tempGroundC, const float Qi, const float Dair, const float slopeI)
{
    if (Qi > 0.0f) //Ice on ground
    {
        //Made up formula using variables from papers and help of AI
        //Thermal conductivity soil https://www.cableizer.com/documentation/k_4
        const float k4 = 0.955f * 1.0e-2f; // from meter to W/(cm.K)

        //Density in g/cm3
        const float densI = 0.91f;
        //Ice thickness
        const float currentDens = Qi * Dair * 0.001f; //Convert to g/cm3
        const float amountIce = currentDens / densI;
        const float iceHeight = amountIce * 100.0f; //Convert to cm3, then to cm (height)

        //Contact fraction
        //const float RoughnessSoil = 10.0f; //in mm https://www.sciencedirect.com/science/article/abs/pii/S034181622030014X
        //const float roughnessIce = 0.2f; //in mm https://www.cambridge.org/core/journals/journal-of-glaciology/article/measurement-and-parameterization-of-aerodynamic-roughness-length-variations-at-haut-glacier-darolla-switzerland/30AB8A2DCFF90741FF302B4EB68D359B
        //Taking these into account, we just assume the area fraction to be of around 0.45
        const float areaFraction = 0.45f;

        //How many of this particle
        const float N0I = 4e-4f;

        const float QGround = areaFraction * k4 * tempGroundC / iceHeight;

        const float meltRate = fmax(0.0f, QGround / Lf); //Melting rate in kg/m2/s
        //Convert to kg/kg/s
        return meltRate * N0I * powf(slopeI, -2) * areaFraction / (Dair * 0.001f);
    }
    return 0.0f;
}

__device__ float FPGSMLT(const float tempAirK, const float tempGroundC, const float ps, const float Qs, const float Qv, const float Qr, const float irradiance, const float cloudCover, const float Dair, const float windSpeed, const float dt)
{
    if (Qs > 0.0f) //Snow on ground
    {
        //Using formulas from https://tc.copernicus.org/articles/17/211/2023/
        float Qs = 0.0f, Ql = 0.0f, Qh = 0.0f, Qe = 0.0f, Qg = 0.0f, Qp = 0.0f; //Energy in W/m-2
        const float Tac = tempAirK - 273.15f;

        const float Ts = 0.0f; //Snow surface temp (using 0 because it would be about freezing)
        const float Tsk = Ts + 273.15f; //Now in kelvin

        //--DDFs (shortwave)--
        const float Si = irradiance;
        //Assuming snow lies for no longer then 1 day, Albedo will be about 0.9 with max being 0.95
        const float A = 0.9f;
        Qs = (1 - A) * Si;

        //--DDFl (longwave)--
        const float Qlout = 310.0f; //W/m-2
        const float eac = 9.2e-6f * tempAirK * tempAirK; //Calculate the clear-sky longwave emissivity;
        const float ea = (1 - 0.84f * cloudCover) * eac + 0.84f * cloudCover;
        const float Qlin = ea * oo * Tsk * Tsk * Tsk * Tsk;
        Ql = Qlin - Qlout;

        //DDFh (Sensible heat)
        const float k = 0.41f;
        const float z0m = 0.001f; // Aerodynamic Roughness Lengths in m using from https://tc.copernicus.org/articles/17/211/2023/
        const float z0h = 0.0002f; // heat roughness parameter
        const float Ch = k * k / (logf(1.0f / z0m) * logf(2.0f / z0h)); //Using z = 2 meter high because its the standard
        Qh = Dair * Cpd * Ch * windSpeed * (Tac - Ts);

        //DDFe (latent heat)
        const float Ce = Ch;
        const float Qvs = Qv / (1 + Qv); //Mixing ratio to specific humidity
        Qe = Dair * E0v * Ce * windSpeed * (Qvs - qsGPU(Ts, ps));

        //DDFe (ground)  using https://en.wikipedia.org/wiki/Thermal_conduction
        const float Td = tempGroundC - Ts; //Delta temp
        if (Td > 0.0f) //We don't want to subtract energy (i.e. create more snow)
        {
            //Density in g/cm3
            const float densS = 0.11f;
            //Ice thickness
            const float currentDens = Qs * Dair * 0.001f; //Convert to g/cm3
            const float amountSnow = currentDens / densS;
            const float snowHeight = amountSnow; //Convert to cm3, then to m (height)
            const float ks = 0.3f; //Thermal conductivity of snow in w/m/k: https://online.ucpress.edu/elementa/article/12/1/00086/200266/Snow-thermal-conductivity-and-conductive-flux-in
            Qg = ks * (tempGroundC - Ts) / snowHeight;
        }

        //DDFp (precip)
        Qp = Cvl * 0.001f * Qr * Dair * tempAirK;

        const float Q = Qs + Ql + Qh + Qe + Qg + Qp;

        // Getting kg/m3, due to multiplying by cell height you get kg/m2, 
        // yet, cells are same widht and height, thus multiplying by 1 gives the same answer.
        const float Qss = Qs * Dair;
        const float QE = (Q * dt) / Qss; //Getting energy in J/kg 
        const float meltFrac = QE / Lf; //Get fraction of what melted

        return fmax(0.0f, meltFrac * Qs); //Amount of snow that melts.
    }
    return 0.0f;
}

__device__ float FPGREVP(const float tempAirK, const float tempGroundC, const float ps, const float Qv, const float Qr, const float irradiance, const float Dair, const float windSpeed)
{
    if (Qr)
    {
        const float tempGroundK = tempGroundC + 273.15f;

        //Using formula from: https://en.wikipedia.org/wiki/Penman_equation
        //Net radiation from https://agupubs.onlinelibrary.wiley.com/doi/full/10.1029/2020WR027332

        //Vapour pressure deficit
        const float VPD = esGPU(tempGroundC) * 1000.0f * (1 - Qv / wsGPU(tempGroundC, ps));

        //momentum surface aerodynamic conductance 
        const float k = 0.41f;
        const float z0m = 0.0001f; // Aerodynamic Roughness Lengths in m for winds https://agupubs.onlinelibrary.wiley.com/doi/full/10.1029/2020EA001165
        const float z0h = z0m * 0.1f; // Simply assuming smooth water 
        const float ga = windSpeed * k * k / (logf(1.0f / z0m) * logf(2.0f / z0h)); //Using z = 2 meter high because its the standard

        //Psychrometric constant
        const float y = Cpd * ps * 100.0f / (0.622f * E0v);

        //Calculate net irradiance
        const float es = 0.96f; //Water emissivity https://en.wikipedia.org/wiki/Emissivity
        const float a = 0.08f; //Water albedo https://en.wikipedia.org/wiki/Albedo
        const float aP = esGPU(tempAirK - 273.15f) * 10.0f * (Qv / wsGPU(tempAirK - 273.15f, ps)); //Actual vapor pressure
        const float ea = 1.24f * powf((aP / tempAirK), 1.0f / 7.0f);
        const float Rn = irradiance * (1 - a) + oo * es * (ea * (tempAirK * tempAirK * tempAirK * tempAirK) - (tempGroundK * tempGroundK * tempGroundK * tempGroundK));
        const float RnG = Rn - 0.2f * Rn; //Using ground flux https://agupubs.onlinelibrary.wiley.com/doi/full/10.1002/2016jg003591#jgrg20708-bib-0001

        //Slope of saturation vapor pressure curve
        const float m = esGPU(tempGroundC) * 1000.0f * E0v / (Rsw * tempGroundK * tempGroundK);

        //Ground heat flux - Rn = - H - LE

        //Evaporation in kg/m2*s
        const float Em = (m * RnG + Dair * Cpd * VPD * ga) / (E0v * (m + y));

        //Divide by density of air to get kg/kg*s since our grid is of 1:1 scale.
        const float EmD = fmax(0.0f, Em / Dair);

        //Now we need to assume, since our whole land is not one big pool, water is gathered in pools
        //Assuming random formula to calculate area of water
        //This formula will result in: Qr:0.05 = 0.002m2 | Qr:0.1 = 0.003m2 | Qr:1 = 0.01m2 | Qr:100 = 0.1m2/m2
        const float areaW = fmin(1.0f, 0.01f * sqrt(Qr));

        return EmD * areaW;
    }
    return 0.0f;
}

__device__ float FPGDEVP(float* time, const float Qr, const float Qs, const float Qi, const float Qrs)
{
    if (Qrs)
    {
        if (Qr == 0 && Qs == 0 && Qi == 0)
        {
            float BEG{ 200.0f };	//NotSure		  // Evaporation rate of dry ground
            const float D_ = 1e-6f; // Weigthed mean diffusivity of the ground //TODO: hmm, could tripple check if right
            const float secsInDay = 60 * 60 * 24;
            const float O_ = 0.21f * secsInDay; // evaporative ground water storage coefficient (i.e. only part of the soil can be evaporated) https://en.wikipedia.org/wiki/Specific_storage

            //Note that this is just picked from water storage coefficient which may not is the same as the evaporative soil water storage coefficient.
            //Only if Qgj = 0 (Precip falling) and if we are at the ground

            //https://agupubs.onlinelibrary.wiley.com/doi/full/10.1002/2013WR014872
            const float waterA = Qrs;
            return BEG * D_ * waterA * exp(-*time / O_);
        }
        else
        {
            *time = 0.0f;
        }
    }
    return 0.0f;
}

__device__ float FPGGFR(const float tempAirK, const float tempGroundC, const float ps, const float Qr, const float Qv, const float Qi, const float Dair, const float irradiance, const float cloudCover, const float windSpeed, const float dt)
{
    //Freezing of water
    const float Tac = tempAirK - 273.15f;
    if (Qr && Tac < 0.0f)
    {
        //Using the same principle as FPGRFR (https://journals.ametsoc.org/view/journals/wefo/37/1/WAF-D-21-0085.1.xml)
        float Qw = 0.0f, Qc = 0.0f, Qe = 0.0f, Ql = 0.0f, Qs = 0.0f, Qg = 0.0f, Qf = 0.0f;

        //How much energy to fully convert 
        //Going from J/kg to W/kg by dividing by dt and W/kg to W/m2 by multiplying by air density.
        Qf = Lf * Qr * Dair / dt;

        //How much energy it would take to go to freezing, assuming the water is still as cold as the air
        Qw = Cvl * (273.15f - tempAirK) * Qr * Dair / dt;

        const float k = 0.41f;
        const float z0m = 0.0001f; // Aerodynamic Roughness Lengths in m for winds https://agupubs.onlinelibrary.wiley.com/doi/full/10.1029/2020EA001165
        const float z0h = z0m * 0.1f; // Simply assuming smooth water 
        const float ga = windSpeed * k * k / (logf(1.0f / z0m) * logf(1.0f / z0h)); //Using z = 1 meter high. 
        Qc = Dair * Cpd * ga * (tempGroundC - Tac); //Using ground temp as water temp

        //Using formula from FPGSMLT, energy from latent heat of evaporating water.
        const float Ce = k * k;
        const float Qvs = Qv / (1 + Qv); //Mixing ratio to specific humidity
        Qe = Dair * E0v * Ce * windSpeed * fmax(0.0f, Qvs - qsGPU(0, ps));

        // shortwave
        const float Si = irradiance;
        //Water has a low albedo, especially if its still
        const float A = 0.06f;
        Qs = -(1 - A) * Si;

        // longwave
        const float Qlout = 310.0f; //W/m-2
        const float eac = 9.2e-6f * tempAirK * tempAirK; //Calculate the clear-sky longwave emissivity;
        const float ea = (1 - 0.84f * cloudCover) * eac + 0.84f * cloudCover;
        const float Qlin = ea * oo * 273.15f * 273.15f * 273.15f * 273.15f; //Using 273.15f kelvin as surface temp of ice.
        Ql = Qlin - Qlout;

        //Include current ice if there is any
        if (Qi > 0.0f)
        {
            //Density in g/cm3
            const float densI = 0.91f;
            //Ice thickness
            const float currentDens = Qi * Dair * 0.001f; //Convert to g/cm3
            const float amountIce = currentDens / densI;
            const float iceHeight = amountIce; //Convert to cm3, then to m (height)
            const float ks = 0.3f; //Thermal conductivity of ice in w/m/k: https://online.ucpress.edu/elementa/article/12/1/00086/200266/Snow-thermal-conductivity-and-conductive-flux-in
            Qg = ks * (tempGroundC - 0) / iceHeight;
            Qg *= -1; //Change signature around indicating heating as negative
        }

        //Fraction of water that gets converted to snow
        //We clamp because if its lower then 0, we account for melting inside the melting function
        const float f = fminf(fmaxf((Qw + Qe + Qc + Qs + Ql + Qg) / Qf, 0.0f), 1.0f);

        return f * Qr;
    }
    return 0.0f;
}

__device__ float FPGSFR(const float tempAirK, const float tempGroundC, const float ps, const float Qv, const float Qr, const float Qs, const float Dair, const float windSpeed, const float dt)
{
    //Freezing of water to become snow, only if there is snow
    if (Qr && Qs)
    {
        //Using the same principle as FPGRFR (https://journals.ametsoc.org/view/journals/wefo/37/1/WAF-D-21-0085.1.xml)
        float Qw = 0.0f, Qe = 0.0f, Qg = 0.0f, Qf = 0.0f;

        //How much energy to fully convert 
        //Going from J/kg to W/kg by dividing by dt and W/kg to W/m2 by multiplying by air density.
        Qf = Lf * Qr * Dair / dt;

        //How much energy it would take to go to freezing, assuming the water is still as cold as the air
        Qw = Cvl * (273.15f - tempAirK) * Qr * Dair / dt;

        //No Qc due to the fact that we assume the change is not effected by wind, due to snow blocking wind.

        //Using formula from FPGSMLT, energy from latent heat of evaporating water.
        const float k = 0.41f;
        const float Ce = k * k;
        const float Qvs = Qv / (1 + Qv); //Mixing ratio to specific humidity
        Qe = Dair * E0v * Ce * windSpeed * fmax(0.0f, Qvs - qsGPU(0, ps));
        //Also no Qs and Ql: long/short-wave radiation due to snow blocking sun

        //But we do introduce Qg again

        //Density in g/cm3
        const float densS = 0.11f;
        //Snow thickness
        const float currentDens = Qs * Dair * 0.001f; //Convert to g/cm3
        const float amountSnow = currentDens / densS;
        const float snowHeight = amountSnow; //Convert to cm3, then to m (height)
        const float ks = 0.3f; //Thermal conductivity of snow in w/m/k: https://online.ucpress.edu/elementa/article/12/1/00086/200266/Snow-thermal-conductivity-and-conductive-flux-in
        Qg = ks * (tempGroundC - 0) / snowHeight;
        Qg *= -1; //Change signature around indicating heating as negative


        //Fraction of water that gets converted to snow
        //We clamp because if its lower then 0, we account for melting inside the melting function
        const float f = fminf(fmaxf((Qw + Qe + Qg) / Qf, 0.0f), 1.0f);

        return f * Qr;
    }
    return 0.0f;
}

__device__ float FPGRFR(const float tempAirK, const float tempGroundC, const float ps, const float Qr, const float Qv, const float Qi, const float Qgr, const float Qgs, 
    const float Dair, const float windSpeed, const float3 fallVel)
{
    const float Tac = tempAirK - 273.15f;
    if (!Qgr && !Qgs && Qr && Tac < 0.0f)
    {
        //Using formula from https://journals.ametsoc.org/view/journals/wefo/37/1/WAF-D-21-0085.1.xml
        float Qw = 0.0f, Qc = 0.0f, Qe = 0.0f, Ql = 0.0f, Qs = 0.0f, Qg = 0.0f, Qf = 0.0f;

        //P is mm/h rain rate
        const float densW = 0.99f; // Water density in g/cm3
        const float Rr = Qr * Dair * fallVel.x; // RainRate in kg/kg to kg/m3 to kg/m2/s
        const float P = Rr * 3600.0f / (densW * 1000.0f) * 1000; // Rainrate in m/h to mm/h (Water density to kg/m3)
        const float FFW = sqrtf(powf(P * densW / 3.6f, 2.0f) + powf(0.067f * powf(P, 0.846f) * windSpeed, 2.0f)); //Flux of falling and windblown precipitation
        Qf = Lf * 0.001f * FFW;
        Qw = Cvl * 0.001f * (0 - Tac) * FFW;

        const float k = 0.41f;
        const float z0m = 0.1f; // Aerodynamic Roughness Lengths in mm for winds https://agupubs.onlinelibrary.wiley.com/doi/full/10.1029/2020EA001165
        const float z0h = z0m * 0.1f; // Simply assuming smooth water 
        const float ga = windSpeed * k * k / (logf(1.0f / z0m) * logf(1.0f / z0h)); //Using z = 1 meter high. 
        Qc = Dair * Cpd * ga * (tempGroundC - Tac);
        //Using formula from FPGSMLT
        const float Ce = k * k;
        const float Qvs = Qv / (1 + Qv); //Mixing ratio to specific humidity
        Qe = Dair * E0v * Ce * windSpeed * fmax(0.0f, Qvs - qsGPU(0, ps));

        //TODO: could add long and shortwave radiation.

        const float Td = tempGroundC - 0; //Delta temp
        if (Td > 0.0f) //We don't want to subtract energy (i.e. create more snow)
        {
            //Density in g/cm3
            const float densI = 0.91f;
            //Ice thickness
            const float currentDens = Qi * Dair * 0.001f; //Convert to g/cm3
            const float amountIce = currentDens / densI;
            const float iceHeight = amountIce; //Convert to cm3, then to m (height)
            const float ks = 0.3f; //Thermal conductivity of snow in w/m/k: https://online.ucpress.edu/elementa/article/12/1/00086/200266/Snow-thermal-conductivity-and-conductive-flux-in
            Qg = ks * (tempGroundC - 0) / iceHeight;
            Qg *= -1; //Change signature around indicating heating as negative
        }

        //Fraction of rain that gets converted to ice
        const float f = fminf(fmaxf((Qw + Qc + Qe + Ql + Qs + Qg) / Qf, 0.0f), 1.0f);

        return f * Qr;
    }

    return 0.0f;
}


__global__ void calculateEnvMicroPhysicsGPU(float* _Qv, float* _Qw, float* _Qc, float* _Qr, float* _Qs, float* _Qi, 
    const float dt, const float speed, const float* _temp, const float* _densAir, const float* _pressure, const int* _groundHeight, const float* _groundpressure,
    float* condens, float* depos, float* freeze, 
    const bool graphActive, const int2 minSelectPos, const int2 maxSelectPos, microPhysicsParams& microPhysicsResult)
{
    //----------------------------------------------------------------------------------------------------------------------------------//
    //--------- Formulas and variables from https://research.csiro.au/ccam/wp-content/uploads/sites/520/2024/01/1377337420.pdf ---------//
    //----------------------------------------------------------------------------------------------------------------------------------//

    const int tX = threadIdx.x;
    const int tY = blockIdx.x;
    const int idx = tX + tY * GRIDSIZESKYX;

    __shared__ microPhysicsParams microPhysValuesShared[GRIDSIZESKYX];
    if (graphActive)
    {  
        microPhysValuesShared[tX].reset();
    }

    //Is ground?
    if (tY <= _groundHeight[tX]) return;
    if (tX == GRIDSIZESKYX - 1) return; //Right side is not fully working due to pressure project 


    float PVCON{ 0.0f }; // Condensation/Evaporation rate of cloud water to vapor
    float PVDEP{ 0.0f }; // Deposition/Sublimation rate of cloud ice to vapor.

    float PIMLT{ 0.0f }; // Melting of cloud ice to form cloud water T > 0
    float PIDW{ 0.0f };  // Depositional growth of cloud ice at expense of cloud water
    float PIHOM{ 0.0f }; // Homogeneous freezing of cloud water to form cloud ice
    float PIACR{ 0.0f }; // Accretion of rain by cloud ice; produces snow or graupel depending on the amount of rain
    float PRACI{ 0.0f }; // Accretion of cloud ice by rain; produces snow or graupel depending on the amount of rain.
    float PRAUT{ 0.0f }; // Autoconversion of cloud water to form rain.
    float PRACW{ 0.0f }; // Accretion of cloud water by rain.
    float PREVP{ 0.0f }; // Evaporation of rain.
    float PRACS{ 0.0f }; // Accretion of snow by rain; produces graupel if rain or snow exceeds threshold and T < 0
    float PSACW{ 0.0f }; // Accretion of cloud water by snow; produces snow if T < 0 or rain if T >= 0. Also enhances snow melting for T >= 0
    float PSACR{ 0.0f }; // Accretion of rain by snow. For T < 0, produces graupel if rain or snow exceeds threshold; if now, produces snow. For T >= 0, the accreted water enchances snow melting
    float PSACI{ 0.0f }; // Accretion of cloud ice by snow.
    float PSAUT{ 0.0f }; // Autoconversion (aggregation) of cloud ice to form snow
    float PSFW{ 0.0f };  // Bergeron process (deposition and riming) transfer of cloud water to form snow.
    float PSFI{ 0.0f };  // Transfer rate of cloud ice to snow through growth of Bergeron process embryos.
    float PSDEP{ 0.0f }; // Depositional growth of snow
    float PSSUB{ 0.0f }; // Sublimation of snow
    float PSMLT{ 0.0f }; // Melting of snow to form rain, T > 0
    float PGAUT{ 0.0f }; // Autoconversion (aggregation) of snow to form graupel.
    float PGFR{ 0.0f };  // Probalistic freezing of rain to form graupel.
    float PGACW{ 0.0f }; // Accretion of cloud water by graupel
    float PGACI{ 0.0f }; // Accretion of cloud ice by graupel
    float PGACR{ 0.0f }; // Accretion of rain by graupel
    float PGACS{ 0.0f }; // Accretion of snow by graupel
    float PGDRY{ 0.0f }; // Dry growth of graupel
    float PGSUB{ 0.0f }; // Sublimation of graupel
    float PGMLT{ 0.0f }; // Melting of graupel to form rain, T > 0. (In this regime, PGACW is assumed to be shed off as rain)
    float PGWET{ 0.0f }; // Wet growth of graupel; may involve PGACS and PGACI and must include: PGACW or PGACR, or both. The amount of PGACW which is not able to freeze is shed off as rain.
    float PGACR1{ 0.0f }; // Fallout or growth of hail by wetness

    //Setting data
    const float Qv = _Qv[idx] > 1e-18f ? _Qv[idx] : 0.0f;
    const float Qw = _Qw[idx] > 1e-18f ? _Qw[idx] : 0.0f;
    const float Qc = _Qc[idx] > 1e-18f ? _Qc[idx] : 0.0f;
    const float Qr = _Qr[idx] > 1e-18f ? _Qr[idx] : 0.0f;
    const float Qs = _Qs[idx] > 1e-18f ? _Qs[idx] : 0.0f;
    const float Qi = _Qi[idx] > 1e-18f ? _Qi[idx] : 0.0f;

    const float QwMin = 0.001f; // the minimum cloud water content required before rainmaking begins
    const float QcMin = 0.001f; // the minimum cloud ice content required before snowmaking begins
    const float QiMin = 0.0006f; // the minimum ice content required before snow turns into ice

    float tempK = 0.0f;
    float tempC = 0.0f;

    const float ps = _pressure[idx];
    float3 fallVel = { 0.0f, 0.0f, 0.0f };
    float Dair = _densAir[idx];

    //Setting temperature and falling velocity 
    tempK = float(_temp[idx]) * powf(ps / _groundpressure[tX], Rsd / Cpd);
    tempC = tempK - 273.15f;
    if (Qr > 0.0f || Qs > 0.0f || Qi > 0.0f) fallVel = calculateFallingVelocityGPU(Qr, Qs, Qi, Dair, 3, m_gammaQr, m_gammaQs, m_gammaQi);
    

    const float QWS = wsGPU(tempC, ps); //Maximum water vapor air can hold
    const float QWI = wiGPU(tempC, ps); //Maximum water vapor cold air can hold

    //Production terms, used to sometimes create snow or ice or other stuff (formula 20): https://research.csiro.au/ccam/wp-content/uploads/sites/520/2024/01/1377337420.pdf 
    const float PTerm1 = tempC < 0.0f && Qw + Qc > 0 + 1e-6f ? 1.0f : 0.0f; //TODO: when is there no cloud? (value 1e-6f)
    const float PTerm2 = tempC < 0.0f && Qr < 1e-4f && Qs < 1e-4f ? 1.0f : 0.0f;
    const float PTerm3 = tempC < 0.0f && Qr < 1e-4f ? 1.0f : 0.0f;

    const float slopeR = slopePrecipGPU(Dair, Qr, 0);
    const float slopeS = slopePrecipGPU(Dair, Qs, 1);
    const float slopeI = slopePrecipGPU(Dair, Qi, 2);


    //Setting variables
    PVCON = FPVCON(tempC, ps, Qv, QWS, dt, speed);
    PVDEP = FPVDEP(tempC, ps, Qv, QWI, dt, speed);

    PIMLT = FPIMLT(tempC, Qc);
    PIDW = FPIDW(dt, tempC, Qc, Qw, Dair, ps);
    PIHOM = FPIHOM(tempC, Qw);
    PIACR = FPIACR(Qr, Qc, Dair, slopeR);
    PRACI = FPRACI(Qr, Qc, Dair, slopeR);
    PRAUT = FPRAUT(Qw, QwMin, tempC);
    PRACW = FPRACW(Qr, Qw, Dair, slopeR);
    PREVP = FPREVP(tempC, Qr, Qv, QWS, Dair, ps, slopeR, PTerm1);
    PRACS = FPRACS(Qr, Qs, PTerm2, fallVel, Dair, slopeS, slopeR);
    PSACW = FPSACW(Qs, Qw, Dair, slopeS);
    PSACR = FPSACR(Qr, Qs, fallVel, Dair, slopeR, slopeS);
    PSACI = FPSACI(tempC, Qs, Qc, Dair, slopeS);
    PSAUT = FPSAUT(tempC, Qc, QcMin);
    PSFW = FPSFW(dt, tempC, ps, Qs, Qw, Qc, Qv, QWI, Dair);
    PSFI = FPSFI(dt, tempC, ps, Qs, Qw, Qc, Qv, QWI, Dair);
    PSDEP = FPSDEP(tempC, ps, Qv, QWI, Dair, slopeS);
    PSSUB = FPSSUB(PSDEP);
    PSDEP = fmax(0.0f, PSDEP); //Can't be negative
    PSMLT = FPSMLT(tempC, ps, Qw, Qr, Qs, Qv, Dair, slopeS, PSACW, PSACR, dt * speed);
    PGAUT = FPGAUT(tempC, Qs, QiMin);
    PGFR = FPGFR(tempC, Qr, Dair, slopeR);
    PGACW = FPGACW(Qi, Qw, slopeI, Dair);
    PGACI = FPGACI(Qi, Qc, slopeI, Dair);
    PGACR = FPGACR(Qi, Qr, Dair, fallVel, slopeR, slopeI);
    PGACS = FPGACS(tempC, Qs, Qi, Dair, fallVel, slopeS, slopeI, false);
    PGSUB = FPGSUB(tempC, ps, Qv, QWI, Qi, Dair, slopeI);
    PGMLT = FPGMLT(tempC, ps, Qw, Qr, Qi, Qv, Dair, slopeI, PGACW, PGACR, dt * speed);
    PGDRY = FPGDRY(Qw, Qc, Qr, Qs, PGACW, PGACI, PGACR, PGACS, dt * speed);
    PGWET = FPGWET(tempC, ps, Qc, Qs, Qi, Qv, fallVel, Dair, slopeS, slopeI, PGACI, PGACS, dt * speed);
    PGACR1 = FPGACR1(tempC, Qw, Qc, Qs, Qi, Dair, fallVel, slopeS, slopeI, PGWET, PGACW, PGACI, PGACS, dt * speed);

    //Depending on which value is smaller, we choose or PGDRY or PGWET.
    //In case of PGWET, we use PGACR1 to decide how much rain or ice we gain, else we use PGACR
    //If PGACR1 >= 0, we remove from rain, because some will be frozen
    //IF PGACR1 < 0, we add to rain, since water will be sheded from hail
    float WTerm = 0.0f;
    if (PGDRY > PGWET) WTerm = 1.0f;

    //Limit by speed and add to heat latency.
    if (tempC < 0)
    {
        if (PVCON >= 0) condens[idx] += PVCON = dt * fmin(Qv, speed * PVCON);
        else if (PVCON < 0) condens[idx] += PVCON = dt * fmax(-Qw, speed * PVCON); //Use negative numbers
        if (PVDEP >= 0) depos[idx] += PVDEP = dt * fmin(Qv, speed * PVDEP);
        else if (PVDEP < 0) depos[idx] += PVDEP = dt * fmax(-Qc, speed * PVDEP); //Use negative numbers

        //PIMLT;
        depos[idx] += PIDW = dt * fmin(Qw, speed * PIDW);
        freeze[idx] += PIHOM = dt * fmin(Qw, speed * PIHOM);
        freeze[idx] += PIACR = dt * fmin(Qr, speed * PIACR);
        PRACI = dt * fmin(Qc, speed * PRACI);
        PRAUT = dt * fmin(Qw, speed * PRAUT);
        PRACW = dt * fmin(Qw, speed * PRACW);
        condens[idx] -= PREVP = dt * fmin(Qr, speed * PREVP * (1 - PTerm1));
        PRACS = dt * fmin(Qs, speed * PRACS * (1 - PTerm2));
        freeze[idx] += PSACR = dt * fmin(Qr, speed * PSACR);
        freeze[idx] += PSACW = dt * fmin(Qw, speed * PSACW);
        PSACI = dt * fmin(Qc, speed * PSACI);
        PSAUT = dt * fmin(Qc, speed * PSAUT);
        freeze[idx] += PSFW = dt * fmin(Qw, speed * PSFW);
        PSFI = dt * fmin(Qc, speed * PSFI);
        depos[idx] += PSDEP = dt * fmin(Qv, speed * PSDEP * (PTerm1));
        depos[idx] -= PSSUB = dt * fmin(Qs, speed * PSSUB * (1 - PTerm1));
        //PSMLT;

        PGAUT = dt * fmin(Qs, speed * PGAUT);
        freeze[idx] += PGFR = dt * fmin(Qr, speed * PGFR);
        //PGACW;
        PGACI = dt * fmin(Qc, speed * PGACI);
        freeze[idx] += PGACR = dt * fmin(Qr, speed * PGACR * (1 - WTerm));
        PGACS = dt * fmin(Qs, speed * PGACS);
        depos[idx] -= PGSUB = dt * fmin(Qi, speed * PGSUB * (1 - PTerm1));
        //PGMLT;
        if (WTerm == 0.0f)
        {
            //Values that were used for PGDRY are already limited, and added to the correct heat latency
            //PGDRY is only used as addition, so no use of limiting
            //PGDRY;
        }
        else if (WTerm == 1.0f)
        {
            //Values that were used for PGWET are already limited, and added to the correct heat latency
            //Altough PGWET is calculated a bit different, it still is okay, since the other values are accounted for in PGACR1
            //PGWET;
            //PGACR1 we want to limit on rain or ice depending on if its positive or negative.
            //Although we don't remove PGACR1 from hail, if value is negative, we add to rain (thus take from ice)
            if (PGACR1 >= 0) freeze[idx] += PGACR1 = dt * fmin(Qr, speed * PGACR1);
            else if (PGACR1 < 0) freeze[idx] += PGACR1 = dt * fmax(-Qi, speed * PGACR1); //Use negative numbers
        }
    }
    else
    {
        if (PVCON >= 0) condens[idx] += PVCON = dt * fmin(Qv, speed * PVCON);
        else if (PVCON < 0) condens[idx] += PVCON = dt * fmax(-Qw, speed * PVCON); //Use negative numbers
        if (PVDEP >= 0) depos[idx] += PVDEP = dt * fmin(Qv, speed * PVDEP);
        else if (PVDEP < 0) depos[idx] += PVDEP = dt * fmax(-Qc, speed * PVDEP); //Use negative numbers

        freeze[idx] -= PIMLT = dt * fmin(Qc, speed * PIMLT);
        //PIDW;
        //PIHOM;
        PIACR = 0.0f;
        //PRACI;
        PRAUT = dt * fmin(Qw, speed * PRAUT);
        PRACW = dt * fmin(Qw, speed * PRACW);
        condens[idx] -= PREVP = dt * fmin(Qr, speed * PREVP * (1 - PTerm1));
        //PRACS;
        //PSACR;
        PSACW = dt * fmin(Qw, speed * PSACW);
        //PSACI;
        //PSAUT;
        //PSFW;
        //PSFI;
        //PSDEP;
        //PSSUB;
        freeze[idx] -= PSMLT = dt * fmin(Qs, speed * PSMLT);

        //PGAUT;
        //PGFR;
        freeze[idx] += PGACW = dt * fmin(Qw, speed * PGACW);
        //PGACI;
        //PGACR;
        PGACS = dt * fmin(Qs, speed * PGACS);
        //PGSUB;
        freeze[idx] -= PGMLT = dt * fmin(Qi, speed * PGMLT);
        if (WTerm == 0.0f)
        {
            //Values that were used for PGDRY are already limited, and added to the correct heat latency
            //PGDRY is only used as addition, so no use of limiting
            //PGDRY;
        }
        else if (WTerm == 1.0f)
        {
            //Values that were used for PGWET are already limited, and added to the correct heat latency
            //Altough PGWET is calculated a bit different, it still is okay, since the other values are accounted for in PGACR1
            //PGWET;
            //PGACR1 we want to limit on rain or ice depending on if its positive or negative.
            //if (PGACR1 >= 0) PGACR1;
            //else if (PGACR1 < 0) PGACR1;
        }
    }

    // Var       Add       Sub       Description
    //---------------------------------------------
    //PVCON   | +Qv, Qw | -Qv, Qw | (Depending on positive or negative)
    //PVDEP   | +Qv, Qc | -Qv, Qc | (Depending on positive or negative)
    //PIMLT:  | +Qw     | -Qc     | (if T >= 0)
    //PIDW:   | +Qc     | -Qw     | (if T < 0)
    //PIHOM:  | +Qc     | -Qw     | (if T < -40)
    //PIACR:  | +Qs, Qi | -Qr     | (Depending on PTerm3)
    //PRACI:  | +Qs, Qi | -Qc     | (Depending on PTerm3)
    //PRAUT:  | +Qr     | -Qw     |
    //PRACW:  | +Qr     | -Qw     |
    //PREVP:  | +Qv     | -Qr     |
    //PRACS:  | +Qi     | -Qs     | (Depending on PTerm2, and T < 0)
    //PSACW:  | +Qs, Qr | -Qr, Qw | (+Qs if T < 0, +Qr if T >= 0)
    //PSACR:  | +Qs, Qi | -Qr     | (Depending on PTerm2)
    //PSACI:  | +Qs     | -Qc     |
    //PSAUT:  | +Qs     | -Qc     |
    //PSFW:   | +Qs     | -Qw     |
    //PSFI:   | +Qs     | -Qc     |
    //PSDEP:  | +Qs     | -Qv     |
    //PSSUB:  | +Qv     | -Qs     |
    //PSMLT:  | +Qr     | -Qs     | (if T >= 0)
    //PGAUT:  | +Qi     | -Qs     |
    //PGFR:   | +Qi     | -Qr     |
    //PGACW:  | +Qi, Qr | -Qw     | (Included in PGWET or PGDRY)
    //PGACI:  | +Qi     | -Qc     | (Included in PGWET or PGDRY)
    //PGACR:  | +Qi     | -Qr     | (Included in PGWET or PGDRY)
    //PGACS:  | +Qi     | -Qs     | (Included in PGWET or PGDRY)
    //PGDRY:  | +Qi     |         | (Depending on dry or wet)
    //PGSUB:  | +Qv     | -Qi     |
    //PGMLT:  | +Qr     | -Qi     | (if T >= 0)
    //PGWET:  | +Qi     |         | (Is included in PGACR1)
    //PGACR1: | +Qr, Qi | -Qr, Qi | (If PGWET and depending on positive or negative)

    if (tempC < 0)
    {
        _Qv[idx] += PSSUB * (1 - PTerm1) + PGSUB * (1 - PTerm1) +
            PREVP * (1 - PTerm1) - PSDEP * (PTerm1)-
            PVCON - PVDEP;

        _Qw[idx] += PVCON - PSACW - PSFW - PRAUT - PRACW - PIDW - PIHOM;
        _Qc[idx] += PVDEP + PIDW + PIHOM - PSAUT - PSACI - PRACI - PSFI - PGACI;

        _Qr[idx] += PRAUT + PRACW - PIACR - PSACR -
            PGACR * (1 - WTerm) - PGACR1 * (WTerm)- //Dependent on wet or dry growth
            PGFR - PREVP * (1 - PTerm1);

        _Qs[idx] += PSAUT + PSACI + PSACW + PSFW + PSFI +
            PRACI * (PTerm3)+PIACR * (PTerm3)-PGACS - PGAUT -
            PRACS * (1 - PTerm2) + PSACR * (PTerm2)-
            PSSUB * (1 - PTerm1) + PSDEP * (PTerm1);

        _Qi[idx] += PGAUT + PGFR +
            PGDRY * (1 - WTerm) + PGWET * (WTerm)+ //Wet or dry growth
            PSACR * (1 - PTerm2) + PRACS * (1 - PTerm2) +
            PRACI * (1 - PTerm3) + PIACR * (1 - PTerm3) - PGSUB * (1 - PTerm1);
    }
    else
    {
        _Qv[idx] += PREVP * (1 - PTerm1) -
            PVCON - PVDEP;
        _Qw[idx] += PVCON + PIMLT - PRAUT - PRACW - PGACW - PSACW;

        _Qc[idx] += PVDEP - PIMLT;

        _Qr[idx] += PRAUT + PRACW + PSACW + PGACW +
            PGMLT + PSMLT - PREVP * (1 - PTerm1);


        _Qs[idx] += -PSMLT - PGACS;
        _Qi[idx] += -PGMLT + PGACS;
    }
    __syncthreads();

    //Make data ready for return but only inside the set-region
    if (graphActive)
    {
        bool insideRegion = (tX >= minSelectPos.x && tX <= maxSelectPos.x &&
            tY >= minSelectPos.y && tY <= maxSelectPos.y);

        int LIdx = tX;

        if (insideRegion)
        {
            if (minSelectPos.x >= 0)
            {
                //Offset the index to the left, so we can correctly grab the most left one easily.
                LIdx = tX - minSelectPos.x;
            }

            float PVVAP = 0.0f;
            float PVSUB = 0.0f;
            if (PVCON < 0.0f)
            {
                PVVAP = -PVCON;
                PVCON = 0.0f;
            }
            if (PVDEP < 0.0f)
            {
                PVSUB = -PVDEP;
                PVDEP = 0.0f;
            }
            PGWET *= WTerm;
            PGDRY *= (1 - WTerm);

            microPhysValuesShared[LIdx].init(PVCON, PVDEP, PIMLT, PIDW, PIHOM, PIACR, PRACI, PRAUT,
                PRACW, PREVP, PRACS, PSACW, PSACR, PSACI, PSAUT, PSFW,
                PSFI, PSDEP, PSSUB, PSMLT, PGAUT, PGFR, PGACW, PGACI,
                PGACR, PGDRY, PGACS, PGSUB, PGMLT, PGWET, PGACR1, PVVAP, PVSUB);
        }
        __syncthreads();


        //Basically, we grab half of the block, add all the values on the other side and repeat the process.
        for (int i = GRIDSIZESKYX / 2; i > 0; i >>= 1)
        {
            if (insideRegion && LIdx < i)
            {
                microPhysValuesShared[LIdx] = microPhysValuesShared[LIdx] + microPhysValuesShared[LIdx + i];
            }
            __syncthreads();
        }


        //Using atomicAdd(), we can safely add all block values to a singular value
        if (insideRegion && LIdx == 0)
        {
            microPhysValuesShared->atomicAddValues(microPhysicsResult, microPhysValuesShared[0]);
        }
        __syncthreads();
    }
}

__global__ void calculateGroundMicroPhysicsGPU(float* _Qrs, float* _Qv, float* _Qgr, float* _Qgs, float* _Qgi,
    const float dt, const float speed, float* _tempGround, const float* _tempAir, const float* _densAir, const float* _pressure, const float* groundPressure,
    float* _time, const float irradiance, const float* _windSpeedX, const float* _cloudCover,
    const int* _groundHeight)
{
    const int tX = threadIdx.x;

    const int tY = _groundHeight[tX]; //Y to use index on environment variables
    const int idx = tX + (tY + 1) * GRIDSIZESKYX;


    float PGREVP{ 0.0f }; // Evaporation of (rain)water.
    //float PSDEP{ 0.0f }; // TODO: Depositional growth of snow 
    //float PSSUB{ 0.0f }; // TODO: Sublimation of snow
    //float PIDEP{ 0.0f }; // TODO: Depositional growth of ice 
    //float PISUB{ 0.0f }; // TODO: Sublimation of ice
    float PGSMLT{ 0.0f }; // Melting of snow to form water
    float PGGMLT{ 0.0f };// Melting of ice to form water
    float PGGFR{ 0.0f };  // Freezing of (rain)water to form ice
    float PGSFR{ 0.0f }; // Freezing of (rain)water to form snow

    float PGFLW{ 0.0f };  // Water flowing through the ground (through holes in ground) (From Qr to Qrs)
    float PGDEVP{ 0.0f };  // Evaporation of dry ground


    //Setting data
    const float Qrs = _Qrs[tX] > 1e-18f ? _Qrs[tX] : 0.0f;
    const float Qgr = _Qgr[tX] > 1e-18f ? _Qgr[tX] : 0.0f;
    const float Qgs = _Qgs[tX] > 1e-18f ? _Qgs[tX] : 0.0f;
    const float Qgi = _Qgi[tX] > 1e-18f ? _Qgi[tX] : 0.0f;
    const float Qv = _Qv[idx] > 1e-18f ? _Qv[idx] : 0.0f;

    float condens = 0.0f;
    float freeze = 0.0f;
    float depos = 0.0f;

    const float tempGroundC = _tempGround[tX] - 273.15f;
    float tempAirK = 0.0f;
    const float ps = _pressure[idx];
    const float windSpeed = _windSpeedX[idx];
    const float cloudCover = _cloudCover[tX];
    float Dair = _densAir[idx];
    float* time = &_time[tX];

    //Calculating T from potential temp
    tempAirK = float(_tempAir[idx]) * powf(ps / groundPressure[tX], Rsd / Cpd);
    

    //printf("tempGroundC[(%i, %i), %i (Y + 1)]: %f\n", tX, tY, idx, tempGroundC);
    //printf("TempK[(%i, %i), %i (Y + 1)]: %f\n", tX, tY, idx, tempAirK);

    const float QWS = wsGPU(tempGroundC, ps); //Maximum water vapor air can hold
    const float QWI = wiGPU(tempGroundC, ps); //Maximum water vapor cold air can hold

    const float slopeS = slopePrecipGPU(Dair, Qgs, 1);
    const float slopeI = slopePrecipGPU(Dair, Qgi, 2);

    //Setting variables
    PGREVP = FPGREVP(tempAirK, tempGroundC, ps, Qv, Qgr, irradiance, Dair, windSpeed);
    PGSMLT = FPGSMLT(tempAirK, tempGroundC, ps, Qgs, Qv, Qgr, irradiance, cloudCover, Dair, windSpeed, dt);
    PGGMLT = FPGGMLT(tempGroundC, Qgi, Dair, slopeI);
    PGGFR = FPGGFR(tempAirK, tempGroundC, ps, Qgr, Qv, Qgi, Dair, irradiance, cloudCover, windSpeed, dt);
    PGSFR = FPGSFR(tempAirK, tempGroundC, ps, Qv, Qgr, Qgs, Dair, windSpeed, dt);

    PGFLW = FPGFLW(Qgr);
    PGDEVP = FPGDEVP(time, Qgr, Qgs, Qgi, Qrs);

    condens -= PGREVP = dt * fmin(Qgr, speed * PGREVP);
    freeze -= PGSMLT = dt * fmin(Qgs, speed * PGSMLT);
    freeze -= PGGMLT = dt * fmin(Qgi, speed * PGGMLT);
    freeze += PGGFR = dt * fmin(Qgr, speed * PGGFR);
    freeze += PGSFR = dt * fmin(Qgr, speed * PGSFR);

    PGFLW = dt * fmin(Qgr, speed * PGFLW);
    condens -= PGDEVP = dt * fmin(Qrs, speed * PGDEVP);

    // Var       Add       Sub       Description
    //---------------------------------------------
    //PGREVP: | +Qv     | -Qr     | 
    //PGSMLT: | +Qr     | -Qs     | 
    //PGGMLT: | +Qr     | -Qi     | 
    //PGGFR:  | +Qi     | -Qr     | 
    //PGSFR:  | +Qs     | -Qr     | 
    //PGFLW:  | +Qrs    | -Qr     | 
    //PGDEVP: | +Qv     | -Qrs    | 

    _Qrs[tX] += PGFLW - PGDEVP;
    _Qv[idx] += PGREVP + PGDEVP;
    _Qgr[tX] += PGSMLT + PGGMLT - PGREVP - PGGFR - PGSFR - PGFLW;
    _Qgs[tX] += PGSFR - PGSMLT;
    _Qgi[tX] += PGGFR - PGGMLT;


    //Adding ground heat
    {
        //const float Mair = 0.02896f; //In kg/mol
        //const float Mwater = 0.01802f; //In kg/mol
        //const float XV = (Qv / Mwater) / ((Qv / Mwater) + (1 - Qv) / Mair);
        //const float Mth = XV * Mwater + (1 - XV) * Mair;
        //const float Yair = 1.4f, yV = 1.33f;
        //const float YV = XV * (Mwater / Mth); //Mass fraction of vapor
        //const float yth = YV * yV + (1 - YV) * Yair; //Weighted average

        //TODO: get specifics on this
        // Get specific gas constant
       // const float cpth = R / (Mth * (yth - 1)); //Not multiplied by yth due to not needing pottemp but normal temp
        const float cp_soil = 1500.0f; // J/(kg·K)
        const float rho_soil = 1500.0f; // kg/m3 soil density
        const float depth_layer = 0.1f; // m depth of ground layer you're modeling
        const float mass_soil = rho_soil * depth_layer; // kg/m2 of soil


        float sumPhaseheat = 0.0f;

        sumPhaseheat += LwaterGPU(tempGroundC) / (mass_soil * cp_soil)*condens;
        sumPhaseheat += LiceGPU(tempGroundC) / (mass_soil * cp_soil) * depos;
        sumPhaseheat += Lf / (mass_soil * cp_soil) * freeze;

        _tempGround[tX] += sumPhaseheat;
    }
}


__global__ void calculatePrecipHittingGroundMicroPhysicsGPU(float* _Qv, float* _Qr, float* _Qs, float* _Qi, float* _Qgr, float* _Qgs, float* _Qgi,
    const float dt, const float speed, float* _tempGround, const float* _tempAir, const float* _densAir, const float* _pressure, const float* groundPressure,
    const float* _windSpeedX, const int* _groundHeight)
{
    const int tX = threadIdx.x;

    const int tY = _groundHeight[tX]; //Y to use index on environment variables
    const int idx = tX + (tY + 1) * GRIDSIZESKYX;


    float PGRFR{ 0.0f }; // Freezing rain hitting the ground forming ice


    //Setting data
    const float Qgr = _Qgr[tX] > 1e-18f ? _Qgr[tX] : 0.0f;
    const float Qgs = _Qgs[tX] > 1e-18f ? _Qgs[tX] : 0.0f;
    const float Qgi = _Qgi[tX] > 1e-18f ? _Qgi[tX] : 0.0f;
    const float Qv = _Qv[idx] > 1e-18f ? _Qv[idx] : 0.0f;
    const float Qr = _Qr[idx] > 1e-18f ? _Qr[idx] : 0.0f;
    const float Qs = _Qs[idx] > 1e-18f ? _Qs[idx] : 0.0f;
    const float Qi = _Qi[idx] > 1e-18f ? _Qi[idx] : 0.0f;

    float freeze = 0.0f;

    const float tempGroundC = _tempGround[tX] - 273.15f;
    float tempAirK = _tempAir[idx];
    const float ps = _pressure[idx];
    const float windSpeed = _windSpeedX[idx];
    float3 fallVel = { 0.0f, 0.0f, 0.0f };
    float Dair = _densAir[idx];

    //Setting falling velocity and calculate air temp from pottemp  
    tempAirK = _tempAir[idx] * powf(ps / groundPressure[tX], Rsd / Cpd);
    if (Qgr > 0.0f || Qgs > 0.0f || Qgi > 0.0f)
    {
        fallVel = calculateFallingVelocityGPU(Qgr, Qgs, Qgi, Dair, 3, m_gammaQr, m_gammaQs, m_gammaQi);
    }
    


    float GR{ 0.0f };
    float GS{ 0.0f };
    float GI{ 0.0f };

    GR = Qr;
    GS = Qs;
    GI = Qi;

    //Limit
    GR = dt * fmin(GR * speed, Qr);
    GS = dt * fmin(GS * speed, Qs);
    GI = dt * fmin(GI * speed, Qi);

    PGRFR = FPGRFR(tempAirK, tempGroundC, ps, Qr, Qv, Qi, Qgr, Qgs, Dair, windSpeed, fallVel);

    if (tempAirK < 273.15f)
    {
        freeze += PGRFR = dt * fmin(Qr, speed * PGRFR);
    }

    // Var       Add       Sub       Description
    //---------------------------------------------
    //PGRFR   | +Qgi    | -Qgr   | Subtract from ground rain


    _Qr[idx] += -GR;
    _Qs[idx] += -GS;
    _Qi[idx] += -GI;
    _Qgr[tX] += GR - PGRFR;
    _Qgs[tX] += GS;
    _Qgi[tX] += GI + PGRFR;

    //Adding ground heat
    {
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
        sumPhaseheat += Lf / cpth * freeze;

        _tempGround[tX] += sumPhaseheat;
    }
}
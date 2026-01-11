//#include "meteoconstants.cuh"
//#include "meteoformulas.cuh"
//
//#include <CUDA/cmath>
//#include <cuda_runtime.h>
//#include <cuda.h>
//
//__device__ float es(const float T)
//{
//	//return (0.61094f) * expf((17.67f * T) / (T + 243.5f)); //Worse accuraccy of around 0.2%
//	return 0.61078f * expf((17.27f * T) / (T + 237.3f));
//}
//
//__device__ float ei(const float T)
//{
//	return 0.61078f * expf((21.875f * T) / (T + 265.5f));
//}
//
//__device__ float ws(const float T, const float P)
//{
//	float Es = es(T) * 10.0f; // kPa to hPa
//	return  (ConstantsGPU::E * Es) / (P - Es);
//}
//
//__device__ float wi(const float T, const float P)
//{
//	float Es = ei(T) * 10.0f; // kPa to hPa
//	return (ConstantsGPU::E * Es) / (P - Es);
//}
//
//__device__ float qs(const float T, const float P)
//{
//	float Es = ei(T) * 10.0f; // kPa to hPa
//	return (ConstantsGPU::E * Es) / (P - (1 - ConstantsGPU::E) * Es);
//}
//
//__device__ float qv(const float T, const float P)
//{
//	float Rs = ws(T, P);
//	return Rs / (1 + Rs);
//}
//
//__device__ float slopePrecip(const float D, const float Qj, const int precipType)
//{
//    // constants
//    const float _e = 0.25f;  // E
//
//    switch (precipType)
//    {
//    case 0:  // Rain
//    {
//        // How many of this particle are in this region in cm-4
//        const float N0R = 8e-2f;
//        // Densities in g/cm3
//        const float densW = 0.99f;
//        // Check for division by 0
//        const float numerator = ConstantsGPU::PI * densW * N0R;
//        const float denominator = fmaxf(D * Qj * 0.001f, 1e-14f);  // Convert kg/kg to g/cm3
//        return (numerator == 0 || denominator == 0) ? 0.0f : powf((numerator / denominator), _e);
//        break;
//    }
//    case 1:  // Snow
//    {
//        const float N0S = 3e-2f;
//        const float densS = 0.11f;
//
//        const float numerator = ConstantsGPU::PI * densS * N0S;
//        const float denominator = fmaxf(D * Qj * 0.001f, 1e-14f);  // Convert kg/kg to g/cm3
//        return (numerator == 0 || denominator == 0) ? 0.0f : powf((numerator / denominator), _e);
//        break;
//    }
//    case 2:  // Ice
//    {
//        const float N0I = 4e-4f;
//        const float densI = 0.91f;
//
//        const float numerator = ConstantsGPU::PI * densI * N0I;
//        const float denominator = fmaxf(D * Qj * 0.001f, 1e-14f);  // Convert kg/kg to g/cm3
//        return (numerator == 0 || denominator == 0) ? 0.0f : powf((numerator / denominator), _e);
//        break;
//    }
//    default:
//        break;
//    }
//    return 0.0f;
//}
//
//__device__ float gammaGPU(const float x)
//{
//    return tgammaf(x);
//}
//
//
//__device__ float potentialTempGPU(const float T, const float Pk, const float Pt)
//{
//	return (T + 273.15f) * powf((Pt / Pk), ConstantsGPU::Rsd / ConstantsGPU::Cpd) - 273.15f;
//}
//
//__device__ float MLR(const float T, const float P)
//{
//    float TKelvin = T + 273.15f;
//    float Ws = ws(T, P);
//    float Tw = ((ConstantsGPU::Rsd * TKelvin + ConstantsGPU::Hv * Ws) /
//        (ConstantsGPU::Cpd + (ConstantsGPU::Hv * ConstantsGPU::Hv * Ws * ConstantsGPU::E / (ConstantsGPU::Rsd * TKelvin * TKelvin))));
//    return Tw / P;
//}

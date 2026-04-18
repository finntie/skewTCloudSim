#pragma once

#include <glm/glm.hpp>
#include <vector>

class meteoformulas
{
public:

	/// <summary>Get (Saturated) Water Vapor </summary>
	/// https://www.weather.gov/media/epz/wxcalc/vaporPressure.pdf
	/// https://en.wikipedia.org/wiki/Vapour_pressure_of_water (Better)
	/// <param name="T">(Dewpoint) Temperature in C</param>
	/// <returns>(saturation) vapor pressure in kPa (= 10 hPa)</returns>
	static float es(const float T);

	/// <summary>Get ice Vapor </summary>
    /// https://en.wikipedia.org/wiki/Tetens_equation
    /// <param name="T">(Dewpoint) Temperature in C</param>
    /// <returns> ice vapor pressure in kPa (= 10 hPa)</returns>
    static float ei(const float T);

	/// <summary>Calculates the (saturated) mixing ratio (also called r(s)).
	/// Filling in dewpoint for T will result in the mixing ratio,
	/// Filling in temperature for T will result in the saturated mixing ratio.</summary>
	/// https://vortex.plymouth.edu/~stmiller/stmiller_content/Publications/AtmosRH_Equations_Rev.pdf
	/// https://journals.ametsoc.org/view/journals/mwre/108/7/1520-0493_1980_108_1046_tcoept_2_0_co_2.xml
	/// <param name="T">(Dewpoint) Temperature in C</param>
	/// <param name="P">Pressure in hPa</param>
	/// <returns>(saturated) mixing ratio in (kg(vapor)/kg(air))</returns>
	static float ws(const float T, const float P);

	/// <summary>Mostly the same as the saturated mixing ratio, but now using ice</summary>
	/// <param name="T">Temperature in C</param>
	/// <param name="P">Pressure in hPa</param>
	/// <returns>mixing ratio in (kg(ice)/kg(air))</returns>
	static float wi(const float T, const float P);

	/// <summary>Calculate the specific humidity of air (same as mixing ratio but using moist air instead of dry)</summary>
        /// <param name="T">Temperature in C</param>
        /// <param name="P">Pressure in hPa</param>
	/// <returns>Specific humidity in kg/kg</returns>
	static float qs(const float T, const float P);


	/// <summary>Calculates the mass fraction of water vapor </summary>
	/// Page 80 chapter 3.5.1 https://www.gnss-x.ac.cn/docs/Atmospheric%20Science%20An%20Introductory%20Survey%20(John%20M.%20Wallace,%20Peter%20V.%20Hobbs)%20(z-lib.org).pdf 
	/// <param name="T">Temperature in °C</param>
	/// <param name="P">Pressure in hPa</param>
	/// <returns>% / 100 of water vapor in air</returns>
	static float qv(const float T, const float P);
	
	/// <summary>The concentration of available ice crystals for the nucleation process</summary>
	/// <param name="T">Temperature in °C</param>
	/// <returns>Value in m-3</returns>
	static float Ni(const float T);

	/// <summary>Estimates the density of water in Kg/m3 </summary> https://www.omnicalculator.com/physics/water-density
	/// <param name="T">Temperatuer in celcius</param>
	static float pwater(const float T);

	/// <summary>Specific latent heat for condensation at different temperatures</summary>
	/// https://en.wikipedia.org/wiki/Latent_heat#cite_note-RYfit-26
	/// <param name="T">Temperature in celcius</param>
	/// <returns>Latent Heat in J/kg</returns>
	static float Lwater(const float T);
	
	/// <summary>Specific latent heat for Deposition at different temperatures</summary>
	/// https://en.wikipedia.org/wiki/Latent_heat#cite_note-RYfit-26
	/// <param name="T">Temperature in celcius</param>
	/// <returns>Latent Heat in J/kg</returns>
	static float Lice(const float T);

	/// <summary>Calculates the specific gas constant for moist air</summary>
	/// <param name="T">Temperature in Kelvin</param>
	/// <param name="P">Pressure in Pa</param>
	/// <returns>gas constant in J/kg</returns>
	static float Rm(const float T, const float P);
	
	/// <summary>Calculates the specific heat capacity at constant pressure for moist air</summary>
	/// <param name="T">Temperature in Kelvin</param>
	/// <param name="P">Pressure in Pa</param>
	/// <returns>heat capacity in J/kg</returns>
	static float Cpm(const float T, const float P);

	/// <summary>Calculates the diffusity of water vapor in air </summary>
	/// <param name="T">Temp in celcius</param>
	/// <param name="P">Pressure in hPa</param>
	/// <returns>Diffusity in m2/s-1</returns>
	static float DQVair(const float T, const float P);

	/// <summary>Calculates the viscosity of air using Sutherland's law</summary>
	/// https://doc.comsol.com/5.5/doc/com.comsol.help.cfd/cfd_ug_fluidflow_high_mach.08.27.html
	/// <param name="T">Temp in celcius</param>
	/// <returns>Viscosity in m2/s-1</returns>
	static float ViscAir(const float T);

	/// <summary>Calculates the slope parameter of chosen precip</summary>
	/// <param name="pAir">Density of air</param>
	/// <param name="Qj">Mixing ratio at specific point</param>
	/// <param name="precipType">Type of precip: 0 = Rain, 1 = Snow, 2 = ice</param>
	/// <returns>Slope parameter in g/cm3</returns>
	static float slopePrecip(const float pAir, const float Qj, const int precipType);

	/// <summary>Calculates the gamma function of x from y</summary>
	/// <param name="x">Value for gamma</param>
	/// <param name="y">If set, will use upper incomplete gamma function, i.e. from y until infinity</param>
	/// <returns>Result</returns>
	static float gamma(const float x, const float y = 0.0f);

	/// <summary>The rate constant for vapor deposition on hexagonal crystals (Rate of evaporation causing ice growth?)</summary>
	/// Formula from https://research.csiro.au/ccam/wp-content/uploads/sites/520/2024/01/1377337418.pdf (A.2)
	/// <param name="T">Temperature in C</param>
	/// <param name="P">Pressure in hPa </param>
	static float cvd(const float T, const float P, const float densityAir);

	/// <summary> Calculates the temperature of the (saturated) mixing ratio for every pressure</summary>
	/// <param name="Ws">(Saturated) Mixing ratio</param>
	/// <param name="pressures">Array of all pressures to calculate</param>
	/// <param name="output">Array with the same size as pressures that will be filled by the function</param>
    /// <param name="size">Size of the array</param>
	static void getTempAtWs(const float Ws, const float* pressures, float* output, const size_t size);

	/// <summary>The potential temperature of a parcel of fluid at pressure P 
	/// is the temperature that the parcel would attain if adiabatically brought to a standard reference pressure P0,
	/// usually 1,000 hPa(1,000 mb). 
	/// The difference between this and the dry lapse rate is that for the dry lapse rate we cool the temperature down by moving it upwards,
	/// while for this we move the temperature back down to the reference temperature.(Thus in essential the same formula, but we use it differently)</summary>
	/// <param name="T">Temperature in °C</param>
	/// <param name="PKnown">Known Pressure at Temp in hPa</param>
	/// <param name="PTarget">Target Pressure at height in hPa</param>
	/// <returns>Potential temp in °C</returns>
	static float potentialTemp(const float T, const float PKnown, const float PTarget);

	/// <summary>The dry adiabatic temperature is the temperature a dry parcel decreases with adiabatically,
	/// you could assume its 9.8 in a hydrostatic balanced atmosphere, but we don't assume that here, thus we use poissons' asq equation
	/// The difference between this and the dry lapse rate is that for the dry lapse rate we cool the temperature down by moving it upwards,
	/// while for this we move the temperature back down to the reference temperature. (Thus in essential the same formula, but we use it differently)</summary>
    /// <param name="T">Temperature in °C</param>
    /// <param name="PKnown">Known Pressure at Temp in hPa</param>
    /// <param name="PTarget">Target Pressure at height in hPa</param>
    /// <returns>Temp in °C</returns>
    static float dryLapseTemp(const float T, const float PKnown, const float PTarget);

	/// <summary>The potential temperature of a parcel of fluid at pressure P 
	/// is the temperature that the parcel would attain if adiabatically brought to a standard reference pressure P0,
	/// usually 1,000 hPa(1,000 mb).
	/// https://en.wikipedia.org/wiki/Potential_temperature
    /// while for this we move the temperature back down to the reference temperature.</summary>
	/// <param name="temps">Array of all Temperatures in °C</param>
	/// <param name="PTarget">Target Pressure in hPa</param>
    /// <param name="PsKnown">Array of all pressures at all temps to calculate</param>
    /// <param name="output">Array with the same size as pressures that will be filled by the function</param>
    /// <param name="size">Size of the array</param>
    static void getPotentialTempArray(const float* temps,
                                      const float PTarget,
                                      const float* PsKnown,
                                      float* output,
                                      const size_t size);

	/// <summary>The dry adiabatic is the temperature which the not saturated parcel decreases with in height. 
    /// https://en.wikipedia.org/wiki/Potential_temperature</summary>
    /// <param name="T0">Reference Temperature in °C</param>
    /// <param name="P0">Reference Pressure in hPa</param>
    /// <param name="pressures">Array of all pressures to calculate</param>
    /// <param name="output">Array with the same size as pressures that will be filled by the function</param>
    /// <param name="size">Size of the array</param>
	static void getDryAdiabatic(const float T0, const float P0, const float* pressures, float* output, const size_t size);

	/// <summary>Calculates the moist lapse rate</summary>
    /// <param name="T">Temperature in °C</param>
    /// <param name="P">Pressure in hPa</param>
    /// <returns>Lapse rate in dK/dP (delta Kelving per delta hectoPascal)</returns>
    static float MLR(const float T, const float P);

	/// <summary>Calculate moist lapse rate for given temp at given pressures</summary>
    /// <param name="T0">Reference Temperature in °C</param>
    /// <param name="P0">Reference Pressure in hPa</param>
    /// <param name="pressures">Array of all pressures to calculate</param>
    /// <param name="output">Array with the same size as pressures that will be filled by the function</param>
    /// <param name="size">Size of the array</param>
    static void getMoistTemp(const float T0,
                                 const float P0,
                                 const float* pressures,
                                 float* output,
                                 const size_t size,
                                 int& offset);

	/// <summary>Calculate height using the hypsometric equation under the assumption of a standard atmosphere</summary>
	/// <param name="T0">Reference Temperature in °C</param>
	/// <param name="P">Pressure in hPa</param>
	/// <param name="P0">Reference pressure, (normal is 1000)</param>
	/// <returns>Height in m</returns>
	static float getStandardHeightAtPressure(const float T0, const float P, const float P0 = 1000.0f);

	/// <summary>Calculate pressure using formula above from height 
	/// https://www.mide.com/air-pressure-at-altitude-calculator</summary>
	/// <param name="T0">Reference Temperature in °C</param>
	/// <param name="h">Height in meter</param>
	/// <param name="H0">Reference height in meter</param>
	/// <param name="P0">Reference pressure (default is 1000)</param>
	/// <returns>Pressure in hPa</returns>
	static float getStandardPressureAtHeight(const float T0, const float h, const float H0 = 0.0f, const float P0 = 1000.0f);

	/// <summary>Returns logged value of given value</summary>
	/// <param name="value">Value should range between 1000 and 100 hPa</param>
	/// <param name="Pressure">Is value given in hPa or meter?</param>
	/// <param name="H0">Extra height added to final</param>
	/// <returns>Height in meters from standard pressure</returns>
	static float getLogPHeight(const float value, const bool Pressure, const float H0 = 0.0f);

	/// <summary>Calculates CCL with given parameters</summary>
	/// <param name="P0">Initial pressure in hPa</param>
	/// <param name="D0">Initial dew point in °C</param>
    /// <param name="pressures">Array of all pressures to calculate</param>
    /// <param name="temperatures">Array of all temperatures to calculate</param>
    /// <param name="size">Size of the array</param>
	/// <returns>.x = Temp at the CCL, <para>
	/// .y = pressure at CCL, </para> <para>
	/// .z = Potential Temp at the P0 </para></returns>
	static glm::vec3 getCCL(const float P0, const float D0, const float* pressures, const float* temperatures, const size_t size);

	/// <summary>Calculates LCL with given parameters</summary>
	/// <param name="T0">Initial temperature in °C</param>
	/// <param name="P0">Initial pressure in hPa</param>
	/// <param name="Z0">Initial height in m</param>
	/// <param name="D0">Initial dew point in °C</param>
	/// <returns>.x = Temp at LCL, <para>
	/// .y = pressure at LCL, </para> <para>
	/// .z = height at LCL </para></returns>
	static glm::vec3 getLCL(const float T0, const float P0, const float Z0, const float D0);

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
        static glm::vec3 getLFC(const float T0,
                                const float P0,
                                const float Z0,
                                const float D0,
                                const float* pressures,
                                const float* temperatures,
                                const float* altitude,
                                const size_t size);

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
        static glm::vec3 getEL(const float T0,
                               const float P0,
                               const float Z0,
                               const float D0,
                               const float* pressures,
                               const float* temperatures,
                               const float* altitude,
                               const size_t size);
	
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
        static float calculateCAPE(const float T0,
                                   const float P0,
                                   const float Z0,
                                   const float D0,
                                   const float* pressures,
                                   const float* temperatures,
                                   const float* altitude,
                                   const size_t size);

private:

};


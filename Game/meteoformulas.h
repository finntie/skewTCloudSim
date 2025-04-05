#pragma once

#include <glm/glm.hpp>
#include <vector>

#include "readTable.h"

class meteoformulas
{
public:

	/// <summary>Get (Saturated) Water Vapor </summary>
	/// https://www.weather.gov/media/epz/wxcalc/vaporPressure.pdf
	/// https://en.wikipedia.org/wiki/Vapour_pressure_of_water (Better)
	/// <param name="T">(Dewpoint) Temperature in C</param>
	/// <returns>(saturation) vapor pressure in kPa (= 10 hPa)</returns>
	static float es(const float T);


	/// <summary>Calculates the (saturated) mixing ratio (also called r(s)).
	/// Filling in dewpoint for T will result in the mixing ratio,
	/// Filling in temperature for T will result in the saturated mixing ratio.</summary>
	/// https://vortex.plymouth.edu/~stmiller/stmiller_content/Publications/AtmosRH_Equations_Rev.pdf
	/// https://journals.ametsoc.org/view/journals/mwre/108/7/1520-0493_1980_108_1046_tcoept_2_0_co_2.xml
	/// <param name="T">(Dewpoint) Temperature in C</param>
	/// <param name="P">Pressure in hPa</param>
	/// <returns>(saturated) mixing ratio in (kg(vapor)/kg(air))</returns>
	static float ws(const float T, const float P);

	/// <summary>Calculates the mass fraction of water vapor </summary>
	/// Page 80 chapter 3.5.1 https://www.gnss-x.ac.cn/docs/Atmospheric%20Science%20An%20Introductory%20Survey%20(John%20M.%20Wallace,%20Peter%20V.%20Hobbs)%20(z-lib.org).pdf 
	/// <param name="T">Temperature in °C</param>
	/// <param name="P">Pressure in hPa</param>
	/// <returns>% / 100 of water vapor in air</returns>
	static float qv(const float T, const float P);

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

	/// <summary>Calculates the moist lapse rate</summary>
	/// <param name="T">Temperature in °C</param>
	/// <param name="P">Pressure in hPa</param>
	/// <returns>Lapse rate in dK/dP (delta Kelving per delta hectoPascal)</returns>
	static float MLR(const float T, const float P);

	/// <summary> Calculates the temperature of the (saturated) mixing ratio for every pressure</summary>
	/// <param name="Ws">(Saturated) Mixing ratio</param>
	/// <param name="pressures">Vector of all pressures to calculate</param>
	/// <returns>A vector of temperatures corrisponding to the input</returns>
	static std::vector<float> getTempAtWs(const float Ws, const std::vector<float>& pressures);

	/// <summary>The DALR (Γd) (Dry lapse rate) is the temperature gradient experienced in an ascending or descending packet of air that is not saturated with water vapor, i.e., 
	/// with less than 100 % relative humidity. (Warning) for the skew-T, use the potential temperature  
	/// https://en.wikipedia.org/wiki/Lapse_rate</summary>
	/// <param name="T0">Reference Temperature in °C</param>
	/// <param name="heights">Vector of given heights in meters</param>
	/// <returns>Vector of temperatures in °C corrosponding to vector of heights</returns>
	static std::vector<float> getDryLapseRate(const float T0, const std::vector<float>& heights);

	/// <summary>The potential temperature of a parcel of fluid at pressure P 
	/// is the temperature that the parcel would attain if adiabatically brought to a standard reference pressure P0,
	/// usually 1,000 hPa(1,000 mb).
	/// https://en.wikipedia.org/wiki/Potential_temperature</summary>
	/// <param name="T0">Reference Temperature in °C</param>
	/// <param name="Pref">Reference Pressure in hPa</param>
	/// <param name="pressures">Vector of given pressures</param>
	/// <returns>Vector of temperatures in °C corrosponding to vector of pressures</returns>
	static std::vector<float> getPotentialTemp(const float T0, const float P0, const std::vector<float>& pressures);

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

	/// <summary>Calculate moist lapse rate for given temp at given pressures</summary>
	/// <param name="T0">Reference Temperature in °C</param>
	/// <param name="Pref">Reference Pressure in hPa</param>
	/// <param name="pressures">Vector of given pressures</param>
	/// <returns>Vector of temperatures in °C corrosponding to vector of pressures</returns>
	static std::vector<float> getMoistTemp(const float T0, const float P0, const std::vector<float>& pressures);

	/// <summary>Calculates CCL with given parameters</summary>
	/// <param name="P0">Initial pressure in hPa</param>
	/// <param name="D0">Initial dew point in °C</param>
	/// <param name="OData">Observed data</param>
	/// <returns>.x = Temp at the CCL, <para>
	/// .y = pressure at CCL, </para> <para>
	/// .z = Potential Temp at the P0 </para></returns>
	static glm::vec3 getCCL(const float P0, const float D0, const skewTInfo& OData);

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
	/// <param name="OData">skewTInfo struct of observed data</param>
	/// <returns>Height of which an ideal air parcel is free to rise. (first time air parcel is warming than its surrounding)
	/// <para>Returns -1 if no EL was found</para></returns>
	static glm::vec3 getLFC(const float T0, const float P0, const float Z0, const float D0, const skewTInfo& OData);

	/// <summary>Calculates the Equilibrium Level (EL) or: Top of clouds</summary>
	/// <param name="T0">Initial temperature in °C</param>
	/// <param name="P0">Initial pressure in hPa</param>
	/// <param name="Z0">Initial height in m</param>
	/// <param name="D0">Initial dew point in °C</param>
	/// <param name="OData">skewTInfo struct of observed data</param>
	/// <returns>Height of tops (last time parcel was warmer than observed temp)
	/// <para>Returns -1 if no EL was found</para></returns>
	static glm::vec3 getEL(const float T0, const float P0, const float Z0, const float D0, const skewTInfo& OData);
	
	/// <summary>Calculates the CAPE (Convective Available Potential Energy)</summary>
	/// <param name="T0">Initial temperature in °C</param>
	/// <param name="P0">Initial pressure in hPa</param>
	/// <param name="Z0">Initial height in m</param>
	/// <param name="D0">Initial dew point in °C</param>
	/// <param name="OData">skewTInfo struct of observed data</param>
	/// <returns>CAPE in J/kg</returns>
	static float calculateCAPE(const float T0, const float P0, const float Z0, const float D0, const skewTInfo& OData);

private:

};


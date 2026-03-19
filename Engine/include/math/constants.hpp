

namespace Constants
{
	const float Lb = -0.0065f; //Standard lapse rate in K/m
	const float g = 9.8076f; //Earth's gravitational acceleration in m/s2
	const float Hv = 2501000; //Heat vaporation of water in J/kg (also written as L) what about 2501000?2268000 https://library.wmo.int/viewer/59923/?offset=#page=220&viewer=picture&o=search&n=0&q=Lv
	const float R = 8.3145f; //Universal gas constant in J/mol*k
	const float Rsd = 287.0528f; //specific gas constant of dry air in J/kg*K
	const float Rsw = 461.5f; //specific gas constant of water vapor in J/kg*K
	const float E = Rsd / Rsw; //(Rsd / Rsw) the dimensionless ratio of the specific gas constant of dry air to the specific gas constant for water vapour
	const float Cvv = 1418.0f; // specific heat	capacity of water vapor at constant volume
	const float Cva = 719.0f; // (Probably) The specific heat capicity of dry air? (refering to https://escholarship.org/content/qt0d72911v/qt0d72911v.pdf?t=pghwe7)
	const float Cpa = Rsd + Cva; //specific heat capacity at constant pressure for dry air in j/kg*K
	const float Cpd = 1003.5f; //The specific heat of dry air at constant pressure in j/kg*K
	const float Cpv = 717.0f; //The specific heat of dry air at constant Volume in j/kg*K
	const float Cpvw = Cvv + Rsw; //the specific heat capacity of water vapor at constant pressure
	const float Cvl = 4119.0f; //Specific heat capacity at constant volume for liquid water in J/kg*K
	const float Cpi = 2093.0f; //Specific heat of ice in J/kg/K
    const float Cpds = 800.0f; //Specific heat capacity of dry soil
    const float Cpws = 1480.0f; // Specific heat capacity of wet soil
	const float Mda = 28.966f; //Molair mass of dry air at constant pressure in g/mol
	const float Mw = 18.02f; //Molair mass of water in g/mol
	const float ptrip = 611.2f; //Triple point of water in pascal (6.11657 hPa)
	const float Ttrip = 273.16f; //Triple point of water in Kelvin
	const float E0v = 2.374e+6f; // Heat latency of vaporising water in J/kg
	const float Lf = 3.3355e+5f;  // Heat latency of Fusion of water in J/kg
    const float Ls = 2.834e+6f;  // Heat latency of deposition of water in J/kg
	const float E0s = 0.3337e+6f; // The difference in specific internal energy between liquid and solid at the triple point. in J/kg
	const float euler = 2.7182818284f; //Euler's number
    const float Ka = 2.40e-2f; // thermal conductivity of air in J/m/s/K
    const float PI = 3.14159265359f;
    const float oo = 5.67e-8f; // Boltzmann constant in W / m-2 / K-4
    const float ge = 0.95f; // Ground emissivity
    }
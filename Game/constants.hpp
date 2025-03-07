

namespace Constants
{
	const float Lb = -0.0065f; //Standard lapse rate in K/m
	const float g = 9.8076f; //Earth's gravitational acceleration in m/s2
	const float Hv = 2268000; //Heat vaporation of water in J/kg (also written as L) what about 2501000?
	const float R = 8.3145f; //Universal gas constant in J/mol*k
	const float Rsd = 287.0528f; //specific gas constant of dry air in J/kg*K
	const float Rsw = 461.5f; //specific gas constant of water in J/kg*K
	const float E = Rsd / Rsw; //(Rsd / Rsw) the dimensionless ratio of the specific gas constant of dry air to the specific gas constant for water vapour
	const float Cvv = 1418.0f; // specific heat	capacity of water vapor at constant volume
	const float Cva = 719.0f; // (Probably) The specific heat capicity of dry air? (refering to https://escholarship.org/content/qt0d72911v/qt0d72911v.pdf?t=pghwe7)
	const float Cpa = Rsd + Cva; //specific heat capacity at constant pressure for dry air in j/kg*K
	const float Cpd = 1003.5f; //The specific heat of dry air at constant pressure in j/kg*K
	const float Cpv = 717.0f; //The specific heat of dry air at constant Volume in j/kg*K
	const float Cpvw = Cvv + Rsw; //the specific heat capacity of water vapor at constant pressure
	const float Cvl = 4119.0f; //Specific heat capacity at constant volume for liquid water in J/kg*K
	const float Mda = 28.966f; //Molair mass of dry air at constant pressure in g/mol
	const float Mw = 18.02f; //Molair mass of water in g/mol
	const float ptrip = 611.2f; //Triple point of water in pascal (6.11657 hPa)
	const float Ttrip = 273.16f; //Triple point of water in Kelvin
	const float E0v = 2.374e+6f; // Heat latency of vaporising water in J/kg (Could be a different value i.e: 2.3740×10^6)
	const float euler = 2.7182818284f; //Euler's number
}
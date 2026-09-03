#pragma once
#include "config.h"
#include <vector> // Also in PCH, but needed for std::vector
 
class environmentGPU;
struct envDebugData;

class environment
{
public:

	//TODO: doubles are used for the temps, due to only 14 million precision between biggest and smallest number stored.
	struct gridDataSky 
	{
		// Initialize data with size in number of items
		void init(const int size) 
		{
			Qv = new float[size];
			Qw = new float[size];
			Qc = new float[size];
			Qr = new float[size];
			Qs = new float[size];
			Qi = new float[size];
			potTemp = new float[size];
			velField = new glm::vec3[size];
			pressure = new float[size];
			initialized = true;
		}
		float* Qv; //  Mixing Ratio of Water Vapor
		float* Qw; //	Mixing Ratio of	Liquid Water
		float* Qc; //	Mixing Ratio of Ice 
		float* Qr; //	Mixing Ratio of Rain
		float* Qs; //	Mixing Ratio of Snow
		float* Qi; //	Mixing Ratio of Ice (precip)
		float* potTemp;			 // Potential temperature
		glm::vec3* velField;	// Velocity field  (fluid sim)
		float* pressure;

		gridDataSky() = default;
		~gridDataSky()
		{
			if (initialized)
			{
				delete[] Qv;
				delete[] Qw;
				delete[] Qc;
				delete[] Qr;
				delete[] Qs;
				delete[] Qi;
				delete[] potTemp;
				delete[] velField;
				delete[] pressure;
				initialized = false;
			}
		}

		// Delete copy
		gridDataSky(const gridDataSky&) = delete;
		gridDataSky& operator=(const gridDataSky&) = delete;

		// Move constructor
		gridDataSky(gridDataSky&& other) noexcept
		{
			Qv = other.Qv; Qw = other.Qw; Qc = other.Qc; Qr = other.Qr; Qs = other.Qs; Qi = other.Qi;
			potTemp = other.potTemp; velField = other.velField; pressure = other.pressure;
			initialized = other.initialized;
			other.initialized = false; // Since other is now empty
		}
		// Equal will now move:
		gridDataSky& operator=(gridDataSky&& other) noexcept
		{
			if (this != &other)
			{
				if (initialized) // If current is already initialized, we need to free our data
				{
					delete[] Qv;
					delete[] Qw;
					delete[] Qc;
					delete[] Qr;
					delete[] Qs;
					delete[] Qi;
					delete[] potTemp;
					delete[] velField;
					delete[] pressure;
				}
				Qv = other.Qv; Qw = other.Qw; Qc = other.Qc; Qr = other.Qr; Qs = other.Qs; Qi = other.Qi;
				potTemp = other.potTemp; velField = other.velField; pressure = other.pressure;
				initialized = other.initialized;
				other.initialized = false; // Since other is now empty
			}
			return *this;
		}

	private:
		bool initialized = false;
	};

	//TODO: do we want halfs or not? precision lies on about 6e-8f; 
	struct gridDataGround 
	{
		// Initialize data with size in number of items
		void init(const int size)
		{
			Qrs = new float[size];
			Qgr = new float[size];
			Qgs = new float[size];
			Qgi = new float[size];
			P = new float[size];
			t = new float[size];
			T = new float[size];
			initialized = true;
		}

		float* Qrs; // Subsurface water content
		float* Qgr; // Rain content
		float* Qgs; // Snow content
		float* Qgi; // Ice content
		float* P;   // Ground Pressure
		float* t;   // Time since ground was wet
		float* T;   // Ground temperature

		gridDataGround() = default;
		~gridDataGround()
		{
			if (initialized)
			{
				delete[] Qrs;
				delete[] Qgr;
				delete[] Qgs;
				delete[] Qgi;
				delete[] P;
				delete[] t;
				delete[] T;
			}
		}

		// Delete copy
		gridDataGround(const gridDataGround&) = delete;
		gridDataGround& operator=(const gridDataGround&) = delete;

		// Move constructor
		gridDataGround(gridDataGround&& other) noexcept
		{
			Qrs = other.Qrs; Qgr = other.Qgr; Qgs = other.Qgs; Qgi = other.Qgi; P = other.P; t = other.t; T = other.T;
			initialized = other.initialized;
			other.initialized = false; // Since other is now empty
		}
		// Equal will now move:
		gridDataGround& operator=(gridDataGround&& other) noexcept
		{
			if (this != &other)
			{
				if (initialized) // If current is already initialized, we need to free our data
				{
					delete[] Qrs;
					delete[] Qgr;
					delete[] Qgs;
					delete[] Qgi;
					delete[] P;
					delete[] t;
					delete[] T;
				}
				Qrs = other.Qrs; Qgr = other.Qgr; Qgs = other.Qgs; Qgi = other.Qgi; P = other.P; t = other.t; T = other.T;
				initialized = other.initialized;
				other.initialized = false; // Since other is now empty
			}
			return *this;
		}

	private:
		bool initialized = false;
	};

	environment();
	~environment();

	//void init(float* potTemps, glm::vec2* velField, float* Qv, float* groundTemp, float* groundPres, float* pressures);

	//void EditorData();

	//void Update(float dt, float speed);

	////--------------------------------Ground---------------------------------
	//float irridiance();
	//float groundCoverageFactor(const int index);
	//void updateGroundTemps(const float dt, const int index, const float Irradiance, const float cloudCoverage);
	//void advectMicroPhysicsGround(const float dt, const int index);
	//void updateMicroPhysicsGround(const float dt, const int index, const float Tair, const float irr, const float c, const float density);
	//float calculateSumPhaseHeatGround(const int i);


	////----------------------------------Sky----------------------------------
	//void diffuseAndAdvectTemp(const float dt);
	//void getInterpolValueTemp(float* arrayFull, const glm::vec2 Ppos, float& output);
	////type: rain = 1, snow = 2, ice = 3
	//void diffuseAndAdvect(const float dt, float* array, std::vector<float>& density, bool vapor = false, const int fallVelocityType = 0);
	//void interPolatePrecip(const float dt, float* array, const int fallVelocityType);
	//bool getInterpolValue(float* array,const glm::vec2 Ppos, const bool Vapor, float& output);
	//void PPMWAdvect(float* array, float* defaultVal, const int i, const float dt);
	//void PPMWAdvectLR(float* array, float* defaultVal, const int i, const float dt, const bool x);
	//float PPMWAdvectFlux(float* array, float* defaultVal, const int i, const float dt, const bool x, const bool right);
	//void updateVelocityField(const float dt);
	//float calculateBuoyancy(const int index);
	//float averageEnvironment(const int index, const int distanceFromidx, const float maxDistance, const bool temp);
	//glm::vec2 vorticityConfinement(const int index);
	//bool getInterpolVel(glm::vec2 Ppos, bool U, float& output);
	////-----PressureProject-----
	//void pressureProjectVelField();
	//void calculatePresProj(std::vector<float>& pressureProj);
	//void calculateDivergence(std::vector<float>& output);
	//void calculatePrecon(std::vector<float>& output, std::vector<glm::ivec3>& A);
	//void applyPreconditioner(std::vector<float>& precon, std::vector<float>& r, std::vector<glm::ivec3>& A, std::vector<float>& storageQ, std::vector<float>& output);
	//void applyA(std::vector<float>& s, std::vector<glm::ivec3>& A, std::vector<float>& output);
	///// <summary>Calculates the mass-weighted mean terminal velocity of all types of precip</summary>
	///// <param name = "type"> 1 = rain, 2 = snow, 3 = hail, 4 = all</param>
	///// <returns>x: rain, y: snow, z: ice</returns>
	//glm::vec3 calculateFallingVelocity(const int index, const float density, const int type);
	//void updateMicroPhysics(const float dt, const int index, const float T, const float density);
	//float calculateSumPhaseHeat(const float dt, const int index, const float Temp);
	//void computeHeatTransfer(const int index, const float sumHeat);

	///// <summary> Get UV from the velocity field which is in MAC grid</summary>
	//glm::vec2 getUV(const int index);
	glm::vec3 getUV(const glm::vec3* velField, const int x, const int y, const int z);

	///// <summary>Get ambient temp at height. Using avaraged lapse rate between 5 and 2 km. </summary>
	//float getIsentropicTemp(const float y);
	//float getIsentropicVapor(const float y);
	//float curl(const int index, bool raw = false);
	//float div(const int index);
	//glm::vec2 lap(const int index);
	//bool outside(const int i); //For index, does not work on x bounds
	//bool outside(const float x, const float y); //For coords
	//bool isGround(int i);
	//bool isGround(int x, int y);
	//bool isGroundLevel(int i);
	//bool isGroundLevel(int x, int y);

	//void computeNeighArray();

	//Neigh m_NeighData[GRIDSIZESKY]; //Neighbour data
	envDebugData* getDebugData();

private:
	gridDataSky m_envGrid;
	gridDataGround m_groundGrid;

	//float m_time = 43200.0f; //0 to 86.400 time in seconds
	//const float m_dayLightDuration = 14.0f;
	//const float m_hourOfSunrise = 6.0f;
	//float m_speed = 1.0f;
	//float m_longitude = 52.37f; //Longitude on earth, 52.37 is Amsterdam
	//int m_day = 130; //Day of the year
	//float m_sunStrength = 1.0f;
	//bool m_pauseDiurnal = false;

	//float m_condens = 0.0f; //Heat from condensation
	//float m_freeze = 0.0f; //Heat from freezing
	//float m_depos = 0.0f; //Heat from deposition (gas to solid)

	//float m_isenTropicTemps[GRIDSIZESKYY];
	//float m_isenTropicVapor[GRIDSIZESKYY];
	//float m_pressures[GRIDSIZESKYY];
	//float m_defaultVel[GRIDSIZESKYY];
	////int m_GHeight[GRIDSIZEGROUND];
	//float m_dummyArray[GRIDSIZESKYY];

	//float velocityX[GRIDSIZESKY];
	//float velocityY[GRIDSIZESKY];

	//float m_debugArray0[GRIDSIZESKY];
	//float m_debugArray1[GRIDSIZESKY];
	//float m_debugArray2[GRIDSIZESKY];
};


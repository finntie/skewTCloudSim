#pragma once
#include "core/ecs.hpp"
#include "tools/inspectable.hpp"

#include "half/half.hpp"
#include <glm/glm.hpp>

#define GRIDSIZESKYX 32
#define GRIDSIZESKYY 64

#define GRIDSIZESKY (GRIDSIZESKYX * GRIDSIZESKYY)
#define GRIDSIZEGROUND (GRIDSIZESKYX)
#define VOXELSIZE 10.0f //Meters


class environment : public bee::System, public bee::IPanel
{
public:

	struct gridDataSky //28 bytes
	{
		half_float::half Qv[GRIDSIZESKY]{ half_float::half(0.01f) }; //  Mixing Ratio of Water Vapor
		half_float::half Qw[GRIDSIZESKY]{ half_float::half(0.01f) }; //	Mixing Ratio of	Liquid Water
		half_float::half Qc[GRIDSIZESKY]{ half_float::half(0.01f) }; //	Mixing Ratio of Ice 
		half_float::half Qr[GRIDSIZESKY]{ half_float::half(0.01f) }; //	Mixing Ratio of Rain
		half_float::half Qs[GRIDSIZESKY]{ half_float::half(0.01f) }; //	Mixing Ratio of Snow
		half_float::half Qi[GRIDSIZESKY]{ half_float::half(0.01f) }; //	Mixing Ratio of Ice (precip)
		float potTemp[GRIDSIZESKY]{ 1.0f };			 // Potential temperature
		glm::vec2 velField[GRIDSIZESKY]{};				 // Velocity field (fluid sim) needs to be changed to vec3 later

		float dummy{ 0.0f }; //Room for 4 bytes?		
	};

	struct gridDataGround //16 bytes
	{
		half_float::half Qrs[GRIDSIZEGROUND]{ half_float::half(0.01f) }; // Subsurface water content
		half_float::half Qgr[GRIDSIZEGROUND]{ half_float::half(0.01f) }; // Rain content
		half_float::half Qgs[GRIDSIZEGROUND]{ half_float::half(0.01f) }; // Snow content
		half_float::half Qgi[GRIDSIZEGROUND]{ half_float::half(0.01f) }; // Ice content
		half_float::half P[GRIDSIZEGROUND]{ half_float::half(1000.0f) }; // Ground Pressure
		half_float::half t[GRIDSIZEGROUND]{ half_float::half(0.0f) }; // Time since ground was wet
		float T[GRIDSIZEGROUND]{ 0.0f };  // Ground temperature
	};

	environment();
	~environment();

	void init(float* potTemps, glm::vec2* velField, half_float::half* Qv, float* groundTemp, half_float::half* groundPres);

#ifdef BEE_INSPECTOR
	void OnPanel() override;
	std::string GetName() const override;
	std::string GetIcon() const override;
#endif

	void DebugRender();

	void Update(float dt) override;

	//Ground
	float groundCoverageFactor(const int index);
	void updateGroundTemps(const float dt, const int index, const float Irradiance, const float cloudCoverage);
	void updateMicroPhysicsGround(const float dt, const int index);

	//Sky
	void updateVelocityField(const float dt, const int index, const float* m_pressures, const float density, const float Buoyancy, const glm::vec2 extraForces);
	void diffuseAndAdvect(const float dt, const int index, float* array);
	void diffuseAndAdvect(const float dt, const int index, half_float::half* array, bool vapor = false, const float fallVelocity = 0.0f);
	/// <summary>Calculates the mass-weighted mean terminal velocity of all types of precip</summary>
	/// <returns>x: rain, y: snow, z: ice</returns>
	glm::vec3 calculateFallingVelocity(const float dt, const int index, const float density);
	//void backTracing(const float dt, const int index, const float fallingVelocity);
	//float trilinearSampling(const glm::vec2 pos, half_float::half* array);
	glm::vec2 vorticityConfinement(const int index);
	/// <summary> returns eta (?) using nabla velocity, set raw to true to return raw nabla velocity</summary>
	float eta(const int index, bool raw = false);
	float calculateBuoyancy(const int index, const float* m_pressures, const float T);
	void updateMicroPhysics(const float dt, const int index, const float* m_pressures, const float T, const float density);
	void computeHeatTransfer(const float dt, const int index, const float sumHeat);
	float calculateSumPhaseHeat(const int index, const float Temp, const float pQv, const float pQw, const float pQc, const float pQr, const float pQs, const float pQi);
	float calculateSumPhaseHeatGround(const int i, const float pQgr, const float pQgs, const float pQgi);

private:
	

	gridDataSky m_envGrid;
	gridDataGround m_groundGrid;

	//TODO: Convert to 60 seconds, 3600 for minute and 86.400 for day
	float guard1 = 123456.0f;
	float m_time = 12.0f; //0 to 24
	float m_dayLightDuration = 10.0f;
	float m_hourOfSunrise = 6.0f;
	float guard2 = 123456.0f;

	float m_pressures[GRIDSIZESKY]{0.0f};
	
	//Debug
	int debugViewSky = 0;
	int debugViewGround = 0;

	int mousePointingIndex = 0;
	bool simulationActive = false;
	int simulationStep = 0;
};


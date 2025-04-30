#pragma once
#include "core/ecs.hpp"
#include "tools/inspectable.hpp"

#include "half/half.hpp"
#include <glm/glm.hpp>

#define GRIDSIZESKYX 64
#define GRIDSIZESKYY 64

#define GRIDSIZESKY (GRIDSIZESKYX * GRIDSIZESKYY)
#define GRIDSIZEGROUND (GRIDSIZESKYX)
#define VOXELSIZE 1.0f //Meters


class environment : public bee::System, public bee::IPanel
{
public:

	struct gridDataSky //28 bytes
	{
		half_float::half Qv[GRIDSIZESKY]{ half_float::half(1) }; //  Mixing Ratio of Water Vapor
		half_float::half Qw[GRIDSIZESKY]{ half_float::half(1) }; //	Mixing Ratio of	Liquid Water
		half_float::half Qc[GRIDSIZESKY]{ half_float::half(1) }; //	Mixing Ratio of Ice 
		half_float::half Qr[GRIDSIZESKY]{ half_float::half(1) }; //	Mixing Ratio of Rain
		half_float::half Qs[GRIDSIZESKY]{ half_float::half(1) }; //	Mixing Ratio of Snow
		half_float::half Qi[GRIDSIZESKY]{ half_float::half(1) }; //	Mixing Ratio of Ice (precip)
		float potTemp[GRIDSIZESKY]{ 1.0f };			 // Potential temperature
		glm::vec2 velField[GRIDSIZESKY]{};				 // Velocity field (fluid sim) needs to be changed to vec3 later

		float dummy{ 0.0f }; //Room for 4 bytes?		
	};

	struct gridDataGround //16 bytes
	{
		float Qjv[GRIDSIZEGROUND]{ 0.0f }; // Water content
		float T[GRIDSIZEGROUND]{ 0.0f };  // Ground temperature
		float P[GRIDSIZEGROUND]{ 0.0f };  // Ground pressure
		float Lc[GRIDSIZEGROUND]{ 0.0f };  // Cloud covering factor
	};

	environment();
	~environment() {};

#ifdef BEE_INSPECTOR
	void OnPanel() override;
	std::string GetName() const override;
	std::string GetIcon() const override;
#endif

	void Update(float dt) override;
	void updateVelocityField(const float dt, const int index, const float* pressures, const float density, const float Buoyancy);
	void diffuseAndAdvect(const float dt, const int index, float* array);
	void diffuseAndAdvect(const float dt, const int index, half_float::half* array, const float fallVelocity = 0.0f);
	/// <summary>Calculates the mass-weighted mean terminal velocity of all types of precip</summary>
	/// <returns>x: rain, y: snow, z: ice</returns>
	glm::vec3 calculateFallingVelocity(const float dt, const int index, const float density);
	void backTracing(const float dt, const int index, const float fallingVelocity);
	float trilinearSampling(const glm::vec2 pos, half_float::half* array);
	float calculateBuoyancy(const int index, const float* pressures, const float Temp);
	void updateMicroPhysics(const float dt, const int index, const float* pressures, const float T, const float density);
	void computeHeatTransfer(const float dt, const int index, const float sumHeat);
	float calculateSumPhaseHeat(const int index, const float Temp, const float pQv, const float pQw, const float pQc, const float pQr, const float pQs, const float pQi);

private:
	

	gridDataSky envGrid;
	gridDataGround groundGrid;

};


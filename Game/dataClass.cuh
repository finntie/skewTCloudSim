#pragma once

#include <cuda_runtime.h> 
#include <CUDA/cmath>
#include "config.h"

struct microPhysicsParams;

struct microPhysEnvValues
{

	float PVCON[MAXGRAPHLENGTH]; // Condensation rate of cloud water to vapor
	float PVDEP[MAXGRAPHLENGTH]; // Deposition of cloud ice to vapor.
	float PIMLT[MAXGRAPHLENGTH]; // Melting of cloud ice to form cloud water T > 0
	float PIDW[MAXGRAPHLENGTH];  // Depositional growth of cloud ice at expense of cloud water
	float PIHOM[MAXGRAPHLENGTH]; // Homogeneous freezing of cloud water to form cloud ice
	float PIACR[MAXGRAPHLENGTH]; // Accretion of rain by cloud ice; produces snow or graupel depending on the amount of rain
	float PRACI[MAXGRAPHLENGTH]; // Accretion of cloud ice by rain; produces snow or graupel depending on the amount of rain.
	float PRAUT[MAXGRAPHLENGTH]; // Autoconversion of cloud water to form rain.
	float PRACW[MAXGRAPHLENGTH]; // Accretion of cloud water by rain.
	float PREVP[MAXGRAPHLENGTH]; // Evaporation of rain.
	float PRACS[MAXGRAPHLENGTH]; // Accretion of snow by rain; produces graupel if rain or snow exceeds threshold and T < 0
	float PSACW[MAXGRAPHLENGTH]; // Accretion of cloud water by snow; produces snow if T < 0 or rain if T >= 0. Also enhances snow melting for T >= 0
	float PSACR[MAXGRAPHLENGTH]; // Accretion of rain by snow. For T < 0, produces graupel if rain or snow exceeds threshold; if now, produces snow. For T >= 0, the accreted water enchances snow melting
	float PSACI[MAXGRAPHLENGTH]; // Accretion of cloud ice by snow.
	float PSAUT[MAXGRAPHLENGTH]; // Autoconversion (aggregation) of cloud ice to form snow
	float PSFW[MAXGRAPHLENGTH];  // Bergeron process (deposition and riming) transfer of cloud water to form snow.
	float PSFI[MAXGRAPHLENGTH];  // Transfer rate of cloud ice to snow through growth of Bergeron process embryos.
	float PSDEP[MAXGRAPHLENGTH]; // Depositional growth of snow
	float PSSUB[MAXGRAPHLENGTH]; // Sublimation of snow
	float PSMLT[MAXGRAPHLENGTH]; // Melting of snow to form rain, T > 0
	float PGAUT[MAXGRAPHLENGTH]; // Autoconversion (aggregation) of snow to form graupel.
	float PGFR[MAXGRAPHLENGTH];  // Probalistic freezing of rain to form graupel.
	float PGACW[MAXGRAPHLENGTH]; // Accretion of cloud water by graupel
	float PGACI[MAXGRAPHLENGTH]; // Accretion of cloud ice by graupel
	float PGACR[MAXGRAPHLENGTH]; // Accretion of rain by graupel
	float PGDRY[MAXGRAPHLENGTH]; // Dry growth of graupel
	float PGACS[MAXGRAPHLENGTH]; // Accretion of snow by graupel
	float PGSUB[MAXGRAPHLENGTH]; // Sublimation of graupel
	float PGMLT[MAXGRAPHLENGTH]; // Melting of graupel to form rain, T > 0. (In this regime, PGACW is assumed to be shed off as rain)
	float PGWET[MAXGRAPHLENGTH]; // Wet growth of graupel; may involve PGACS and PGACI and must include: PGACW or PGACR, or both. The amount of PGACW which is not able to freeze is shed off as rain.
	float PGACR1[MAXGRAPHLENGTH];// Fallout or growth of hail by wetness
	//EXTRA ADDED VALUES FOR CLARITY
	float PVVAP[MAXGRAPHLENGTH]; // Evaporation rate of cloud water to vapor
	float PVSUB[MAXGRAPHLENGTH]; // Sublimation rate of cloud ice to vapor.


	//Other graph data
	int atIndex;
	float biggest;
	float smallest;

	__host__ __device__ void init()
	{
		memset(this, 0, sizeof(*this));
	}

	//Set values to 0
	__host__ __device__ void reset()
	{
		memset(this, 0, sizeof(*this));
	}



};

class dataClass
{
private:

	microPhysEnvValues microPhysEnvDataResult;

public:
	dataClass();
	~dataClass();

	void confirmMicroPhysCheckRegion(const int2 minCorner, const int2 maxCorner);
	void cancelMicroPhysCheckRegion();
	void setMicroPhysicsData(const microPhysicsParams* params);

	void drawMicroPhysGraph();
	void addValueToGraph(const char* name, const float* value);
	void detailedLegendEntry(const char* label, const char* details,  const char* addsTo, const char* removesFrom, const char* extra = " ");

	bool microPhysCheckActive{ false};
	char3 dummy;
	int2 microPhysMinPos{ -1, -1 };
	int2 microPhysMaxPos{ -1, -1 };
};



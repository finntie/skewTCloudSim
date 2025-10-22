#pragma once
#include <glm/glm.hpp>

class microPhys
{
public:

	microPhys();
	~microPhys() {};

	struct microPhysResult
	{
		microPhysResult(float _Qv, float _Qw, float _Qc, float _Qr, float _Qs, float _Qi, 
			const float _dt, const float _speed, const float _temp, const float _pressure, const float _density, const int _groundHeight, const glm::vec3 _fallVel) :
			Qv(_Qv), Qw(_Qw), Qc(_Qc), Qr(_Qr), Qs(_Qs), Qi(_Qi), 
			dt(_dt), speed(_speed), temp(_temp), pressure(_pressure), density(_density), groundHeight(_groundHeight), fallingVelocity(_fallVel) 
		{ }
		~microPhysResult() {};
		//Input and output data
		float Qv{0.0f}; //  Result Mixing Ratio of Water Vapor
		float Qw{0.0f}; //	Result Mixing Ratio of	Liquid Water
		float Qc{0.0f}; //	Result Mixing Ratio of Ice 
		float Qr{0.0f}; //	Result Mixing Ratio of Rain
		float Qs{0.0f}; //	Result Mixing Ratio of Snow
		float Qi{0.0f}; //	Result Mixing Ratio of Ice (precip)

		//Return heat latency values.
		float condens = 0.0f; //Heat from condensation
		float freeze = 0.0f; //Heat from freezing
		float depos = 0.0f; //Heat from deposition (gas to solid) 

		//Input data
		const float dt{ 0.0f };
		const float speed{ 0.0f };
		const float temp{ 0.0f }; //In Kelvin
		const float pressure{ 1000.0f };
		const float density{ 1.0f };
		const int groundHeight{ 0 };
		const glm::vec3 fallingVelocity{ 0.0f };
	};

	struct microPhysGroundResult
	{
		microPhysGroundResult(float _Qrs, float _Qv, float _Qr, float _Qs, float _Qi,
			const float _dt, const float _speed, const float _temp, 
			const float _tempAir, const float _pressure, const float _density, const float _time, const float _irradiance, const float _windSpeed, const float _cloudCover) :
			Qrs(_Qrs), Qv(_Qv), Qr(_Qr), Qs(_Qs), Qi(_Qi),
			dt(_dt), speed(_speed), temp(_temp), tempAir(_tempAir), 
			pressure(_pressure), density(_density), time(_time), irradiance(_irradiance), windSpeed(_windSpeed), cloudCover(_cloudCover)
		{
		}
		~microPhysGroundResult() {};
		//Input and output data
		float Qrs{ 0.0f };//    Result Mixing Ratio of Subsurface water content
		float Qv{ 0.0f }; //    Mixing ratio of vapor in the air
		float Qr{ 0.0f }; //	Result Mixing Ratio of Rain
		float Qs{ 0.0f }; //	Result Mixing Ratio of Snow
		float Qi{ 0.0f }; //	Result Mixing Ratio of Ice
		float time{ 0.0f };

		//Return heat latency values.
		float condens = 0.0f; //Heat from condensation
		float freeze = 0.0f; //Heat from freezing
		float depos = 0.0f; //Heat from deposition (gas to solid) 

		//Input data
		const float dt{ 0.0f };
		const float speed{ 0.0f };
		const float temp{ 0.0f }; //In Kelvin
		const float tempAir{ 0.0f }; //In Kelvin
		const float pressure{ 1000.0f };
		const float density{ 1.0f };
		const float irradiance{ 0.0f };
		const float windSpeed{ 0.0f };
		const float cloudCover{ 0.0f }; //in fraction (1.0f is cloudy)
	};

	struct microPhysHittingGroundResult
	{
		microPhysHittingGroundResult(float _Qv, float _Qr, float _Qs, float _Qi, float _Qgr, float _Qgs, float _Qgi,
			const float _dt, const float _speed, const float _temp,
			const float _tempAir, const float _pressure, const float _density, const glm::vec3 _fallVel) :
			Qv(_Qv), Qr(_Qr), Qs(_Qs), Qi(_Qi), Qgr(_Qgr), Qgs(_Qgs), Qgi(_Qgi),
			dt(_dt), speed(_speed), temp(_temp), tempAir(_tempAir),
			pressure(_pressure), density(_density), fallingVelocity(_fallVel)
		{
		}
		~microPhysHittingGroundResult() {};
		//Input and output data
		float Qv{ 0.0f }; //    Mixing ratio of vapor in the air
		float Qr{ 0.0f }; //	Result Mixing Ratio of Rain
		float Qs{ 0.0f }; //	Result Mixing Ratio of Snow
		float Qi{ 0.0f }; //	Result Mixing Ratio of Ice

		float Qgr{ 0.0f }; //	Result Mixing Ratio of Rain in ground
		float Qgs{ 0.0f }; //	Result Mixing Ratio of Snow in ground
		float Qgi{ 0.0f }; //	Result Mixing Ratio of Ice in ground

		//Return heat latency values.
		float condens = 0.0f; //Heat from condensation
		float freeze = 0.0f; //Heat from freezing
		float depos = 0.0f; //Heat from deposition (gas to solid) 

		//Input data
		const float dt{ 0.0f };
		const float speed{ 0.0f };
		const float temp{ 0.0f }; //In Kelvin
		const float tempAir{ 0.0f }; //In Kelvin
		const float pressure{ 1000.0f };
		const float density{ 1.0f };
		const glm::vec3 fallingVelocity{ 0.0f };
	};

	void calculateEnvMicroPhysics(microPhysResult& data);

	void calculateMicroPhysicsGround(microPhysGroundResult& data);

	//To calculate the precipitation that will hit the ground
	void calculateEnvMicroPhysicsHittingGround(microPhysHittingGroundResult& data);

private:

	//Functions
	float FPVCON();
	float FPVDEP();

	float FPIMLT(); //TODO: limit melting?
	float FPIDW();
	float FPIHOM();
	float FPIACR();
	float FPRACI();
	float FPRAUT();
	float FPRACW();
	float FPREVP();
	float FPRACS();
	float FPSACW();
	float FPSACR();
	float FPSACI();
	float FPSAUT();
	float FPSFW(); //TODO: use this function?
	float FPSFI(); //TODO: also use old formula?
	float FPSDEP();
	float FPSSUB(const float PSDEP);
	float FPSMLT(const float PSACW, const float PSACR);
	float FPGAUT();
	float FPGFR();
	float FPGACW();
	float FPGACI();
	float FPGACR();
	float FPGACS(const bool EGS1 = false);
	float FPGDRY(const float PGACW, const float PGACI, const float PGACR, const float PGACS);
	float FPGSUB();
	float FPGMLT(const float PGACW, const float PGACR);
	float FPGWET(const float PGACI, const float PGACS);
	float FPGACR1(const float PGWET, const float PGACW, const float PGACI, const float PGACS);

	//Ground variables
	float FPGEVP();
	float FPGFLW();
	float FPGGMLT();
	float FPGSMLT();
	float FPGREVP();
	float FPGDEVP();
	float FPGGFR();
	float FPGSFR();

	//Precip hitting ground variables
	float FPGRFR();

	float m_Qv{ 0.0f }; 
	float m_Qw{ 0.0f }; 
	float m_Qc{ 0.0f }; 
	float m_Qr{ 0.0f }; 
	float m_Qs{ 0.0f }; 
	float m_Qi{ 0.0f }; 
	float m_Qrs{ 0.0f };

	float m_Qgr{ 0.0f };
	float m_Qgs{ 0.0f };
	float m_Qgi{ 0.0f };

	float dt{ 0.0f }; // Without m because its just deltatime
	float m_speed{ 0.0f };
	float m_T{ 0.0f };
	float m_Tc{ 0.0f };
	float m_Ta{ 0.0f }; //Temp of air above
	float m_ps{ 1000.0f }; //Pressure in hPa
	float m_Dair{ 1.0f };
	int   m_GHeight{ 0 };
	glm::vec3 m_fallVel{ 0.0f };
	float m_time{ 0.0f };
	float m_irradiance{ 0.0f }; // solar Irradiance in W/m-2
	float m_windSpeed{ 0.0f }; // m/s2
	float m_cloudCover{ 0.0f }; // In fraction

	float m_QWS{ 0.0f };
	float m_QWI{ 0.0f };
	const float m_qwmin = 0.001f; // the minimum cloud water content required before rainmaking begins
	const float m_qcmin = 0.001f; // the minimum cloud ice content required before snowmaking begins
	const float m_qimin = 0.0006f; // the minimum ice content required before snow turns into ice

	//Production terms, used to sometimes create snow or ice or other stuff (formula 20): https://research.csiro.au/ccam/wp-content/uploads/sites/520/2024/01/1377337420.pdf 
	float m_PTerm1{0.0f};
	float m_PTerm2{0.0f};
	float m_PTerm3{0.0f};

	//Gammas and slopes
	float m_slopeR{ 0.0f };
	float m_slopeS{ 0.0f };
	float m_slopeI{ 0.0f };
	float m_gammaSS{ 0.0f }; //Smelting snow; (d + 5) / 2
	float m_gammaR{ 0.0f }; //Rain; 3 + b
	float m_gammaRC{ 0.0f }; //Rain to ice; 6 + b
	float m_gammaER{ 0.0f }; //Evaporating rain: (b + 5) / 2
	float m_gammaS{ 0.0f }; //Snow; 3 + d
	float m_gammaI{ 0.0f }; //Ice; 3.5f
	float m_gammaRI{ 0.0f }; //Smelting Ice; 2.75f
};


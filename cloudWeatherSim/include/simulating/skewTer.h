#pragma once

#include <glm/glm.hpp>
#include <memory>

class DrawImage;

class skewTer
{
public:
	
	skewTer();
	~skewTer();


	struct skewTInfo
	{
		~skewTInfo() {};

		enum skewTParam
		{
			TEMP, DP, P
		};

		//Initialize the struct. If used, own pointers can be deleted afterwards.
		void init(const int _size, float* T, float* D, float* Ps, int height = 0)
		{
			temps = new float[_size];
			dewPoints = new float[_size];
			pressures = new float[_size];
			size = _size;
			heightPos = height;

			std::memcpy(temps, T, size * sizeof(float));
			std::memcpy(dewPoints, D, size * sizeof(float));
			std::memcpy(pressures, Ps, size * sizeof(float));
		}
		float* temps;
		float* dewPoints; //TODO: this data may get deleted due to it being a normal pointer
		float* pressures;
		int size = 0;
		int heightPos = 0;

		//No winds yet
	};

	void setSkewT(skewTInfo skewT);

	void drawSkewT();

	void setVariable(skewTInfo::skewTParam param, const int index, const float value);
	void setArray(skewTInfo::skewTParam param, const float* input);
	void setStartHeight(const int height);
	void setAllArrays(float* T, float* D, float* Ps);
	DrawImage* getSkewTImage() { return skewTImage; };

	//How skewed is the SkewT
	float tanTheta = glm::tan(glm::radians(45.0f));

	glm::vec2 skewTSize{ 100,100 };
	glm::vec2 skewTPos{ -40,0 };
private:


	void drawBackground();
	void drawEnvironment();
	void drawDryAndMoist();

	//Converts temp and height in meters or pressure to plotting so its easily modified.
	//Warning: Not using pressure (so height in meters) could lead up to different values due to observed not being the same as standard.
	glm::vec2 convertToPlottingCoordinates(const float temp, const float value, const bool pressure, const float width, const float height);

	skewTInfo m_skewT;
	
	const glm::vec2 normalSkewTSize{ 100,100 };
	int startHeight = 0;
	DrawImage* skewTImage = nullptr;
};


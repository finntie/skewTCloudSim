#pragma once

#include <glm/glm.hpp>
#include <cstring>
#include <vector>

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
			temps.assign(T, T + _size);
			dewPoints.assign(D, D + _size);
			pressures.assign(Ps, Ps + _size);
			size = _size;
			heightPos = height;
		}
		std::vector<float> temps;
		std::vector<float> dewPoints;
		std::vector<float> pressures;
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


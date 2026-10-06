#include "simulating/readTable.h"

#include "math/meteoconstants.cuh"
#include "math/meteoformulas.cuh"

#include "cloud_hub.hpp"

#include "rendering/render_hub.hpp"
#include "rendering/shapes_gl.hpp"
#include "rendering/colors.hpp"

#include "simulating/simulation_hub.hpp"
#include "simulating/cuda/environment.cuh"

#include "utils.cuh"

#include <vector>
#include <memory> 
#include <glm/glm.hpp>

static float lerpEnvValue(const float H1, const float H2, const float HC, const float V1, const float V2)
{
	if (HC == H1) return V1;
	else if (HC == H2) return V2;
	const float t = (HC - H1) / (H2 - H1);
	return V1 + t * (V2 - V1);
}

void readTable::initEnvironment()
{

	std::vector<int> indices;
	std::vector<float> potTempSmall;
	std::vector<float> potTemp;
	std::vector<glm::vec3> velField;
	std::vector<float> Qv;

	indices.resize(GRIDSIZESKYY);
	potTempSmall.resize(GRIDSIZESKYY);
	potTemp.resize(GRIDSIZESKY);
	velField.resize(GRIDSIZESKY);
	Qv.resize(GRIDSIZESKY);

	std::vector<float> groundTemp;
	std::vector<float>  groundPressure;
	std::vector<float>  pressures;
	std::vector<float>  pressuresSmall;

	groundTemp.resize(GRIDSIZEGROUND);
	groundPressure.resize(GRIDSIZEGROUND);
	pressures.resize(GRIDSIZESKY);
	pressuresSmall.resize(GRIDSIZESKYY);

	int j = 0;
	for (float y = 0; y < GRIDSIZESKYY * VOXELSIZE; y += VOXELSIZE)
	{
		const float H0 = skewTData.data.altitude[0];
		int i = getIndexAtHeight(y + H0);
		int Pi = i == 0 ? 0 : i - 1;
		pressuresSmall[j] = lerpEnvValue(skewTData.data.altitude[Pi] - H0, skewTData.data.altitude[i] - H0, y, skewTData.data.pressure[Pi], skewTData.data.pressure[i]);
		potTempSmall[j] = lerpEnvValue(skewTData.data.altitude[Pi] - H0, skewTData.data.altitude[i] - H0, y, skewTData.data.temperature[Pi], skewTData.data.temperature[i]);
		//potTempSmall[j] = skewTData.data.temperature[i];
		indices[j] = i;
		j++;
	}

	MForms::getPotentialTempArray(potTempSmall.data(), skewTData.data.pressure[0], pressuresSmall.data(), potTempSmall.data(), GRIDSIZESKYY);
	//Duplicate across x and z direction
	for (int z = 0; z < GRIDSIZESKYZ; z++)
	{
		for (int i = 0; i < GRIDSIZESKYY; i++) 
		{
			for (int x = 0; x < GRIDSIZESKYX; x++)
			{
				const int idx = getIdx(x, i, z);
				potTemp[idx] = static_cast<float>(potTempSmall[i] + 273.15f);
			}
		}
	}

	//Converting 2D into 1D: 90degrees = -1, 270 degrees = 1
	for (int i = 0; i < GRIDSIZESKYY; i++)
	{
		const float H0 = skewTData.data.altitude[0];
		const int idx = indices[i];
		int Pidx = idx == 0 ? 0 : idx - 1;
		int Pi = i == 0 ? 0 : i - 1;
		// Sin for X and cos for Y to get previous and current values
		const float partPI = ConstantsCPU::PI / 180.0f;
		float velFieldValueX = std::sinf((skewTData.data.windDir[idx] - 180.0f) * partPI) * skewTData.data.windSpeed[idx];
		const float prevVelFieldValueX = std::sinf((skewTData.data.windDir[Pidx] - 180.0f) * partPI) * skewTData.data.windSpeed[Pidx];
		float velFieldValueZ = std::cosf((skewTData.data.windDir[idx] - 180.0f) * partPI) * skewTData.data.windSpeed[idx];
		const float prevVelFieldValueZ = std::cosf((skewTData.data.windDir[Pidx] - 180.0f) * partPI) * skewTData.data.windSpeed[Pidx];
		// Now lerp
		velFieldValueX = lerpEnvValue(skewTData.data.altitude[Pidx] - H0, skewTData.data.altitude[idx] - H0, i * VOXELSIZE, prevVelFieldValueX, velFieldValueX);
		velFieldValueZ = lerpEnvValue(skewTData.data.altitude[Pidx] - H0, skewTData.data.altitude[idx] - H0, i * VOXELSIZE, prevVelFieldValueZ, velFieldValueZ);

		const float psValue = lerpEnvValue(skewTData.data.altitude[Pidx] - H0, skewTData.data.altitude[idx] - H0, i * VOXELSIZE, pressuresSmall[Pi], pressuresSmall[i]);
		const float QvValue = MForms::ws(lerpEnvValue(skewTData.data.altitude[Pidx] - H0, skewTData.data.altitude[idx] - H0, i * VOXELSIZE, 
			skewTData.data.dewPoint[Pidx], skewTData.data.dewPoint[idx]), psValue);
		for (int z = 0; z < GRIDSIZESKYZ; z++)
		{
			for (int x = 0; x < GRIDSIZESKYX; x++)
			{
				const int idxFull = getIdx(x, i, z);
				velField[idxFull] = { velFieldValueX, 0, velFieldValueZ };
				Qv[idxFull] = QvValue;
				pressures[idxFull] = psValue;
			}
		}
	}

	//Convert velfield into MAC-grid - TODO: could be done in loop above if loop was counting down.
	for (int z = 0; z < GRIDSIZESKYZ; z++)
	{
		for (int y = 0; y < GRIDSIZESKYY; y++)
		{
			for (int x = 0; x < GRIDSIZESKYX; x++)
			{
				const int Nx = x + 1 >= GRIDSIZESKYX ? x : x + 1;
				const int Ny = y + 1 >= GRIDSIZESKYY ? y : y + 1;
				const int Nz = z + 1 >= GRIDSIZESKYZ ? z : z + 1;
				const int idx = getIdx(x, y, z);


				velField[idx] = { (velField[idx].x + velField[getIdx(Nx, y, z)].x) / 2.0f,
					(velField[idx].y + velField[getIdx(x, Ny, z)].y) / 2.0f,
					(velField[idx].z + velField[getIdx(x, y, Nz)].z) / 2.0f };
			}
		}
	}


	for (int i = 0; i < GRIDSIZEGROUND; i++)
	{
		groundTemp[i] = skewTData.data.temperature[0] + 273.15f;
		groundPressure[i] = skewTData.data.pressure[0];
	}

	// TODO: environment cuh hookup
    CloudHub.CloudSim().Environment().init(potTemp.data(), velField.data(), Qv.data(), groundTemp.data(), groundPressure.data(), pressures.data(), pressuresSmall.data());
}
 


int readTable::getIndexAtHeight(float height)
{
	for (int i = 0; i < skewTData.data.dataSize; i++)
	{
		if (height <= skewTData.data.altitude[i])
		{
			return i;
		}
	}
	return -1;
}


glm::vec2 readTable::convertToPlottingCoordinates(const float temp, const float value, const bool pressure, const float scaleWidth, const float maxHeight)
{
	//Respect hPa for height in meter using standard pressure
	float height = value;
	if (!pressure) height = MForms::getStandardPressureAtHeight(0, value, 0, 1000.0f); //TODO: use standard height/pressure?


	//---------------------Log()----------------------

	height = (log10f(height) - log10f(100)) / (log10f(1000) - log10f(100)) * maxHeight;
	height = maxHeight - height;

	//-------------------------------------------------


	//Skew value
	float skewedTemp = temp + tanTheta * height;
	skewedTemp *= scaleWidth;

	return { skewedTemp, height };
}




void readTable::debugDrawData()
{
	const float divV = plotHeight;
	const float pi = 3.14159265359f;
	const glm::vec2 hodoOffset(90, 100);

	//Making pressures array
	const int pressuresSize = 900;
	std::unique_ptr<float[]> pressures = std::make_unique<float[]>(pressuresSize);
	{
		int i = 0;
		for (float p = 1000.0f; p > 100; p -= 1.0f)
		{
			pressures[i] = p;
			i++;
		}
	}
	std::unique_ptr<float[]> potTemps = std::make_unique<float[]>(pressuresSize);
	std::unique_ptr<float[]> temps = std::make_unique<float[]>(skewTData.data.dataSize);



	for (float p = 0; p < 1000; p += 100) 
	{
		glm::vec2 coords = convertToPlottingCoordinates(0, p, true, sizeSkewT.x, sizeSkewT.y);
        CloudHub.CloudRender().shapesGLObj().AddLine(glm::vec3(-40, coords.y, 0), glm::vec3(40, coords.y, 0), Colors::GreyA);
	}
	for (float i = -100; i < 40; i += 10)
	{
		glm::vec2 coords = convertToPlottingCoordinates(i, 30000, false, sizeSkewT.x, sizeSkewT.y);
		CloudHub.CloudRender().shapesGLObj().AddLine(glm::vec3(i, 0, 0), glm::vec3(coords, 0), Colors::GreyA);
	}
	CloudHub.CloudRender().shapesGLObj().AddLine(glm::vec3(-40, 0, 0), glm::vec3(40, 0, 0), Colors::BlackA);
	CloudHub.CloudRender().shapesGLObj().AddLine(glm::vec3(-40, 0, 0), glm::vec3(-40, 30000 * divV, 0), Colors::BlackA);


	//Dry and moist adiabatic 
	for (float i = -40; i <= 40; i += 5)
	{
		//Dry adiabatic (potential temps)
		{
			MForms::getDryAdiabatic(i, 1000.0f, pressures.get(), potTemps.get(), pressuresSize);

			for (int j = 10; j < pressuresSize; j += 10)
			{
				glm::vec2 coords = convertToPlottingCoordinates(potTemps[j], pressures[j], true, sizeSkewT.x, sizeSkewT.y);
				glm::vec2 coordsPrev = convertToPlottingCoordinates(potTemps[j - 10], pressures[j - 10], true, sizeSkewT.x, sizeSkewT.y);

				CloudHub.CloudRender().shapesGLObj().AddLine(glm::vec3(coords, 0), glm::vec3(coordsPrev, 0), Colors::GreyA);
			}
		}

		//Moist adiabatic
		{
			int offset = 0;
			MForms::getMoistTemp(i, 1000.0f, pressures.get(), potTemps.get(), pressuresSize, offset);
			if (offset == -1) continue;

			for (int j = 10 + offset; j < pressuresSize; j += 10)
			{
				glm::vec2 coords = convertToPlottingCoordinates(potTemps[j], pressures[j], true, sizeSkewT.x, sizeSkewT.y);
				glm::vec2 coordsPrev = convertToPlottingCoordinates(potTemps[j - 10], pressures[j - 10], true, sizeSkewT.x, sizeSkewT.y);

				CloudHub.CloudRender().shapesGLObj().AddLine(glm::vec3(coords, 0), glm::vec3(coordsPrev, 0), Colors::GreyA);
			}
		}
	}


	//LCL
	glm::vec3 LCL = MForms::getLCL(skewTData.data.temperature[0], skewTData.data.pressure[0], 0, skewTData.data.dewPoint[0]);
	glm::vec2 coords = convertToPlottingCoordinates(LCL.x, LCL.y, true, sizeSkewT.x, sizeSkewT.y);
	CloudHub.CloudRender().shapesGLObj().AddLine(glm::vec3(35, coords.y, 0), glm::vec3(40, coords.y, 0), Colors::GreenA);


	//CCL
	glm::vec3 CCL = MForms::getCCL(skewTData.data.pressure[0], skewTData.data.dewPoint[0], skewTData.data.pressure, skewTData.data.temperature, skewTData.data.dataSize);
	coords = convertToPlottingCoordinates(CCL.x, CCL.y, true, sizeSkewT.x, sizeSkewT.y);
	CloudHub.CloudRender().shapesGLObj().AddLine(glm::vec3(35, coords.y, 0), glm::vec3(40, coords.y, 0), Colors::WhiteA);
	glm::vec2 coords2 = convertToPlottingCoordinates(skewTData.data.dewPoint[0], skewTData.data.pressure[0], true, sizeSkewT.x, sizeSkewT.y);
	CloudHub.CloudRender().shapesGLObj().AddLine(glm::vec3(coords.x, coords.y, 0), glm::vec3(coords2.x, coords2.y, 0), Colors::GreyA);

	coords = convertToPlottingCoordinates(CCL.z, skewTData.data.pressure[0], true, sizeSkewT.x, sizeSkewT.y);
	CloudHub.CloudRender().shapesGLObj().AddLine(glm::vec3(coords.x, 0, 0), glm::vec3(coords.x, 2, 0), Colors::RedA);


	//Dry adiabatic to LCL
	MForms::getDryAdiabatic(skewTData.data.temperature[0], skewTData.data.pressure[0], skewTData.data.pressure, temps.get(), skewTData.data.dataSize);

	for (int j = 1; j < skewTData.data.dataSize; j++)
	{
		//TODO: should convertToPlottingCoordinates include setting default pressure height? (maybe an extra function that sets it)
		coords = convertToPlottingCoordinates(temps[j], skewTData.data.pressure[j], true, sizeSkewT.x, sizeSkewT.y);
		glm::vec2 coordsPrev = convertToPlottingCoordinates(temps[j - 1], skewTData.data.pressure[j - 1], true, sizeSkewT.x, sizeSkewT.y);

		CloudHub.CloudRender().shapesGLObj().AddLine(glm::vec3(coords, 0), glm::vec3(coordsPrev, 0), Colors::BlackA);
	}

	//Moist adiabatic at LCL
	int offset = 0;
	MForms::getMoistTemp(LCL.x, LCL.y, skewTData.data.pressure, temps.get(), skewTData.data.dataSize, offset);
	if (offset != -1)
	{
		for (int j = 1 + offset; j < skewTData.data.dataSize; j++)
		{
			//TODO: should convertToPlottingCoordinates include setting default pressure height? (maybe an extra function that sets it)
			coords = convertToPlottingCoordinates(temps[j], skewTData.data.pressure[j], true, sizeSkewT.x, sizeSkewT.y);
			glm::vec2 coordsPrev = convertToPlottingCoordinates(temps[j - 1], skewTData.data.pressure[j - 1], true, sizeSkewT.x, sizeSkewT.y);

			CloudHub.CloudRender().shapesGLObj().AddLine(glm::vec3(coords, 0), glm::vec3(coordsPrev, 0), Colors::BlackA);
		}
	}

	//LFC
	const glm::vec3 LFC = MForms::getLFC(skewTData.data.temperature[0], skewTData.data.pressure[0], skewTData.data.altitude[0], skewTData.data.dewPoint[0], skewTData.data.pressure, skewTData.data.temperature, skewTData.data.altitude, skewTData.data.dataSize);
	coords = convertToPlottingCoordinates(skewTData.data.temperature[0], LFC.y, true, sizeSkewT.x, sizeSkewT.y);
	CloudHub.CloudRender().shapesGLObj().AddLine(glm::vec3(35, coords.y, 0), glm::vec3(40, coords.y, 0), Colors::YellowA);

	//EL
	const glm::vec3 EL = MForms::getEL(skewTData.data.temperature[0], skewTData.data.pressure[0], skewTData.data.altitude[0], skewTData.data.dewPoint[0], skewTData.data.pressure, skewTData.data.temperature, skewTData.data.altitude, skewTData.data.dataSize);
	coords = convertToPlottingCoordinates(skewTData.data.temperature[0], EL.y, true, sizeSkewT.x, sizeSkewT.y);
	CloudHub.CloudRender().shapesGLObj().AddLine(glm::vec3(35, coords.y, 0), glm::vec3(40, coords.y, 0), Colors::PinkA);

	//CAPE
	CAPE = MForms::calculateCAPE(skewTData.data.temperature[0], skewTData.data.pressure[0], 0, skewTData.data.dewPoint[0], skewTData.data.pressure, skewTData.data.temperature, skewTData.data.altitude, skewTData.data.dataSize);


	CloudHub.CloudRender().shapesGLObj().AddCircle(glm::vec3(hodoOffset, 0), 0.1f, glm::vec3(0, 0, 1), Colors::BlackA);
	CloudHub.CloudRender().shapesGLObj().AddCircle(glm::vec3(hodoOffset, 0), 10, glm::vec3(0, 0, 1), Colors::BlackA);
	CloudHub.CloudRender().shapesGLObj().AddCircle(glm::vec3(hodoOffset, 0), 20, glm::vec3(0, 0, 1), Colors::BlackA);
	CloudHub.CloudRender().shapesGLObj().AddCircle(glm::vec3(hodoOffset, 0), 30, glm::vec3(0, 0, 1), Colors::BlackA);
	CloudHub.CloudRender().shapesGLObj().AddCircle(glm::vec3(hodoOffset, 0), 40, glm::vec3(0, 0, 1), Colors::BlackA);

	glm::vec2 prevDir = { 0,0 };

	//Due to how skew-T's are drawn, the observed data (in this case) starts at hPa 1000 or the first line.
	//TODO: research this, what if station has higher offset?
	//const float heightAt0 = meteoformulas::getStandardHeightAtPressure(0, 1000, 1013.25f);
	//const float offset = skewTData.data.altitude[0] - heightAt0;

	for (int i = 1; i < skewTData.data.dataSize; i++)
	{
		glm::vec2 tempCoords = convertToPlottingCoordinates(skewTData.data.temperature[i], skewTData.data.pressure[i], true, sizeSkewT.x, sizeSkewT.y);
		glm::vec2 tempPrevCoords = convertToPlottingCoordinates(skewTData.data.temperature[i - 1], skewTData.data.pressure[i - 1], true, sizeSkewT.x, sizeSkewT.y);

		glm::vec2 dewCoords = convertToPlottingCoordinates(skewTData.data.dewPoint[i], skewTData.data.pressure[i], true, sizeSkewT.x, sizeSkewT.y);
		glm::vec2 dewPrevCoords = convertToPlottingCoordinates(skewTData.data.dewPoint[i - 1], skewTData.data.pressure[i - 1], true, sizeSkewT.x, sizeSkewT.y);


		CloudHub.CloudRender().shapesGLObj().AddLine(glm::vec3(tempCoords, 0.0f), glm::vec3(tempPrevCoords, 0.0f), Colors::RedA);
		CloudHub.CloudRender().shapesGLObj().AddLine(glm::vec3(dewCoords, 0.0f), glm::vec3(dewPrevCoords, 0.0f), Colors::GreenA);


		glm::vec4 color = Colors::BlackA;

		if (skewTData.data.altitude[i] > 0) color = Colors::PurpleA;
		if (skewTData.data.altitude[i] > 1000) color = Colors::RedA;
		if (skewTData.data.altitude[i] > 2000) color = Colors::OrangeA;
		if (skewTData.data.altitude[i] > 6000) color = Colors::YellowA;
		if (skewTData.data.altitude[i] > 9000) color = Colors::CyanA;


		//Hodograph
		//Get wind dir
		float radians = skewTData.data.windDir[i] * (pi / 180.0f);
		glm::vec2 dir = { -glm::sin(radians), -glm::cos(radians) };
		dir *= skewTData.data.windSpeed[i];

		CloudHub.CloudRender().shapesGLObj().AddLine(glm::vec3(dir + hodoOffset, 0.0f), glm::vec3(prevDir + hodoOffset, 0.0f), color);
		prevDir = dir;

	}
}



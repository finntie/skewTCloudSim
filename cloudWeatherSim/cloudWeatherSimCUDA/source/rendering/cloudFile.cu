#include "rendering/cloudFile.cuh"

#include "rendering/cuda_render.cuh"
#include "rendering/cuda_render_gl.hpp"

#include "config.hpp"
#include "utils.cuh"

#include <fstream>
#include <sstream>
#include <filesystem>
#include <string>
#include <iostream>
#include <utility>
#include <algorithm>
#include <unordered_map>
#include <stdio.h>
#include <memory.h>

#include "msvc_defines.h"
#include <hip/hip_runtime.h>

cloudFile::cloudFile(const simParams& params) : m_params(params)
{
}

cloudFile::~cloudFile()
{
	if (m_GPUDataInitialized)
	{
		for (int i = 0; i < m_sizeGPUData + 1; i++)
		{
			hipFree(m_groundDataGPU[i].T);
			hipFree(m_groundDataGPU[i].t);
			hipFree(m_groundDataGPU[i].P);
			hipFree(m_groundDataGPU[i].Qgi);
			hipFree(m_groundDataGPU[i].Qgs);
			hipFree(m_groundDataGPU[i].Qgr);
			hipFree(m_groundDataGPU[i].Qrs);

			hipFree(m_skyDataGPU[i].pressure);
			hipFree(m_skyDataGPU[i].velfieldZ);
			hipFree(m_skyDataGPU[i].velfieldY);
			hipFree(m_skyDataGPU[i].velfieldX);
			hipFree(m_skyDataGPU[i].potTemp);
			hipFree(m_skyDataGPU[i].Qi);
			hipFree(m_skyDataGPU[i].Qs);
			hipFree(m_skyDataGPU[i].Qr);
			hipFree(m_skyDataGPU[i].Qc);
			hipFree(m_skyDataGPU[i].Qw);
			hipFree(m_skyDataGPU[i].Qv);
		}
	}

}

void cloudFile::initGPUData()
{
	// Include 1 extra, this will hold the lerped data that is used
	m_skyDataGPU = new gridDataSkyGPU[m_sizeGPUData + 1]();
	m_groundDataGPU = new gridDataGroundGPU[m_sizeGPUData + 1]();

	// Environment Values
	for (int i = 0; i < m_sizeGPUData + 1; i++)
	{
		hipMallocAsync((void**)&m_skyDataGPU[i].Qv, m_params.size * sizeof(float), reinterpret_cast<hipStream_t>(getStream()));
		hipMallocAsync((void**)&m_skyDataGPU[i].Qw, m_params.size * sizeof(float), reinterpret_cast<hipStream_t>(getStream()));
		hipMallocAsync((void**)&m_skyDataGPU[i].Qc, m_params.size * sizeof(float), reinterpret_cast<hipStream_t>(getStream()));
		hipMallocAsync((void**)&m_skyDataGPU[i].Qr, m_params.size * sizeof(float), reinterpret_cast<hipStream_t>(getStream()));
		hipMallocAsync((void**)&m_skyDataGPU[i].Qs, m_params.size * sizeof(float), reinterpret_cast<hipStream_t>(getStream()));
		hipMallocAsync((void**)&m_skyDataGPU[i].Qi, m_params.size * sizeof(float), reinterpret_cast<hipStream_t>(getStream()));
		hipMallocAsync((void**)&m_skyDataGPU[i].potTemp, m_params.size * sizeof(float), reinterpret_cast<hipStream_t>(getStream()));
		hipMallocAsync((void**)&m_skyDataGPU[i].velfieldX, m_params.size * sizeof(float), reinterpret_cast<hipStream_t>(getStream()));
		hipMallocAsync((void**)&m_skyDataGPU[i].velfieldY, m_params.size * sizeof(float), reinterpret_cast<hipStream_t>(getStream()));
		hipMallocAsync((void**)&m_skyDataGPU[i].velfieldZ, m_params.size * sizeof(float), reinterpret_cast<hipStream_t>(getStream()));
		hipMallocAsync((void**)&m_skyDataGPU[i].pressure, m_params.size * sizeof(float), reinterpret_cast<hipStream_t>(getStream()));
	}

	// Ground values
	for (int i = 0; i < m_sizeGPUData + 1; i++)
	{
		hipMallocAsync((void**)&m_groundDataGPU[i].Qrs, m_params.sizeGround * sizeof(float), reinterpret_cast<hipStream_t>(getStream()));
		hipMallocAsync((void**)&m_groundDataGPU[i].Qgr, m_params.sizeGround * sizeof(float), reinterpret_cast<hipStream_t>(getStream()));
		hipMallocAsync((void**)&m_groundDataGPU[i].Qgs, m_params.sizeGround * sizeof(float), reinterpret_cast<hipStream_t>(getStream()));
		hipMallocAsync((void**)&m_groundDataGPU[i].Qgi, m_params.sizeGround * sizeof(float), reinterpret_cast<hipStream_t>(getStream()));
		hipMallocAsync((void**)&m_groundDataGPU[i].P, m_params.sizeGround * sizeof(float), reinterpret_cast<hipStream_t>(getStream()));
		hipMallocAsync((void**)&m_groundDataGPU[i].t, m_params.sizeGround * sizeof(float), reinterpret_cast<hipStream_t>(getStream()));
		hipMallocAsync((void**)&m_groundDataGPU[i].T, m_params.sizeGround * sizeof(float), reinterpret_cast<hipStream_t>(getStream()));
	}

	hipError_t err = hipGetLastError();
	if (err != hipSuccess) {
		std::cerr << "error: " << hipGetErrorString(err) << std::endl;
		assert(false);
	}

	m_GPUDataInitialized = true;
}


bool cloudFile::updateViewing(gridDataSkyGPU*& output, float dt, bool force)
{
	if (m_data.m_cloudViewActive || m_data.m_cloudViewStepOnce != 0 || force)
	{
		if (m_data.m_cloudViewStepOnce < 0) m_data.m_cloudViewSpeedMult *= -1;
		else m_data.m_cloudViewSpeedMult = abs(m_data.m_cloudViewSpeedMult);

		// Get data about the time
		float beforeTime, afterTime;
		int frame = -1;
		if (!getSurroundedFrameTimes(m_data.m_time, beforeTime, afterTime, frame))
		{
			printf("Warning: No frames were found in the file\n");
			return false;
		}

		// Do not continue if already at the end
		if (!force && getLastFrameTime() == m_data.m_time) return false;

		// Apply bounds of time in the simulation
		if (beforeTime < 0.0f) m_data.m_time = afterTime, beforeTime = afterTime;
		else if (afterTime < 0.0f) m_data.m_time = beforeTime, afterTime = beforeTime;

		// Convert from time to frameTime with decimal indicating progress from current frame to next one
		const float a = afterTime - beforeTime;
		const float b = m_data.m_time - beforeTime;
		const float frameTime = b / (a + 1e-16f);

		// Lerp between values between frames
		getLerpedFrameData(output, nullptr, frame, frameTime);

		// Calculate last frametime to see if we need to updateInput the SDF
		// 1. We are fully in between 2 frames
		// 2. distance is larger than 1.5 frames
		// 3. or we have not even set it yet
		// 4. or we need to force
		bool updateSDF = false;
		if (force || abs(m_lastFrameTime - (frameTime + frame)) > 0.5f && (frameTime > 0.25f && frameTime < 0.75f) ||
			m_lastFrameTime < 0.0f || abs(m_lastFrameTime - (frameTime + frame)) > 1.5f)
		{
			printf("Updated old time: %f, to now: %f\n", m_lastFrameTime, frameTime + frame);
			m_lastFrameTime = frameTime + frame;
			updateSDF = true;
		}

		// Reset parameters if hidden
		for (int i = 0; i < 7; i++)
		{
			if (i == 6 && !m_data.m_visibleTypes[i])
			{
				hipMemsetAsync(typeToPointerGPU(7, output, nullptr, true), 0, m_params.size * sizeof(float), static_cast<hipStream_t>(getStream()));
				hipMemsetAsync(typeToPointerGPU(8, output, nullptr, true), 0, m_params.size * sizeof(float), static_cast<hipStream_t>(getStream()));
				hipMemsetAsync(typeToPointerGPU(9, output, nullptr, true), 0, m_params.size * sizeof(float), static_cast<hipStream_t>(getStream()));
			}
			else if (!m_data.m_visibleTypes[i])
			{
				hipMemsetAsync(typeToPointerGPU(i, output, nullptr, true), 0, m_params.size * sizeof(float), static_cast<hipStream_t>(getStream()));
			}
		}



		// Forcing will not updateInput time, since it is only meant to updateInput values
		if (force) return updateSDF;

		// Update time, since environment would actually do that.
		m_data.m_time += dt * m_data.m_cloudViewSpeedMult;

		m_data.m_cloudViewStepOnce = 0;
		return updateSDF;
	}
	return false;
}

bool cloudFile::tryCreateFrame(gridDataSky& _skyData, gridDataGround& _groundData, float time)
{
	if (!m_data.m_recording || m_data.m_paused) return false;

	float secondsPerFrame = 3600.0f / m_data.m_framesPerHour;
	float timeChange = time - m_data.m_currentTime;

	// If enough time passed, we create a new frame
	if (timeChange >= secondsPerFrame)
	{
		lockGlobal();

		gridDataSky skyData;
		gridDataGround groundData;
		skyData.init(m_params.size);
		groundData.init(m_params.sizeGround);

		// Based on active types, save correct data
		if (m_data.m_typesSky[0]) // Qw
		{
			memcpy(skyData.Qw, _skyData.Qw, m_params.size * sizeof(float));
		}
		if (m_data.m_typesSky[1]) // Qc
		{
			memcpy(skyData.Qc, _skyData.Qc, m_params.size * sizeof(float));
		}
		if (m_data.m_typesSky[2]) // Qr
		{
			memcpy(skyData.Qr,  _skyData.Qr, m_params.size * sizeof(float));
		}
		if (m_data.m_typesSky[3]) // Qs
		{
			memcpy(skyData.Qs, _skyData.Qs, m_params.size * sizeof(float));
		}
		if (m_data.m_typesSky[4]) // Qi
		{
			memcpy(skyData.Qi, _skyData.Qi, m_params.size * sizeof(float));
		}
		if (m_data.m_typesSky[5]) // Qv
		{
			memcpy(skyData.Qv, _skyData.Qv, m_params.size * sizeof(float));
		}
		if (m_data.m_typesSky[6]) // Temp
		{
			memcpy(skyData.potTemp, _skyData.potTemp, m_params.size * sizeof(float));
		}
		if (m_data.m_typesSky[7] || m_data.m_typesSky[8] || m_data.m_typesSky[9]) // Wind, all the same
		{
			memcpy(skyData.velFieldX, _skyData.velFieldX, m_params.size * sizeof(float));
			memcpy(skyData.velFieldY, _skyData.velFieldY, m_params.size * sizeof(float));
			memcpy(skyData.velFieldZ, _skyData.velFieldZ, m_params.size * sizeof(float));
		}
		if (m_data.m_typesSky[10]) // Pressure
		{
			memcpy(skyData.pressure, _skyData.pressure, m_params.size * sizeof(float));
		}
		
		// Ground values

		if (m_data.m_typesGround[0]) // Temp
		{
			memcpy(groundData.T,  _groundData.T, m_params.sizeGround * sizeof(float));
		}
		if (m_data.m_typesGround[1]) // Water
		{
			memcpy(groundData.Qrs, _groundData.Qrs, m_params.sizeGround * sizeof(float));
		}
		if (m_data.m_typesGround[2]) // Qr
		{
			memcpy(groundData.Qgr, _groundData.Qgs, m_params.sizeGround * sizeof(float));
		}
		if (m_data.m_typesGround[3]) // Qs
		{
			memcpy(groundData.Qgs, _groundData.Qgs, m_params.sizeGround * sizeof(float));
		}
		if (m_data.m_typesGround[4]) // Qi
		{
			memcpy(groundData.Qgi, _groundData.Qgi, m_params.sizeGround * sizeof(float));
		}

		m_skyData.push_back(std::move(skyData));
		m_groundData.push_back(std::move(groundData));
		m_data.m_totalFrames++;
		m_data.m_frames.push_back(time);

		m_data.m_currentTime = time;
		unlockGlobal();
		return true;
	}

	return false;
}

std::vector<std::string> cloudFile::getSimulationFiles()
{
	std::vector <std::string> output;
	std::filesystem::path directory = "assets/output";

	// If directory does not exist, we just do not have any files yet
	if (!std::filesystem::exists(directory)) return output;

	// Go through all files and add them
	for (const auto& entry : std::filesystem::directory_iterator(directory))
	{
		output.push_back(entry.path().filename().string());
	}
	return output;
}

bool cloudFile::getMetaData(const char* fileName, int& sizeX, int& sizeY, int& sizeZ, float& voxelSize, int& totalFrames, std::string& includedTypes)
{
	if (!loadCloudFile(fileName, true)) return false;
	sizeX = m_tempSizeX;
	sizeY = m_tempSizeY;
	sizeZ = m_tempSizeZ;
	voxelSize = m_tempVoxelSize;
	totalFrames = m_data.m_totalFrames;
	includedTypes.clear();

	for (int j = 0; j < 11; j++)
	{
		if (m_data.m_typesSky[j])
		{
			includedTypes += typeToString(j, true) + " ";	
		}
	}
	for (int j = 0; j < 5; j++)
	{
		if (m_data.m_typesGround[j])
		{
			includedTypes += typeToString(j, false) + " ";
		}
	}
	return true;
}

bool cloudFile::loadCloudFile(const char* fileName, bool onlyMeta)
{
	// Reset everything
	m_tempSizeX = 0;
	m_tempSizeY = 0;
	m_tempSizeZ = 0;
	m_tempVoxelSize = 0.0f;
	m_amountTypes = 0;
	m_data.m_totalFrames = 0;
	memset(m_data.m_typesSky, 0, 11 * sizeof(bool));
	memset(m_data.m_typesGround, 0, 5 * sizeof(bool));

	std::filesystem::path directory = "assets/output";
	directory = directory / fileName;

	// TODO: has extension in filename?
	if (!std::filesystem::exists(directory))
	{
		std::printf("Error, could not find file %s to load", fileName);
		return false;
	}
	
	// Data to be received
	int gridSizeFull = 0;
	int gridsizeGround = 0;


	/*	File Template
	*
	*	1. Simulation Size
	* 	2. Voxel Size
	* 	3. Total amount of frames
	* 	4. Timestamp of each frame (0 to 24 hours with decimal for minute and second)
	* 	5. Amount of types we store
	* 	6. Types we store in order
	* 	7. Data for each type
	* 
	* 	Example:
	* 
	* 	1. 32 32 32
	* 	2. 128
	* 	3. 240
	* 	4. 12.0341 12.0349 12.0357 12.0365 12.0383 12.0381 (etc) (In Binary)
	* 	5. 7
	* 	6. Qw Qc Qr Qs Qi GTemp Grs
	* 	7. 0.0 0.0 0.0 0.0 0.00001 0.0002 0.00001 0.0 0.0 (etc x for each type) (In Binray)
	*/


	bool valid = true;
	bool keepGoing = true;
	bool skipGetLine = false;

	// Open file in binary
	std::ifstream loadedFile(directory, std::ios::binary);
	std::string line;
	std::string word;
	int row = 0;
	
	// First line to check if it is correct file
	if (std::getline(loadedFile, line) && line == "CloudFile") {}
	else
	{
		valid = false;
	}


	// For the binary data we do not want to getLine, since this will offset the cursor.
	while (valid && keepGoing && (skipGetLine || std::getline(loadedFile, line)))
	{
		row++;
		std::stringstream ss(line);
		int count = 0;
		while (keepGoing && valid && ss >> word)
		{
			switch (row)
			{
			case 1: // Simulation Size
				if (count == 0) m_tempSizeX = std::stoi(word);
				else if (count == 1) m_tempSizeY = std::stoi(word);
				else if (count == 2)
				{
					m_tempSizeZ = std::stoi(word);
					gridSizeFull = m_tempSizeX * m_tempSizeY * m_tempSizeZ;
					gridsizeGround = m_tempSizeX * m_tempSizeZ;
				}
				break;
			case 2: // Voxel Size
				m_tempVoxelSize = std::stof(word);
				break;
			case 3: // Total amount of frames
				m_data.m_totalFrames = std::stoi(word);
				m_data.m_frames.resize(m_data.m_totalFrames);
				skipGetLine = true;
				break;
			case 4: // Timestamp of each frame (Binary)
				// Advance cursor if not interested in the data itself
				if (onlyMeta) loadedFile.seekg(m_data.m_totalFrames * sizeof(float), std::ios::cur);
				else loadedFile.read(reinterpret_cast<char*>(m_data.m_frames.data()), m_data.m_totalFrames * sizeof(float));
				skipGetLine = false;
				break;
			case 5: // Amount of types we store
				m_amountTypes = std::stoi(word);
				break;
			case 6: // Types we store in order
			{
				bool sky = false;
				int type = 0;
				type = stringToType(word, sky);
				if (type == -1) { valid = false; break; } // Invalid
				if (sky) m_data.m_typesSky[type] = true;
				else m_data.m_typesGround[type] = true;
				skipGetLine = true;
			}
				break;
			case 7: // Data for each type (Binary)

				// Not interested if only reading meta data
				if (onlyMeta)
				{
					keepGoing = false;
					break;
				}
				m_skyData.resize(m_data.m_totalFrames);
				m_groundData.resize(m_data.m_totalFrames);

				for (int i = 0; i < m_data.m_totalFrames; i++)
				{
					m_skyData[i].init(gridSizeFull);
					m_groundData[i].init(gridsizeGround);

					for (int j = 0; j < 11; j++)
					{
						if (m_data.m_typesSky[j])
						{
							loadedFile.read(reinterpret_cast<char*>(typeToPointer(j, &m_skyData[i], &m_groundData[i], true)), gridSizeFull * sizeof(float));
						}
					}
					for (int j = 0; j < 5; j++)
					{
						if (m_data.m_typesGround[j])
						{
							loadedFile.read(reinterpret_cast<char*>(typeToPointer(j, &m_skyData[i], &m_groundData[i], false)), gridsizeGround * sizeof(float));
						}
					}
				}
				skipGetLine = false;
				keepGoing = false;
				break;
			default:
				break;
			}

			count++;
			if (count > 999) valid = false;
		}
		if (row > 999) valid = false;
	}
	// Check if everything is valid
	if (m_tempSizeX <= 0 || m_tempSizeX >= 2048) valid = false;
	if (m_tempSizeY <= 0 || m_tempSizeY >= 2048) valid = false;
	if (m_tempSizeZ <= 0 || m_tempSizeZ >= 2048) valid = false;
	if (m_tempVoxelSize <= 0 || m_tempVoxelSize >= 4096) valid = false;
	if (m_amountTypes <= 0 || m_amountTypes > 11 + 5) valid = false;
	if (m_data.m_totalFrames <= 0 || m_data.m_totalFrames > 10'000) valid = false; // Lets assume 10.000 frames is too much
	if ((m_data.m_frames.empty() || m_data.m_frames.size() >= 10'000) && !onlyMeta) valid = false;
	if (!isOneTypeSelected(m_data)) valid = false;
	if (m_skyData.empty() && m_groundData.empty() && !onlyMeta) valid = false;

	if (!valid)
	{
		printf("Error, something went wrong with loading data, make sure the file is correct\n");
	}
	else
	{
		// Make every type visible and update by advancing 1 step
		for (int i = 0; i < 7; i++)
		{
			m_data.m_visibleTypes[i] = true;
		}
		m_data.m_cloudViewStepOnce = 1;
	}

	return valid;
}

bool cloudFile::loadSettingsFile(const char* fileName, renderSettings& outputSettings)
{
	renderSettings settings = renderSettings();


	std::filesystem::path directory = "assets/output";
	directory = directory / fileName;

	// TODO: has extension in filename?
	if (!std::filesystem::exists(directory))
	{
		std::printf("Error, could not find file %s to load", fileName);
		return false;
	}

	/*	File Template
	*
	*	1. Noise Reduction
	* 	2. Min Qw
	* 	3. Max Qw
	* 	4. Multiple Scattering
	* 	5. Ambient Light Scattering
	* 	6. Ray Random Offset
	* 	7. MS attenuation
	* 	8. MS contribution
	* 	9. MS eccentricity
	* 	10. Sun Strength
	* 	11. Light Exposure
	* 	12. Sun direction
	* 	13. Sun Color
	* 
	* 	14. Persistence
	* 	15. GridSize
	* 	16. Lacunarity
	*
	* 	Example:
	*
	* 	1. 0.45
	* 	2. 0.0001
	* 	3. 0.005
	* 	4. 5.0
	* 	5. 0.5
	* 	6. 0.25
	* 	7. 0.85
	* 	8. 0.75
	* 	9. 0.55
	* 	10. 1.0
	* 	11. 10.0
	* 	12. 0.1 1.0 0.45
	* 	13. 1.0 1.0 1.0
	* 
	* 	14. 0.5
	* 	15. 4
	* 	16. 2.0
	*/

	bool valid = true;
	bool keepGoing = true;
	bool skipGetLine = false;

	// Open file in binary
	std::ifstream loadedFile(directory, std::ios::binary);
	std::string line;
	std::string word;
	int row = 0;

	// First line to check if it is correct file
	if (std::getline(loadedFile, line) && line == "SettingsFile") {}
	else
	{
		valid = false;
	}

	// For the binary data we do not want to getLine, since this will offset the cursor.
	while (valid && keepGoing && (skipGetLine || std::getline(loadedFile, line)))
	{
		row++;
		std::stringstream ss(line);
		int count = 0;
		while (keepGoing && valid && ss >> word)
		{
			switch (row)
			{
			case 1: // Noise Reduction 
				settings.noiseReduction = std::stof(word);
				break;
			case 2: // Min Qw
				settings.minQw = std::stof(word);
				break;
			case 3: // Max Qw
				settings.maxQw = std::stof(word);
				break;
			case 4: // Multiple Scattering
				settings.multipleScatteringDepthPower = std::stof(word);
				break;
			case 5: // Ambient Light Scattering
				settings.ambientLightStrength = std::stof(word);
				break;
			case 6: // Ray Random Offset
				settings.rayRandomOffset = std::stof(word);
				break;
			case 7: // MS attenuation
				settings.attenuation = std::stof(word);
				break;
			case 8: // MS contribution
				settings.contribution = std::stof(word);
				break;
			case 9: // MS eccentricity
				settings.eccentricAttenuation = std::stof(word);
				break;
			case 10: // Sun Strength
				settings.sunStrength = std::stof(word);
				break;
			case 11: // Light Exposure
				settings.exposure = std::stof(word);
				break;
			case 12: // Sun direction
				if (count == 0) settings.sunDirection[0] = std::stof(word);
				else if (count == 1) settings.sunDirection[1] = std::stof(word);
				else if (count == 2) settings.sunDirection[2] = std::stof(word);
				break;
			case 13: // Sun Color
				if (count == 0) settings.sunColor[0] = std::stof(word);
				else if (count == 1) settings.sunColor[1] = std::stof(word);
				else if (count == 2) settings.sunColor[2] = std::stof(word);
				break;
			case 14: //  Persistence
				settings.persistence = std::stof(word);
				break;
			case 15: //  GridSize
				settings.texGridSize = std::stoi(word);
				break;
			case 16: //  Lacunarity
				settings.lacunarity = std::stof(word);
				break;
			default:
				break;
			}

			count++;
			if (count > 999) valid = false;
		}
		if (row > 999) valid = false;
	}

	// Check if everything is valid
	if (settings.noiseReduction < 0 || settings.noiseReduction >= 100.0f) valid = false;
	if (settings.minQw < 0.0f || settings.minQw >= 1.0f) valid = false;
	if (settings.maxQw < 0.0f || settings.maxQw >= 1.0f) valid = false;
	if (settings.multipleScatteringDepthPower < 0 || settings.multipleScatteringDepthPower >= 256.0f) valid = false;
	if (settings.ambientLightStrength < 0 || settings.ambientLightStrength > 256.0f) valid = false;
	if (settings.rayRandomOffset < 0 || settings.rayRandomOffset > 256.0f) valid = false;
	if (settings.attenuation <= 0 || settings.attenuation > 1.0f) valid = false;
	if (settings.contribution <= 0 || settings.contribution > 1.0f) valid = false;
	if (settings.eccentricAttenuation <= 0 || settings.eccentricAttenuation > 1.0f) valid = false;
	if (settings.sunStrength <= 0 || settings.sunStrength > 10'000) valid = false;
	if (settings.exposure <= 0 || settings.exposure > 10'000) valid = false;
	if (settings.sunDirection[0] < 0 || settings.sunDirection[0] > 1.0f || 
		settings.sunDirection[1] < 0 || settings.sunDirection[1] > 1.0f ||
		settings.sunDirection[2] < 0 || settings.sunDirection[2] > 1.0f) valid = false;
	if (settings.sunColor[0] < 0 || settings.sunColor[0] > 1.0f ||
		settings.sunColor[1] < 0 || settings.sunColor[1] > 1.0f ||
		settings.sunColor[2] < 0 || settings.sunColor[2] > 1.0f) valid = false;
	if (settings.persistence < 0 || settings.persistence > 1.0f) valid = false;
	if (settings.texGridSize <= 0 || settings.texGridSize > 10'000) valid = false;
	if (settings.lacunarity <= 0 || settings.lacunarity > 10'000) valid = false;

	if (!valid)
	{
		printf("Error, something went wrong with loading data, make sure the file is correct and the data inside makes sense\n");
	}
	// Copy succesfull data over
	else memcpy(&outputSettings, &settings, sizeof(renderSettings));

	return valid;
}

void cloudFile::saveToFile(cloudFileData& CFData)
{
	std::string fullFile = "assets/output/" + CFData.m_fileName + ".bin";

	std::ofstream writeFile(fullFile.c_str(), std::ios::binary);

	// File Type
	writeFile << "CloudFile" << "\n";
	// Simulation Size
	writeFile << m_params.sizeX << " " << m_params.sizeY << " " << m_params.sizeZ << "\n";
	// Voxel Size
	writeFile << m_params.voxelSize << "\n";
	// Total Frames
	writeFile << CFData.m_totalFrames << "\n";
	// Time frames
	writeFile.write(reinterpret_cast<char*>(CFData.m_frames.data()), CFData.m_frames.size() * sizeof(float));
	// Total amount of types
	writeFile << totalTypesSelected(CFData) << "\n";
	// Types in order
	for (int i = 0; i < 11; i++) if (CFData.m_typesSky[i]) writeFile << typeToString(i, true) << " ";
	for (int i = 0; i < 5; i++) if (CFData.m_typesGround[i]) writeFile << typeToString(i, false) << " ";
	writeFile << "\n";
	// Data of all types in order
	lockGlobal(); // To be certain
	for (int i = 0; i < CFData.m_totalFrames; i++)
	{
		for (int j = 0; j < 11; j++)
		{
			if (CFData.m_typesSky[j])
			{
				writeFile.write(reinterpret_cast<char*>(typeToPointer(j, &m_skyData[i], &m_groundData[i], true)), m_params.size * sizeof(float));
			}
		}
		for (int j = 0; j < 5; j++)
		{
			if (CFData.m_typesGround[j])
			{
				writeFile.write(reinterpret_cast<char*>(typeToPointer(j, &m_skyData[i], &m_groundData[i], false)), m_params.sizeGround * sizeof(float));
				writeFile << "\n";
			}
		}
	}
	unlockGlobal();

	writeFile.close();
}

void cloudFile::saveSettingsToFile(renderSettings& settings, std::string fileName)
{
	std::string fullFile = "assets/output/" + fileName + ".bin";

	std::ofstream writeFile(fullFile.c_str(), std::ios::binary);

	// File Type
	writeFile << "SettingsFile" << "\n";
	// 1. Noise Reduction
	writeFile << settings.noiseReduction << "\n";
	// 2. Min Qw
	writeFile << settings.minQw << "\n";
	// 3. Max Qw
	writeFile << settings.maxQw << "\n";
	// 4. Multiple Scattering
	writeFile << settings.multipleScatteringDepthPower << "\n";
	// 5. Ambient Light Scattering
	writeFile << settings.ambientLightStrength << "\n";
	// 6. Ray Random Offset
	writeFile << settings.rayRandomOffset << "\n";
	// 7. MS attenuation
	writeFile << settings.attenuation << "\n";
	// 8. MS contribution
	writeFile << settings.contribution << "\n";
	// 9. MS eccentricity
	writeFile << settings.eccentricAttenuation << "\n";
	// 10. Sun Strength
	writeFile << settings.sunStrength << "\n";
	// 11. Light Exposure
	writeFile << settings.exposure << "\n";
	// 12. Sun direction
	writeFile << settings.sunDirection[0] << " " << settings.sunDirection[1] << " " << settings.sunDirection[2] << "\n";
	// 13. Sun Color
	writeFile << settings.sunColor[0] << " " << settings.sunColor[1] << " " << settings.sunColor[2] << "\n";
	// 14. Persistence
	writeFile << settings.persistence << "\n";
	// 15. GridSize
	writeFile << settings.texGridSize << "\n";
	// 16. Lacunarity
	writeFile << settings.lacunarity << "\n";

	writeFile.close();
}


inline int iDivUp(int a, int b) { return (a % b != 0) ? (a / b + 1) : (a / b); }

__global__ void lerpFramesType(float* result, float* frame1Data, float* frame2Data, float t, int3 size)
{
	int x = threadIdx.x + blockDim.x * blockIdx.x;
	int y = threadIdx.y + blockDim.y * blockIdx.y;
	int z = threadIdx.z + blockDim.z * blockIdx.z;

	if (x >= size.x || y >= size.y || z >= size.z) return;

	int idx = x + y * size.x + z * size.x * size.y;

	const float a = frame1Data[idx];
	const float b = frame2Data[idx];

	// Possible room for change/improvement
	result[idx] = a + t * (b - a);
}

void cloudFile::getLerpedFrameData(gridDataSkyGPU*& outputSkyData, gridDataGroundGPU* outputGroundData, int currentFrame, float frameTime)
{
	int t1 = currentFrame;
	int t2 = currentFrame + 1;
	float t = frameTime;
	if (t2 >= m_skyData.size()) t2 = t1;

	// Check if we need to updateInput the GPU data
	checkIfUpdateGPU(t1, t2);

	outputSkyData = &m_skyDataGPU[m_sizeGPUData]; // Extra index acts as storage

	// Lerp data on the GPU, input the correct array index from the GPU array
	dim3 blockSize(8, 8, 8);
	dim3 gridSize = dim3(unsigned(iDivUp(m_params.sizeX, blockSize.x)),
		unsigned(iDivUp(m_params.sizeY, blockSize.y)),
		unsigned(iDivUp(m_params.sizeZ, blockSize.z)));
	for (int i = 0; i < 11; i++) if (m_data.m_typesSky[i]) lerpFramesType <<<gridSize, blockSize, 0, static_cast<hipStream_t>(getStream()) >>> (
		typeToPointerGPU(i, outputSkyData, outputGroundData, true),
		typeToPointerGPU(i, &m_skyDataGPU[m_GPUFrames[t1 - m_firstFrameGPU]], nullptr, true),
		typeToPointerGPU(i, &m_skyDataGPU[m_GPUFrames[t2 - m_firstFrameGPU]], nullptr, true), 
		t, 
		make_int3(m_params.sizeX, m_params.sizeY, m_params.sizeZ));

	hipError_t err = hipGetLastError();
	if (err != hipSuccess) {
		std::cerr << "error: " << hipGetErrorString(err) << std::endl;
		assert(false);
	}
}

bool cloudFile::getSurroundedFrameTimes(float time, float& outputBeforeTime, float& outputAfterTime, int& beforeFrameNum)
{
	outputBeforeTime = -1.0f;
	outputAfterTime = -1.0f;
	if (m_data.m_frames.empty()) return false;

	// Use lower bound to get the first value which is greater or the same as the input
	// If (1, 3, 5, 7, 9) with input of value 6, it returns index to 7 (index[3])
	auto it = std::lower_bound(m_data.m_frames.begin(), m_data.m_frames.end(), time);

	// If none was found, time is larger than any value in the vector
	if (it == m_data.m_frames.end())
	{
		beforeFrameNum = int(m_data.m_frames.size()) - 1;
		outputBeforeTime = m_data.m_frames.back();
		return true;
	}
	// If time is smaller than any value in the vector
	else if (*it == m_data.m_frames.front())
	{
		beforeFrameNum = 0;
		outputAfterTime = m_data.m_frames.front();
		return true;
	}

	// time is in between values
	beforeFrameNum = int(std::distance(std::begin(m_data.m_frames), it)) - 1;
	outputBeforeTime = *(it - 1);
	outputAfterTime = *(it);

	return true;
}

void cloudFile::resetRecording()
{
	m_skyData.clear();
	m_groundData.clear();
}

const cloudFileData* cloudFile::getCloudFileData()
{
	return &m_data;
}

void cloudFile::setCloudFileData(cloudFileData data)
{
	m_data = data;
}

bool cloudFile::isOneTypeSelected(cloudFileData& CFData)
{
	for (int i = 0; i < 11; i++)
	{
		if (CFData.m_typesSky[i]) return true;
	}
	for (int i = 0; i < 5; i++)
	{
		if (CFData.m_typesGround[i]) return true;
	}
	return false;
}

int cloudFile::totalTypesSelected(cloudFileData& CFData)
{
	int types = 0;
	for (int i = 0; i < 11; i++)
	{
		if (CFData.m_typesSky[i]) types++;
	}
	for (int i = 0; i < 5; i++)
	{
		if (CFData.m_typesGround[i]) types++;
	}
	return types;
}


float* cloudFile::typeToPointer(int type, gridDataSky* skyData, gridDataGround* groundData, bool sky)
{
	if (sky && skyData)
	{
		switch (type)
		{
		case 0: return (*skyData).Qw;
		case 1: return (*skyData).Qc;
		case 2: return (*skyData).Qr;
		case 3: return (*skyData).Qs;
		case 4: return (*skyData).Qi;
		case 5: return (*skyData).Qv;
		case 6: return (*skyData).potTemp;
		case 7: return (*skyData).velFieldX;
		case 8: return (*skyData).velFieldY;
		case 9: return (*skyData).velFieldZ;
		case 10: return (*skyData).pressure;
		default: break;
		}
	}
	else if (groundData)
	{
		switch (type)
		{
		case 0: return (*groundData).T;
		case 1: return (*groundData).Qrs;
		case 2: return (*groundData).Qgr;
		case 3: return (*groundData).Qgs;
		case 4: return (*groundData).Qgi;
		default: break;
		}
	}
	return nullptr;
}

float* cloudFile::typeToPointerGPU(int type, gridDataSkyGPU* skyData, gridDataGroundGPU* groundData, bool sky)
{
	if (sky && skyData)
	{
		switch (type)
		{
		case 0: return (*skyData).Qw;
		case 1: return (*skyData).Qc;
		case 2: return (*skyData).Qr;
		case 3: return (*skyData).Qs;
		case 4: return (*skyData).Qi;
		case 5: return (*skyData).Qv;
		case 6: return (*skyData).potTemp;
		case 7: return (*skyData).velfieldX;
		case 8: return (*skyData).velfieldY;
		case 9: return (*skyData).velfieldZ;
		case 10: return (*skyData).pressure;
		default: break;
		}
	}
	else if (groundData)
	{
		switch (type)
		{
		case 0: return (*groundData).T;
		case 1: return (*groundData).Qrs;
		case 2: return (*groundData).Qgr;
		case 3: return (*groundData).Qgs;
		case 4: return (*groundData).Qgi;
		default: break;
		}
	}
	return nullptr;
}

std::string cloudFile::typeToString(int type, bool sky)
{
	if (sky)
	{
		switch (type)
		{
		case 0: return "Qw";
		case 1: return "Qc";
		case 2: return "Qr";
		case 3: return "Qs";
		case 4: return "Qi";
		case 5: return "Qv";
		case 6: return "Temp";
		case 7: return "Wind";
		case 8: return "Wind";
		case 9: return "Wind";
		case 10: return "Pressure";
		default: break;
		}
	}
	else
	{
		switch (type)
		{
		case 0: return "GTemp";
		case 1: return "GWater";
		case 2: return "GQr";
		case 3: return "GQs";
		case 4: return "GQi";
		default: break;
		}
	}

	return "";
}

int cloudFile::stringToType(std::string type, bool& outputSky)
{
	if (type == "Qw") { outputSky = true; return 0; }
	else if (type == "Qc") { outputSky = true; return 1; }
	else if (type == "Qr") { outputSky = true; return 2; }
	else if (type == "Qs") { outputSky = true; return 3; }
	else if (type == "Qi") { outputSky = true; return 4; }
	else if (type == "Qv") { outputSky = true; return 5; }
	else if (type == "Temp") { outputSky = true; return 6; }
	else if (type == "Wind") { outputSky = true; return 7; } // Wind accounts for X Y and Z
	else if (type == "Pressure") { outputSky = true; return 10; }

	else if (type == "GTemp") { outputSky = false; return 0; }
	else if (type == "GWater") { outputSky = false; return 1; }
	else if (type == "GQr") { outputSky = false; return 2; }
	else if (type == "GQs") { outputSky = false; return 3; }
	else if (type == "GQi") { outputSky = false; return 4; }

	outputSky = false;
	return -1;
}


void cloudFile::checkIfUpdateGPU(int frame1, int frame2)
{
	// First check if the first frames are already initialized
	if (m_firstFrameGPU == m_lastFrameGPU && m_firstFrameGPU == 0 && m_lastFrameGPU == 0)
	{
		// Not yet initialized
		// So, initialize every type that is saved for the amount of frames we want to copy to the GPU
		m_firstFrameGPU = frame1;
		for (int j = 0; j < m_sizeGPUData; j++)
		{
			// Make sure to only copy types that we saved and not to overshoot it.
			if (frame1 + j < int(m_data.m_frames.size()))
			{
				for (int i = 0; i < 11; i++)
				{
					if (m_data.m_typesSky[i]) copyOver1Frame(&m_skyDataGPU[j], i, frame1 + j);
				}
				m_lastFrameGPU = frame1 + j;
			}
			m_GPUFrames[j] = j; // Initialize GPU frames which we know is in order currently
		}
		return;
	}


	if (frame2 >= m_lastFrameGPU)
	{
		// Save frames onwards starting from frame1

		int range = frame1 + m_sizeGPUData - 1;
		if (range >= int(m_data.m_frames.size())) range = int(m_data.m_frames.size()) - 1;
		copyNewFrames(frame1, range);
		return;
	}
	if (frame1 < m_firstFrameGPU)
	{
		// Save previous frames, since it looks like we are going backwards

		int range = frame2 - m_sizeGPUData + 1;
		if (range < 0) range = 0;
		copyNewFrames(range, frame2);
		return;
	}
}

void cloudFile::copyNewFrames(int firstFrame, int lastFrame)
{
	// We should always check to copy max amount of frames
	if (lastFrame - firstFrame > m_sizeGPUData || lastFrame - firstFrame <= 0)
	{
		printf("Warning, unable to copy new frames over, firstFrame: %i, lastFrame: %i\n", firstFrame, lastFrame);
		return;
	}

	// Key = CPU frame
	// Value = (Index , Index in the GPU array) 
	std::unordered_map<int, std::pair<int, int>> overlapData2;

	// Check which frames overlap and need to move the values of
	for (int i = 0; i < m_lastFrameGPU - m_firstFrameGPU + 1; i++)
	{
		// Write overlapped frames
		// Saving (CPU frame), as key and (GPU array index) as value
		if (m_firstFrameGPU + i >= firstFrame && m_firstFrameGPU + i <= lastFrame) overlapData2[m_firstFrameGPU + i] = { i, m_GPUFrames[i] };
	}
	
	// So, new begin of frames will be #firstFrame, we put the old frames in the correct order, then we can copy over the other data
	for (int i = 0; i < m_sizeGPUData; i++)
	{
		// Loop over the new frames, starting at firstFrame
		// If we already had this value saved, we reuse it by setting it in the new correct position
		auto it = overlapData2.find(firstFrame + i);
		if (it != overlapData2.end())
		{
			// We found it! Pfew, lets reuse it and not copy!
			// We do this by swapping the values, after this, we have all the reusables at the correct place
			int value = m_GPUFrames[i];
			m_GPUFrames[i] = it->second.second;
			m_GPUFrames[it->second.first] = value;
		}
	}
	
	// Now that we have the reusable frames at the correct place, we ignore those indices
	// We don't loop over the full GPU size, since maybe we want to copy less, so loop over the inputted frame range
	for (int i = 0; i < lastFrame - firstFrame + 1; i++)
	{
		auto it = overlapData2.find(firstFrame + i);
		// If we did NOT find it, we will copy over
		if (it == overlapData2.end())
		{
			for (int j = 0; j < 11; j++)
			{
				if (m_data.m_typesSky[j]) copyOver1Frame(&m_skyDataGPU[m_GPUFrames[i]], j, firstFrame + i);
			}
		}
	}

	hipError_t err = hipGetLastError();
	if (err != hipSuccess) {
		std::cerr << "error: " << hipGetErrorString(err) << std::endl;
		assert(false);
	}


	// Set data
	m_firstFrameGPU = firstFrame;
	m_lastFrameGPU = lastFrame;
}

void cloudFile::copyOver1Frame(gridDataSkyGPU* dst, int type, int frame)
{
	hipMemcpyAsync(typeToPointerGPU(type, dst, nullptr, true), typeToPointer(type, &m_skyData[frame], &m_groundData[frame], true), m_params.size * sizeof(float), hipMemcpyHostToDevice, static_cast<hipStream_t>(getStream()));
}

#include "pch.h"

#include "game.h"

//Engine
#include "platform/cuda/cuda_render_gl.h"
#include "readTable.h"
#include "environment.h"
#include "editor.h"
#include "skewTer.h"
#include "microPhys.h"
#include "environment.cuh"
#include "dataClass.cuh"
#include "skewTMaker.h"
#include "skewTFile.h"

// Make the game a global variable on free store memory.
game Game;

// Global mutex to lock parts of code for threading 
// Recursive mutex for possible multiple locks
std::recursive_mutex mtx;

// Thread data
static std::thread simThread;
static std::atomic<bool> m_running;
static std::atomic<float> m_dt;
static std::atomic<float> m_speed;
constexpr float maxDeltaTimeSimulation = 1.0f / 2.5f; // set max delatime. 


game::game()
{
}

game::~game()
{
	//Cleanup from last created to first.
	delete m_skewTFileObj;
	delete m_skewTMakerObj;
	delete m_dataClassObj;
	delete m_microPhysObj;
	delete m_skewTerObj;
	delete m_envGPUObj;
	delete m_editorObj;
	delete m_environmentObj;
	delete m_readTableObj;
	delete m_cudaRenderObj;
}

void game::shutdown()
{
	// Don't put in the destructor due to this class being a global class
	m_running = false;
	if (simThread.joinable())
	{
		simThread.join();
	}

	m_cudaRenderObj->cleanUp();
}

void game::Initialize()
{
	m_cudaRenderObj = new CudaRender();
	m_readTableObj = new readTable();
#if USE_GPU
	m_envGPUObj = new environmentGPU();
	m_editorObj = new editor(m_envGPUObj->getDebugData());
#else
	m_environmentObj = new environment();
	m_editorObj = new editor(m_environmentObj->getDebugData());
#endif
	m_skewTerObj = new skewTer();
	m_microPhysObj = new microPhys();
	m_dataClassObj = new dataClass();
	m_skewTMakerObj = new skewTMaker();
	m_skewTFileObj = new skewTFile();

	// Initialize the simulation thread
	m_running = true;

#if USE_GPU
	simThread = std::thread([this]() {

		// Initialize deltatime at about 1 ms or larger
		auto time = std::chrono::high_resolution_clock::now();
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
		float accumulator = 0.0f;
		const float fixedDt = 1.0f / 15.0f; // Fps we want to target
		auto prevTime = std::chrono::high_resolution_clock::now();

		while (m_running)
		{
			float speed = m_speed.load();
			if (speed > 0.0f)
			{
				// Calculate deltatime based on time taken for previous update
				auto ctime = std::chrono::high_resolution_clock::now();
				float dt = std::chrono::duration<float>(ctime - prevTime).count();
				prevTime = ctime;
				dt = std::min(dt, maxDeltaTimeSimulation);

				accumulator += dt;

				accumulator = std::min(accumulator, fixedDt * 5.0f); // Make sure accumulator does not stack up higher and higher

				// Make sure the simulation does not run faster than it needs to be
				while (accumulator >= fixedDt)
				{
					// If slow, pass the current dt to speed up the simulation to real time. 
					const float passedDt = std::min(fixedDt, dt);
					m_envGPUObj->updateGPU(passedDt, speed);
					accumulator -= passedDt;
				}
			}
			else
			{
				accumulator = 0.0f;
			}

			// Sleep to increase dt, else it will round to 0, meaning accumulator will never add up
			std::this_thread::sleep_for(std::chrono::microseconds(100));
		}
		}
	);
#endif
}

//Update function 
void game::Update(float dt)
{
	
	m_editorObj->update(dt);
	m_editorObj->viewData();


#if USE_GPU

	// Set speed if valid, else set to 0.0 making the thread not run
	float speed = 1.0f;
	if (playSettings(speed))
	{
		m_dt.store(dt);
		m_speed.store(speed);
	}
	else
	{
		m_speed.store(0.0f);
	}
#else
	m_environmentObj->Update(dt, speed);
#endif
}


void game::Render()
{
	if (m_cudaRenderObj) m_cudaRenderObj->display();
}


bool game::playSettings(float& speed)
{
	speed = Game.Editor().getSpeed();

	//Play data
	if (!Game.Editor().getSimulate())
	{
		int step = Game.Editor().getStep();
		if (step > 0) Game.Editor().setStep(--step);
		else if (step < 0)
		{
			Game.Editor().setStep(++step);
			speed *= -1;
		}
		else return false;
	}
	return true;
}

void lockGlobal()
{
	mtx.lock();
}

void unlockGlobal()
{
	mtx.unlock();
}

#include "game.h"

//Engine

#include "readTable.h"
#include "environment.h"
#include "editor.h"
#include "skewTer.h"
#include "microPhys.h"
#include "environment.cuh"

// Make the game a global variable on free store memory.
game Game;


game::game()
{
}

game::~game()
{
	//Cleanup from last created to first.
	delete m_microPhysObj;
	delete m_skewTerObj;
	delete m_envGPUObj;
	delete m_editorObj;
	delete m_environmentObj;
	delete m_readTableObj;
}

void game::Initialize()
{
	m_readTableObj = new readTable();
	m_environmentObj = new environment();
	m_editorObj = new editor(m_environmentObj->getDebugData());
	m_envGPUObj = new environmentGPU();
	m_skewTerObj = new skewTer();
	m_microPhysObj = new microPhys();
}

//Update function 
void game::Update(float dt)
{
	float speed = 1.0f;
	if (playSettings(speed))
	{
#if USE_GPU
		//Also sets editor data afterwards
		m_envGPUObj->updateGPU(dt, speed);
#else
		m_environmentObj->Update(dt, speed);
#endif
	}
	m_editorObj->update(dt);
	m_editorObj->viewData();
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
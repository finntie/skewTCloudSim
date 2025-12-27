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
	m_environmentObj->Update(dt);
	m_editorObj->update(dt);
	m_editorObj->viewData();
}

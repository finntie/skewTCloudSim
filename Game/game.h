#pragma once

//Do we want to use the GPU?
#define USE_GPU 1

class environment;
class editor;
class readTable;
class skewTer;
class microPhys;
class environmentGPU;;


class game
{
public:
	game();
	~game();

	void Initialize();

	void Update(float dt);

	bool playSettings(float& speed);

	environment& Environment() { return *m_environmentObj; }
	readTable& ReadTable() { return *m_readTableObj; }
	editor& Editor() { return *m_editorObj; }
	skewTer& SkewT() { return *m_skewTerObj; }
	microPhys& mPhys() { return *m_microPhysObj; }
	environmentGPU& EnvGPU() { return *m_envGPUObj; }

private:

	environment* m_environmentObj = nullptr;
	readTable* m_readTableObj = nullptr;
	editor* m_editorObj = nullptr;
	skewTer* m_skewTerObj = nullptr;
	microPhys* m_microPhysObj = nullptr;
	environmentGPU* m_envGPUObj = nullptr;
};

extern game Game;

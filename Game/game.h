#pragma once



class environment;
class editor;
class readTable;
class skewTer;
class microPhys;
class environmentGPU;;
class dataClass;
class skewTMaker;
class skewTFile;

class game
{
public:
	game();
	~game();
	void shutdown();

	void Initialize();

	void Update(float dt);

	bool playSettings(float& speed);

	environment& Environment() { return *m_environmentObj; }
	readTable& ReadTable() { return *m_readTableObj; }
	editor& Editor() { return *m_editorObj; }
	skewTer& SkewT() { return *m_skewTerObj; }
	microPhys& mPhys() { return *m_microPhysObj; }
	environmentGPU& EnvGPU() { return *m_envGPUObj; }
	dataClass& DataClass() { return *m_dataClassObj; }
	skewTMaker& SkewTMaker() { return *m_skewTMakerObj; }
	skewTFile& SkewTFile() { return *m_skewTFileObj; }
private:

	environment* m_environmentObj = nullptr;
	readTable* m_readTableObj = nullptr;
	editor* m_editorObj = nullptr;
	skewTer* m_skewTerObj = nullptr;
	microPhys* m_microPhysObj = nullptr;
	environmentGPU* m_envGPUObj = nullptr;
	dataClass* m_dataClassObj = nullptr;
	skewTMaker* m_skewTMakerObj = nullptr;
	skewTFile* m_skewTFileObj = nullptr;

};

extern game Game;

// Lock and unlock global mutex
void lockGlobal();
void unlockGlobal();
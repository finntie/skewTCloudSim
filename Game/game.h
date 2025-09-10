#pragma once

class environment;
class editor;
class readTable;
class skewTer;
class microPhys;

class game
{
public:
	game();
	~game();

	void Initialize();

	void Update(float dt);

	environment& Environment() { return *m_environmentObj; }
	readTable& ReadTable() { return *m_readTableObj; }
	editor& Editor() { return *m_editorObj; }
	skewTer& SkewT() { return *m_skewTerObj; }
	microPhys& mPhys() { return *m_microPhysObj; }

private:

	environment* m_environmentObj = nullptr;
	readTable* m_readTableObj = nullptr;
	editor* m_editorObj = nullptr;
	skewTer* m_skewTerObj = nullptr;
	microPhys* m_microPhysObj = nullptr;
};

extern game Game;

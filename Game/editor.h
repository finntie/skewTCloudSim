#pragma once
class editor
{
public:

	editor();
	~editor() {};

	void panel();

private:

	void mainButtons();


	bool m_editMode{ false };

	int m_viewParamSky{ 0 };
	int m_viewParamGround{ 0 };
	int m_editParamSky{ 0 };

	int m_mousePointingIndex{ 0 };
	bool m_simulationActive{ false };
	int m_simulationStep{ 0	};
};


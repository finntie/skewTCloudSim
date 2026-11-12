#pragma once

class device;

class game
{
public:

	game();
	~game();
	
	void core();

	void imguiInit();

	void imguiUpdate();

	device& getDevice() { return *m_deviceObj; }

private:

	device* m_deviceObj{ nullptr };


};


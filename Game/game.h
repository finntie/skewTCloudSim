#pragma once

class device;

class game
{
public:

	game();
	~game();
	
	void core();

	void imguiInitTest();

	device& getDevice() { return *m_deviceObj; }

private:

	device* m_deviceObj{ nullptr };


};


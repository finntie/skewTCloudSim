#pragma once


struct GLFWwindow;

class device
{
public:

	device();
	~device();


	void render();

	bool windowShouldClose();

	GLFWwindow* getWindow() { return m_window; }

private:

	int m_width{ 0 };
	int m_height{ 0 };

	GLFWwindow* m_window{ nullptr };
};


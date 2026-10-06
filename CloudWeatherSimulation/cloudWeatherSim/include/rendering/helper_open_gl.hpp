#pragma once

#include <string>


class newShader
{

public:

    newShader() = default;
    ~newShader();
	unsigned int loadShader(const char* vertexShader, const char* fragmentShader);
    unsigned int loadShaderFromFile(const char* vertexShaderFile, const char* fragmentShaderFile);

    void useProgram();
    void unuseProgram();

private:


	std::string m_vertexShader;
    std::string m_fragmentShader;
	unsigned int m_program = 0;

};


void renderQuad();
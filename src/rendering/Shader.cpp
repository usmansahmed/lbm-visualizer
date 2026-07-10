#include "rendering/Shader.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

Shader::Shader(const std::string &vertexPath, const std::string &fragmentPath)
{

    std::ifstream vertexfile(vertexPath);
    std::ifstream fragmentfile(fragmentPath);

    if (!vertexfile.is_open())
    {
        throw std::runtime_error("Could not open file: " + vertexPath);
    }
    if (!fragmentfile.is_open())
    {
        throw std::runtime_error("Could not open file: " + fragmentPath);
    }

    std::stringstream buffer;
    buffer << vertexfile.rdbuf();
    const std::string vertexShaderSource = buffer.str();

    buffer.str("");
    buffer.clear();

    buffer << fragmentfile.rdbuf();
    const std::string fragmentShaderSource = buffer.str();

    const char *rawSource = vertexShaderSource.c_str();

    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &rawSource, nullptr);
    glCompileShader(vertexShader);

    rawSource = fragmentShaderSource.c_str();

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &rawSource, nullptr);
    glCompileShader(fragmentShader);

    programId_ = glCreateProgram();
    glAttachShader(programId_, vertexShader);
    glAttachShader(programId_, fragmentShader);

    glLinkProgram(programId_);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}

Shader::~Shader()
{
    glDeleteProgram(programId_);
}

void Shader::use() const
{
    glUseProgram(programId_);
}

void Shader::setInt(const std::string &name, int value) const
{
    glUniform1i(glGetUniformLocation(programId_, name.c_str()), value);
}

void Shader::setFloat(const std::string &name, float value) const
{
    glUniform1f(glGetUniformLocation(programId_, name.c_str()), value);
}

#include "rendering/Shader.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

Shader::Shader(const std::string &vertexPath, const std::string &fragmentPath)
{

    // Load vertex and fragment shader source files from disk.
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

    // Compile the vertex shader.
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &rawSource, nullptr);
    glCompileShader(vertexShader);

    rawSource = fragmentShaderSource.c_str();

    // Compile the fragment shader.
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &rawSource, nullptr);
    glCompileShader(fragmentShader);

    // Link both shaders into one OpenGL shader program.
    programId_ = glCreateProgram();
    glAttachShader(programId_, vertexShader);
    glAttachShader(programId_, fragmentShader);

    glLinkProgram(programId_);

    // The shader objects are no longer needed after linking.
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}

Shader::~Shader()
{
    // Delete the linked shader program.
    glDeleteProgram(programId_);
}

void Shader::use() const
{
    // Make this shader program active for the next draw calls.
    glUseProgram(programId_);
}

void Shader::setInt(const std::string &name, int value) const
{
    // Set an integer uniform, for example a texture unit index.
    glUniform1i(glGetUniformLocation(programId_, name.c_str()), value);
}

void Shader::setFloat(const std::string &name, float value) const
{
    // Set a float uniform, for example the visualization color range.
    glUniform1f(glGetUniformLocation(programId_, name.c_str()), value);
}
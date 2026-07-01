#include "Application.hpp"

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>

int main(int argc, char* argv[])
{
    try
    {
        const std::filesystem::path dataDirectory =
            argc >= 2
                ? std::filesystem::path{argv[1]}
                : std::filesystem::path{
                    "simulation_output"
                };

        Application application;
        application.run();
    }
    catch (const std::exception& exception)
    {
        std::cerr
            << "Application error: "
            << exception.what()
            << '\n';

        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
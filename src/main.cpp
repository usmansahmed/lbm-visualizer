#include "app/Application.hpp"

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <fstream>

int main()
{
    try
    {
        Application application;
        application.run();
    }
    catch (const std::exception& exception)
    {
        std::cerr
            << "Application error: "
            << exception.what()
            << '\n';

        std::cerr << "Press Enter to exit...";
        std::cin.get();

        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
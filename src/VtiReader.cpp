#include <string_view>
#include <string>
#include <tinyxml2.h>
#include <iostream>
#include <array>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <filesystem>

#include "VtiReader.hpp"

namespace {
tinyxml2::XMLElement* findDataArray(
    tinyxml2::XMLElement* parent,
    std::string_view requiredName)
{
    if (parent == nullptr)
        return nullptr;

    for (
        tinyxml2::XMLElement* array =
            parent->FirstChildElement("DataArray");

        array != nullptr;

        array =
            array->NextSiblingElement("DataArray"))
    {
        const char* name =
            array->Attribute("Name");

        if (
            name != nullptr &&
            requiredName == name
        )
        {
            return array;
        }
    }

    return nullptr;
}

std::array<int, 6> parseExtent(
    const char* extentText)
{
    if (extentText == nullptr)
    {
        throw std::runtime_error(
            "VTI extent is missing."
        );
    }

    std::istringstream stream{
        std::string{extentText}
    };

    std::array<int, 6> extent{};

    if (!(stream
            >> extent[0]
            >> extent[1]
            >> extent[2]
            >> extent[3]
            >> extent[4]
            >> extent[5]))
    {
        throw std::runtime_error(
            "Failed to parse VTI extent."
        );
    }

    return extent;
}
}

std::filesystem::path makeVtiFilePath(
    const std::filesystem::path& directory,
    int fileNumber)
{
    std::ostringstream filename;

    filename
        << "channel_"
        << std::setw(5)
        << std::setfill('0')
        << fileNumber
        << ".vti";

    return directory / filename.str();
}

SimulationFrame loadVtiFile(const std::filesystem::path& filePath)
{
    tinyxml2::XMLDocument document;

    const tinyxml2::XMLError loadResult =
        document.LoadFile(filePath.string().c_str());

    if (loadResult != tinyxml2::XML_SUCCESS)
    {
        throw std::runtime_error(
            "Could not load VTI file: " +
            filePath.string() +
            "\nTinyXML2 error: " +
            document.ErrorStr()
        );
    }

    tinyxml2::XMLElement* vtkFile =
        document.FirstChildElement("VTKFile");

    if (vtkFile == nullptr)
    {
        throw std::runtime_error(
            "VTKFile element was not found."
        );
    }

    const char* vtkType =
        vtkFile->Attribute("type");

    if (
        vtkType == nullptr ||
        std::string_view{vtkType} != "ImageData"
    )
    {
        throw std::runtime_error(
            "The file is not a VTI ImageData file."
        );
    }

    tinyxml2::XMLElement* imageData =
        vtkFile->FirstChildElement("ImageData");

    if (imageData == nullptr)
    {
        throw std::runtime_error(
            "ImageData element was not found."
        );
    }

    tinyxml2::XMLElement* piece =
        imageData->FirstChildElement("Piece");

    if (piece == nullptr)
    {
        throw std::runtime_error(
            "Piece element was not found."
        );
    }

    /*
     * Normally Piece has Extent. WholeExtent is used
     * as a fallback.
     */
    const char* extentText =
        piece->Attribute("Extent");

    if (extentText == nullptr)
    {
        extentText =
            imageData->Attribute("WholeExtent");
    }

    const std::array<int, 6> extent =
        parseExtent(extentText);

    SimulationFrame simulationFrame;
    simulationFrame.nx = extent[1] - extent[0] + 1;
    simulationFrame.ny = extent[3] - extent[2] + 1;
    simulationFrame.nz = extent[5] - extent[4] + 1;

    if (simulationFrame.nx <= 0 || simulationFrame.ny <= 0 || simulationFrame.nz <= 0)
    {
        throw std::runtime_error(
            "Invalid VTI grid dimensions."
        );
    }

    const std::size_t pointCount =
        static_cast<std::size_t>(simulationFrame.nx) *
        static_cast<std::size_t>(simulationFrame.ny) *
        static_cast<std::size_t>(simulationFrame.nz);

    tinyxml2::XMLElement* pointData =
        piece->FirstChildElement("PointData");

    if (pointData == nullptr)
    {
        throw std::runtime_error(
            "PointData element was not found."
        );
    }

    tinyxml2::XMLElement* velocityArray =
        findDataArray(
            pointData,
            "Velocity"
        );

    if (velocityArray == nullptr)
    {
        throw std::runtime_error(
            "Velocity DataArray was not found."
        );
    }

    const char* format =
        velocityArray->Attribute("format");

    if (
        format == nullptr ||
        std::string_view{format} != "ascii"
    )
    {
        throw std::runtime_error(
            "This loader supports only "
            "format=\"ascii\" VTI files."
        );
    }

    int numberOfComponents = 1;

    velocityArray->QueryIntAttribute(
        "NumberOfComponents",
        &numberOfComponents
    );

    if (numberOfComponents != 3)
    {
        throw std::runtime_error(
            "Velocity must contain three components."
        );
    }

    const char* velocityText =
        velocityArray->GetText();

    if (velocityText == nullptr)
    {
        throw std::runtime_error(
            "Velocity DataArray is empty."
        );
    }

    simulationFrame.velocityX.resize(pointCount);
    simulationFrame.velocityY.resize(pointCount);
    simulationFrame.velocityZ.resize(pointCount);

    std::istringstream velocityStream{
        std::string{velocityText}
    };

    for (std::size_t i = 0; i < pointCount; ++i)
    {
        if (!(velocityStream
                >> simulationFrame.velocityX[i]
                >> simulationFrame.velocityY[i]
                >> simulationFrame.velocityZ[i]))
        {
            throw std::runtime_error(
                "Failed to read velocity vector " +
                std::to_string(i)
            );
        }
    }

    std::cout
        << "Loaded: " << filePath << '\n'
        << "Grid dimensions: "
        << simulationFrame.nx << " x "
        << simulationFrame.ny << " x "
        << simulationFrame.nz << '\n'
        << "Velocity vectors: "
        << pointCount << '\n';

    return simulationFrame;
}
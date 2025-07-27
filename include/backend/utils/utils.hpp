#pragma once

#include <filesystem>
#include <iostream>

namespace utils
{

inline bool copySingleFile (const std::filesystem::path& from, const std::filesystem::path& to)
{
    try
    {
        // Optional: vorhandene Datei überschreiben
        std::filesystem::copy_file(from, to, std::filesystem::copy_options::overwrite_existing);
        return true;
    }
    catch(const std::filesystem::filesystem_error& e)
    {
        std::cerr << "Fehler beim Kopieren der Datei: " << e.what() << std::endl;
        return false;
    }
}

inline bool copyDirectory (const std::filesystem::path& source, const std::filesystem::path& destination)
{
    try
    {
        std::filesystem::copy(source, destination, std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing);
        return true;
    }
    catch(const std::filesystem::filesystem_error& e)
    {
        std::cerr << "Fehler beim Kopieren des Ordners: " << e.what() << std::endl;
        return false;
    }
}

inline bool checkDirectoryExist (const std::string& path) { return std::filesystem::exists(path) && std::filesystem::is_directory(path); }

inline bool checkFileExist (const std::string& path) { return std::filesystem::exists(path) && std::filesystem::is_regular_file(path); }

}  // namespace utils

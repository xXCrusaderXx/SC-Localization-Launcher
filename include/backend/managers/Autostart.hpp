#pragma once

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <system_error>

#ifdef _WIN32
#include <QSettings>
#include <QString>
#endif

#include "backend/utils/paths.hpp"
#include "backend/utils/platform.hpp"
#include "logging/logging.h"

// Starts the launcher at login: an XDG autostart entry on Linux (also for the AppImage and the Flatpak), the user's
// Run key on Windows. The entry passes --autostart, so the window can go straight to the tray.
class Autostart
{
   public:
    inline static const std::string APP_ID = "io.github.xXCrusaderXx.SCLocalizationLauncher";
    inline static const std::string ARGUMENT = "--autostart";

    // Folder of the autostart entries. Inside the Flatpak XDG_CONFIG_HOME is the sandbox's own; the desktop reads the
    // host's ~/.config/autostart, which the Flatpak may write (--filesystem=host).
    static std::filesystem::path entryFolder ()
    {
        std::error_code ec;
        if(std::filesystem::exists("/.flatpak-info", ec)) return PATHS::getUserRootPath() / ".config" / "autostart";
        return PATHS::xdgDir("XDG_CONFIG_HOME", ".config") / "autostart";
    }

    static std::filesystem::path entryFile (const std::filesystem::path& folder) { return folder / (APP_ID + ".desktop"); }

    // Quotes a program path for the Exec key of a desktop entry, so spaces and special characters survive. The value
    // is unescaped as a string first (\\ -> \) and then as a quoted argument (\" -> "), so a reserved character takes
    // two backslashes in the file and a backslash four.
    static std::string quoted (const std::string& program)
    {
        std::string result = "\"";
        for(char c : program)
        {
            if(c == '\\')
                result += "\\\\\\\\";
            else if(c == '"' || c == '`' || c == '$')
                result += std::string("\\\\") + c;
            // A field code starts with %, a literal one is doubled
            else if(c == '%')
                result += "%%";
            else
                result += c;
        }
        return result + "\"";
    }

#ifndef _WIN32
    // The Exec value that starts this launcher again the way it was installed; nullopt if it cannot be found
    static std::optional<std::string> command ()
    {
        std::error_code ec;
        if(std::filesystem::exists("/.flatpak-info", ec)) return "flatpak run " + APP_ID;
        // The AppImage runtime names the file that was started; the program itself runs from a temporary mount
        auto appImage = Platform::environmentPath("APPIMAGE");
        if(!appImage.empty()) return quoted(appImage.string());
        auto folder = Platform::executableFolder();
        if(!folder) return std::nullopt;
        return quoted((*folder / "sc-localization-launcher").string());
    }
#endif

    static std::string entry (const std::string& command)
    {
        return "[Desktop Entry]\n"
               "Type=Application\n"
               "Name=SC Localization Launcher\n"
               "Comment=Installs and updates community translations for Star Citizen\n"
               "Exec=" +
               command + " " + ARGUMENT +
               "\n"
               "Icon=" +
               APP_ID +
               "\n"
               "Terminal=false\n"
               "X-GNOME-Autostart-enabled=true\n";
    }

    // Creates or removes the entry. Written again on every start while switched on, so a moved AppImage is found.
    static bool apply (bool on, LoggerFramework::LogEx& lg)
    {
#ifdef _WIN32
        QSettings run("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
        if(!on)
        {
            run.remove("SC Localization Launcher");
            return true;
        }
        auto folder = Platform::executableFolder();
        if(!folder) return false;
        // The Run key takes a command line; Windows paths keep their backslashes
        run.setValue("SC Localization Launcher",
                     "\"" + QString::fromStdWString((*folder / "sc-localization-launcher.exe").wstring()) + "\" " + QString::fromStdString(ARGUMENT));
        run.sync();
        return run.status() == QSettings::NoError;
#else
        return apply(on, entryFolder(), command(), lg);
#endif
    }

    static bool apply (bool on, const std::filesystem::path& folder, const std::optional<std::string>& program, LoggerFramework::LogEx& lg)
    {
        std::error_code ec;
        auto file = entryFile(folder);
        if(!on)
        {
            std::filesystem::remove(file, ec);
            if(ec) LOG_ERROR(lg) << "Autostart - cannot remove " << file << " :: " << ec.message();
            return !ec;
        }
        if(!program)
        {
            LOG_ERROR(lg) << "Autostart - the launcher's own program was not found";
            return false;
        }
        std::filesystem::create_directories(folder, ec);
        // Written next to it and renamed, so the desktop never reads half an entry
        auto next = file;
        next += ".new";
        {
            std::ofstream out(next, std::ios::binary | std::ios::trunc);
            out << entry(*program);
            if(!out.good())
            {
                LOG_ERROR(lg) << "Autostart - cannot write " << next;
                std::filesystem::remove(next, ec);
                return false;
            }
        }
        std::filesystem::rename(next, file, ec);
        if(ec)
        {
            LOG_ERROR(lg) << "Autostart - cannot write " << file << " :: " << ec.message();
            std::filesystem::remove(next, ec);
            return false;
        }
        LOG_DEBUG(lg) << "Autostart - " << file << " starts " << *program;
        return true;
    }
};

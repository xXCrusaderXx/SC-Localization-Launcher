#pragma once

#include <git2.h>
#include <filesystem>
#include <iostream>

class GitRepoHandler
{
   private:
    std::string remoteUrl = "https://github.com/rjcncpt/StarCitizen-Deutsch-INI.git";
    std::string repoPath = std::filesystem::current_path() / "StarCitizen-Deutsch-INI";

   public:
    GitRepoHandler ();
    ~GitRepoHandler ();

    bool isUpToDate ();
    bool cloneRepo ();
    bool checkUpdates ();
};

#pragma once

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <optional>
#include <regex>
#include <string>
#include <vector>

// A translation project's repository on GitHub, "owner/name" as languages.json names it, and the files of it that a
// language downloads. GitHub compares owner and name without regard to case.
namespace GitHubRepository
{

inline const std::string PAGE_PREFIX = "https://github.com/";

// Where raw.githubusercontent.com serves the files of a branch: <prefix><owner>/<name>/<branch>/<path>
inline std::string rawPrefix ()
{
#ifdef SC_TESTING
    // Test builds only: the tests serve the translation files from a local server
    if(const char* url = std::getenv("SC_GITHUB_RAW_URL")) return url;
#endif
    return "https://raw.githubusercontent.com/";
}

inline std::string lower (std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(), [] (unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

// GitHub's own rules: an owner of up to 39 letters, digits and inner hyphens, a name of letters, digits, '.', '_' and '-'
inline bool isValid (const std::string& repository)
{
    static const std::regex pattern("[A-Za-z0-9](?:[A-Za-z0-9]|-(?=[A-Za-z0-9])){0,38}/[A-Za-z0-9._-]{1,100}");
    if(!std::regex_match(repository, pattern)) return false;
    std::string name = repository.substr(repository.find('/') + 1);
    return name != "." && name != ".." && !lower(name).ends_with(".git");
}

// "owner/name" of a page on github.com or a file on raw.githubusercontent.com; empty for any other address
inline std::string of (const std::string& url)
{
    for(const auto& prefix : {PAGE_PREFIX, rawPrefix()})
    {
        if(!url.starts_with(prefix)) continue;
        std::string rest = url.substr(prefix.size());
        size_t slash = rest.find('/');
        if(slash == std::string::npos) return {};
        std::string repository = rest.substr(0, rest.find_first_of("/?#", slash + 1));
        return isValid(repository) ? repository : std::string();
    }
    return {};
}

inline bool same (const std::string& a, const std::string& b) { return lower(a) == lower(b); }

// A languages.json entry may only name the repository its files or its project page are on
inline bool belongsTo (const std::string& repository, const std::string& projectUrl, const std::vector<std::string>& fileUrls)
{
    if(!isValid(repository)) return false;
    std::string project = projectUrl;
    while(project.ends_with("/")) project.pop_back();
    if(same(project, PAGE_PREFIX + repository)) return true;
    return std::any_of(fileUrls.begin(), fileUrls.end(), [&] (const std::string& url) { return same(of(url), repository); });
}

struct File
{
    std::string branch;
    // Inside the repository, e.g. "live/global.ini"
    std::string path;
};

// The branch and path of a raw.githubusercontent.com file of this repository; nullopt for any other address
inline std::optional<File> fileOf (const std::string& url, const std::string& repository)
{
    std::string prefix = rawPrefix() + repository + "/";
    if(!isValid(repository) || url.size() <= prefix.size() || !same(url.substr(0, prefix.size()), prefix)) return std::nullopt;
    std::string rest = url.substr(prefix.size());
    if(rest.find_first_of("?#%\\") != std::string::npos) return std::nullopt;
    if(rest.starts_with("refs/heads/")) rest.erase(0, 11);
    size_t slash = rest.find('/');
    if(slash == 0 || slash == std::string::npos || slash + 1 == rest.size()) return std::nullopt;
    File file{rest.substr(0, slash), rest.substr(slash + 1)};
    // Only plain path components; the path is sent to the API, never used on disk
    for(size_t start = 0; start <= file.path.size();)
    {
        size_t end = std::min(file.path.find('/', start), file.path.size());
        std::string part = file.path.substr(start, end - start);
        if(part.empty() || part == "." || part == "..") return std::nullopt;
        start = end + 1;
    }
    return file;
}

}  // namespace GitHubRepository

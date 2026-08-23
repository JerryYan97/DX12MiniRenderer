#include "StrPathUtils.h"
#include <vector>
#include <filesystem>
#include <algorithm>
#include <math.h>
#include <cstdint>

std::string GetFileDir(
    const std::string& pathName)
{
    size_t pos = 0;

    size_t p1 = pathName.rfind("\\");
    size_t p2 = pathName.rfind("/");

    if (p1 != std::string::npos)
    {
        pos = std::max(pos, p1);
    }

    if (p2 != std::string::npos)
    {
        pos = std::max(pos, p2);
    }

    return pathName.substr(0, pos);
}

bool IsFile(const std::string& pathName)
{
    size_t found = pathName.find_last_of(".");
    if (found != std::string::npos)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool IsAbsolutePath(const std::string& pathName)
{
    size_t found = pathName.find_first_of(":");
    if (found != std::string::npos)
    {
        return true;
    }
    else
    {
        return false;
    }
}

void CleanOrCreateDir(const std::string& dir)
{
    if (std::filesystem::exists(dir))
    {
        std::filesystem::remove_all(dir);
    }

    std::filesystem::create_directory(dir);
}

bool GetAbsolutePathName(
    const std::string& pathName,
    std::string& output)
{
    if (IsAbsolutePath(pathName))
    {
        output = pathName;
        return true;
    }
    else
    {
        std::filesystem::path workPath = std::filesystem::current_path();
        std::string workPathStr = workPath.string();

        size_t found = pathName.find_first_of(".");
        if (found == std::string::npos)
        {
            return false;
        }

        std::string pathNameSubStr = pathName.substr(found + 1);
        std::replace(pathNameSubStr.begin(), pathNameSubStr.end(), '/', '\\');

        output = workPathStr + pathNameSubStr;
        return true;
    }
}

void GetAllFileNames(
    const std::string& dir,
    std::vector<std::string>& outputVec)
{
    for (const auto& entry : std::filesystem::directory_iterator(dir))
    {
        outputVec.push_back(entry.path().filename().string());
    }
}

bool GetFilePostfix(
    const std::string& pathName,
    std::string&       postfix)
{
    size_t found = pathName.find_last_of(".");
    if (found != std::string::npos)
    {
        postfix = pathName.substr(found + 1);
        return true;
    }
    else
    {
        return false;
    }
}

uint32_t GetFileCountByExtension(const std::string& dir, const std::string& extension, bool recursive)
{
    if (!std::filesystem::exists(dir) || !std::filesystem::is_directory(dir))
    {
        return 0;
    }

    std::string ext = extension;
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (!ext.empty() && ext[0] != '.')
    {
        ext = "." + ext;
    }

    uint32_t count = 0;

    if (recursive)
    {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(dir))
        {
            if (!entry.is_regular_file())
            {
                continue;
            }

            std::string entryExt = entry.path().extension().string();
            std::transform(entryExt.begin(), entryExt.end(), entryExt.begin(), ::tolower);
            if (entryExt == ext)
            {
                count++;
            }
        }
    }
    else
    {
        for (const auto& entry : std::filesystem::directory_iterator(dir))
        {
            if (!entry.is_regular_file())
            {
                continue;
            }

            std::string entryExt = entry.path().extension().string();
            std::transform(entryExt.begin(), entryExt.end(), entryExt.begin(), ::tolower);
            if (entryExt == ext)
            {
                count++;
            }
        }
    }

    return count;
}

std::string GetRootPath()
{
#if defined(RELEASE_PKG)
    // TODO: Add proper root path for release package.
    return "";
#else
    return SOURCE_PATH;
#endif
}
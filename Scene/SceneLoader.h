#pragma once
#include <string>

class Level;

class SceneLoader
{
public:
    SceneLoader();
    ~SceneLoader();

    // Load a scene file into a new level
    void LoadAsLevel(const std::string& fileNamePath, Level* o_pLevel);

    // Load a scene file into the input level
    void LoadInLevel(Level* i_pLevel, Level* o_pSubLevel) {}

private:
};
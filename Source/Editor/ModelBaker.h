#pragma once

#include "Common.h"

#include "FileSystem.h"

namespace fbxsdk
{
    class FbxManager;
}

class ModelBaker : public NonCopyable
{
public:
    ModelBaker(FileCollection& all_files);
    ~ModelBaker();
    void AutoBakeModels();
    void FillBakedFiles(map<string, UCharVec>& baked_files);

private:
    UCharVec BakeFile(const string& fname, File& file);

    FileCollection& allFiles;
    map<string, UCharVec> bakedFiles {};
    fbxsdk::FbxManager* fbxManager {};
};

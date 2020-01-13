#pragma once

#include "Common.h"

class File;
class FileCollection;
namespace glslang
{
    class TIntermediate;
}

class EffectBaker : public NonCopyable
{
public:
    EffectBaker(FileCollection& all_files);
    ~EffectBaker();
    void AutoBakeEffects();
    void FillBakedFiles(map<string, UCharVec>& baked_files);

private:
    void BakeShaderProgram(const string& fname, const string& content);
    void BakeShaderStage(const string& fname_wo_ext, glslang::TIntermediate* intermediate);

    FileCollection& allFiles;
    map<string, UCharVec> bakedFiles;
    std::mutex bakedFilesLocker;
};

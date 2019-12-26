#pragma once

#include "Common.h"

class IImageBaker;
using ImageBaker = shared_ptr<IImageBaker>;
class FileCollection;

class IImageBaker : public NonCopyable
{
public:
    static ImageBaker Create(FileCollection& all_files);
    virtual void AutoBakeImages() = 0;
    virtual void BakeImage(const string& fname_with_opt) = 0;
    virtual void FillBakedFiles(map<string, UCharVec>& baked_files) = 0;
    virtual ~IImageBaker() = default;
};

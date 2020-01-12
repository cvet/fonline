#pragma once

#include "Common.h"

class IModelBaker;
using ModelBaker = shared_ptr<IModelBaker>;
class FileCollection;

class IModelBaker : public NonCopyable
{
public:
    static ModelBaker Create(FileCollection& all_files);
    virtual void AutoBakeModels() = 0;
    virtual void FillBakedFiles(map<string, UCharVec>& baked_files) = 0;
    virtual ~IModelBaker() = default;
};

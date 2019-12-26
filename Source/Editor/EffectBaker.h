#pragma once

#include "Common.h"

class IEffectBaker;
using EffectBaker = shared_ptr<IEffectBaker>;
class FileCollection;

class IEffectBaker : public NonCopyable
{
public:
    static EffectBaker Create(FileCollection& all_files);
    virtual void AutoBakeEffects() = 0;
    virtual void FillBakedFiles(map<string, UCharVec>& baked_files) = 0;
    virtual ~IEffectBaker() = default;
};

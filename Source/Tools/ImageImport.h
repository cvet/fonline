// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#include "FileSystem.h"

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(ImageBakerException);

namespace ImageImport
{
    inline constexpr int32_t MAX_DIRS_MINUS_ONE {7};

    struct FrameShot
    {
        isize32 Size {};
        ipos32 Offset {};
        ipos32 Motion {};
        timespan Duration {};
        vector<uint8_t> Data {};
        bool Shared {};
        int32_t SharedIndex {};
        bool PreserveHiddenColor {};
    };

    struct FrameSequence
    {
        int16_t OffsX {};
        int16_t OffsY {};
        vector<FrameShot> Frames {};
    };

    struct FrameCollection
    {
        FrameSequence Main {};
        array<FrameSequence, MAX_DIRS_MINUS_ONE> Dirs {};
        string NewName {};
    };

    struct Context
    {
        ptr<const FileCollection> Files;
        function<FrameCollection(string_view)> Resolve {};
    };

    using LoadFunc = copyable_function<FrameCollection(string_view, string_view, FileReader, const Context&)>;
    using OutputResolver = copyable_function<string(string_view)>;

    struct DependencyReference
    {
        string Path {};
        bool Optional {};
    };

    using DependencyDiscovery = copyable_function<vector<DependencyReference>(string_view, const FileCollection&)>;

    struct Loader
    {
        LoadFunc Load {};
        OutputResolver ResolveOutput {};
        DependencyDiscovery DiscoverDependencies {};
    };

    void CheckEnabled(string_view fname);
    void SetFrameSequenceDuration(FrameSequence& sequence, timespan duration);
    auto GetDirectionCount(const FrameCollection& collection) -> uint8_t;
    auto GetDuration(const FrameSequence& sequence) -> timespan;
}

FO_END_NAMESPACE

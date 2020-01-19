#pragma once

#include "Common.h"

#include "FileSystem.h"

DECLARE_EXCEPTION(ImageBakerException);

class ImageBaker : public NonCopyable
{
public:
    ImageBaker(FileCollection& all_files);
    void AutoBakeImages();
    void BakeImage(const string& fname_with_opt);
    void FillBakedFiles(map<string, UCharVec>& baked_files);

private:
    static const int MaxFrameSequence = 50;
    static const int MaxDirsMinusOne = 7;

    struct FrameShot : public NonCopyable
    {
        ushort Width {};
        ushort Height {};
        short NextX {};
        short NextY {};
        UCharVec Data {};
        bool Shared {};
        ushort SharedIndex {};
    };

    struct FrameSequence : public NonCopyable
    {
        short OffsX {};
        short OffsY {};
        FrameShot Frames[MaxFrameSequence] {};
    };

    struct FrameCollection : public NonCopyable
    {
        ushort SequenceSize {};
        ushort AnimTicks {};
        FrameSequence Main {};
        bool HaveDirs {};
        FrameSequence Dirs[MaxDirsMinusOne] {};
        string EffectName {};
        string NewExtension {};
    };

    using LoadFunc = std::function<FrameCollection(const string&, const string&, File&)>;

    void ProcessImages(const string& target_ext, LoadFunc loader);
    void BakeCollection(const string& fname, const FrameCollection& collection);

    FrameCollection LoadAny(const string& fname_with_opt);
    FrameCollection LoadFofrm(const string& fname, const string& opt, File& file);
    FrameCollection LoadFrm(const string& fname, const string& opt, File& file);
    FrameCollection LoadFrX(const string& fname, const string& opt, File& file);
    FrameCollection LoadRix(const string& fname, const string& opt, File& file);
    FrameCollection LoadArt(const string& fname, const string& opt, File& file);
    FrameCollection LoadSpr(const string& fname, const string& opt, File& file);
    FrameCollection LoadZar(const string& fname, const string& opt, File& file);
    FrameCollection LoadTil(const string& fname, const string& opt, File& file);
    FrameCollection LoadMos(const string& fname, const string& opt, File& file);
    FrameCollection LoadBam(const string& fname, const string& opt, File& file);
    FrameCollection LoadPng(const string& fname, const string& opt, File& file);
    FrameCollection LoadTga(const string& fname, const string& opt, File& file);

    FileCollection& allFiles;
    map<string, UCharVec> bakedFiles {};
    unordered_map<string, File> cachedFiles {};
};

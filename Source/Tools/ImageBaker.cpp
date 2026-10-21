//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <aka.cvet@gmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#include "ImageBaker.h"
#include "Application.h"
#include "ConfigFile.h"
#include "FileSystem.h"
#include "ImageBaker-Art.h"
#include "ImageBaker-Bam.h"
#include "ImageBaker-Fofrm.h"
#include "ImageBaker-Frm.h"
#include "ImageBaker-Mos.h"
#include "ImageBaker-Png.h"
#include "ImageBaker-Rix.h"
#include "ImageBaker-Spr.h"
#include "ImageBaker-Tga.h"
#include "ImageBaker-Til.h"
#include "ImageBaker-Zar.h"
#include "ImageDependencyCache.h"
#include "ImageDependencyGraph.h"
#include "Settings.h"
#include "SpriteMeshing.h"
#include "SpriteResource.h"

FO_BEGIN_NAMESPACE

static auto LoadImage(const ImageImport::Loader& loader, string_view fname, string_view opt, FileReader reader, const ImageImport::Context& context) -> ImageImport::FrameCollection;
static auto PadSpriteFrame(const ImageImport::FrameShot& shot, int32_t padding) -> ImageImport::FrameShot;
static auto ResolveSpriteFramePadding(const ImageImport::FrameShot& shot, const BakedSpriteMesh& mesh, int32_t build_padding) -> int32_t;
static void TranslateSpriteMesh(BakedSpriteMesh& mesh, int32_t offset, const ImageImport::FrameShot& shot);
static auto CropSpriteFrameToMeshBounds(const ImageImport::FrameShot& shot, const ImageImport::FrameShot& source_shot, int32_t padding, BakedSpriteMesh& mesh) -> optional<ImageImport::FrameShot>;

struct SpriteInfoBakingStats
{
    uint64_t Collections {};
    uint64_t Directions {};
    uint64_t FrameSlots {};
    uint64_t UniqueFrames {};
    uint64_t SharedFrameReferences {};
};

ImageBaker::ImageBaker(shared_ptr<BakingContext> ctx) :
    BaseBaker(std::move(ctx), NAME, BakingOutputKind::Resource)
{
    AddLoader(ImageBakerFofrm::Load, {"fofrm"});
#if FO_IMAGE_FRM
    AddLoader(ImageBakerFrm::Load, {"frm"}, ImageBakerFrm::ResolveOutput, ImageBakerFrm::DiscoverDependencies);
    AddLoader(ImageBakerFrm::LoadFrX, {"fr0"}, ImageBakerFrm::ResolveOutput, ImageBakerFrm::DiscoverFrXDependencies);
#endif
#if FO_IMAGE_RIX
    AddLoader(ImageBakerRix::Load, {"rix"});
#endif
#if FO_IMAGE_ART
    AddLoader(ImageBakerArt::Load, {"art"});
#endif
#if FO_IMAGE_SPR
    AddLoader(ImageBakerSpr::Load, {"spr"});
#endif
#if FO_IMAGE_ZAR
    AddLoader(ImageBakerZar::Load, {"zar"});
#endif
#if FO_IMAGE_TIL
    AddLoader(ImageBakerTil::Load, {"til"});
#endif
#if FO_IMAGE_MOS
    AddLoader(ImageBakerMos::Load, {"mos"});
#endif
#if FO_IMAGE_BAM
    AddLoader(ImageBakerBam::Load, {"bam"});
#endif
    AddLoader(ImageBakerPng::Load, {"png"});
    AddLoader(ImageBakerTga::Load, {"tga"});
}

void ImageBaker::AddLoader(const ImageImport::LoadFunc& loader, const vector<string>& file_extensions, const ImageImport::OutputResolver& output_resolver, const ImageImport::DependencyDiscovery& dependency_discovery)
{
    for (const auto& ext : file_extensions) {
        _fileLoaders[ext] = ImageImport::Loader {loader, output_resolver, dependency_discovery};
    }
}

auto ImageBaker::CollectOutputCandidates(string_view pack, const FileCollection& files) const -> vector<ImageOutputCandidate>
{
    vector<ImageOutputCandidate> candidates;

    for (const FileHeader& source : files) {
        auto loader = _fileLoaders.find(strex(source.GetPath()).get_file_extension());

        if (loader == _fileLoaders.end()) {
            continue;
        }

        string path {source.GetPath()};
        candidates.push_back({string {pack}, {string {source.GetDataSource()->GetPackName()}, path}, loader->second.ResolveOutput ? loader->second.ResolveOutput(path) : path});
    }

    return candidates;
}

void ImageBaker::BakeFiles(const FileCollection& files, string_view target_path) const
{
    FO_TRACE_ZONE(Baking);

    shared_ptr<const ImageDependencyGraph> graph = _context->GetImageDependencies();

    if (!graph) {
        ImageSourceCatalog sources;
        sources.AddFiles(files);
        auto candidates = CollectOutputCandidates(_context->GetPackName(), files);
        graph = safe_alloc::make_shared<ImageDependencyGraph>(BuildImageDependencyGraph(std::move(sources), candidates, _fileLoaders, MakeImageBakingFingerprint(*_context->GetSettings())));
    }

    const FileCollection& source_files = graph->Files;
    const auto& root_outputs = GetRootOutputs(graph->Outputs, _context->GetPackName());
    string sprite_info_path = strex("{}/{}.foinfo", SPRITE_INFO_DIRECTORY, _context->GetPackName()).str();
    bool scan_mode = target_path.empty();
    bool bake_sprite_info = scan_mode || target_path == sprite_info_path;
    bool cache_enabled = !_context->GetSettings()->Baking.BakeOutput.empty();
    bool force = _context->GetSettings()->Baking.ForceBaking;

    if (!bake_sprite_info && !std::ranges::any_of(root_outputs, [target_path](const ImageOutputCandidate& output) { return output.ResourcePath == target_path; })) {
        return;
    }

    if (IsBakingReportEnabled()) {
        SpriteMeshBakeConfig mesh_config = ResolveSpriteMeshBakeConfig(_context->GetSettings());
        RecordSpriteMeshBakingSettings(SpriteMeshBakingReportSettings {
            .Enabled = mesh_config.Enabled,
            .AlphaThreshold = mesh_config.AlphaThreshold,
            .MaxTriangles = mesh_config.MaxTriangles,
            .AreaSavingsWeight = mesh_config.AreaSavingsWeight,
            .BaseDilation = SPRITE_MESH_DILATION,
            .MaximumPadding = SPRITE_MESH_MAXIMUM_PADDING,
        });
    }

    for (const FileHeader& file : files) {
        ImageImport::CheckEnabled(file.GetPath());
    }

    map<string, SpriteInfoFileEntry> sprite_info_entries;
    nptr<const FileSystem> sprite_info_files = _context->GetPackBakedFiles() ? _context->GetPackBakedFiles() : _context->GetBakedFiles();
    FO_VERIFY_AND_THROW(sprite_info_files, "Baker context has no baked file registry");

    if (bake_sprite_info && !cache_enabled && sprite_info_files->IsFileExists(sprite_info_path)) {
        File existing = sprite_info_files->ReadFile(sprite_info_path);
        FO_VERIFY_AND_THROW(existing, "Baked sprite info resource is not readable", sprite_info_path);

        for (SpriteInfoFileEntry& entry : ReadSpriteInfoFile(sprite_info_path, existing.GetStr())) {
            bool inserted = sprite_info_entries.emplace(entry.SourcePath, std::move(entry)).second;
            FO_VERIFY_AND_THROW(inserted, "Baked sprite info resource contains a duplicate source path", sprite_info_path);
        }
    }

    struct ImageWork
    {
        File Input;
        ImageImport::Loader Loader;
        ImageOutputCandidate Output;
        string Fingerprint;
    };

    vector<ImageWork> work;
    set<string> current_sprite_sources;
    uint64_t maximum_source_write_time = 0;

    for (const auto& output : root_outputs) {
        current_sprite_sources.emplace(output.Source.Path);
        auto source = graph->Sources.Find(output.Source.Path);
        FO_VERIFY_AND_THROW(source, "Image output source is missing from dependency snapshot", output.Source.Path);
        maximum_source_write_time = std::max(maximum_source_write_time, source->GetWriteTime());

        if (!bake_sprite_info && output.ResourcePath != target_path) {
            continue;
        }

        if (_context->IsOutputDiscovery()) {
            (void)CheckOutput(output.ResourcePath, source->GetWriteTime(), false);
            continue;
        }

        string fingerprint = GetOutputFingerprint(*graph, output);
        optional<SpriteInfoFileEntry> snapshot;
        optional<bool> required;

        if (cache_enabled) {
            snapshot = ReadImageDependencySnapshot(*_context->GetSettings(), output, fingerprint);
            required = force || !snapshot.has_value();
        }

        bool scheduled = CheckOutput(output.ResourcePath, source->GetWriteTime(), required);

        if (!scheduled) {
            if (snapshot && bake_sprite_info) {
                sprite_info_entries.insert_or_assign(output.Source.Path, std::move(*snapshot));
            }

            continue;
        }

        work.push_back({File::Load(*source), _fileLoaders.at(strex(output.Source.Path).get_file_extension()), output, std::move(fingerprint)});
    }

    bool sprite_info_needs_write = !_context->HasBakeChecker();
    string pack_fingerprint = GetPackFingerprint(*graph, _context->GetPackName());

    if (bake_sprite_info) {
        _context->KeepOutput(sprite_info_path);
        optional<bool> required;

        if (_context->IsOutputDiscovery()) {
            required = false;
        }
        else if (cache_enabled) {
            required = force || !IsImagePackInfoCurrent(*_context->GetSettings(), _context->GetPackName(), pack_fingerprint);
        }

        sprite_info_needs_write = CheckOutput(sprite_info_path, maximum_source_write_time, required);
    }

    if (_context->IsOutputDiscovery()) {
        return;
    }

    vector<std::future<SpriteInfoFileEntry>> tasks;

    for (size_t index = 0; index < work.size(); index++) {
        tasks.emplace_back(run_async(GetAsyncMode(), strex("BakeImage-{}", work[index].Output.Source.Path), [this, &source_files, &work, index]() FO_DEFERRED -> SpriteInfoFileEntry {
            auto& item = work[index];
            string_view path = item.Input.GetPath();
            ImageImport::Context context {&source_files, [this, &source_files](string_view nested_path) { return LoadAny(nested_path, source_files); }};
            auto collection = LoadImage(item.Loader, path, "", item.Input.GetReader(), context);
            string actual_output = collection.NewName.empty() ? string {path} : collection.NewName;
            FO_VERIFY_AND_THROW(actual_output == item.Output.ResourcePath, "Image loader output requires a matching ResolveOutput declaration", path, actual_output, item.Output.ResourcePath);

            return BakeCollection(path, collection);
        }));
    }

    size_t errors = 0;
    string first_error;
    vector<SpriteInfoFileEntry> completed;
    completed.reserve(tasks.size());

    for (size_t index = 0; index < tasks.size(); index++) {
        try {
            SpriteInfoFileEntry entry = tasks[index].get();
            sprite_info_entries.insert_or_assign(entry.SourcePath, entry);
            completed.emplace_back(std::move(entry));
        }
        catch (const std::exception& ex) {
            logging::write("Image baking error for '{}': {}", work[index].Output.Source.Path, ex.what());

            if (errors == 0) {
                first_error = ex.what();
            }

            errors++;
        }
    }

    if (errors != 0) {
        throw ImageBakerException("Errors during images baking", errors, first_error);
    }

    if (bake_sprite_info) {
        size_t previous_entry_count = sprite_info_entries.size();
        std::erase_if(sprite_info_entries, [&current_sprite_sources](const auto& entry) { return !current_sprite_sources.contains(entry.first); });
        bool removed_entries = previous_entry_count != sprite_info_entries.size();
        FO_VERIFY_AND_THROW(sprite_info_entries.size() == current_sprite_sources.size(), "Sprite info index is incomplete; perform a full resource rebake", _context->GetPackName(), sprite_info_entries.size(), current_sprite_sources.size());

        if (sprite_info_needs_write || !work.empty() || removed_entries) {
            vector<SpriteInfoFileEntry> entries;
            entries.reserve(sprite_info_entries.size());

            for (const auto& [path, entry] : sprite_info_entries) {
                ignore_unused(path);
                entries.emplace_back(entry);
            }

            string text = WriteSpriteInfoFile(entries);
            _context->WriteData(sprite_info_path, make_const_span(text));
        }
    }

    for (size_t index = 0; index < completed.size(); index++) {
        WriteImageDependencySnapshot(*_context->GetSettings(), work[index].Output, work[index].Fingerprint, completed[index]);
    }

    if (bake_sprite_info && cache_enabled) {
        WriteImagePackInfoSnapshot(*_context->GetSettings(), _context->GetPackName(), pack_fingerprint);
    }
}

auto ImageBaker::BakeCollection(string_view fname, const ImageImport::FrameCollection& collection) const -> SpriteInfoFileEntry
{
    FO_TRACE_ZONE(Baking);

    vector<uint8_t> data;
    auto writer = data_writer(data);

    auto dirs = ImageImport::GetDirectionCount(collection);
    SpriteMeshBakeConfig mesh_config = ResolveSpriteMeshBakeConfig(_context->GetSettings());
    string output_path = collection.NewName.empty() ? string(fname) : collection.NewName;
    SpriteInfoFileEntry sprite_info_entry {
        .SourcePath = string(fname),
        .ResourcePath = output_path,
        .Info =
            {
                .Directions = vector<SpriteDirInfo>(dirs),
            },
    };
    bool report_enabled = IsBakingReportEnabled();
    vector<SpriteMeshBakingFrameReport> frame_reports;
    SpriteInfoBakingStats stats {
        .Collections = 1,
        .Directions = dirs,
    };

    writer.write<uint8_t>(SPRITE_RESOURCE_MAGIC);
    writer.write<uint8_t>(SPRITE_RESOURCE_VERSION);
    writer.write<uint8_t>(dirs);

    for (uint8_t dir = 0; dir < dirs; dir++) {
        const auto& sequence = dir == 0 ? collection.Main : collection.Dirs[dir - 1];
        SpriteDirInfo& direction_info = sprite_info_entry.Info.Directions[dir];
        uint32_t duration = ImageImport::GetDurationMs(sequence);
        writer.write<uint16_t>(numeric_cast<uint16_t>(sequence.Frames.size()));
        writer.write<uint32_t>(duration);
        direction_info.Frames.resize(sequence.Frames.size());
        vector<optional<BakedSpriteMesh>> prepared_meshes(sequence.Frames.size());
        vector<int32_t> mesh_build_paddings(sequence.Frames.size());
        vector<int32_t> frame_paddings(sequence.Frames.size());
        vector<bool> preserve_pixels(sequence.Frames.size());

        for (size_t index = 0; index < sequence.Frames.size(); index++) {
            const auto& shot = sequence.Frames[index];
            FO_VERIFY_AND_THROW(!shot.Shared || shot.SharedIndex < index, "Shared sprite frame points outside previously baked frames", fname, dir, index, shot.SharedIndex);
            FO_VERIFY_AND_THROW(shot.Size.width >= 0 && shot.Size.height >= 0 && shot.Size.width <= std::numeric_limits<uint16_t>::max() && shot.Size.height <= std::numeric_limits<uint16_t>::max(), "Sprite frame dimensions are outside the serialized range", fname, dir, index, shot.Size);
            FO_VERIFY_AND_THROW(shot.Motion.x >= std::numeric_limits<int16_t>::min() && shot.Motion.x <= std::numeric_limits<int16_t>::max() && shot.Motion.y >= std::numeric_limits<int16_t>::min() && shot.Motion.y <= std::numeric_limits<int16_t>::max(), "Sprite frame motion is outside the serialized range", fname, dir, index, shot.Motion);

            if (shot.PreserveHiddenColor) {
                size_t owner = index;

                while (sequence.Frames[owner].Shared) {
                    owner = sequence.Frames[owner].SharedIndex;
                }

                preserve_pixels[owner] = true;
            }
        }

        if (mesh_config.Enabled) {
            for (size_t frame_index = 0; frame_index < sequence.Frames.size(); frame_index++) {
                const ImageImport::FrameShot& shot = sequence.Frames[frame_index];

                if (shot.Shared || preserve_pixels[frame_index]) {
                    continue;
                }

                optional<ImageImport::FrameShot> maximum_padded_shot;
                ptr<const ImageImport::FrameShot> mesh_shot = &shot;

                if (shot.Size.width > 0 && shot.Size.height > 0) {
                    maximum_padded_shot = PadSpriteFrame(shot, SPRITE_MESH_MAXIMUM_PADDING);
                    mesh_shot = &*maximum_padded_shot;
                    mesh_build_paddings[frame_index] = SPRITE_MESH_MAXIMUM_PADDING;
                }

                int64_t reference_quad_double_area = numeric_cast<int64_t>(shot.Size.width) * shot.Size.height * 2;
                prepared_meshes[frame_index] = BuildSpriteMesh(mesh_shot->Data, mesh_shot->Size, mesh_config, reference_quad_double_area);
                frame_paddings[frame_index] = ResolveSpriteFramePadding(shot, *prepared_meshes[frame_index], mesh_build_paddings[frame_index]);
            }
        }

        for (size_t i = 0; i < sequence.Frames.size(); i++) {
            const auto& shot = sequence.Frames[i];
            SpriteFrameInfo& frame_info = direction_info.Frames[numeric_cast<size_t>(i)];
            stats.FrameSlots++;

            if (!shot.Shared) {
                stats.UniqueFrames++;
                int32_t frame_padding = frame_paddings[numeric_cast<size_t>(i)];
                optional<ImageImport::FrameShot> padded_shot;
                ptr<const ImageImport::FrameShot> bake_shot = &shot;

                if (frame_padding > 0) {
                    padded_shot = PadSpriteFrame(shot, frame_padding);
                    bake_shot = &*padded_shot;
                }

                FO_VERIFY_AND_THROW(bake_shot->Data.size() == numeric_cast<size_t>(bake_shot->Size.width) * bake_shot->Size.height * 4, "Animation frame RGBA payload size does not match frame dimensions", bake_shot->Data.size(), bake_shot->Size);
                BakedSpriteMesh mesh;

                if (prepared_meshes[numeric_cast<size_t>(i)].has_value() && prepared_meshes[numeric_cast<size_t>(i)]->Kind != SpriteMeshKind::Quad) {
                    mesh = std::move(*prepared_meshes[numeric_cast<size_t>(i)]);
                    TranslateSpriteMesh(mesh, frame_padding - mesh_build_paddings[numeric_cast<size_t>(i)], *bake_shot);
                }
                else {
                    int64_t reference_quad_double_area = numeric_cast<int64_t>(shot.Size.width) * shot.Size.height * 2;
                    auto frame_mesh_config = mesh_config;

                    if (preserve_pixels[i]) {
                        frame_mesh_config.Enabled = false;
                    }

                    mesh = BuildSpriteMesh(bake_shot->Data, bake_shot->Size, frame_mesh_config, reference_quad_double_area);
                }

                optional<ImageImport::FrameShot> cropped_shot;

                if (mesh.Kind == SpriteMeshKind::Mesh) {
                    cropped_shot = CropSpriteFrameToMeshBounds(*bake_shot, shot, frame_padding, mesh);

                    if (cropped_shot.has_value()) {
                        bake_shot = &*cropped_shot;
                    }
                }

                ipos32 source_offset {
                    numeric_cast<int32_t>(numeric_cast<int64_t>(sequence.OffsX) + shot.Offset.x),
                    numeric_cast<int32_t>(numeric_cast<int64_t>(sequence.OffsY) + shot.Offset.y),
                };

                isize32 source_size = shot.Size;
                isize32 cropped_size = bake_shot->Size;
                FO_VERIFY_AND_THROW(mesh.Kind != SpriteMeshKind::Mesh || (source_size.width > 0 && source_size.height > 0 && cropped_size.width > 0 && cropped_size.height > 0), "Sprite frame offset resolution requires positive dimensions", source_size, cropped_size);

                ipos32 frame_offset = mesh.Kind == SpriteMeshKind::Mesh ?
                    ipos32 {
                        numeric_cast<int32_t>(numeric_cast<int64_t>(source_offset.x) + cropped_size.width / 2 - source_size.width / 2 + mesh.Data.SourceOffset.x),
                        numeric_cast<int32_t>(numeric_cast<int64_t>(source_offset.y) + cropped_size.height - source_size.height + mesh.Data.SourceOffset.y),
                    } :
                    source_offset;

                if (report_enabled) {
                    optional<SpriteMeshBakingCandidateReport> best_rejected_candidate;

                    if (mesh.BestRejectedCandidate.has_value()) {
                        const SpriteMeshCandidateSummary& candidate = *mesh.BestRejectedCandidate;
                        best_rejected_candidate = SpriteMeshBakingCandidateReport {
                            .SelectionOrigin = string(SpriteMeshCandidateSourceName(candidate.Source)),
                            .TriangleCount = candidate.TriangleCount,
                            .VertexCount = candidate.VertexCount,
                            .SubmittedGeometryDoubleArea = candidate.DoubleArea,
                            .Score = candidate.Score,
                            .SimplifyTolerance = candidate.SimplifyTolerance,
                            .ActualDilation = candidate.ActualDilation,
                        };
                    }

                    frame_reports.emplace_back(SpriteMeshBakingFrameReport {
                        .SourcePath = string(fname),
                        .OutputPath = output_path,
                        .Direction = dir,
                        .FrameIndex = numeric_cast<uint32_t>(i),
                        .Form = mesh.Kind == SpriteMeshKind::Mesh ? "mesh" :
                            mesh.Kind == SpriteMeshKind::Empty    ? "empty" :
                                                                    "quad",
                        .SelectionOrigin = string(SpriteMeshCandidateSourceName(mesh.Source)),
                        .QuadReason = string(SpriteMeshQuadReasonName(mesh.QuadReason)),
                        .TriangleCount = numeric_cast<uint32_t>(mesh.Data.Indices.size() / 3),
                        .VertexCount = numeric_cast<uint32_t>(mesh.Data.Vertices.size()),
                        .SourceComponentCount = mesh.SourceComponentCount,
                        .DilatedComponentCount = mesh.DilatedComponentCount,
                        .Padding = frame_padding,
                        .SimplifyTolerance = mesh.SimplifyTolerance,
                        .ActualDilation = mesh.ActualDilation,
                        .SourceFramePixels = numeric_cast<uint64_t>(shot.Size.width) * shot.Size.height,
                        .BakedCanvasPixels = numeric_cast<uint64_t>(bake_shot->Size.width) * bake_shot->Size.height,
                        .VisiblePixels = mesh.VisiblePixels,
                        .SubmittedGeometryDoubleArea = mesh.DoubleArea,
                        .SelectionScore = mesh.SelectionScore,
                        .BestRejectedCandidate = std::move(best_rejected_candidate),
                    });
                }

                writer.write<uint32_t>(shot.DurationMs);
                writer.write<int32_t>(frame_offset.x);
                writer.write<int32_t>(frame_offset.y);
                writer.write<int16_t>(numeric_cast<int16_t>(shot.Motion.x));
                writer.write<int16_t>(numeric_cast<int16_t>(shot.Motion.y));
                writer.write<uint8_t>(uint8_t {0});
                writer.write<uint16_t>(numeric_cast<uint16_t>(bake_shot->Size.width));
                writer.write<uint16_t>(numeric_cast<uint16_t>(bake_shot->Size.height));
                frame_info.Offset = frame_offset;
                frame_info.Size = cropped_size;
                frame_info.NextOffset = shot.Motion;
                frame_info.DurationMs = shot.DurationMs;

                if (!bake_shot->Data.empty()) {
                    writer.write_bytes({bake_shot->Data.data(), bake_shot->Data.size()});
                }

                writer.write<uint8_t>(static_cast<uint8_t>(mesh.Kind));

                if (mesh.Kind == SpriteMeshKind::Mesh) {
                    writer.write<uint16_t>(numeric_cast<uint16_t>(mesh.Data.Vertices.size()));
                    writer.write<uint32_t>(numeric_cast<uint32_t>(mesh.Data.Indices.size()));
                    writer.write<uint16_t>(numeric_cast<uint16_t>(mesh.Data.SourceSize.width));
                    writer.write<uint16_t>(numeric_cast<uint16_t>(mesh.Data.SourceSize.height));
                    writer.write<int32_t>(mesh.Data.SourceOffset.x);
                    writer.write<int32_t>(mesh.Data.SourceOffset.y);

                    for (ipos32 vertex : mesh.Data.Vertices) {
                        writer.write<uint16_t>(numeric_cast<uint16_t>(vertex.x));
                        writer.write<uint16_t>(numeric_cast<uint16_t>(vertex.y));
                    }
                    for (uint16_t index : mesh.Data.Indices) {
                        writer.write<uint16_t>(index);
                    }
                }
            }
            else {
                stats.SharedFrameReferences++;
                FO_VERIFY_AND_THROW(shot.SharedIndex < i, "Shared sprite frame points outside previously baked frames", fname, dir, i, shot.SharedIndex);
                const auto& reference = sequence.Frames[shot.SharedIndex];
                frame_info = direction_info.Frames[shot.SharedIndex];
                frame_info.SharedFrameIndex = shot.SharedIndex;
                frame_info.Offset = {
                    numeric_cast<int32_t>(numeric_cast<int64_t>(frame_info.Offset.x) + shot.Offset.x - reference.Offset.x),
                    numeric_cast<int32_t>(numeric_cast<int64_t>(frame_info.Offset.y) + shot.Offset.y - reference.Offset.y),
                };

                frame_info.NextOffset = shot.Motion;
                frame_info.DurationMs = shot.DurationMs;
                writer.write<uint32_t>(frame_info.DurationMs);
                writer.write<int32_t>(frame_info.Offset.x);
                writer.write<int32_t>(frame_info.Offset.y);
                writer.write<int16_t>(numeric_cast<int16_t>(shot.Motion.x));
                writer.write<int16_t>(numeric_cast<int16_t>(shot.Motion.y));
                writer.write<uint8_t>(uint8_t {1});
                writer.write<uint16_t>(shot.SharedIndex);
            }
        }
    }

    writer.write<uint8_t>(SPRITE_RESOURCE_MAGIC);

    _context->WriteData(output_path, data);

    if (!report_enabled) {
        return sprite_info_entry;
    }

    AddBakingReportCounter("collections", stats.Collections);
    AddBakingReportCounter("directions", stats.Directions);
    AddBakingReportCounter("frameSlots", stats.FrameSlots);
    AddBakingReportCounter("uniqueFrames", stats.UniqueFrames);
    AddBakingReportCounter("sharedFrameReferences", stats.SharedFrameReferences);
    AddBakingReportHistogramValue("sourceFormats", strex(fname).get_file_extension());

    for (const SpriteMeshBakingFrameReport& frame_report : frame_reports) {
        RecordSpriteMeshBakingFrame(frame_report);
    }
    if (stats.SharedFrameReferences != 0) {
        RecordSharedSpriteMeshBakingFrames(stats.SharedFrameReferences);
    }

    return sprite_info_entry;
}

static auto PadSpriteFrame(const ImageImport::FrameShot& shot, int32_t padding) -> ImageImport::FrameShot
{
    FO_VERIFY_AND_THROW(padding > 0, "Sprite frame padding must be positive", padding);
    FO_VERIFY_AND_THROW(shot.Data.size() == numeric_cast<size_t>(shot.Size.width) * shot.Size.height * 4, "Animation frame RGBA payload size does not match frame dimensions", shot.Data.size(), shot.Size);

    int32_t padded_width = shot.Size.width + padding * 2;
    int32_t padded_height = shot.Size.height + padding * 2;
    FO_VERIFY_AND_THROW(padded_width <= std::numeric_limits<uint16_t>::max() && padded_height <= std::numeric_limits<uint16_t>::max(), "Sprite frame padding exceeds the serialized dimension range", shot.Size, padding);

    ImageImport::FrameShot result;
    result.Size = {padded_width, padded_height};
    result.Motion = shot.Motion;
    result.Offset = shot.Offset;
    result.DurationMs = shot.DurationMs;
    result.Data.resize(numeric_cast<size_t>(result.Size.width) * result.Size.height * 4);

    size_t source_row_size = numeric_cast<size_t>(shot.Size.width) * 4;
    size_t destination_row_size = numeric_cast<size_t>(result.Size.width) * 4;
    size_t destination_x_offset = numeric_cast<size_t>(padding) * 4;

    for (int32_t y = 0; y < shot.Size.height; y++) {
        size_t source_offset = numeric_cast<size_t>(y) * source_row_size;
        size_t destination_offset = numeric_cast<size_t>(y + padding) * destination_row_size + destination_x_offset;
        memory::copy(result.Data.data() + destination_offset, shot.Data.data() + source_offset, source_row_size);
    }

    return result;
}

static auto ResolveSpriteFramePadding(const ImageImport::FrameShot& shot, const BakedSpriteMesh& mesh, int32_t build_padding) -> int32_t
{
    if (mesh.Kind != SpriteMeshKind::Mesh || mesh.Data.Indices.empty()) {
        return 0;
    }

    int32_t required_padding = 0;

    for (ipos32 vertex : mesh.Data.Vertices) {
        int32_t relative_x = vertex.x - build_padding;
        int32_t relative_y = vertex.y - build_padding;
        required_padding = std::max({required_padding, -relative_x, relative_x - shot.Size.width, -relative_y, relative_y - shot.Size.height});
    }

    FO_VERIFY_AND_THROW(required_padding <= SPRITE_MESH_MAXIMUM_PADDING, "Resolved sprite mesh padding exceeds the internal maximum", required_padding, SPRITE_MESH_MAXIMUM_PADDING);
    return required_padding;
}

static void TranslateSpriteMesh(BakedSpriteMesh& mesh, int32_t offset, const ImageImport::FrameShot& shot)
{
    if (mesh.Kind != SpriteMeshKind::Mesh || offset == 0) {
        return;
    }

    for (ipos32 vertex : mesh.Data.Vertices) {
        int64_t translated_x = numeric_cast<int64_t>(vertex.x) + offset;
        int64_t translated_y = numeric_cast<int64_t>(vertex.y) + offset;
        FO_VERIFY_AND_THROW(translated_x >= 0 && translated_x <= shot.Size.width && translated_y >= 0 && translated_y <= shot.Size.height, "Translated sprite mesh vertex lies outside the baked frame", vertex, shot.Size, offset);
    }

    for (ipos32& vertex : mesh.Data.Vertices) {
        vertex.x = numeric_cast<int32_t>(numeric_cast<int64_t>(vertex.x) + offset);
        vertex.y = numeric_cast<int32_t>(numeric_cast<int64_t>(vertex.y) + offset);
    }
}

static auto CropSpriteFrameToMeshBounds(const ImageImport::FrameShot& shot, const ImageImport::FrameShot& source_shot, int32_t padding, BakedSpriteMesh& mesh) -> optional<ImageImport::FrameShot>
{
    FO_VERIFY_AND_THROW(mesh.Kind == SpriteMeshKind::Mesh && !mesh.Data.Vertices.empty() && !mesh.Data.Indices.empty(), "Sprite frame cropping requires a non-empty mesh");
    FO_VERIFY_AND_THROW(shot.Data.size() == numeric_cast<size_t>(shot.Size.width) * shot.Size.height * 4, "Animation frame RGBA payload size does not match frame dimensions", shot.Data.size(), shot.Size);

    ipos32 minimum_vertex {shot.Size.width, shot.Size.height};
    ipos32 maximum_vertex {};

    for (ipos32 vertex : mesh.Data.Vertices) {
        FO_VERIFY_AND_THROW(vertex.x >= 0 && vertex.x <= shot.Size.width && vertex.y >= 0 && vertex.y <= shot.Size.height, "Sprite mesh vertex lies outside the frame selected for cropping", vertex, shot.Size);
        minimum_vertex.x = std::min(minimum_vertex.x, vertex.x);
        minimum_vertex.y = std::min(minimum_vertex.y, vertex.y);
        maximum_vertex.x = std::max(maximum_vertex.x, vertex.x);
        maximum_vertex.y = std::max(maximum_vertex.y, vertex.y);
    }

    isize32 cropped_size = {maximum_vertex.x - minimum_vertex.x, maximum_vertex.y - minimum_vertex.y};
    FO_VERIFY_AND_THROW(cropped_size.width > 0 && cropped_size.height > 0, "Sprite mesh bounds must have positive dimensions", minimum_vertex, maximum_vertex);

    mesh.Data.SourceSize = source_shot.Size;
    mesh.Data.SourceOffset = {minimum_vertex.x - padding, minimum_vertex.y - padding};

    for (ipos32& vertex : mesh.Data.Vertices) {
        vertex -= minimum_vertex;
    }

    if (minimum_vertex == ipos32 {} && cropped_size == shot.Size) {
        return std::nullopt;
    }

    ImageImport::FrameShot result;
    result.Size = cropped_size;
    result.Motion = shot.Motion;
    result.Offset = shot.Offset;
    result.DurationMs = shot.DurationMs;
    result.Data.resize(numeric_cast<size_t>(result.Size.width) * result.Size.height * 4);

    size_t source_row_size = numeric_cast<size_t>(shot.Size.width) * 4;
    size_t cropped_row_size = numeric_cast<size_t>(result.Size.width) * 4;
    size_t source_x_offset = numeric_cast<size_t>(minimum_vertex.x) * 4;

    for (int32_t y = 0; y < result.Size.height; y++) {
        size_t source_y = numeric_cast<size_t>(y + minimum_vertex.y);
        size_t source_offset = source_y * source_row_size + source_x_offset;
        size_t destination_offset = numeric_cast<size_t>(y) * cropped_row_size;
        memory::copy(result.Data.data() + destination_offset, shot.Data.data() + source_offset, cropped_row_size);
    }

    return optional<ImageImport::FrameShot> {std::move(result)};
}

auto ImageBaker::LoadAny(string_view fname_with_opt, const FileCollection& files) const -> ImageImport::FrameCollection
{
    string ext = strex(fname_with_opt).get_file_extension();
    string dir = strex(fname_with_opt).extract_dir();
    string_view name = strvex(fname_with_opt).extract_file_name().erase_file_extension().substring_until('$');
    string fname = strex(dir).combine_path(strex("{}.{}", name, ext)).str();
    string_view opt = strvex(fname_with_opt).extract_file_name().erase_file_extension().substring_after('$');

    ImageImport::CheckEnabled(fname);

    auto file = files.FindFileByPath(fname);

    if (!file) {
        throw ImageBakerException("Image file not found", fname, fname_with_opt, dir, name, ext);
    }

    if (auto it = _fileLoaders.find(ext); it != _fileLoaders.end()) {
        ImageImport::Context context {&files, [this, &files](string_view nested_path) { return LoadAny(nested_path, files); }};
        return LoadImage(it->second, fname, opt, file.GetReader(), context);
    }
    else {
        throw ImageBakerException("Invalid image file extension", fname);
    }
}

static auto LoadImage(const ImageImport::Loader& loader, string_view fname, string_view opt, FileReader reader, const ImageImport::Context& context) -> ImageImport::FrameCollection
{
    size_t input_size = reader.GetSize();

    try {
        return loader.Load(fname, opt, std::move(reader), context);
    }
    catch (const std::exception& ex) {
        throw ImageBakerException("Image import failed", fname, opt, input_size, ex.what());
    }
}

FO_END_NAMESPACE

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
#include "Settings.h"
#include "SpriteMeshing.h"
#include "SpriteResource.h"

FO_BEGIN_NAMESPACE

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
    BaseBaker(std::move(ctx), NAME)
{
    AddLoader(ImageBakerFofrm::Load, {"fofrm"});
#if FO_IMAGE_FRM
    AddLoader(ImageBakerFrm::Load, {"frm"}, ImageBakerFrm::ResolveOutput);
    AddLoader(ImageBakerFrm::LoadFrX, {"fr0"}, ImageBakerFrm::ResolveOutput);
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

void ImageBaker::AddLoader(const ImageImport::LoadFunc& loader, const vector<string>& file_extensions, const ImageImport::OutputResolver& output_resolver)
{
    for (const auto& ext : file_extensions) {
        _fileLoaders[ext] = ImageImport::Loader {loader, output_resolver};
    }
}

void ImageBaker::BakeFiles(const FileCollection& files, string_view target_path) const
{
    FO_TRACE_ZONE(Baking);

    if (IsBakingReportEnabled()) {
        SpriteMeshBakeConfig mesh_config = ResolveSpriteMeshBakeConfig(_context->Settings);
        RecordSpriteMeshBakingSettings(SpriteMeshBakingReportSettings {
            .Enabled = mesh_config.Enabled,
            .AlphaThreshold = mesh_config.AlphaThreshold,
            .MaxTriangles = mesh_config.MaxTriangles,
            .AreaSavingsWeight = mesh_config.AreaSavingsWeight,
            .BaseDilation = SPRITE_MESH_DILATION,
            .MaximumPadding = SPRITE_MESH_MAXIMUM_PADDING,
        });
    }

    vector<pair<File, ImageImport::Loader>> files_to_bake;
    files_to_bake.reserve(files.GetFilesCount());
    string sprite_info_path = strex("{}/{}.foinfo", SPRITE_INFO_DIRECTORY, _context->PackName);
    bool scan_mode = target_path.empty();
    bool sprite_info_target = target_path == sprite_info_path;
    bool bake_sprite_info = scan_mode || sprite_info_target;
    bool sprite_info_needs_write = !_context->BakeChecker;
    map<string, SpriteInfoFileEntry> sprite_info_entries;
    set<string> current_sprite_sources;
    uint64_t maximum_source_write_time = 0;

    nptr<const FileSystem> sprite_info_files = _context->PackBakedFiles ? _context->PackBakedFiles : _context->BakedFiles;
    FO_VERIFY_AND_THROW(sprite_info_files, "Baker context has no baked file registry");

    if (scan_mode && sprite_info_files->IsFileExists(sprite_info_path)) {
        File sprite_info_file = sprite_info_files->ReadFile(sprite_info_path);
        FO_VERIFY_AND_THROW(sprite_info_file, "Baked sprite info resource is not readable", sprite_info_path);

        for (SpriteInfoFileEntry& entry : ReadSpriteInfoFile(sprite_info_path, sprite_info_file.GetStr())) {
            bool inserted = sprite_info_entries.emplace(entry.SourcePath, std::move(entry)).second;
            FO_VERIFY_AND_THROW(inserted, "Baked sprite info resource contains a duplicate source path", sprite_info_path);
        }
    }

    for (const auto& file_header : files) {
        ImageImport::CheckEnabled(file_header.GetPath());
    }

    if (bake_sprite_info) {
        for (auto&& [ext, loader] : _fileLoaders) {
            for (const auto& file_header : files) {
                string file_ext = strex(file_header.GetPath()).get_file_extension();

                if (file_ext != ext) {
                    continue;
                }
                current_sprite_sources.emplace(file_header.GetPath());
                maximum_source_write_time = std::max(maximum_source_write_time, file_header.GetWriteTime());
                // The checker is handed the resource this source bakes to: what it registers is the output the master
                // baker keeps, and a critter frame registered under its source spelling is renamed back to it
                if (scan_mode && _context->BakeChecker) {
                    string resource_path {file_header.GetPath()};
                    if (loader.ResolveOutput) {
                        resource_path = loader.ResolveOutput(resource_path);
                    }

                    if (!_context->BakeChecker(resource_path, file_header.GetWriteTime())) {
                        continue;
                    }
                }

                files_to_bake.emplace_back(File::Load(file_header), loader);
            }
        }
    }
    else {
        string ext = strex(target_path).get_file_extension();

        if (!_fileLoaders.contains(ext)) {
            return;
        }

        auto file = files.FindFileByPath(target_path);

        if (!file) {
            return;
        }
        if (_context->BakeChecker && !_context->BakeChecker(target_path, file.GetWriteTime())) {
            return;
        }

        files_to_bake.emplace_back(std::move(file), _fileLoaders.at(ext));
    }

    if (bake_sprite_info && !current_sprite_sources.empty() && _context->BakeChecker) {
        sprite_info_needs_write = _context->BakeChecker(sprite_info_path, maximum_source_write_time);
    }

    vector<std::future<SpriteInfoFileEntry>> file_bakings;

    for (size_t file_index = 0; file_index < files_to_bake.size(); file_index++) {
        string task_name = strex("BakeImage-{}", files_to_bake[file_index].first.GetPath()).str();
        file_bakings.emplace_back(run_async(GetAsyncMode(), task_name, [this, &files, &files_to_bake, file_index]() FO_DEFERRED -> SpriteInfoFileEntry {
            auto& file_to_bake = files_to_bake[file_index];
            string_view path = file_to_bake.first.GetPath();
            ImageImport::Context context {&files, [this, &files](string_view nested_path) { return LoadAny(nested_path, files); }};
            auto collection = file_to_bake.second.Load(path, "", file_to_bake.first.GetReader(), context);
            return BakeCollection(path, collection);
        }));
    }

    size_t errors = 0;
    string first_error;

    for (size_t file_index = 0; file_index < file_bakings.size(); file_index++) {
        try {
            SpriteInfoFileEntry entry = file_bakings[file_index].get();
            sprite_info_entries[entry.SourcePath] = std::move(entry);
        }
        catch (const std::exception& ex) {
            logging::write("Image baking error for '{}': {}", files_to_bake[file_index].first.GetPath(), ex.what());

            if (errors == 0) {
                first_error = ex.what();
            }

            errors++;
        }
    }

    if (errors != 0) {
        throw ImageBakerException("Errors during images baking", errors, first_error);
    }

    if (!bake_sprite_info || _context->OutputDiscovery) {
        return;
    }

    size_t sprite_info_entry_count = sprite_info_entries.size();
    std::erase_if(sprite_info_entries, [&current_sprite_sources](const auto& entry) { return !current_sprite_sources.contains(entry.first); });
    bool removed_sprite_info_entries = sprite_info_entries.size() != sprite_info_entry_count;
    FO_VERIFY_AND_THROW(sprite_info_entries.size() == current_sprite_sources.size(), "Sprite info index is incomplete; perform a full resource rebake", _context->PackName, sprite_info_entries.size(), current_sprite_sources.size());

    if (!sprite_info_needs_write && files_to_bake.empty() && !removed_sprite_info_entries) {
        return;
    }

    if (!sprite_info_entries.empty()) {
        vector<SpriteInfoFileEntry> entries;
        entries.reserve(sprite_info_entries.size());

        for (auto& [source_path, entry] : sprite_info_entries) {
            ignore_unused(source_path);
            entries.emplace_back(std::move(entry));
        }

        string sprite_info = WriteSpriteInfoFile(entries);
        vector<uint8_t> sprite_info_data(sprite_info.begin(), sprite_info.end());
        _context->WriteData(sprite_info_path, sprite_info_data);
    }
}

auto ImageBaker::BakeCollection(string_view fname, const ImageImport::FrameCollection& collection) const -> SpriteInfoFileEntry
{
    FO_TRACE_ZONE(Baking);

    vector<uint8_t> data;
    auto writer = data_writer(data);

    auto dirs = numeric_cast<uint8_t>(collection.HaveDirs ? GameSettings::MAP_DIR_COUNT : 1);
    SpriteMeshBakeConfig mesh_config = ResolveSpriteMeshBakeConfig(_context->Settings);
    string output_path = collection.NewName.empty() ? string(fname) : collection.NewName;
    SpriteInfoFileEntry sprite_info_entry {
        .SourcePath = string(fname),
        .ResourcePath = output_path,
        .Info =
            {
                .FrameCount = collection.SequenceSize,
                .Duration = std::chrono::milliseconds {collection.AnimTicks},
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
    writer.write<uint16_t>(collection.SequenceSize);
    writer.write<uint16_t>(collection.AnimTicks);
    writer.write<uint8_t>(dirs);

    for (uint8_t dir = 0; dir < dirs; dir++) {
        const auto& sequence = dir == 0 ? collection.Main : collection.Dirs[dir - 1];
        SpriteDirInfo& direction_info = sprite_info_entry.Info.Directions[dir];
        direction_info.Frames.resize(collection.SequenceSize);
        vector<optional<BakedSpriteMesh>> prepared_meshes(sequence.Frames.size());
        vector<int32_t> mesh_build_paddings(sequence.Frames.size());
        vector<int32_t> frame_paddings(sequence.Frames.size());

        if (mesh_config.Enabled) {
            for (size_t frame_index = 0; frame_index < sequence.Frames.size(); frame_index++) {
                const ImageImport::FrameShot& shot = sequence.Frames[frame_index];
                if (shot.Shared) {
                    continue;
                }

                optional<ImageImport::FrameShot> maximum_padded_shot;
                ptr<const ImageImport::FrameShot> mesh_shot = &shot;

                if (shot.Width > 0 && shot.Height > 0) {
                    maximum_padded_shot = PadSpriteFrame(shot, SPRITE_MESH_MAXIMUM_PADDING);
                    mesh_shot = &*maximum_padded_shot;
                    mesh_build_paddings[frame_index] = SPRITE_MESH_MAXIMUM_PADDING;
                }

                int64_t reference_quad_double_area = numeric_cast<int64_t>(shot.Width) * shot.Height * 2;
                prepared_meshes[frame_index] = BuildSpriteMesh(mesh_shot->Data, {mesh_shot->Width, mesh_shot->Height}, mesh_config, reference_quad_double_area);
                frame_paddings[frame_index] = ResolveSpriteFramePadding(shot, *prepared_meshes[frame_index], mesh_build_paddings[frame_index]);
            }
        }

        for (int32_t i = 0; i < collection.SequenceSize; i++) {
            const auto& shot = sequence.Frames[i];
            SpriteFrameInfo& frame_info = direction_info.Frames[numeric_cast<size_t>(i)];
            stats.FrameSlots++;
            writer.write<bool>(shot.Shared);

            if (!shot.Shared) {
                stats.UniqueFrames++;
                int32_t frame_padding = frame_paddings[numeric_cast<size_t>(i)];
                optional<ImageImport::FrameShot> padded_shot;
                ptr<const ImageImport::FrameShot> bake_shot = &shot;

                if (frame_padding > 0) {
                    padded_shot = PadSpriteFrame(shot, frame_padding);
                    bake_shot = &*padded_shot;
                }

                FO_VERIFY_AND_THROW(bake_shot->Data.size() == numeric_cast<size_t>(bake_shot->Width) * bake_shot->Height * 4, "Animation frame RGBA payload size does not match frame dimensions", bake_shot->Data.size(), bake_shot->Width, bake_shot->Height);
                BakedSpriteMesh mesh;

                if (prepared_meshes[numeric_cast<size_t>(i)].has_value() && prepared_meshes[numeric_cast<size_t>(i)]->Kind != SpriteMeshKind::Quad) {
                    mesh = std::move(*prepared_meshes[numeric_cast<size_t>(i)]);
                    TranslateSpriteMesh(mesh, frame_padding - mesh_build_paddings[numeric_cast<size_t>(i)], *bake_shot);
                }
                else {
                    int64_t reference_quad_double_area = numeric_cast<int64_t>(shot.Width) * shot.Height * 2;
                    mesh = BuildSpriteMesh(bake_shot->Data, {bake_shot->Width, bake_shot->Height}, mesh_config, reference_quad_double_area);
                }

                optional<ImageImport::FrameShot> cropped_shot;

                if (mesh.Kind == SpriteMeshKind::Mesh) {
                    cropped_shot = CropSpriteFrameToMeshBounds(*bake_shot, shot, frame_padding, mesh);

                    if (cropped_shot.has_value()) {
                        bake_shot = &*cropped_shot;
                    }
                }

                ipos32 source_offset {sequence.OffsX, sequence.OffsY};
                isize32 source_size {shot.Width, shot.Height};
                isize32 cropped_size {bake_shot->Width, bake_shot->Height};
                FO_VERIFY_AND_THROW(mesh.Kind != SpriteMeshKind::Mesh || (source_size.width > 0 && source_size.height > 0 && cropped_size.width > 0 && cropped_size.height > 0), "Sprite frame offset resolution requires positive dimensions", source_size, cropped_size);

                ipos32 frame_offset = mesh.Kind == SpriteMeshKind::Mesh ?
                    ipos32 {
                        source_offset.x + cropped_size.width / 2 - source_size.width / 2 + mesh.Data.SourceOffset.x,
                        source_offset.y + cropped_size.height - source_size.height + mesh.Data.SourceOffset.y,
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
                        .SourceFramePixels = numeric_cast<uint64_t>(shot.Width) * shot.Height,
                        .BakedCanvasPixels = numeric_cast<uint64_t>(bake_shot->Width) * bake_shot->Height,
                        .VisiblePixels = mesh.VisiblePixels,
                        .SubmittedGeometryDoubleArea = mesh.DoubleArea,
                        .SelectionScore = mesh.SelectionScore,
                        .BestRejectedCandidate = std::move(best_rejected_candidate),
                    });
                }

                writer.write<int16_t>(numeric_cast<int16_t>(frame_offset.x));
                writer.write<int16_t>(numeric_cast<int16_t>(frame_offset.y));
                writer.write<uint16_t>(bake_shot->Width);
                writer.write<uint16_t>(bake_shot->Height);
                writer.write<int16_t>(bake_shot->NextX);
                writer.write<int16_t>(bake_shot->NextY);
                frame_info.Offset = frame_offset;
                frame_info.Size = cropped_size;
                frame_info.NextOffset = {bake_shot->NextX, bake_shot->NextY};

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
                writer.write<uint16_t>(shot.SharedIndex);
                FO_VERIFY_AND_THROW(shot.SharedIndex < i, "Shared sprite frame points outside previously baked frames", fname, dir, i, shot.SharedIndex);
                frame_info = direction_info.Frames[shot.SharedIndex];
                frame_info.SharedFrameIndex = shot.SharedIndex;
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
    FO_VERIFY_AND_THROW(shot.Data.size() == numeric_cast<size_t>(shot.Width) * shot.Height * 4, "Animation frame RGBA payload size does not match frame dimensions", shot.Data.size(), shot.Width, shot.Height);

    int32_t padded_width = numeric_cast<int32_t>(shot.Width) + padding * 2;
    int32_t padded_height = numeric_cast<int32_t>(shot.Height) + padding * 2;
    FO_VERIFY_AND_THROW(padded_width <= std::numeric_limits<uint16_t>::max() && padded_height <= std::numeric_limits<uint16_t>::max(), "Sprite frame padding exceeds the serialized dimension range", shot.Width, shot.Height, padding);

    ImageImport::FrameShot result;
    result.Width = numeric_cast<uint16_t>(padded_width);
    result.Height = numeric_cast<uint16_t>(padded_height);
    result.NextX = shot.NextX;
    result.NextY = shot.NextY;
    result.Data.resize(numeric_cast<size_t>(result.Width) * result.Height * 4);

    size_t source_row_size = numeric_cast<size_t>(shot.Width) * 4;
    size_t destination_row_size = numeric_cast<size_t>(result.Width) * 4;
    size_t destination_x_offset = numeric_cast<size_t>(padding) * 4;

    for (uint16_t y = 0; y < shot.Height; y++) {
        size_t source_offset = numeric_cast<size_t>(y) * source_row_size;
        size_t destination_offset = numeric_cast<size_t>(numeric_cast<int32_t>(y) + padding) * destination_row_size + destination_x_offset;
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
        required_padding = std::max({required_padding, -relative_x, relative_x - shot.Width, -relative_y, relative_y - shot.Height});
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
        FO_VERIFY_AND_THROW(translated_x >= 0 && translated_x <= shot.Width && translated_y >= 0 && translated_y <= shot.Height, "Translated sprite mesh vertex lies outside the baked frame", vertex, shot.Width, shot.Height, offset);
    }

    for (ipos32& vertex : mesh.Data.Vertices) {
        vertex.x = numeric_cast<int32_t>(numeric_cast<int64_t>(vertex.x) + offset);
        vertex.y = numeric_cast<int32_t>(numeric_cast<int64_t>(vertex.y) + offset);
    }
}

static auto CropSpriteFrameToMeshBounds(const ImageImport::FrameShot& shot, const ImageImport::FrameShot& source_shot, int32_t padding, BakedSpriteMesh& mesh) -> optional<ImageImport::FrameShot>
{
    FO_VERIFY_AND_THROW(mesh.Kind == SpriteMeshKind::Mesh && !mesh.Data.Vertices.empty() && !mesh.Data.Indices.empty(), "Sprite frame cropping requires a non-empty mesh");
    FO_VERIFY_AND_THROW(shot.Data.size() == numeric_cast<size_t>(shot.Width) * shot.Height * 4, "Animation frame RGBA payload size does not match frame dimensions", shot.Data.size(), shot.Width, shot.Height);

    ipos32 minimum_vertex {shot.Width, shot.Height};
    ipos32 maximum_vertex {};

    for (ipos32 vertex : mesh.Data.Vertices) {
        FO_VERIFY_AND_THROW(vertex.x >= 0 && vertex.x <= shot.Width && vertex.y >= 0 && vertex.y <= shot.Height, "Sprite mesh vertex lies outside the frame selected for cropping", vertex, shot.Width, shot.Height);
        minimum_vertex.x = std::min(minimum_vertex.x, vertex.x);
        minimum_vertex.y = std::min(minimum_vertex.y, vertex.y);
        maximum_vertex.x = std::max(maximum_vertex.x, vertex.x);
        maximum_vertex.y = std::max(maximum_vertex.y, vertex.y);
    }

    isize32 cropped_size = {maximum_vertex.x - minimum_vertex.x, maximum_vertex.y - minimum_vertex.y};
    FO_VERIFY_AND_THROW(cropped_size.width > 0 && cropped_size.height > 0, "Sprite mesh bounds must have positive dimensions", minimum_vertex, maximum_vertex);

    mesh.Data.SourceSize = {source_shot.Width, source_shot.Height};
    mesh.Data.SourceOffset = {minimum_vertex.x - padding, minimum_vertex.y - padding};

    for (ipos32& vertex : mesh.Data.Vertices) {
        vertex -= minimum_vertex;
    }

    if (minimum_vertex == ipos32 {} && cropped_size == isize32 {shot.Width, shot.Height}) {
        return std::nullopt;
    }

    ImageImport::FrameShot result;
    result.Width = numeric_cast<uint16_t>(cropped_size.width);
    result.Height = numeric_cast<uint16_t>(cropped_size.height);
    result.NextX = shot.NextX;
    result.NextY = shot.NextY;
    result.Data.resize(numeric_cast<size_t>(result.Width) * result.Height * 4);

    size_t source_row_size = numeric_cast<size_t>(shot.Width) * 4;
    size_t cropped_row_size = numeric_cast<size_t>(result.Width) * 4;
    size_t source_x_offset = numeric_cast<size_t>(minimum_vertex.x) * 4;

    for (uint16_t y = 0; y < result.Height; y++) {
        size_t source_y = numeric_cast<size_t>(numeric_cast<int32_t>(y) + minimum_vertex.y);
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
    string fname = strex("{}/{}.{}", dir, name, ext);
    string_view opt = strvex(fname_with_opt).extract_file_name().erase_file_extension().substring_after('$');

    ImageImport::CheckEnabled(fname);

    auto file = files.FindFileByPath(fname);

    if (!file) {
        throw ImageBakerException("Image file not found", fname, fname_with_opt, dir, name, ext);
    }

    if (auto it = _fileLoaders.find(ext); it != _fileLoaders.end()) {
        ImageImport::Context context {&files, [this, &files](string_view nested_path) { return LoadAny(nested_path, files); }};
        return it->second.Load(fname, opt, file.GetReader(), context);
    }
    else {
        throw ImageBakerException("Invalid image file extension", fname);
    }
}

FO_END_NAMESPACE

// FOnline Engine
// SPDX-License-Identifier: MIT

#include "RawCopyBaker.h"

#include "Application.h"

FO_BEGIN_NAMESPACE

RawCopyBaker::RawCopyBaker(shared_ptr<BakingContext> ctx) :
    BaseBaker(std::move(ctx), NAME, BakingOutputKind::Resource)
{
}

RawCopyBaker::~RawCopyBaker()
{
}

void RawCopyBaker::BakeFiles(const FileCollection& files, string_view target_path) const
{
    FO_TRACE_ZONE(Baking);

    // Collect files
    vector<File> filtered_files {};

    if (target_path.empty()) {
        for (const auto& file_header : files) {
            if (_context->IsImageOutputOwned(file_header.GetPath())) {
                continue;
            }

            string ext {static_cast<string&&>(strex(file_header.GetPath()).get_file_extension())};
            auto it {std::ranges::find(_context->GetSettings()->Baking.RawCopyFileExtensions, ext)};

            if (it == _context->GetSettings()->Baking.RawCopyFileExtensions.end()) {
                continue;
            }
            if (_context->HasBakeChecker() && !_context->CheckOutput(file_header.GetPath(), file_header.GetWriteTime())) {
                continue;
            }

            filtered_files.emplace_back(File::Load(file_header));
        }
    }
    else {
        if (_context->IsImageOutputOwned(target_path)) {
            return;
        }

        string ext {static_cast<string&&>(strex(target_path).get_file_extension())};
        auto it {std::ranges::find(_context->GetSettings()->Baking.RawCopyFileExtensions, ext)};

        if (it == _context->GetSettings()->Baking.RawCopyFileExtensions.end()) {
            return;
        }

        auto file {files.FindFileByPath(target_path)};

        if (!file) {
            return;
        }
        if (_context->HasBakeChecker() && !_context->CheckOutput(file.GetPath(), file.GetWriteTime())) {
            return;
        }

        filtered_files.emplace_back(std::move(file));
    }

    if (filtered_files.empty()) {
        return;
    }

    // Process files
    for (const auto& file : filtered_files) {
        _context->WriteData(file.GetPath(), file.GetData());
    }
}

FO_END_NAMESPACE

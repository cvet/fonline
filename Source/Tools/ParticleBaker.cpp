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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "ParticleBaker.h"
#include "SparkExtension.h"

FO_DISABLE_WARNINGS_PUSH()
#include "SPARK.h"
FO_DISABLE_WARNINGS_POP()

FO_BEGIN_NAMESPACE

ParticleBaker::ParticleBaker(shared_ptr<BakingContext> ctx) :
    BaseBaker(std::move(ctx))
{
    FO_STACK_TRACE_ENTRY();

    SPK::FO::EnsureSparkParticleObjectsRegistered();
}

void ParticleBaker::BakeParticleFile(const File& file) const
{
    FO_STACK_TRACE_ENTRY();

    const string_view path = file.GetPath();

    WriteLog("Baking particle: {}", path);

    // Load SPARK XML
    const_span<uint8_t> file_data = file.GetDataSpan();
    auto system = SPK::IO::IOManager::get().loadFromBuffer("xml", ptr<const uint8_t> {file_data.data()}.reinterpret_as<char>().get(), numeric_cast<unsigned>(file_data.size()));

    if (!system) {
        throw ParticleBakerException("Failed to load SPARK particle XML", path);
    }

    // Save to SPARK binary format
    std::ostringstream oss(std::ios::binary);

    if (!SPK::IO::IOManager::get().save("spk", oss, system)) {
        throw ParticleBakerException("Failed to save SPARK particle binary", path);
    }

    const std::string str = oss.str();
    vector<uint8_t> binary(str.begin(), str.end());

    _context->WriteData(path, binary);
}

void ParticleBaker::BakeFiles(const FileCollection& files, string_view target_path) const
{
    FO_STACK_TRACE_ENTRY();

    vector<File> filtered_files;

    if (target_path.empty()) {
        for (const auto& file_header : files) {
            const string extension = strex(file_header.GetPath()).get_file_extension();
            if (extension != "fopts") {
                continue;
            }
            if (_context->BakeChecker && !_context->BakeChecker(file_header.GetPath(), file_header.GetWriteTime())) {
                continue;
            }

            filtered_files.emplace_back(File::Load(file_header));
        }
    }
    else {
        const string extension = strex(target_path).get_file_extension();
        if (extension != "fopts") {
            return;
        }

        auto file = files.FindFileByPath(target_path);

        if (!file) {
            return;
        }
        if (_context->BakeChecker && !_context->BakeChecker(target_path, file.GetWriteTime())) {
            return;
        }

        filtered_files.emplace_back(std::move(file));
    }

    if (filtered_files.empty()) {
        return;
    }

    for (const auto& file : filtered_files) {
        BakeParticleFile(file);
    }
}

FO_END_NAMESPACE

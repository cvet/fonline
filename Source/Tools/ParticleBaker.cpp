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

#if FO_SPARK_PARTICLES || FO_EFFEKSEER_PARTICLES
FO_DISABLE_WARNINGS_PUSH()
#if FO_EFFEKSEER_PARTICLES
#include "Effekseer.h"
#endif
#if FO_SPARK_PARTICLES
#include "SPARK.h"
#endif
FO_DISABLE_WARNINGS_POP()
#endif

FO_BEGIN_NAMESPACE

ParticleBaker::ParticleBaker(shared_ptr<BakingContext> ctx) :
    BaseBaker(std::move(ctx))
{
    FO_STACK_TRACE_ENTRY();

#if FO_SPARK_PARTICLES
    SPK::FO::EnsureSparkParticleObjectsRegistered();
#endif
}

void ParticleBaker::BakeFiles(const FileCollection& files, string_view target_path) const
{
    FO_STACK_TRACE_ENTRY();

    vector<File> filtered_files;

    if (target_path.empty()) {
        for (const auto& file_header : files) {
            const string extension = strex(file_header.GetPath()).get_file_extension();
            ignore_unused(extension);
            const bool supported_extension =
#if FO_SPARK_PARTICLES
                extension == "spark" ||
#endif
#if FO_EFFEKSEER_PARTICLES
                extension == "efk" || extension == "efkefc" ||
#endif
                false;

            if (!supported_extension) {
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
        ignore_unused(extension);
        const bool supported_extension =
#if FO_SPARK_PARTICLES
            extension == "spark" ||
#endif
#if FO_EFFEKSEER_PARTICLES
            extension == "efk" || extension == "efkefc" ||
#endif
            false;

        if (!supported_extension) {
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
        if (strex(file.GetPath()).get_file_extension() == "spark") {
#if FO_SPARK_PARTICLES
            BakeSparkFile(file);
#else
            FO_VERIFY_AND_THROW(false, "SPARK particle reached a baker with its backend disabled", file.GetPath());
#endif
        }
        else {
#if FO_EFFEKSEER_PARTICLES
            BakeEffekseerFile(file);
#else
            FO_VERIFY_AND_THROW(false, "Effekseer particle reached a baker with its backend disabled", file.GetPath());
#endif
        }
    }
}

#if FO_SPARK_PARTICLES
void ParticleBaker::BakeSparkFile(const File& file) const
{
    FO_STACK_TRACE_ENTRY();

    const string_view path = file.GetPath();

    WriteLog("Baking SPARK particle: {}", path);

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
#endif

#if FO_EFFEKSEER_PARTICLES
void ParticleBaker::BakeEffekseerFile(const File& file) const
{
    FO_STACK_TRACE_ENTRY();

    const string_view path = file.GetPath();
    const string extension = strex(path).get_file_extension();
    const_span<uint8_t> file_data = file.GetDataSpan();

    WriteLog("Validating Effekseer particle: {}", path);

    constexpr size_t magic_size = 4;

    if (file_data.size() < magic_size) {
        throw ParticleBakerException("Effekseer particle is truncated", path);
    }

    const string_view expected_magic = extension == "efk" ? string_view {"SKFE"} : string_view {"EFKE"};
    const string_view actual_magic {ptr<const uint8_t> {file_data.data()}.reinterpret_as<char>().get(), magic_size};

    if (actual_magic != expected_magic) {
        throw ParticleBakerException("Effekseer particle has invalid magic", path, expected_magic, actual_magic);
    }
    if (file_data.size() > numeric_cast<size_t>(std::numeric_limits<int32_t>::max())) {
        throw ParticleBakerException("Effekseer particle is too large", path, file_data.size());
    }

    const Effekseer::SettingRef setting = Effekseer::Setting::Create();
    const Effekseer::EffectRef effect = Effekseer::Effect::Create(setting, file_data.data(), numeric_cast<int32_t>(file_data.size()), 1.0f, u"");

    if (!effect) {
        throw ParticleBakerException("Effekseer core rejected particle binary", path);
    }

    _context->WriteData(path, vector<uint8_t> {file_data.begin(), file_data.end()});
}
#endif

FO_END_NAMESPACE

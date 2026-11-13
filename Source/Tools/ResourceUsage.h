// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

FO_BEGIN_NAMESPACE

class FileSystem;
class Properties;

enum class BakingOutputKind : uint8_t
{
    Required,
    Resource,
};

struct ResourceOutput
{
    string Pack {};
    string Path {};
    string Baker {};
    BakingOutputKind Kind {};
};

class ResourceUsage final
{
public:
    using OutputAvailable = function<bool(const ResourceOutput&)>;
    using Dependencies = function<vector<string>(const ResourceOutput&)>;

    [[nodiscard]] auto GetOutputs() const -> vector<ResourceOutput>;

    void RegisterPack(string_view pack);
    void RegisterOutput(string_view pack, string_view path, string_view baker, BakingOutputKind kind);
    void AddReference(string_view path, string_view reason);
    void AddProperties(const Properties& properties, string_view owner);
    void CollectReferences(const FileSystem& resources);
    auto CreateManifest(string_view build_hash, string_view config_hash, bool filter_resources, const OutputAvailable& available, const Dependencies& dependencies) const -> string;

private:
    mutable mutex _locker {};
    set<string> _packs FO_TSA_GUARDED_BY(_locker) {};
    map<string, ResourceOutput> _outputs FO_TSA_GUARDED_BY(_locker) {};
    map<string, set<string>> _references FO_TSA_GUARDED_BY(_locker) {};
};

FO_END_NAMESPACE

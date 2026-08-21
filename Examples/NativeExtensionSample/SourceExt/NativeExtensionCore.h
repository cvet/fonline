#pragma once

#include <cstdint>

namespace NativeExtensionSample
{

struct ServerState final
{
    std::int32_t BaseValue {};
};

inline constexpr std::int32_t InitialBaseValue = 41;

[[nodiscard]] auto EvaluateValue(const ServerState& state, std::int32_t delta) noexcept -> std::int32_t;

}

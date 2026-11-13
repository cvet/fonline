#pragma once

#include <cstdint>

namespace NativeExtensionSample
{

    struct ServerState
    {
        std::int32_t BaseValue {};
    };

    inline constexpr std::int32_t InitialBaseValue {41};

    auto EvaluateValue(const ServerState& state, std::int32_t delta) noexcept -> std::int32_t;

}

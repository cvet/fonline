#include "NativeExtensionCore.h"

auto NativeExtensionSample::EvaluateValue(const ServerState& state, std::int32_t delta) noexcept -> std::int32_t
{
    return state.BaseValue + delta;
}

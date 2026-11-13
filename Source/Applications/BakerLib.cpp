// FOnline Engine
// SPDX-License-Identifier: MIT

#include "Common.h"

#include "Baker.h"
#include "Logging.h"

#if FO_TRACE_ENABLED
#include <client/TracyProfiler.hpp>
#include <tracy/Tracy.hpp>
#endif

FO_USING_NAMESPACE();

FO_EXPORT_FUNC auto FO_BakeResources(void* baking_settings) noexcept -> bool
{
#if FO_TRACE_ENABLED
    tracy::GetProfiler().RequestShutdown();
#endif

    // The caller carries on in this process, so a set built here has the log writer and pools it started
    // joined before control goes back. A set that already existed belongs to the application around us
    bool owns_global_data {global_data::create()};

    auto join_before_return {scope_exit([owns_global_data]() noexcept {
        if (owns_global_data) {
            global_data::destroy();
        }
    })};

    logging::to_file(strex("{}_BakerLib.log", FO_DEV_NAME));

    auto settings {cast_from_void<BakingSettings*>(baking_settings)};

    if (!settings) {
        logging::write("Baker DLL: baking rejected, settings pointer is null");
        return false;
    }

    auto baker {MasterBaker(settings)};

    if (!baker.BakeAll()) {
        return false;
    }

    return true;
}

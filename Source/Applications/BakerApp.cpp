// FOnline Engine
// SPDX-License-Identifier: MIT

#include "Common.h"

#include "Application.h"
#include "Baker.h"

FO_USING_NAMESPACE();

#if !FO_TESTING_APP
int main(int argc, char** argv)
#else
[[maybe_unused]] static auto BakerApp(CommandLineArgs args) -> int
#endif
{
#if !FO_TESTING_APP
    ProgramArgs program_args {numeric_cast<int32_t>(argc), argv};
    CommandLineArgs args {program_args.GetArgs()};
#endif

    try {
        InitApp(args, AppInitFlags::DisableLogTags);

        bool baking_result {};

        {
            auto baker {MasterBaker(&GetApp()->Settings)};
            baking_result = baker.BakeAll();
        }

        exit_app(baking_result);
    }
    catch (const std::exception& ex) {
        exceptions::report_and_exit(ex);
    }
    catch (...) {
        FO_UNKNOWN_EXCEPTION();
    }
}

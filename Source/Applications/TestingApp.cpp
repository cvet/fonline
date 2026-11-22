// FOnline Engine
// SPDX-License-Identifier: MIT

#ifndef FO_TESTING_APP
#error FO_TESTING_APP is not defined
#endif

#if !FO_TESTING_APP
#error FO_TESTING_APP is not enabled
#endif

#include "Common.h"

#include "catch_amalgamated.hpp"

#include "Application.h"

FO_USING_NAMESPACE();

int32_t main(int32_t argc, char** argv)
{
    try {
        IsTestingInProgress = true;
        InitAppForTesting();
        int32_t result {Catch::Session().run(argc, argv)};
        IsTestingInProgress = false;
        return result;
    }
    catch (const std::exception& ex) {
        exceptions::report_and_exit(ex);
    }
    catch (...) {
        FO_UNKNOWN_EXCEPTION();
    }
}

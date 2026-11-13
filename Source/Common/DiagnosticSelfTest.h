// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

FO_BEGIN_NAMESPACE

// FO_SELFTEST_CRASH drives deliberate process failures for crash-to-log tests.
// Keep this surface invisible to production configuration and scripts
namespace DiagnosticSelfTest
{
    // Induces the FO_SELFTEST_CRASH mode; unset or unknown modes are no-ops
    void RunIfRequested();
}

FO_END_NAMESPACE

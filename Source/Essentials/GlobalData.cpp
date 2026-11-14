// FOnline Engine
// SPDX-License-Identifier: MIT

#include "GlobalData.h"

FO_BEGIN_NAMESPACE

global_data::callback global_data::create_callbacks[global_data::MAX_CALLBACKS] {};
global_data::callback global_data::delete_callbacks[global_data::MAX_CALLBACKS] {};
const char* global_data::callback_names[global_data::MAX_CALLBACKS] {};
int32_t global_data::callbacks_count {};

// This module is the process-wide bootstrap registry that every engine instance is built on, so its state
// is file-scope by necessity. The arrays are filled by static constructors, which run single-threaded
static std::recursive_mutex global_data_locker {};
static bool global_data_created {false};
static bool global_data_sweeping {false};

// A callback that reaches back into a sweep would build or tear down the set on top of itself. The lock is
// recursive so that the second entry arrives here and says what happened, instead of hanging inside it
static void verify_not_sweeping()
{
    if (global_data_sweeping) {
        global_data::report_misuse_and_exit("global data set", "swept from inside one of its own callbacks");
    }
}

auto global_data::create() -> bool
{
    std::scoped_lock locker {global_data_locker};

    verify_not_sweeping();

    if (global_data_created) {
        return false;
    }

    global_data_sweeping = true;

    // The callbacks are noexcept, so this loop either completes or the process is gone: no caller ever
    // observes a partially built set of globals, and nothing can skip the reset below
    for (int32_t i {0}; i < global_data::callbacks_count; i++) {
        global_data::create_callbacks[i]();
    }

    global_data_sweeping = false;
    global_data_created = true;
    return true;
}

void global_data::destroy(teardown_observer observer, void* observer_context)
{
    std::scoped_lock locker {global_data_locker};

    verify_not_sweeping();

    if (!global_data_created) {
        return;
    }

    global_data_created = false;
    global_data_sweeping = true;

    for (int32_t i {0}; i < global_data::callbacks_count; i++) {
        if (observer != nullptr) {
            observer(observer_context, global_data::callback_names[i] != nullptr ? global_data::callback_names[i] : "");
        }

        global_data::delete_callbacks[i]();
    }

    global_data_sweeping = false;
}

[[noreturn]] void global_data::report_misuse_and_exit(const char* class_name, const char* misuse) noexcept
{
    // Below the logging layer, so the message goes straight out; the log file may not even exist yet
    std::cerr << "\nGLOBAL DATA MISUSE!\n" << class_name << " " << misuse << "\n";
    std::cerr.flush();

    ignore_unused(break_into_debugger());
    exit_app(false);
}

FO_END_NAMESPACE

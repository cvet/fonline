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

#include "Common.h"

#include "AdminPanelHeadless.h"
#include "Application.h"

FO_USING_NAMESPACE();

namespace
{
    auto ParseAdminPanelHeadlessOptions(const CommandLineArgs& args) -> AdminPanelHeadlessOptions
    {
        AdminPanelHeadlessOptions options;

        for (size_t i = 1; i < args.size(); i++) {
            const string_view arg = args.Get(i);

            if (arg == "--connect") {
                options.AutoConnect = true;
            }
            else if (arg == "--observer") {
                options.RequestControl = false;
            }
            else if (arg == "--no-discovery") {
                options.DiscoveryEnabled = false;
            }
            else if (strex(arg).starts_with("--host=")) {
                options.Host = string(arg.substr(7));
            }
            else if (strex(arg).starts_with("--password=")) {
                options.Password = string(arg.substr(11));
            }
            else if (strex(arg).starts_with("--port=")) {
                const auto port_str = arg.substr(7);

                if (strvex(port_str).is_number()) {
                    const auto port = strvex(port_str).to_int32();

                    if (port > 0 && port <= std::numeric_limits<uint16_t>::max()) {
                        options.Port = numeric_cast<uint16_t>(port);
                    }
                }
            }
        }

        return options;
    }
}

#if !FO_TESTING_APP
int main(int argc, char** argv)
#else
[[maybe_unused]] static auto AdminPanelHeadlessApp(CommandLineArgs args) -> int
#endif
{
    FO_STACK_TRACE_ENTRY();

#if !FO_TESTING_APP
    const CommandLineArgs args {numeric_cast<int32_t>(argc), argv};
#endif

    try {
        InitApp(args, AppInitFlags::PrebakeResources);

        AdminPanelHeadless panel(ParseAdminPanelHeadlessOptions(args));
        panel.Run();

        ExitApp(true);
    }
    catch (const std::exception& ex) {
        ReportExceptionAndExit(ex);
    }
    catch (...) {
        FO_UNKNOWN_EXCEPTION();
    }
}

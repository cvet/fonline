#include "NativeExtensionCore.h"

#include <iostream>

int main()
{
    NativeExtensionSample::ServerState first {NativeExtensionSample::InitialBaseValue};
    NativeExtensionSample::ServerState second {7};

    if (NativeExtensionSample::EvaluateValue(first, 1) != 42 ||
        NativeExtensionSample::EvaluateValue(second, -2) != 5) {
        std::cerr << "native extension core contract failed\n";
        return 1;
    }

    std::cout << "native_extension_core_test_passed\n";
    return 0;
}

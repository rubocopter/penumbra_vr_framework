#include "native_input_bridge_test_access.hpp"

#include <iostream>
#include <string>

int main() {
    std::string error;
    if (!penumbra_vr::backends::black_plague::
            RunNativeInputBridgeContractHarness(error)) {
        std::cerr << "native input bridge contract failed: " << error << '\n';
        return 1;
    }
    std::cout << "native input bridge crouch ownership harness passed\n";
    return 0;
}

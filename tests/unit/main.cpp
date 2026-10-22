#include "test.h"

int main() {
    int failed_cases = 0;
    for (const auto &c : test::registry()) {
        int before = test::failures();
        c.fn();
        bool ok = test::failures() == before;
        std::printf("[%s] %s\n", ok ? " OK " : "FAIL", c.name);
        failed_cases += ok ? 0 : 1;
    }
    std::printf("\n%zu tests, %d failed\n", test::registry().size(), failed_cases);
    return failed_cases == 0 ? 0 : 1;
}

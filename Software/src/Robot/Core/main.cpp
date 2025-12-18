#include <iostream>
#include "mapping.h"
#include "navigation.h"
#include "executor.h"

int main() {
    std::cout << "START\n";

    if(!runMapping()){
        std::cout << "MAPPING_FAIL\n";
        return 1;
    }
    std::cout << "MAPPING_OK\n";

    if(!runNavigation()){
        std::cout << "NAV_FAIL\n";
        return 2;
    }
    std::cout << "NAV_OK\n";

    if(!runExecuteNavigation()){
        std::cout << "EXEC_FAIL\n";
        return 3;
    }
    std::cout << "EXEC_OK\n";

    std::cout << "DONE\n";
    return 0;
}

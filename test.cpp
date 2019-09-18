#include "stdlib/thread.hpp"
#include <iostream>

int main() {
    sync::thread t{[]{std::cout << "hello from thread!";}};
    t.join(); 
    return 0;
}

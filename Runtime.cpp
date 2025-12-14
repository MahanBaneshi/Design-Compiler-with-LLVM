// Runtime.cpp
#include <iostream>

extern "C" void print_i32(int x) {
    std::cout << x << std::endl;
}

extern "C" void print_f64(double x) {
    std::cout << x << std::endl;
}

extern "C" void print_bool(bool x) {
    std::cout << (x ? "true" : "false") << std::endl;
}

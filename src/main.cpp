#include <exception>
#include <iostream>

#include "app/Application.h"

int main() {
    try {
        return runApplication();
    } catch (const std::exception& error) {
        std::cerr << "Fatal error: " << error.what() << '\n';
        return 1;
    }
}

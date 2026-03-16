#pragma once

extern "C" {
#include "../memory.h"
}

class Ram {
public:
    Ram();
    ~Ram();

    Ram(const Ram&) = delete;
    Ram& operator=(const Ram&) = delete;

    Memory* raw();
    const Memory* raw() const;

private:
    Memory mem_{};
};

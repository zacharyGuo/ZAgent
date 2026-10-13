#pragma once

#include <string>
#include <vector>

namespace zagent {

struct Version {
    int major;
    int minor;
    int patch;

    std::string ToString() const;
};

struct Platform {
    static std::string Name();
    static std::string Arch();
};

} // namespace zagent

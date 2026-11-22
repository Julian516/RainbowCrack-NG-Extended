#pragma once

#include <cstdint>
#include <vector>
#include <stdexcept>
#include <fstream>
#include <string>

class CoverageBitmap {
public:
    explicit CoverageBitmap(uint64_t space)
        : space_(space), bits_((space + 7) / 8, 0) {}

    void mark(uint64_t index) {
        if (index >= space_)
            throw std::out_of_range("Coverage index out of range");

        bits_[index >> 3] |=
            static_cast<uint8_t>(1u << (index & 7));
    }

    void merge(const CoverageBitmap& other) {
        if (space_ != other.space_)
            throw std::invalid_argument("Coverage space mismatch");

        for (size_t i = 0; i < bits_.size(); ++i)
            bits_[i] |= other.bits_[i];
    }

    uint64_t count() const {
        uint64_t total = 0;
        for (uint8_t byte : bits_)
            total += __builtin_popcount(
                static_cast<unsigned int>(byte)
            );
        return total;
    }

    bool save(const std::string& path) const {
        std::ofstream out(path, std::ios::binary);
        if (!out)
            return false;

        out.write(
            reinterpret_cast<const char*>(bits_.data()),
            bits_.size()
        );

        out.close();
        return out.good();
    }

private:
    uint64_t space_;
    std::vector<uint8_t> bits_;
};
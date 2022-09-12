#pragma once

#include <array>
#include <string>
#include <climits>
#include <vector>

template <typename IntegerType>
inline std::string intToHexString(IntegerType value,
                                  size_t digits = sizeof(IntegerType) << 1)
{
    static constexpr std::array<char, 16> digitsArray = {
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
    std::string rc(digits, '0');
    size_t bitIndex = (digits - 1) * 4;
    for (size_t digitIndex = 0; digitIndex < digits; digitIndex++)
    {
        rc[digitIndex] = digitsArray[(value >> bitIndex) & 0x0f];
        bitIndex -= 4;
    }
    return rc;
}

inline std::vector<std::string> intToHexByteArray(uint32_t value,
                                  size_t digits = sizeof(uint32_t) << 1)
{
    static constexpr std::array<char, 16> digitsArray = {
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
    std::string rc(digits, '0');
    size_t bitIndex = (digits - 1) * 4;
    for (size_t digitIndex = 0; digitIndex < digits; digitIndex++)
    {
        rc[digitIndex] = digitsArray[(value >> bitIndex) & 0x0f];
        bitIndex -= 4;
    }

    size_t len = 2;
    std::vector<std::string> hexArray;
    for(auto i = digits ; i >= len ; i = i-len)
    {
        hexArray.push_back("0x" + rc.substr(i-len, 2));
    }

    return hexArray;
}


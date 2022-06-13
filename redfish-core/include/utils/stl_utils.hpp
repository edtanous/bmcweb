#pragma once

#include <algorithm>
#include <string>
#include <vector>

namespace redfish
{

namespace stl_utils
{

template <typename ForwardIterator>
ForwardIterator firstDuplicate(ForwardIterator first, ForwardIterator last)
{
    auto newLast = first;

    for (auto current = first; current != last; ++current)
    {
        if (std::find(first, newLast, *current) == newLast)
        {
            if (newLast != current)
            {
                *newLast = *current;
            }
            ++newLast;
        }
    }

    return newLast;
}

template <typename T>
void removeDuplicate(T& t)
{
    t.erase(firstDuplicate(t.begin(), t.end()), t.end());
}

std::vector<uint8_t> getBytes(std::string const& s)
{
    std::vector<uint8_t> bytes;
    bytes.reserve(s.size());

    std::transform(std::begin(s), std::end(s), std::back_inserter(bytes),
                   [](char c) { return static_cast<uint8_t>(c); });

    return bytes;
}

} // namespace stl_utils
} // namespace redfish

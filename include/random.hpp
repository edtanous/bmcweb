#pragma once

#include <openssl/rand.h>

#include <iostream>

namespace bmcweb
{

struct OpenSSLGenerator
{
    uint8_t operator()()
    {
        uint8_t index = 0;
        int rc = RAND_bytes(&index, sizeof(index));
        if (rc != opensslSuccess)
        {
            std::cerr << "Cannot get random number\n";
            err = true;
        }

        return index;
    }

    static constexpr uint8_t max()
    {
        return std::numeric_limits<uint8_t>::max();
    }
    static constexpr uint8_t min()
    {
        return std::numeric_limits<uint8_t>::min();
    }

    bool error() const
    {
        return err;
    }

    // all generators require this variable
    using result_type = uint8_t;

  private:
    // RAND_bytes() returns 1 on success, 0 otherwise. -1 if bad function
    static constexpr int opensslSuccess = 1;
    bool err = false;
};

} // namespace bmcweb

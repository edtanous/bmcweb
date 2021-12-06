#pragma once

#include <cstdlib>
#include <cstring>
#include <array>
#include <string>
#include <vector>

const std::string stubCertname("/tmp/test.crt");
const std::string stubCn("cn");

std::vector<char> stubPkeyPwd = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a,
    0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
    0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
};

int mockPkeyPwdCb(char* buf, int, int, void*)
{
    memcpy(buf, stubPkeyPwd.data(), static_cast<size_t>(stubPkeyPwd.size()));
    return static_cast<int>(stubPkeyPwd.size());
}

std::vector<char> stubPkeyInvalidPwd = {
    0x01, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a,
    0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
    0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
};
int mockPkeyInvalidPwdCb(char* buf, int, int, void*)
{
    memcpy(buf, stubPkeyInvalidPwd.data(),
           static_cast<size_t>(stubPkeyInvalidPwd.size()));
    return static_cast<int>(stubPkeyInvalidPwd.size());
}

int mockNoPwdCb(char*, int, int, void*)
{
    return 0;
}

// Non-encrypted EC key with x509 certificate data
static std::array<char, 1162UL> mockNonEncryptedCredFile = {
    "\x2D\x2D\x2D\x2D\x2D\x42\x45\x47\x49\x4E\x20\x50\x52\x49\x56\x41"
    "\x54\x45\x20\x4B\x45\x59\x2D\x2D\x2D\x2D\x2D\x0A\x4D\x49\x47\x32"
    "\x41\x67\x45\x41\x4D\x42\x41\x47\x42\x79\x71\x47\x53\x4D\x34\x39"
    "\x41\x67\x45\x47\x42\x53\x75\x42\x42\x41\x41\x69\x42\x49\x47\x65"
    "\x4D\x49\x47\x62\x41\x67\x45\x42\x42\x44\x44\x69\x4C\x37\x59\x7A"
    "\x4A\x77\x2F\x4F\x4F\x70\x30\x6B\x56\x47\x57\x65\x0A\x57\x51\x30"
    "\x34\x71\x38\x4A\x51\x71\x4C\x63\x2F\x39\x2F\x31\x64\x6C\x6D\x42"
    "\x7A\x6C\x5A\x66\x4C\x32\x6A\x69\x70\x52\x66\x4C\x37\x61\x74\x4C"
    "\x4D\x6B\x34\x32\x4A\x2B\x4C\x53\x4F\x47\x45\x36\x68\x5A\x41\x4E"
    "\x69\x41\x41\x52\x30\x4C\x6A\x30\x74\x50\x54\x45\x62\x0A\x33\x50"
    "\x6E\x4E\x4F\x2F\x34\x30\x4A\x57\x57\x6D\x33\x6D\x75\x4F\x62\x2F"
    "\x32\x72\x68\x67\x33\x35\x4D\x34\x76\x32\x36\x35\x71\x43\x70\x49"
    "\x4D\x51\x6C\x50\x61\x57\x4A\x6E\x6D\x45\x4B\x2F\x32\x4C\x75\x43"
    "\x31\x6D\x68\x2F\x44\x4B\x2B\x78\x46\x4B\x74\x35\x41\x4B\x0A\x67"
    "\x2B\x44\x66\x74\x79\x66\x44\x34\x33\x4D\x45\x6D\x34\x49\x2B\x49"
    "\x2F\x43\x39\x4D\x4A\x4C\x77\x32\x71\x6C\x76\x33\x34\x6A\x59\x31"
    "\x55\x51\x6C\x30\x42\x4B\x6F\x79\x38\x37\x61\x37\x38\x47\x52\x4A"
    "\x32\x4F\x73\x71\x33\x49\x3D\x0A\x2D\x2D\x2D\x2D\x2D\x45\x4E\x44"
    "\x20\x50\x52\x49\x56\x41\x54\x45\x20\x4B\x45\x59\x2D\x2D\x2D\x2D"
    "\x2D\x0A\x2D\x2D\x2D\x2D\x2D\x42\x45\x47\x49\x4E\x20\x43\x45\x52"
    "\x54\x49\x46\x49\x43\x41\x54\x45\x2D\x2D\x2D\x2D\x2D\x0A\x4D\x49"
    "\x49\x43\x53\x7A\x43\x43\x41\x64\x47\x67\x41\x77\x49\x42\x41\x67"
    "\x49\x45\x52\x4B\x70\x6E\x6F\x54\x41\x4B\x42\x67\x67\x71\x68\x6B"
    "\x6A\x4F\x50\x51\x51\x44\x41\x6A\x41\x78\x4D\x51\x73\x77\x43\x51"
    "\x59\x44\x56\x51\x51\x47\x45\x77\x4A\x56\x55\x7A\x45\x51\x0A\x4D"
    "\x41\x34\x47\x41\x31\x55\x45\x43\x67\x77\x48\x54\x33\x42\x6C\x62"
    "\x6B\x4A\x4E\x51\x7A\x45\x51\x4D\x41\x34\x47\x41\x31\x55\x45\x41"
    "\x77\x77\x48\x62\x57\x39\x6A\x61\x79\x42\x6A\x62\x6A\x41\x65\x46"
    "\x77\x30\x79\x4D\x6A\x41\x78\x4D\x44\x55\x78\x4D\x44\x4D\x34\x0A"
    "\x4E\x44\x42\x61\x46\x77\x30\x7A\x4D\x6A\x41\x78\x4D\x44\x4D\x78"
    "\x4D\x44\x4D\x34\x4E\x44\x42\x61\x4D\x44\x45\x78\x43\x7A\x41\x4A"
    "\x42\x67\x4E\x56\x42\x41\x59\x54\x41\x6C\x56\x54\x4D\x52\x41\x77"
    "\x44\x67\x59\x44\x56\x51\x51\x4B\x44\x41\x64\x50\x63\x47\x56\x75"
    "\x0A\x51\x6B\x31\x44\x4D\x52\x41\x77\x44\x67\x59\x44\x56\x51\x51"
    "\x44\x44\x41\x64\x74\x62\x32\x4E\x72\x49\x47\x4E\x75\x4D\x48\x59"
    "\x77\x45\x41\x59\x48\x4B\x6F\x5A\x49\x7A\x6A\x30\x43\x41\x51\x59"
    "\x46\x4B\x34\x45\x45\x41\x43\x49\x44\x59\x67\x41\x45\x64\x43\x34"
    "\x39\x0A\x4C\x54\x30\x78\x47\x39\x7A\x35\x7A\x54\x76\x2B\x4E\x43"
    "\x56\x6C\x70\x74\x35\x72\x6A\x6D\x2F\x39\x71\x34\x59\x4E\x2B\x54"
    "\x4F\x4C\x39\x75\x75\x61\x67\x71\x53\x44\x45\x4A\x54\x32\x6C\x69"
    "\x5A\x35\x68\x43\x76\x39\x69\x37\x67\x74\x5A\x6F\x66\x77\x79\x76"
    "\x73\x52\x0A\x53\x72\x65\x51\x43\x6F\x50\x67\x33\x37\x63\x6E\x77"
    "\x2B\x4E\x7A\x42\x4A\x75\x43\x50\x69\x50\x77\x76\x54\x43\x53\x38"
    "\x4E\x71\x70\x62\x39\x2B\x49\x32\x4E\x56\x45\x4A\x64\x41\x53\x71"
    "\x4D\x76\x4F\x32\x75\x2F\x42\x6B\x53\x64\x6A\x72\x4B\x74\x79\x6F"
    "\x34\x47\x35\x0A\x4D\x49\x47\x32\x4D\x41\x38\x47\x41\x31\x55\x64"
    "\x45\x77\x45\x42\x2F\x77\x51\x46\x4D\x41\x4D\x42\x41\x66\x38\x77"
    "\x45\x67\x59\x44\x56\x52\x30\x52\x42\x41\x73\x77\x43\x59\x49\x48"
    "\x62\x57\x39\x6A\x61\x79\x42\x6A\x62\x6A\x41\x64\x42\x67\x4E\x56"
    "\x48\x51\x34\x45\x0A\x46\x67\x51\x55\x4C\x66\x71\x59\x70\x54\x35"
    "\x56\x53\x6A\x6B\x4F\x67\x77\x72\x78\x2B\x4B\x37\x64\x51\x67\x48"
    "\x6C\x4C\x4B\x34\x77\x48\x77\x59\x44\x56\x52\x30\x6A\x42\x42\x67"
    "\x77\x46\x6F\x41\x55\x4C\x66\x71\x59\x70\x54\x35\x56\x53\x6A\x6B"
    "\x4F\x67\x77\x72\x78\x0A\x2B\x4B\x37\x64\x51\x67\x48\x6C\x4C\x4B"
    "\x34\x77\x43\x77\x59\x44\x56\x52\x30\x50\x42\x41\x51\x44\x41\x67"
    "\x57\x67\x4D\x42\x4D\x47\x41\x31\x55\x64\x4A\x51\x51\x4D\x4D\x41"
    "\x6F\x47\x43\x43\x73\x47\x41\x51\x55\x46\x42\x77\x4D\x42\x4D\x43"
    "\x30\x47\x43\x57\x43\x47\x0A\x53\x41\x47\x47\x2B\x45\x49\x42\x44"
    "\x51\x51\x67\x46\x68\x35\x48\x5A\x57\x35\x6C\x63\x6D\x46\x30\x5A"
    "\x57\x51\x67\x5A\x6E\x4A\x76\x62\x53\x42\x50\x63\x47\x56\x75\x51"
    "\x6B\x31\x44\x49\x48\x4E\x6C\x63\x6E\x5A\x70\x59\x32\x55\x77\x43"
    "\x67\x59\x49\x4B\x6F\x5A\x49\x0A\x7A\x6A\x30\x45\x41\x77\x49\x44"
    "\x61\x41\x41\x77\x5A\x51\x49\x78\x41\x4B\x57\x45\x5A\x78\x55\x33"
    "\x4F\x4B\x7A\x46\x79\x5A\x31\x78\x35\x49\x4E\x52\x72\x43\x30\x34"
    "\x34\x43\x63\x75\x33\x6D\x59\x51\x6B\x51\x39\x74\x6C\x31\x4C\x51"
    "\x30\x73\x51\x61\x79\x46\x6C\x34\x0A\x6D\x4E\x62\x50\x6B\x34\x4D"
    "\x42\x70\x73\x46\x69\x37\x63\x56\x41\x6C\x67\x49\x77\x50\x67\x36"
    "\x6C\x71\x65\x7A\x54\x6D\x69\x39\x62\x65\x69\x72\x42\x36\x6B\x58"
    "\x58\x4C\x54\x4C\x75\x43\x6B\x58\x44\x62\x45\x70\x77\x78\x69\x6C"
    "\x41\x37\x33\x51\x65\x6E\x2F\x41\x34\x0A\x73\x6A\x75\x66\x71\x5A"
    "\x67\x68\x30\x76\x62\x6E\x47\x65\x45\x64\x63\x48\x4D\x63\x0A\x2D"
    "\x2D\x2D\x2D\x2D\x45\x4E\x44\x20\x43\x45\x52\x54\x49\x46\x49\x43"
    "\x41\x54\x45\x2D\x2D\x2D\x2D\x2D\x0A"};

size_t mockNonEncryptedCredFileSize = sizeof(mockNonEncryptedCredFile);

// Encrypted EC key with x509 certificate data encrypted with mock2_pkeyPwd
static std::array<char, 1320> mockEncryptedCredFile = {
    "\x2D\x2D\x2D\x2D\x2D\x42\x45\x47\x49\x4E\x20\x45\x4E\x43\x52\x59"
    "\x50\x54\x45\x44\x20\x50\x52\x49\x56\x41\x54\x45\x20\x4B\x45\x59"
    "\x2D\x2D\x2D\x2D\x2D\x0A\x4D\x49\x49\x42\x48\x44\x42\x58\x42\x67"
    "\x6B\x71\x68\x6B\x69\x47\x39\x77\x30\x42\x42\x51\x30\x77\x53\x6A"
    "\x41\x70\x42\x67\x6B\x71\x68\x6B\x69\x47\x39\x77\x30\x42\x42\x51"
    "\x77\x77\x48\x41\x51\x49\x59\x2F\x67\x64\x30\x5A\x4F\x36\x55\x6D"
    "\x49\x43\x41\x67\x67\x41\x0A\x4D\x41\x77\x47\x43\x43\x71\x47\x53"
    "\x49\x62\x33\x44\x51\x49\x4A\x42\x51\x41\x77\x48\x51\x59\x4A\x59"
    "\x49\x5A\x49\x41\x57\x55\x44\x42\x41\x45\x71\x42\x42\x44\x58\x4E"
    "\x4C\x49\x72\x56\x50\x72\x64\x58\x50\x4C\x4F\x4D\x34\x35\x70\x38"
    "\x42\x6F\x4E\x42\x49\x48\x41\x0A\x53\x36\x53\x52\x77\x6B\x65\x6C"
    "\x4E\x62\x7A\x78\x67\x4F\x38\x32\x4E\x46\x7A\x6D\x66\x74\x54\x6E"
    "\x7A\x5A\x35\x4A\x52\x74\x63\x76\x62\x63\x6A\x67\x58\x70\x74\x39"
    "\x64\x4C\x58\x6A\x73\x6F\x4C\x6C\x53\x56\x46\x2F\x54\x62\x68\x33"
    "\x4F\x32\x44\x64\x69\x73\x51\x77\x0A\x33\x4D\x43\x78\x51\x69\x63"
    "\x54\x62\x46\x68\x42\x6F\x70\x69\x58\x57\x4A\x76\x73\x52\x58\x39"
    "\x37\x68\x48\x66\x32\x4E\x4A\x56\x65\x59\x46\x47\x6D\x53\x45\x36"
    "\x57\x75\x67\x38\x33\x49\x61\x4D\x6B\x59\x69\x32\x5A\x48\x76\x6F"
    "\x38\x4F\x42\x46\x50\x6D\x4A\x4E\x4E\x0A\x69\x56\x64\x4A\x74\x32"
    "\x63\x6D\x76\x50\x75\x32\x76\x4E\x32\x78\x58\x66\x53\x52\x34\x47"
    "\x46\x67\x37\x31\x79\x64\x2F\x2B\x45\x6A\x71\x6A\x30\x67\x68\x59"
    "\x61\x54\x34\x44\x66\x70\x35\x39\x66\x72\x33\x54\x75\x46\x37\x37"
    "\x72\x50\x54\x56\x77\x31\x44\x38\x4C\x33\x0A\x58\x4C\x65\x35\x43"
    "\x75\x6B\x6D\x58\x73\x45\x6B\x34\x4A\x76\x53\x32\x64\x74\x6E\x2B"
    "\x4E\x63\x64\x4E\x45\x70\x4B\x4F\x79\x78\x45\x33\x47\x59\x77\x75"
    "\x6C\x35\x41\x47\x49\x2B\x4C\x62\x6A\x34\x42\x74\x4D\x52\x45\x68"
    "\x68\x64\x4E\x4D\x4A\x64\x43\x78\x39\x4B\x6F\x0A\x2D\x2D\x2D\x2D"
    "\x2D\x45\x4E\x44\x20\x45\x4E\x43\x52\x59\x50\x54\x45\x44\x20\x50"
    "\x52\x49\x56\x41\x54\x45\x20\x4B\x45\x59\x2D\x2D\x2D\x2D\x2D\x0A"
    "\x2D\x2D\x2D\x2D\x2D\x42\x45\x47\x49\x4E\x20\x43\x45\x52\x54\x49"
    "\x46\x49\x43\x41\x54\x45\x2D\x2D\x2D\x2D\x2D\x0A\x4D\x49\x49\x43"
    "\x53\x7A\x43\x43\x41\x64\x47\x67\x41\x77\x49\x42\x41\x67\x49\x45"
    "\x52\x4B\x70\x6E\x6F\x54\x41\x4B\x42\x67\x67\x71\x68\x6B\x6A\x4F"
    "\x50\x51\x51\x44\x41\x6A\x41\x78\x4D\x51\x73\x77\x43\x51\x59\x44"
    "\x56\x51\x51\x47\x45\x77\x4A\x56\x55\x7A\x45\x51\x0A\x4D\x41\x34"
    "\x47\x41\x31\x55\x45\x43\x67\x77\x48\x54\x33\x42\x6C\x62\x6B\x4A"
    "\x4E\x51\x7A\x45\x51\x4D\x41\x34\x47\x41\x31\x55\x45\x41\x77\x77"
    "\x48\x62\x57\x39\x6A\x61\x79\x42\x6A\x62\x6A\x41\x65\x46\x77\x30"
    "\x79\x4D\x6A\x41\x78\x4D\x44\x55\x78\x4D\x44\x4D\x34\x0A\x4E\x44"
    "\x42\x61\x46\x77\x30\x7A\x4D\x6A\x41\x78\x4D\x44\x4D\x78\x4D\x44"
    "\x4D\x34\x4E\x44\x42\x61\x4D\x44\x45\x78\x43\x7A\x41\x4A\x42\x67"
    "\x4E\x56\x42\x41\x59\x54\x41\x6C\x56\x54\x4D\x52\x41\x77\x44\x67"
    "\x59\x44\x56\x51\x51\x4B\x44\x41\x64\x50\x63\x47\x56\x75\x0A\x51"
    "\x6B\x31\x44\x4D\x52\x41\x77\x44\x67\x59\x44\x56\x51\x51\x44\x44"
    "\x41\x64\x74\x62\x32\x4E\x72\x49\x47\x4E\x75\x4D\x48\x59\x77\x45"
    "\x41\x59\x48\x4B\x6F\x5A\x49\x7A\x6A\x30\x43\x41\x51\x59\x46\x4B"
    "\x34\x45\x45\x41\x43\x49\x44\x59\x67\x41\x45\x64\x43\x34\x39\x0A"
    "\x4C\x54\x30\x78\x47\x39\x7A\x35\x7A\x54\x76\x2B\x4E\x43\x56\x6C"
    "\x70\x74\x35\x72\x6A\x6D\x2F\x39\x71\x34\x59\x4E\x2B\x54\x4F\x4C"
    "\x39\x75\x75\x61\x67\x71\x53\x44\x45\x4A\x54\x32\x6C\x69\x5A\x35"
    "\x68\x43\x76\x39\x69\x37\x67\x74\x5A\x6F\x66\x77\x79\x76\x73\x52"
    "\x0A\x53\x72\x65\x51\x43\x6F\x50\x67\x33\x37\x63\x6E\x77\x2B\x4E"
    "\x7A\x42\x4A\x75\x43\x50\x69\x50\x77\x76\x54\x43\x53\x38\x4E\x71"
    "\x70\x62\x39\x2B\x49\x32\x4E\x56\x45\x4A\x64\x41\x53\x71\x4D\x76"
    "\x4F\x32\x75\x2F\x42\x6B\x53\x64\x6A\x72\x4B\x74\x79\x6F\x34\x47"
    "\x35\x0A\x4D\x49\x47\x32\x4D\x41\x38\x47\x41\x31\x55\x64\x45\x77"
    "\x45\x42\x2F\x77\x51\x46\x4D\x41\x4D\x42\x41\x66\x38\x77\x45\x67"
    "\x59\x44\x56\x52\x30\x52\x42\x41\x73\x77\x43\x59\x49\x48\x62\x57"
    "\x39\x6A\x61\x79\x42\x6A\x62\x6A\x41\x64\x42\x67\x4E\x56\x48\x51"
    "\x34\x45\x0A\x46\x67\x51\x55\x4C\x66\x71\x59\x70\x54\x35\x56\x53"
    "\x6A\x6B\x4F\x67\x77\x72\x78\x2B\x4B\x37\x64\x51\x67\x48\x6C\x4C"
    "\x4B\x34\x77\x48\x77\x59\x44\x56\x52\x30\x6A\x42\x42\x67\x77\x46"
    "\x6F\x41\x55\x4C\x66\x71\x59\x70\x54\x35\x56\x53\x6A\x6B\x4F\x67"
    "\x77\x72\x78\x0A\x2B\x4B\x37\x64\x51\x67\x48\x6C\x4C\x4B\x34\x77"
    "\x43\x77\x59\x44\x56\x52\x30\x50\x42\x41\x51\x44\x41\x67\x57\x67"
    "\x4D\x42\x4D\x47\x41\x31\x55\x64\x4A\x51\x51\x4D\x4D\x41\x6F\x47"
    "\x43\x43\x73\x47\x41\x51\x55\x46\x42\x77\x4D\x42\x4D\x43\x30\x47"
    "\x43\x57\x43\x47\x0A\x53\x41\x47\x47\x2B\x45\x49\x42\x44\x51\x51"
    "\x67\x46\x68\x35\x48\x5A\x57\x35\x6C\x63\x6D\x46\x30\x5A\x57\x51"
    "\x67\x5A\x6E\x4A\x76\x62\x53\x42\x50\x63\x47\x56\x75\x51\x6B\x31"
    "\x44\x49\x48\x4E\x6C\x63\x6E\x5A\x70\x59\x32\x55\x77\x43\x67\x59"
    "\x49\x4B\x6F\x5A\x49\x0A\x7A\x6A\x30\x45\x41\x77\x49\x44\x61\x41"
    "\x41\x77\x5A\x51\x49\x78\x41\x4B\x57\x45\x5A\x78\x55\x33\x4F\x4B"
    "\x7A\x46\x79\x5A\x31\x78\x35\x49\x4E\x52\x72\x43\x30\x34\x34\x43"
    "\x63\x75\x33\x6D\x59\x51\x6B\x51\x39\x74\x6C\x31\x4C\x51\x30\x73"
    "\x51\x61\x79\x46\x6C\x34\x0A\x6D\x4E\x62\x50\x6B\x34\x4D\x42\x70"
    "\x73\x46\x69\x37\x63\x56\x41\x6C\x67\x49\x77\x50\x67\x36\x6C\x71"
    "\x65\x7A\x54\x6D\x69\x39\x62\x65\x69\x72\x42\x36\x6B\x58\x58\x4C"
    "\x54\x4C\x75\x43\x6B\x58\x44\x62\x45\x70\x77\x78\x69\x6C\x41\x37"
    "\x33\x51\x65\x6E\x2F\x41\x34\x0A\x73\x6A\x75\x66\x71\x5A\x67\x68"
    "\x30\x76\x62\x6E\x47\x65\x45\x64\x63\x48\x4D\x63\x0A\x2D\x2D\x2D"
    "\x2D\x2D\x45\x4E\x44\x20\x43\x45\x52\x54\x49\x46\x49\x43\x41\x54"
    "\x45\x2D\x2D\x2D\x2D\x2D\x0A"};

size_t mockEncryptedCredFileSize = sizeof(mockEncryptedCredFile);

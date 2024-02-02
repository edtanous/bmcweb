#include "type_id.hpp"

#include <optional>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace bmcweb
{
namespace
{

class TestClass
{};

TEST(TypeId, IsOptional)
{
    static_assert(IsOptional<std::optional<TestClass>>::value);
    static_assert(IsOptional<std::optional<const TestClass>>::value);
    static_assert(IsOptional<std::optional<int>>::value);

    static_assert(!IsOptional<std::optional<TestClass>&>::value);
    static_assert(!IsOptional<std::optional<TestClass>&&>::value);
    static_assert(!IsOptional<const std::optional<TestClass>>::value);
    static_assert(!IsOptional<std::vector<TestClass>>::value);
    static_assert(!IsOptional<std::vector<int>>::value);
    static_assert(!IsOptional<int>::value);
}

TEST(TypeId, IsVector)
{
    static_assert(IsVector<std::vector<TestClass>>::value);
    static_assert(IsVector<std::vector<const TestClass>>::value);
    static_assert(IsVector<std::vector<int>>::value);

    static_assert(!IsVector<std::vector<TestClass>&>::value);
    static_assert(!IsVector<std::vector<TestClass>&&>::value);
    static_assert(!IsVector<const std::vector<TestClass>>::value);
    static_assert(!IsVector<std::optional<TestClass>>::value);
    static_assert(!IsVector<std::optional<int>>::value);
    static_assert(!IsVector<int>::value);
}

TEST(TypeId, IsStdArray)
{
    static_assert(IsStdArray<std::array<TestClass, 1>>::value);
    static_assert(IsStdArray<std::array<const TestClass, 1>>::value);
    static_assert(IsStdArray<std::array<int, 1>>::value);

    static_assert(!IsStdArray<std::array<TestClass, 1>&>::value);
    static_assert(!IsStdArray<std::array<TestClass, 1>&&>::value);
    static_assert(!IsStdArray<const std::array<TestClass, 1>>::value);
    static_assert(!IsStdArray<std::optional<TestClass>>::value);
    static_assert(!IsStdArray<std::optional<int>>::value);
    static_assert(!IsStdArray<int>::value);
}

} // namespace
} // namespace bmcweb

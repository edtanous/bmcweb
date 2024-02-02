#pragma once

#include <array>
#include <optional>
#include <type_traits>
#include <vector>

namespace bmcweb
{

template <typename Type>
struct IsOptional : std::false_type
{};

template <typename Type>
struct IsOptional<std::optional<Type>> : std::true_type
{};

template <typename Type>
struct IsVector : std::false_type
{};

template <typename Type>
struct IsVector<std::vector<Type>> : std::true_type
{};

template <typename Type>
struct IsStdArray : std::false_type
{};

template <typename Type, std::size_t size>
struct IsStdArray<std::array<Type, size>> : std::true_type
{};
} // namespace bmcweb

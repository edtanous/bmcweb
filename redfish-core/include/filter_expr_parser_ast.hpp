#pragma once
#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/spirit/home/x3.hpp>
#include <boost/spirit/home/x3/support/ast/variant.hpp>

#include <iostream>
#include <list>
#include <numeric>
#include <string>

namespace redfish
{
namespace filter_ast
{

// Because some program elements can reference operand recursively, they need to
// be forward declared so that x3::forward_ast can be used
struct program;
struct logical_or;
struct logical_and;
struct negated;
struct equaltity_comparision;

// Represents a string that references an identifier
// (ie 'Enabled')
struct quoted_string : std::string
{};

// Represents a string that matches an identifier
struct unquoted_string : std::string
{};

// A struct that represents a key that can be operated on with arguments
// Note, we need to use the x3 variant here because this is recursive (ie it can
// contain itself)
struct operand :
    boost::spirit::x3::variant<
        unsigned int, quoted_string, unquoted_string,
        boost::spirit::x3::forward_ast<negated>,
        boost::spirit::x3::forward_ast<program>,
        boost::spirit::x3::forward_ast<logical_or>,
        boost::spirit::x3::forward_ast<logical_and>,
        boost::spirit::x3::forward_ast<equaltity_comparision>>
{
    using base_type::base_type;
    using base_type::operator=;
};

std::ostream& operator<<(std::ostream& stream, operand /*op*/)
{
    stream << "operand";
    return stream;
}

enum class RelationalComparisonEnum
{
    GreaterThan,
    GreaterThanOrEqual,
    LessThan,
    LessThanOrEqual
};

std::ostream& operator<<(std::ostream& stream,
                         RelationalComparisonEnum item)
{
    stream << static_cast<int>(item);
    return stream;
}

struct RelationalComparison :
    boost::spirit::x3::symbols<RelationalComparisonEnum>
{
    RelationalComparison()
    {
        add("gt", RelationalComparisonEnum::GreaterThan)(
            "ge", RelationalComparisonEnum::GreaterThanOrEqual)(
            "lt", RelationalComparisonEnum::LessThan)(
            "le", RelationalComparisonEnum::LessThanOrEqual);
    }

} relational_comparison_token;

enum class EqualityComparisonEnum
{
    Equals,
    NotEquals,
};


std::ostream& operator<<(std::ostream& stream,
                         EqualityComparisonEnum item)
{
    stream << static_cast<int>(item);
    return stream;
}

struct EqualityComparison : boost::spirit::x3::symbols<EqualityComparisonEnum>
{
    EqualityComparison()
    {
        add("ne", EqualityComparisonEnum::Equals)(
            "eq", EqualityComparisonEnum::NotEquals);
    }

} equality_comparison_token;

// An expression that has been negated with not()
struct negated
{
    operand operand_;
    std::ostream& operator<<(std::ostream& stream)
    {
        stream << "not" << operand_;
        return stream;
    }
};

// An operation between two operands (for example and, or, gt, lt)
struct operation
{
    // Note, because the ast captures by "two character" operators, and
    // "and" is 3 characters, in the parser, we use lit() to "throw away" the
    // d, and turn the operator into "an".  This prevents us from having to
    // malloc strings for every operator, and works because there are no
    // other operators that start with "an" that would conflict.  All other
    // comparisons are two characters

    RelationalComparisonEnum operator1;
    // char operator2;
    operand operand_;

    std::ostream& operator<<(std::ostream& stream)
    {
        stream << "operation" << static_cast<int>(operator1) << operand_;
        return stream;
    }
};

struct logical_and
{
    operand first;
    std::list<operand> rest;
};

struct logical_or
{
    operand first;
    std::list<operand> rest;
};

struct equality_operation
{
    EqualityComparisonEnum operator1;
    // char operator2;
    operand operand_;
};

struct equaltity_comparision
{
    operand first;
    std::list<equality_operation> rest;
};

// A list of expressions to execute
struct program
{
    operand first;
    std::list<operation> rest;
};
} // namespace filter_ast
} // namespace redfish

BOOST_FUSION_ADAPT_STRUCT(redfish::filter_ast::operation, operator1, operand_)

BOOST_FUSION_ADAPT_STRUCT(redfish::filter_ast::equaltity_comparision, first,
                          rest);
BOOST_FUSION_ADAPT_STRUCT(redfish::filter_ast::equality_operation, operator1,
                          operand_);

BOOST_FUSION_ADAPT_STRUCT(redfish::filter_ast::negated, operand_)

BOOST_FUSION_ADAPT_STRUCT(redfish::filter_ast::program, first, rest)

BOOST_FUSION_ADAPT_STRUCT(redfish::filter_ast::logical_or, first, rest)

BOOST_FUSION_ADAPT_STRUCT(redfish::filter_ast::logical_and, first, rest)

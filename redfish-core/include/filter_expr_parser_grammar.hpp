#pragma once

#include "filter_expr_parser_ast.hpp"

#include <boost/spirit/home/x3.hpp>

namespace redfish::filter_grammar
{

namespace details
{
// The below rules very intentionally use the same naming as section 7.3.4 of
// the redfish specification and are declared in the order of the precedence
// that the standard requires

// clang-format off
using boost::spirit::x3::rule;
rule<class expression, filter_ast::operand> const expression("expression");
rule<class logical_negation, filter_ast::negated> const logical_negation("logical_negation");
rule<class relational_comparison, filter_ast::program> const relational_comparison("relational_comparison");
rule<class equality_comparison, filter_ast::equaltity_comparision> const equality_comparison("equality_comparison");
rule<class logical_or, filter_ast::logical_or> const logical_or("logical_or");
rule<class logical_and, filter_ast::logical_and> const logical_and("logical_and");
rule<class quoted_string, filter_ast::quoted_string> const quoted_string("quoted_string");
rule<class unquoted_string, filter_ast::unquoted_string> const unquoted_string("unquoted_string");
// clang-format on

using boost::spirit::x3::char_;
using boost::spirit::x3::lit;
using boost::spirit::x3::uint_;
using filter_ast::equality_comparison_token;
using filter_ast::relational_comparison_token;

const auto quoted_string_def = '\'' >> *('\\' >> char_ | ~char_('\'')) >> '\'';

const auto unquoted_string_def = char_("a-zA-Z") >> *(char_("a-zA-Z0-9[]/"));

const auto basic_types = quoted_string | unquoted_string | uint_;

const auto logical_or_def = expression >> *(lit("or") >> expression);

const auto logical_and_def = logical_or >> *(lit("and") >> logical_or);

const auto equality_comparison_def = logical_and >>
                                     *(equality_comparison_token >>
                                       logical_and);

const auto relational_comparison_def = equality_comparison >>
                                       *(relational_comparison_token >>
                                         equality_comparison);

const auto logical_negation_def = /**lit("not") >>*/ relational_comparison;

const auto expression_def = lit('(') >> logical_negation >> lit(')') |
                             basic_types;

BOOST_SPIRIT_DEFINE(logical_or, logical_and, equality_comparison,
                    unquoted_string, quoted_string, relational_comparison, logical_negation,
                    expression);

static auto grammar = expression;
} // namespace details

using details::grammar;

} // namespace redfish::filter_grammar

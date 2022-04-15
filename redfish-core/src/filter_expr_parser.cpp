#include "filter_expr_parser.hpp"

#include "filter_expr_parser_ast.hpp"
#include "filter_expr_parser_grammar.hpp"
#include "logging.hpp"

#include <iostream>
#include <list>
#include <numeric>
#include <string>

namespace redfish
{
namespace ast
{
///////////////////////////////////////////////////////////////////////////
//  The AST Printer
///////////////////////////////////////////////////////////////////////////
struct printer
{
    typedef void result_type;

    void operator()(unsigned int n) const
    {
        std::cout << n;
    }

    void operator()(const filter_ast::operation& /*x*/) const
    {
        /*
        boost::apply_visitor(*this, x.operand_);

        switch (x.operator1)
        {

            case 'g':
                switch (x.operator2)
                {
                    case 't':
                        std::cout << " greater than";
                        break;
                    case 'e':
                        std::cout << " greater equal";
                        break;
                }
                break;
            case 'l':
                switch (x.operator2)
                {
                    case 't':
                        std::cout << " less than";
                        break;
                    case 'e':
                        std::cout << " less equal";
                        break;
                }
                break;
            case 'a':
                switch (x.operator2)
                {
                    case 'n':
                        std::cout << " and";
                        break;
                }
                break;
            case 'o':
                switch (x.operator2)
                {
                    case 'r':
                        std::cout << " or";
                        break;
                }
                break;
            case 'e':
                switch (x.operator2)
                {
                    case 'q':
                        std::cout << " equals";
                        break;
                }
                break;
            case 'n':
                switch (x.operator2)
                {
                    case 'e':
                        std::cout << " not equals";
                        break;
                }
                break;
        }
        */
    }
    void operator()(const filter_ast::quoted_string& x) const
    {
        std::cout << " quoted string " << x;
    }

    void operator()(const filter_ast::unquoted_string& x) const
    {
        std::cout << " unquoted string " << x;
    }

    void operator()(const filter_ast::negated& x) const
    {
        boost::apply_visitor(*this, x.operand_);
        std::cout << " not";
    }

    void operator()(const filter_ast::program& x) const
    {
        boost::apply_visitor(*this, x.first);
        for (const filter_ast::operation& oper : x.rest)
        {
            std::cout << ' ';
            (*this)(oper);
        }
    }

    void operator()(const filter_ast::logical_or& x) const
    {
        boost::apply_visitor(*this, x.first);
        /* TODO
        for (const filter_ast::operand& oper : x.rest)
        {
            std::cout << ' ';
            (*this)(oper);
        }
        */
    }

    void operator()(const filter_ast::logical_and& x) const
    {
        boost::apply_visitor(*this, x.first);
    }

    void operator()(const filter_ast::equaltity_comparision& x) const
    {
        boost::apply_visitor(*this, x.first);
    }
};

///////////////////////////////////////////////////////////////////////////
//  The AST evaluator
///////////////////////////////////////////////////////////////////////////
struct eval
{
    typedef int result_type;

    int operator()(unsigned int n) const
    {
        return static_cast<int>(n);
    }

    int operator()(int /*lhs*/, const filter_ast::operation& /*x*/) const
    {
        /*
        int rhs = boost::apply_visitor(*this, x.operand_);

        switch (x.operator1)
        {
            case 'g':
                switch (x.operator2)
                {
                    case 't':
                        return lhs > rhs ? 1 : 0;
                    case 'e':
                        return lhs >= rhs ? 1 : 0;
                }
                break;
            case 'l':
                switch (x.operator2)
                {
                    case 't':
                        return lhs < rhs ? 1 : 0;
                    case 'e':
                        return lhs <= rhs ? 1 : 0;
                }
                break;
            case 'a':
                switch (x.operator2)
                {
                    case 'n':
                        return lhs > 0 && rhs > 0;
                }
                break;
            case 'o':
                switch (x.operator2)
                {
                    case 'r':
                        return lhs > 0 || rhs > 0;
                }
                break;
            case 'e':
                switch (x.operator2)
                {
                    case 'q':
                        return lhs == rhs;
                }
                break;
            case 'n':
                switch (x.operator2)
                {
                    case 'e':
                        return lhs != rhs;
                }
                break;
        }
        */

        BOOST_ASSERT(0);
        return 0;
    }

    int operator()(const filter_ast::negated& x) const
    {
        return boost::apply_visitor(*this, x.operand_) == 0;
    }

    int operator()(const filter_ast::quoted_string& /*x*/) const
    {
        return 0;
    }

    int operator()(const filter_ast::unquoted_string& /*x*/) const
    {
        return 0;
    }

    int operator()(const filter_ast::logical_or& /*x*/) const
    {
        return 0;
        // return std::accumulate(x.rest.begin(), x.rest.end(),
        //                        boost::apply_visitor(*this, x.first), *this);
    }
    int operator()(const filter_ast::logical_and& /*x*/) const
    {
        return 0;
        // return std::accumulate(x.rest.begin(), x.rest.end(),
        //                        boost::apply_visitor(*this, x.first), *this);
    }
    int operator()(const filter_ast::equaltity_comparision& /*x*/) const
    {
        return 0;
        // return std::accumulate(x.rest.begin(), x.rest.end(),
        //                        boost::apply_visitor(*this, x.first), *this);
    }
    int operator()(const filter_ast::program& x) const
    {
        return std::accumulate(x.rest.begin(), x.rest.end(),
                               boost::apply_visitor(*this, x.first), *this);
    }
};
} // namespace ast
} // namespace redfish

bool parseFilterExpression(std::string_view expr)
{
    auto& calc = redfish::filter_grammar::grammar;
    //redfish::filter_ast::program program;
    redfish::filter_ast::operand program;
    //redfish::filter_ast::negated program;

    boost::spirit::x3::ascii::space_type space;
    std::string_view::iterator iter = expr.begin();
    const std::string_view::iterator end = expr.end();
    bool r = boost::spirit::x3::phrase_parse(iter, end, calc, space, program);

    if (!r)
    {
        std::string rest(iter, end);

        BMCWEB_LOG_ERROR("Parsing failed stopped at {}", rest);
    }
    return r;
}

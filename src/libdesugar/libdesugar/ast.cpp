#include <libutl/common/utilities.hpp>
#include <libdesugar/ast.hpp>
#include <libdesugar/ast_formatters.hpp>

auto ast::Qualified_name::is_upper() const noexcept -> bool
{
    return primary_name.is_upper.get();
}

auto ast::Qualified_name::is_unqualified() const noexcept -> bool
{
    return !root_qualifier.has_value() && middle_qualifiers.empty();
}

#define DEFINE_AST_FORMAT_TO(...)                                               \
    auto ast::format_to(__VA_ARGS__ const& object, std::string& output) -> void \
    {                                                                           \
        std::format_to(std::back_inserter(output), "{}", object);               \
    }

DEFINE_AST_FORMAT_TO(Wildcard);
DEFINE_AST_FORMAT_TO(Expression);
DEFINE_AST_FORMAT_TO(Pattern);
DEFINE_AST_FORMAT_TO(Type);
DEFINE_AST_FORMAT_TO(Definition);
DEFINE_AST_FORMAT_TO(Mutability);
DEFINE_AST_FORMAT_TO(Qualified_name);
DEFINE_AST_FORMAT_TO(Class_reference);
DEFINE_AST_FORMAT_TO(Function_parameter);
DEFINE_AST_FORMAT_TO(Function_argument);
DEFINE_AST_FORMAT_TO(Template_parameter);
DEFINE_AST_FORMAT_TO(Template_argument);
DEFINE_AST_FORMAT_TO(pattern::Field);
DEFINE_AST_FORMAT_TO(pattern::Constructor_body);
DEFINE_AST_FORMAT_TO(pattern::Constructor);
DEFINE_AST_FORMAT_TO(Template_parameters);
DEFINE_AST_FORMAT_TO(definition::Field);
DEFINE_AST_FORMAT_TO(definition::Constructor_body);
DEFINE_AST_FORMAT_TO(definition::Constructor);

#undef DEFINE_AST_FORMAT_TO

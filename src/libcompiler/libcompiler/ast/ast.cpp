#include <libutl/utilities.hpp>
#include <libcompiler/ast/ast.hpp>
#include <libcompiler/ast/formatters.hpp>

kieli::AST::~AST() = default;

kieli::AST::AST(Module&& module) : module(std::make_unique<Module>(std::move(module))) {}

auto ast::Path::is_simple_name() const noexcept -> bool
{
    return !root.has_value() && segments.empty();
}

#define DEFINE_AST_FORMAT_TO(...)                                               \
    auto ast::format_to(__VA_ARGS__ const& object, std::string& output) -> void \
    {                                                                           \
        std::format_to(std::back_inserter(output), "{}", object);               \
    }

DEFINE_AST_FORMAT_TO(Expression);
DEFINE_AST_FORMAT_TO(Pattern);
DEFINE_AST_FORMAT_TO(Type);
DEFINE_AST_FORMAT_TO(Definition);
DEFINE_AST_FORMAT_TO(Mutability);
DEFINE_AST_FORMAT_TO(Path);
DEFINE_AST_FORMAT_TO(Class_reference);
DEFINE_AST_FORMAT_TO(Function_parameter);
DEFINE_AST_FORMAT_TO(Function_argument);
DEFINE_AST_FORMAT_TO(Template_parameter);
DEFINE_AST_FORMAT_TO(Template_argument);
DEFINE_AST_FORMAT_TO(Template_parameters);

#undef DEFINE_AST_FORMAT_TO

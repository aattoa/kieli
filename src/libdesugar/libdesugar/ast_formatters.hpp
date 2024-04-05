#pragma once

#include <libutl/common/utilities.hpp>
#include <libdesugar/ast.hpp>

#define LIBDESUGAR_DECLARE_FORMATTER(...)                                                \
    template <>                                                                          \
    struct std::formatter<__VA_ARGS__> : utl::fmt::Formatter_base {                      \
        auto format(__VA_ARGS__ const&, auto& context) const -> decltype(context.out()); \
    }

#define LIBDESUGAR_DEFINE_FORMATTER(...)                                              \
    auto std::formatter<__VA_ARGS__>::format(__VA_ARGS__ const& value, auto& context) \
        const -> decltype(context.out())

LIBDESUGAR_DECLARE_FORMATTER(ast::Wildcard);
LIBDESUGAR_DECLARE_FORMATTER(ast::Expression);
LIBDESUGAR_DECLARE_FORMATTER(ast::Pattern);
LIBDESUGAR_DECLARE_FORMATTER(ast::Type);
LIBDESUGAR_DECLARE_FORMATTER(ast::Definition);
LIBDESUGAR_DECLARE_FORMATTER(ast::Mutability);
LIBDESUGAR_DECLARE_FORMATTER(ast::Qualified_name);
LIBDESUGAR_DECLARE_FORMATTER(ast::Class_reference);
LIBDESUGAR_DECLARE_FORMATTER(ast::Function_argument);
LIBDESUGAR_DECLARE_FORMATTER(ast::Function_parameter);
LIBDESUGAR_DECLARE_FORMATTER(ast::Template_argument);
LIBDESUGAR_DECLARE_FORMATTER(ast::Template_parameter);
LIBDESUGAR_DECLARE_FORMATTER(ast::pattern::Field);
LIBDESUGAR_DECLARE_FORMATTER(ast::pattern::Constructor_body);
LIBDESUGAR_DECLARE_FORMATTER(ast::pattern::Constructor);
LIBDESUGAR_DECLARE_FORMATTER(ast::definition::Field);
LIBDESUGAR_DECLARE_FORMATTER(ast::definition::Constructor_body);
LIBDESUGAR_DECLARE_FORMATTER(ast::definition::Constructor);
LIBDESUGAR_DECLARE_FORMATTER(ast::Template_parameters);

namespace libdesugar::dtl {
    template <class Out>
    struct Expression_format_visitor {
        Out out;

        auto operator()(kieli::literal auto const& literal)
        {
            std::format_to(out, "{}", literal);
        }

        auto operator()(ast::expression::Self const&)
        {
            std::format_to(out, "self");
        }

        auto operator()(ast::expression::Block const& block)
        {
            std::format_to(out, "{{");
            for (ast::Expression const& side_effect : block.side_effects) {
                std::format_to(out, " {};", side_effect);
            }
            std::format_to(out, " {} }}", block.result);
        }

        auto operator()(ast::expression::Tuple const& tuple)
        {
            std::format_to(out, "({:n})", tuple.fields);
        }

        auto operator()(ast::expression::Template_application const& application)
        {
            std::format_to(out, "{}[{:n}]", application.name, application.template_arguments);
        }

        auto operator()(ast::expression::Addressof const& addressof)
        {
            std::format_to(out, "(&{} {})", addressof.mutability, addressof.place_expression);
        }

        auto operator()(ast::expression::Type_cast const& cast)
        {
            std::format_to(out, "({} as {})", cast.expression, cast.target_type);
        }

        auto operator()(ast::expression::Type_ascription const& ascription)
        {
            std::format_to(out, "({}: {})", ascription.expression, ascription.ascribed_type);
        }

        auto operator()(ast::expression::Conditional const& conditional)
        {
            std::format_to(
                out,
                "if {} {} else {}",
                conditional.condition,
                conditional.true_branch,
                conditional.false_branch);
        }

        auto operator()(ast::expression::Meta const& meta)
        {
            std::format_to(out, "meta({})", meta.expression);
        }

        auto operator()(ast::expression::Unit_initializer const& initializer)
        {
            std::format_to(out, "{}", initializer.constructor);
        }

        auto operator()(ast::expression::Tuple_initializer const& initializer)
        {
            std::format_to(out, "{}({:n})", initializer.constructor, initializer.initializers);
        }

        auto operator()(ast::expression::Struct_initializer const& initializer)
        {
            std::format_to(out, "{} {{", initializer.constructor);
            for (auto const& [name, initializer] : initializer.initializers) {
                std::format_to(out, " {} = {}", name, initializer);
            }
            std::format_to(out, " }}");
        }

        auto operator()(ast::expression::Dereference const& dereference)
        {
            std::format_to(out, "(*{})", dereference.reference_expression);
        }

        auto operator()(ast::expression::Struct_field_access const& access)
        {
            std::format_to(out, "{}.{}", access.base_expression, access.field_name);
        }

        auto operator()(ast::expression::Tuple_field_access const& access)
        {
            std::format_to(out, "{}.{}", access.base_expression, access.field_index);
        }

        auto operator()(ast::expression::Array_index_access const& access)
        {
            std::format_to(out, "{}.[{}]", access.base_expression, access.index_expression);
        }

        auto operator()(ast::expression::Array_literal const& literal)
        {
            std::format_to(out, "[{}]", literal.elements);
        }

        auto operator()(ast::expression::Binary_operator_invocation const& invocation)
        {
            std::format_to(out, "({} {} {})", invocation.left, invocation.op, invocation.right);
        }

        auto operator()(ast::expression::Break const& break_)
        {
            std::format_to(out, "break {}", break_.result);
        }

        auto operator()(ast::expression::Continue const&)
        {
            std::format_to(out, "continue");
        }

        auto operator()(ast::expression::Hole const&)
        {
            std::format_to(out, R"(???)");
        }

        auto operator()(ast::expression::Invocation const& invocation)
        {
            std::format_to(out, "{}({})", invocation.invocable, invocation.arguments);
        }

        auto operator()(ast::expression::Let_binding const& binding)
        {
            std::format_to(out, "let {}", binding.pattern);
            if (binding.type.has_value()) {
                std::format_to(out, ": {}", binding.type.value());
            }
            std::format_to(out, " = {}", binding.initializer);
        }

        auto operator()(ast::expression::Local_type_alias const& alias)
        {
            std::format_to(out, "alias {} = {}", alias.name, alias.type);
        }

        auto operator()(ast::expression::Loop const& loop)
        {
            std::format_to(out, "loop {}", loop.body);
        }

        auto operator()(ast::expression::Match const& match)
        {
            std::format_to(out, "match {} {{", match.expression);
            for (auto const& match_case : match.cases) {
                std::format_to(out, " {} -> {}", match_case.pattern, match_case.expression);
            }
            std::format_to(out, " }}");
        }

        auto operator()(ast::expression::Method_invocation const& invocation)
        {
            std::format_to(out, "{}.{}", invocation.base_expression, invocation.method_name);
            if (invocation.template_arguments.has_value()) {
                std::format_to(out, "[{}]", invocation.template_arguments.value());
            }
            std::format_to(out, "({})", invocation.function_arguments);
        }

        auto operator()(ast::expression::Move const& move)
        {
            std::format_to(out, "mov {}", move.place_expression);
        }

        auto operator()(ast::expression::Ret const& ret)
        {
            if (ret.expression.has_value()) {
                std::format_to(out, "ret {}", ret.expression.value());
            }
            std::format_to(out, "ret");
        }

        auto operator()(ast::expression::Sizeof const& sizeof_)
        {
            std::format_to(out, "sizeof({})", sizeof_.inspected_type);
        }

        auto operator()(ast::expression::Unsafe const& unsafe)
        {
            std::format_to(out, "unsafe {}", unsafe.expression);
        }

        auto operator()(ast::expression::Variable const& variable)
        {
            std::format_to(out, "{}", variable.name);
        }
    };

    template <class Out>
    struct Pattern_format_visitor {
        Out out;

        auto operator()(kieli::literal auto const& literal)
        {
            std::format_to(out, "{}", literal);
        }

        auto operator()(ast::pattern::Tuple const& tuple)
        {
            std::format_to(out, "({:n})", tuple.field_patterns);
        }

        auto operator()(ast::pattern::Slice const& slice)
        {
            std::format_to(out, "[{:n}]", slice.element_patterns);
        }

        auto operator()(ast::Wildcard const&)
        {
            std::format_to(out, "_");
        }

        auto operator()(ast::pattern::Alias const& alias)
        {
            std::format_to(out, "{} as {} {}", alias.pattern, alias.mutability, alias.name);
        }

        auto operator()(ast::pattern::Constructor const& constructor)
        {
            std::format_to(out, "{}{}", constructor.name, constructor.body);
        }

        auto operator()(ast::pattern::Abbreviated_constructor const& constructor)
        {
            std::format_to(out, "{}{}", constructor.name, constructor.body);
        }

        auto operator()(ast::pattern::Name const& name)
        {
            std::format_to(out, "{} {}", name.mutability, name.name);
        }

        auto operator()(ast::pattern::Guarded const& guarded)
        {
            std::format_to(out, "{} if {}", guarded.guarded_pattern, guarded.guard_expression);
        }
    };

    template <class Out>
    struct Type_format_visitor {
        Out out;

        auto operator()(kieli::built_in_type::Integer const integer)
        {
            std::format_to(out, "{}", kieli::built_in_type::integer_name(integer));
        }

        auto operator()(kieli::built_in_type::Floating const&)
        {
            std::format_to(out, "Float");
        }

        auto operator()(kieli::built_in_type::Character const&)
        {
            std::format_to(out, "Char");
        }

        auto operator()(kieli::built_in_type::Boolean const&)
        {
            std::format_to(out, "Bool");
        }

        auto operator()(kieli::built_in_type::String const&)
        {
            std::format_to(out, "String");
        }

        auto operator()(ast::Wildcard const&)
        {
            std::format_to(out, "_");
        }

        auto operator()(ast::type::Function const& function)
        {
            std::format_to(out, "fn({:n}): {}", function.parameter_types, function.return_type);
        }

        auto operator()(ast::type::Self const&)
        {
            std::format_to(out, "Self");
        }

        auto operator()(ast::type::Tuple const& tuple)
        {
            std::format_to(out, "({:n})", tuple.field_types);
        }

        auto operator()(ast::type::Array const& array)
        {
            std::format_to(out, "[{}; {}]", array.element_type, array.length);
        }

        auto operator()(ast::type::Instance_of const& instance_of)
        {
            std::format_to(out, "inst {}", utl::fmt::join(instance_of.classes, " + "));
        }

        auto operator()(ast::type::Pointer const& pointer)
        {
            std::format_to(out, "*{} {}", pointer.mutability, pointer.pointee_type);
        }

        auto operator()(ast::type::Reference const& reference)
        {
            std::format_to(out, "&{} {}", reference.mutability, reference.referenced_type);
        }

        auto operator()(ast::type::Slice const& slice)
        {
            std::format_to(out, "[{}]", slice.element_type);
        }

        auto operator()(ast::type::Template_application const& application)
        {
            std::format_to(out, "{}[{}]", application.name, application.arguments);
        }

        auto operator()(ast::type::Typename const& name)
        {
            std::format_to(out, "{}", name.name);
        }

        auto operator()(ast::type::Typeof const& typeof_)
        {
            std::format_to(out, "typeof({})", typeof_.inspected_expression);
        }
    };

    template <class Out>
    struct Definition_format_visitor {
        Out out;

        auto operator()(ast::definition::Function const& function)
        {
            std::format_to(out, "fn {}", function.signature.name);
            if (function.signature.template_parameters.has_value()) {
                std::format_to(out, "[{:n}]", function.signature.template_parameters.value());
            }

            std::format_to(
                out,
                "({:n}): {}",
                function.signature.function_parameters,
                function.signature.return_type);

            assert(std::holds_alternative<ast::expression::Block>(function.body.variant));
            std::format_to(out, " {}", function.body);
        }

        auto operator()(ast::definition::Enumeration const& enumeration)
        {
            std::format_to(
                out,
                "enum {}{} = {}",
                enumeration.name,
                enumeration.template_parameters,
                utl::fmt::join(enumeration.constructors, " | "));
        }

        auto operator()(ast::definition::Alias const& alias)
        {
            std::format_to(
                out, "alias {}{} = {}", alias.name, alias.template_parameters, alias.type);
        }

        auto operator()(ast::definition::Typeclass const&)
        {
            cpputil::todo();
        }

        auto operator()(ast::definition::Implementation const&)
        {
            cpputil::todo();
        }

        auto operator()(ast::definition::Instantiation const&)
        {
            cpputil::todo();
        }

        auto operator()(ast::definition::Submodule const&)
        {
            cpputil::todo();
        }
    };
} // namespace libdesugar::dtl

LIBDESUGAR_DEFINE_FORMATTER(ast::Expression)
{
    std::visit(libdesugar::dtl::Expression_format_visitor { context.out() }, value.variant);
    return context.out();
}

LIBDESUGAR_DEFINE_FORMATTER(ast::Pattern)
{
    std::visit(libdesugar::dtl::Pattern_format_visitor { context.out() }, value.variant);
    return context.out();
}

LIBDESUGAR_DEFINE_FORMATTER(ast::Type)
{
    std::visit(libdesugar::dtl::Type_format_visitor { context.out() }, value.variant);
    return context.out();
}

LIBDESUGAR_DEFINE_FORMATTER(ast::Definition)
{
    std::visit(libdesugar::dtl::Definition_format_visitor { context.out() }, value.variant);
    return context.out();
}

LIBDESUGAR_DEFINE_FORMATTER(ast::Wildcard)
{
    (void)value;
    return std::format_to(context.out(), "_");
}

LIBDESUGAR_DEFINE_FORMATTER(ast::Mutability)
{
    std::visit(
        utl::Overload {
            [&](ast::mutability::Concrete const concrete) {
                std::format_to(context.out(), "{}", concrete);
            },
            [&](ast::mutability::Parameterized const& parameterized) {
                std::format_to(context.out(), "mut?{}", parameterized.name);
            },
        },
        value.variant);
    return context.out();
}

LIBDESUGAR_DEFINE_FORMATTER(ast::Qualified_name)
{
    if (value.root_qualifier.has_value()) {
        std::visit(
            utl::Overload {
                [&](ast::Global_root_qualifier const&) {
                    std::format_to(context.out(), "global::");
                },
                [&](utl::Wrapper<ast::Type> const type) {
                    std::format_to(context.out(), "{}::", type);
                },
            },
            value.root_qualifier.value());
    }
    for (ast::Qualifier const& qualifier : value.middle_qualifiers) {
        std::format_to(context.out(), "{}", qualifier.name);
        if (qualifier.template_arguments.has_value()) {
            std::format_to(context.out(), "[{}]", qualifier.template_arguments.value());
        }
        std::format_to(context.out(), "::");
    }
    return std::format_to(context.out(), "{}", value.primary_name);
}

LIBDESUGAR_DEFINE_FORMATTER(ast::Class_reference)
{
    std::format_to(context.out(), "{}", value.name);
    if (value.template_arguments.has_value()) {
        std::format_to(context.out(), "[{}]", value.template_arguments.value());
    }
    return context.out();
}

LIBDESUGAR_DEFINE_FORMATTER(ast::Function_argument)
{
    if (value.name.has_value()) {
        std::format_to(context.out(), "{} = ", value.name.value());
    }
    return std::format_to(context.out(), "{}", value.expression);
}

LIBDESUGAR_DEFINE_FORMATTER(ast::Function_parameter)
{
    std::format_to(context.out(), "{}", value.pattern);
    if (value.type.has_value()) {
        std::format_to(context.out(), ": {}", value.type.value());
    }
    if (value.default_argument.has_value()) {
        std::format_to(context.out(), " = {}", value.default_argument.value());
    }
    return context.out();
}

LIBDESUGAR_DEFINE_FORMATTER(ast::Template_argument)
{
    return std::format_to(context.out(), "{}", value);
}

LIBDESUGAR_DEFINE_FORMATTER(ast::Template_parameter)
{
    std::visit(
        utl::Overload {
            [&](ast::Template_type_parameter const& parameter) {
                std::format_to(context.out(), "{}", parameter.name);
                if (!parameter.classes.empty()) {
                    std::format_to(context.out(), ": {}", utl::fmt::join(parameter.classes, " + "));
                }
                if (parameter.default_argument.has_value()) {
                    std::format_to(context.out(), " = {}", parameter.default_argument.value());
                }
            },
            [&](ast::Template_value_parameter const& parameter) {
                std::format_to(context.out(), "{}", parameter.name);
                if (parameter.type.has_value()) {
                    std::format_to(context.out(), ": {}", parameter.type.value());
                }
                if (parameter.default_argument.has_value()) {
                    std::format_to(context.out(), " = {}", parameter.default_argument.value());
                }
            },
            [&](ast::Template_mutability_parameter const& parameter) {
                std::format_to(context.out(), "{}: mut", parameter.name);
                if (parameter.default_argument.has_value()) {
                    std::format_to(context.out(), " = {}", parameter.default_argument.value());
                }
            },
        },
        value.variant);
    return context.out();
}

LIBDESUGAR_DEFINE_FORMATTER(ast::pattern::Field)
{
    std::format_to(context.out(), "{}", value.name);
    if (value.pattern.has_value()) {
        std::format_to(context.out(), ": {}", value.pattern.value());
    }
    return context.out();
}

LIBDESUGAR_DEFINE_FORMATTER(ast::definition::Field)
{
    return std::format_to(context.out(), "{}: {}", value.name, value.type);
}

LIBDESUGAR_DEFINE_FORMATTER(ast::pattern::Constructor_body)
{
    return std::visit(
        utl::Overload {
            [&](ast::pattern::Struct_constructor const& constructor) {
                return std::format_to(context.out(), "{{ {} }}", constructor.fields);
            },
            [&](ast::pattern::Tuple_constructor const& constructor) {
                return std::format_to(context.out(), "({})", constructor.pattern);
            },
            [&](ast::pattern::Unit_constructor const&) { return context.out(); },
        },
        value);
}

LIBDESUGAR_DEFINE_FORMATTER(ast::definition::Constructor_body)
{
    return std::visit(
        utl::Overload {
            [&](ast::definition::Struct_constructor const& constructor) {
                return std::format_to(context.out(), " {{ {:n} }}", constructor.fields);
            },
            [&](ast::definition::Tuple_constructor const& constructor) {
                return std::format_to(context.out(), "({:n})", constructor.types);
            },
            [&](ast::definition::Unit_constructor const&) { return context.out(); },
        },
        value);
}

LIBDESUGAR_DEFINE_FORMATTER(ast::pattern::Constructor)
{
    return std::format_to(context.out(), "{}{}", value.name, value.body);
}

LIBDESUGAR_DEFINE_FORMATTER(ast::definition::Constructor)
{
    return std::format_to(context.out(), "{}{}", value.name, value.body);
}

LIBDESUGAR_DEFINE_FORMATTER(ast::Template_parameters)
{
    return value.has_value() ? std::format_to(context.out(), "[{:n}]", value.value())
                             : context.out();
}

#undef LIBDESUGAR_DECLARE_FORMATTER
#undef LIBDESUGAR_DEFINE_FORMATTER

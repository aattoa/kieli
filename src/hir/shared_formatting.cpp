#include "utl/utilities.hpp"
#include "ast/ast.hpp"
#include "hir.hpp"


// This translation unit defines formatters for the components that are shared
// between the AST and the HIR, and explicitly instantiates them for both cases.


template <ast::tree_configuration Configuration>
DEFINE_FORMATTER_FOR(ast::Basic_template_argument<Configuration>) {
    if (value.name) {
        std::format_to(context.out(), "{} = ", *value.name);
    }
    return std::visit(utl::Overload {
        [&](ast::Mutability const& mutability) {
            return utl::match(
                mutability.value,

                [&](ast::Mutability::Concrete const concrete) {
                    return std::format_to(context.out(), "{}", concrete.is_mutable ? "mut" : "immut");
                },
                [&](ast::Mutability::Parameterized const parameterized) {
                    return std::format_to(context.out(), "mut?{}", parameterized.identifier);
                }
            );
        },
        [&](ast::Basic_template_argument<Configuration>::Wildcard) {
            return std::format_to(context.out(), "_");
        },
        [&](auto const& argument) {
            return std::format_to(context.out(), "{}", argument);
        }
    }, value.value);
}

template struct std::formatter<ast::Template_argument>;
template struct std::formatter<hir::Template_argument>;



template <ast::tree_configuration Configuration>
DEFINE_FORMATTER_FOR(ast::Basic_qualified_name<Configuration>) {
    auto out = context.out();

    std::visit(utl::Overload {
        [   ](std::monostate)                                   {},
        [out](ast::Basic_root_qualifier<Configuration>::Global) { std::format_to(out, "::"        ); },
        [out](auto const& root)                                { std::format_to(out, "{}::", root); }
        }, value.root_qualifier.value);

    for (auto& qualifier : value.middle_qualifiers) {
        std::format_to(
            out,
            "{}{}::",
            qualifier.name,
            qualifier.template_arguments ? std::format("[{}]", qualifier.template_arguments) : ""
        );
    }

    return std::format_to(out, "{}", value.primary_name.identifier);
}

template struct std::formatter<ast::Qualified_name>;
template struct std::formatter<hir::Qualified_name>;



template <ast::tree_configuration Configuration>
DEFINE_FORMATTER_FOR(ast::Basic_class_reference<Configuration>) {
    return value.template_arguments
        ? std::format_to(context.out(), "{}[{}]", value.name, value.template_arguments)
        : std::format_to(context.out(), "{}", value.name);
};

template struct std::formatter<ast::Class_reference>;
template struct std::formatter<hir::Class_reference>;



template <ast::tree_configuration Configuration>
DEFINE_FORMATTER_FOR(ast::Basic_template_parameter<Configuration>) {
    auto out = std::format_to(context.out(), "{}", value.name);

    utl::match(
        value.value,

        [&](ast::Basic_template_parameter<Configuration>::Type_parameter const& parameter) {
            if (!parameter.classes.empty()) {
                out = std::format_to(out, ": {}", utl::fmt::delimited_range(parameter.classes, " + "));
            }
        },
        [&](ast::Basic_template_parameter<Configuration>::Value_parameter const& parameter) {
            if (parameter.type.has_value()) {
                out = std::format_to(out, ": {}", *parameter.type);
            }
        },
        [&](ast::Basic_template_parameter<Configuration>::Mutability_parameter const&) {
            out = std::format_to(out, ": mut");
        }
    );

    if (value.default_argument.has_value()) {
        out = std::format_to(out, " = {}", value.default_argument);
    }

    return out;
}

template struct std::formatter<ast::Template_parameter>;
template struct std::formatter<hir::Template_parameter>;


template <ast::tree_configuration Configuration>
DEFINE_FORMATTER_FOR(ast::definition::Basic_struct_member<Configuration>) {
    return std::format_to(
        context.out(),
        "{}{}: {}",
        value.is_public ? "pub " : "",
        value.name,
        value.type
    );
}

template struct std::formatter<ast::definition::Struct::Member>;
template struct std::formatter<hir::definition::Struct::Member>;


template <ast::tree_configuration Configuration>
DEFINE_FORMATTER_FOR(ast::definition::Basic_enum_constructor<Configuration>) {
    return std::format_to(context.out(), "{}({})", value.name, value.payload_type);
}

template struct std::formatter<ast::definition::Enum::Constructor>;
template struct std::formatter<hir::definition::Enum::Constructor>;


namespace {
    template <class>
    constexpr char const* header_name = "(should never be visible)";
    template <utl::instance_of<ast::definition::Basic_function> T>
    constexpr char const* header_name<T> = "fn";
    template <utl::instance_of<ast::definition::Basic_struct> T>
    constexpr char const* header_name<T> = "struct";
    template <utl::instance_of<ast::definition::Basic_enum> T>
    constexpr char const* header_name<T> = "enum";
    template <utl::instance_of<ast::definition::Basic_alias> T>
    constexpr char const* header_name<T> = "alias";
    template <utl::instance_of<ast::definition::Basic_typeclass> T>
    constexpr char const* header_name<T> = "class";
    template <utl::instance_of<ast::definition::Basic_implementation> T>
    constexpr char const* header_name<T> = "impl";
    template <utl::instance_of<ast::definition::Basic_instantiation> T>
    constexpr char const* header_name<T> = "inst";
    template <utl::instance_of<ast::definition::Basic_namespace> T>
    constexpr char const* header_name<T> = "namespace";
    template <class T>
    constexpr char const* header_name<ast::definition::Template<T>> = header_name<T>;

    struct Definition_body_formatting_visitor : utl::fmt::Visitor_base {
        auto format_self_parameter(std::optional<ast::Self_parameter> const& parameter, bool const is_only_parameter) {
            if (parameter.has_value()) {
                if (parameter->is_reference) {
                    format("&");
                }
                format("{}self", parameter->mutability);

                if (!is_only_parameter) {
                    format(", ");
                }
            }
        }

        auto operator()(ast::definition::Function const& function) {
            format("(");
            format_self_parameter(function.self_parameter, function.parameters.empty());
            format("{})", function.parameters);
            
            if (function.return_type) {
                format(": {}", *function.return_type);
            }
            return format(" = {}", function.body);
        }
        auto operator()(hir::definition::Function const& function) {
            if (!function.implicit_template_parameters.empty()) {
                format("[{}]", function.implicit_template_parameters);
            }
            format("(");
            format_self_parameter(function.self_parameter, function.parameters.empty());
            format("{})", function.parameters);

            if (function.return_type) {
                format(": {}", *function.return_type);
            }
            return format(" = {}", function.body);
        }
        template <utl::instance_of<ast::definition::Basic_struct> Structure>
        auto operator()(Structure const& structure) {
            return format(" = {}", structure.members);
        }
        template <utl::instance_of<ast::definition::Basic_enum> Enumeration>
        auto operator()(Enumeration const& enumeration) {
            return format(" = {}", enumeration.constructors);
        }
        template <utl::instance_of<ast::definition::Basic_alias> Alias>
        auto operator()(Alias const& alias) {
            return format(" = {}", alias.type);
        }
        template <utl::instance_of<ast::definition::Basic_typeclass> Typeclass>
        auto operator()(Typeclass const& typeclass) {
            format(" {{");

            // FIX

            for (auto& signature : typeclass.function_signatures) {
                format(
                    "fn {}({}): {}\n",
                    signature.name,
                    signature.parameter_types,
                    signature.return_type
                );
            }
            for (auto& signature : typeclass.function_template_signatures) {
                format(
                    "fn {}{}({}): {}\n",
                    signature.function_signature.name,
                    signature.template_parameters,
                    signature.function_signature.parameter_types,
                    signature.function_signature.return_type
                );
            }
            for (auto& signature : typeclass.type_signatures) {
                format(
                    "alias {}{}\n",
                    signature.name,
                    signature.classes.empty() ? "" : ": {}"_format(utl::fmt::delimited_range(signature.classes, " + "))
                );
            }
            for (auto& signature : typeclass.type_template_signatures) {
                format(
                    "alias {}{}",
                    signature.type_signature.name,
                    signature.template_parameters
                );

                if (!signature.type_signature.classes.empty()) {
                    format(": {}", utl::fmt::delimited_range(signature.type_signature.classes, " + "));
                }

                format("\n");
            }

            return format("}}");
        }
        template <utl::instance_of<ast::definition::Basic_namespace> Namespace>
        auto operator()(Namespace const& space) {
            return format("{{\n{}\n}}", utl::fmt::delimited_range(space.definitions, "\n\n"));
        }
        template <utl::instance_of<ast::definition::Basic_implementation> Implementation>
        auto operator()(Implementation const& implementation) {
            return format(
                "{} {{\n{}\n}}",
                implementation.type,
                utl::fmt::delimited_range(implementation.definitions, "\n\n")
            );
        }
        template <utl::instance_of<ast::definition::Basic_instantiation> Instantiation>
        auto operator()(Instantiation const& instantiation) {
            return format(
                "{} {} {{\n{}\n}}",
                instantiation.typeclass,
                instantiation.self_type,
                utl::fmt::delimited_range(instantiation.definitions, "\n\n")
            );
        }
        template <class Definition>
        auto operator()(ast::definition::Template<Definition> const& definition_template) {
            return operator()(definition_template.definition);
        }
    };
}


template <ast::tree_configuration Configuration>
DEFINE_FORMATTER_FOR(ast::Basic_definition<Configuration>) {
    return std::visit([&]<class Definition>(Definition const& definition) {
        auto out = std::format_to(context.out(), header_name<Definition>);

        if constexpr (requires { definition.name; }) {
            out = std::format_to(out, " {}", definition.name);
        }
        if constexpr (requires { definition.template_parameters; }) {
            out = std::format_to(out, "{}", definition.template_parameters);
        }

        return Definition_body_formatting_visitor { { out } }(definition);
    }, value.value);
}

template struct std::formatter<ast::Definition::Basic_definition>;
template struct std::formatter<hir::Definition::Basic_definition>;
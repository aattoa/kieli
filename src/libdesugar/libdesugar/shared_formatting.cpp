#include <libutl/common/utilities.hpp>
#include <libparse/ast.hpp>
#include <libdesugar/hir.hpp>


// This translation unit defines formatters for the components that are shared
// between the AST and the HIR, and explicitly instantiates them for both cases.


template <ast::tree_configuration Configuration>
DEFINE_FORMATTER_FOR(ast::Basic_template_argument<Configuration>) {
    if (value.name)
        fmt::format_to(context.out(), "{} = ", *value.name);
    return std::visit(utl::Overload {
        [&](ast::Mutability const& mutability) {
            return utl::match(mutability.value,
                [&](ast::Mutability::Concrete const concrete) {
                    return fmt::format_to(context.out(), "{}", concrete.is_mutable ? "mut" : "immut");
                },
                [&](ast::Mutability::Parameterized const parameterized) {
                    return fmt::format_to(context.out(), "mut?{}", parameterized.identifier);
                });
        },
        [&](typename ast::Basic_template_argument<Configuration>::Wildcard) {
            return fmt::format_to(context.out(), "_");
        },
        [&](auto const& argument) {
            return fmt::format_to(context.out(), "{}", argument);
        }
    }, value.value);
}

template struct fmt::formatter<ast::Template_argument>;
template struct fmt::formatter<hir::Template_argument>;



template <ast::tree_configuration Configuration>
DEFINE_FORMATTER_FOR(ast::Basic_qualified_name<Configuration>) {
    auto out = context.out();

    std::visit(utl::Overload {
        [   ](std::monostate)             {},
        [out](ast::Global_root_qualifier) { fmt::format_to(out, "global::"); },
        [out](auto const& root)           { fmt::format_to(out, "{}::", root); }
        }, value.root_qualifier.value);

    for (auto& qualifier : value.middle_qualifiers) {
        fmt::format_to(
            out,
            "{}{}::",
            qualifier.name,
            qualifier.template_arguments ? fmt::format("[{}]", qualifier.template_arguments) : "");
    }

    return fmt::format_to(out, "{}", value.primary_name.identifier);
}

template struct fmt::formatter<ast::Qualified_name>;
template struct fmt::formatter<hir::Qualified_name>;



template <ast::tree_configuration Configuration>
DEFINE_FORMATTER_FOR(ast::Basic_class_reference<Configuration>) {
    return value.template_arguments
        ? fmt::format_to(context.out(), "{}[{}]", value.name, value.template_arguments)
        : fmt::format_to(context.out(), "{}", value.name);
};

template struct fmt::formatter<ast::Class_reference>;
template struct fmt::formatter<hir::Class_reference>;



template <ast::tree_configuration Configuration>
DEFINE_FORMATTER_FOR(ast::Basic_template_parameter<Configuration>) {
    auto out = fmt::format_to(context.out(), "{}", value.name);

    utl::match(value.value,
        [&](typename ast::Basic_template_parameter<Configuration>::Type_parameter const& parameter) {
            if (!parameter.classes.empty())
                out = fmt::format_to(out, ": {}", utl::formatting::delimited_range(parameter.classes, " + "));
        },
        [&](typename ast::Basic_template_parameter<Configuration>::Value_parameter const& parameter) {
            if (parameter.type.has_value())
                out = fmt::format_to(out, ": {}", *parameter.type);
        },
        [&](typename ast::Basic_template_parameter<Configuration>::Mutability_parameter const&) {
            out = fmt::format_to(out, ": mut");
        });

    if (value.default_argument.has_value())
        out = fmt::format_to(out, " = {}", value.default_argument);

    return out;
}

template struct fmt::formatter<ast::Template_parameter>;
template struct fmt::formatter<hir::Template_parameter>;


template <ast::tree_configuration Configuration>
DEFINE_FORMATTER_FOR(ast::definition::Basic_struct_member<Configuration>) {
    return fmt::format_to(
        context.out(),
        "{}{}: {}",
        value.is_public.get() ? "pub " : "",
        value.name,
        value.type);
}

template struct fmt::formatter<ast::definition::Struct::Member>;
template struct fmt::formatter<hir::definition::Struct::Member>;


template <ast::tree_configuration Configuration>
DEFINE_FORMATTER_FOR(ast::definition::Basic_enum_constructor<Configuration>) {
    return fmt::format_to(context.out(), "{}({})", value.name, value.payload_type);
}

template struct fmt::formatter<ast::definition::Enum::Constructor>;
template struct fmt::formatter<hir::definition::Enum::Constructor>;


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

    struct Definition_body_formatting_visitor : utl::formatting::Visitor_base {
        auto format_self_parameter(tl::optional<ast::Self_parameter> const& parameter, bool const is_only_parameter) {
            if (parameter.has_value()) {
                if (parameter->is_reference.get())
                    format("&");
                format("{}self", parameter->mutability);
                if (!is_only_parameter)
                    format(", ");
            }
        }

        template <utl::instance_of<ast::Basic_function_signature> Signature>
        auto format_function_signature(Signature const& signature) {
            format("(");
            format_self_parameter(signature.self_parameter, signature.parameters.empty());
            format("{})", signature.parameters);
            if (signature.return_type.has_value())
                format(": {}", *signature.return_type);
        }

        template <utl::instance_of<ast::definition::Basic_function> Function>
        auto operator()(Function const& function) {
            format_function_signature(function.signature);
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
                format("fn {}", signature.name);
                format_function_signature(signature);
            }
            for (auto& signature : typeclass.function_template_signatures) {
                format("fn {}[{}]", signature.function_signature.name, signature.template_parameters);
                format_function_signature(signature.function_signature);
            }
            for (auto& signature : typeclass.type_signatures) {
                format(
                    "alias {}{}\n",
                    signature.name,
                    signature.classes.empty() ? "" : ": {}"_format(utl::formatting::delimited_range(signature.classes, " + ")));
            }
            for (auto& signature : typeclass.type_template_signatures) {
                format(
                    "alias {}{}",
                    signature.type_signature.name,
                    signature.template_parameters);

                if (!signature.type_signature.classes.empty())
                    format(": {}", utl::formatting::delimited_range(signature.type_signature.classes, " + "));

                format("\n");
            }

            return format("}}");
        }
        template <utl::instance_of<ast::definition::Basic_namespace> Namespace>
        auto operator()(Namespace const& space) {
            return format("{{\n{}\n}}", utl::formatting::delimited_range(space.definitions, "\n\n"));
        }
        template <utl::instance_of<ast::definition::Basic_implementation> Implementation>
        auto operator()(Implementation const& implementation) {
            return format(
                "{} {{\n{}\n}}",
                implementation.type,
                utl::formatting::delimited_range(implementation.definitions, "\n\n"));
        }
        template <utl::instance_of<ast::definition::Basic_instantiation> Instantiation>
        auto operator()(Instantiation const& instantiation) {
            return format(
                "{} {} {{\n{}\n}}",
                instantiation.typeclass,
                instantiation.self_type,
                utl::formatting::delimited_range(instantiation.definitions, "\n\n"));
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
        auto out = fmt::format_to(context.out(), header_name<Definition>);

        if constexpr (requires { definition.name; })
            out = fmt::format_to(out, " {}", definition.name);
        if constexpr (requires { definition.template_parameters; })
            out = fmt::format_to(out, "{}", definition.template_parameters);

        return Definition_body_formatting_visitor { { out } }(definition);
    }, value.value);
}

template struct fmt::formatter<ast::Definition::Basic_definition>;
template struct fmt::formatter<hir::Definition::Basic_definition>;

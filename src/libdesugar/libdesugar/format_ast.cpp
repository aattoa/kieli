#include <libutl/common/utilities.hpp>
#include <libdesugar/ast.hpp>

#define DECLARE_FORMATTER(...)                                                       \
template <>                                                                          \
struct std::formatter<__VA_ARGS__> : utl::formatting::Formatter_base {               \
    auto format(__VA_ARGS__ const&, auto& context) const -> decltype(context.out()); \
}

#define DEFINE_FORMATTER(...) \
auto ast::format_to(__VA_ARGS__ const& value, std::string& string) -> void { std::format_to(std::back_inserter(string), "{}", value); } \
auto std::formatter<__VA_ARGS__>::format(__VA_ARGS__ const& value, auto& context) const -> decltype(context.out())

DECLARE_FORMATTER(ast::Expression);
DECLARE_FORMATTER(ast::Pattern);
DECLARE_FORMATTER(ast::Type);
DECLARE_FORMATTER(ast::Definition);
DECLARE_FORMATTER(ast::Mutability);
DECLARE_FORMATTER(ast::Qualified_name);
DECLARE_FORMATTER(ast::Class_reference);
DECLARE_FORMATTER(ast::Function_argument);
DECLARE_FORMATTER(ast::Function_parameter);
DECLARE_FORMATTER(ast::Template_argument);
DECLARE_FORMATTER(ast::Template_parameter);


namespace {
    template <class Out>
    struct Expression_format_visitor {
        Out out;
        explicit Expression_format_visitor(Out&& out) noexcept : out { std::move(out) } {}
        template <class T>
        auto operator()(ast::expression::Literal<T> const& literal) {
            std::format_to(out, "{}", literal.value);
        }
        auto operator()(ast::expression::Literal<utl::Pooled_string> const& literal) {
            std::format_to(out, "\"{}\"", literal.value);
        }
        auto operator()(ast::expression::Literal<compiler::built_in_type::Character> const& literal) {
            std::format_to(out, "'{}'", literal.value);
        }
        auto operator()(ast::expression::Self const&) {
            std::format_to(out, "self");
        }
        auto operator()(ast::expression::Block const& block) {
            std::format_to(out, "{{");
            for (ast::Expression const& side_effect : block.side_effect_expressions)
                std::format_to(out, " {};", side_effect);
            std::format_to(out, " {} }}", block.result_expression);
        }
        auto operator()(ast::expression::Tuple const& tuple) {
            std::format_to(out, "({})", tuple.fields);
        }
        auto operator()(ast::expression::Template_application const& application) {
            std::format_to(out, "{}[{}]", application.name, application.template_arguments);
        }
        auto operator()(ast::expression::Reference const& reference) {
            std::format_to(out, "(&{} {})", reference.mutability, reference.referenced_expression);
        }
        auto operator()(ast::expression::Type_cast const& cast) {
            std::format_to(out, "({} as {})", cast.expression, cast.target_type);
        }
        auto operator()(ast::expression::Type_ascription const& ascription) {
            std::format_to(out, "({}: {})", ascription.expression, ascription.ascribed_type);
        }
        auto operator()(ast::expression::Conditional const& conditional) {
            std::format_to(out, "if {} {} else {}", conditional.condition, conditional.true_branch, conditional.false_branch);
        }
        auto operator()(ast::expression::Meta const& meta) {
            std::format_to(out, "meta({})", meta.expression);
        }
        auto operator()(ast::expression::Struct_initializer const& struct_initializer) {
            std::format_to(out, "{} {{", struct_initializer.struct_type);
            for (auto const& [name, initializer] : struct_initializer.member_initializers)
                std::format_to(out, " {} = {}", name, initializer);
            std::format_to(out, " }}");
        }
        auto operator()(ast::expression::Pointer_dereference const& dereference) {
            std::format_to(out, "dereference({})", dereference.pointer_expression);
        }
        auto operator()(ast::expression::Reference_dereference const& dereference) {
            std::format_to(out, "(*{})", dereference.dereferenced_expression);
        }
        auto operator()(ast::expression::Addressof const& addressof) {
            std::format_to(out, "addressof({})", addressof.lvalue_expression);
        }
        auto operator()(ast::expression::Struct_field_access const& access) {
            std::format_to(out, "{}.{}", access.base_expression, access.field_name);
        }
        auto operator()(ast::expression::Tuple_field_access const& access) {
            std::format_to(out, "{}.{}", access.base_expression, access.field_index);
        }
        auto operator()(ast::expression::Array_index_access const& access) {
            std::format_to(out, "{}.[{}]", access.base_expression, access.index_expression);
        }
        auto operator()(ast::expression::Array_literal const& literal) {
            std::format_to(out, "[{}]", literal.elements);
        }
        auto operator()(ast::expression::Binary_operator_invocation const& invocation) {
            std::format_to(out, "({} {} {})", invocation.left, invocation.op, invocation.right);
        }
        auto operator()(ast::expression::Break const& break_) {
            std::format_to(out, "break {}", break_.result);
        }
        auto operator()(ast::expression::Continue const&) {
            std::format_to(out, "continue");
        }
        auto operator()(ast::expression::Hole const&) {
            std::format_to(out, "\?\?\?");
        }
        auto operator()(ast::expression::Invocation const& invocation) {
            std::format_to(out, "{}({})", invocation.invocable, invocation.arguments);
        }
        auto operator()(ast::expression::Let_binding const& binding) {
            std::format_to(out, "let {}", binding.pattern);
            if (binding.type.has_value())
                std::format_to(out, ": {}", *binding.type);
            std::format_to(out, " = {}", binding.initializer);
        }
        auto operator()(ast::expression::Local_type_alias const& alias) {
            std::format_to(out, "alias {} = {}", alias.alias_name, alias.aliased_type);
        }
        auto operator()(ast::expression::Loop const& loop) {
            if (std::holds_alternative<ast::expression::Block>(loop.body->value))
                std::format_to(out, "loop {}", loop.body);
            else
                std::format_to(out, "loop {{ {} }}", loop.body);
        }
        auto operator()(ast::expression::Match const& match) {
            std::format_to(out, "match {} {{", match.matched_expression);
            for (auto const& match_case : match.cases)
                std::format_to(out, " {} -> {}", match_case.pattern, match_case.handler);
            std::format_to(out, " }}");
        }
        auto operator()(ast::expression::Method_invocation const& invocation) {
            std::format_to(out, "{}.{}", invocation.base_expression, invocation.method_name);
            if (invocation.template_arguments.has_value())
                std::format_to(out, "[{}]", *invocation.template_arguments);
            std::format_to(out, "({})", invocation.function_arguments);
        }
        auto operator()(ast::expression::Move const& move) {
            std::format_to(out, "mov {}", move.lvalue);
        }
        auto operator()(ast::expression::Ret const& ret) {
            if (ret.returned_expression.has_value())
                std::format_to(out, "ret {}", *ret.returned_expression);
            std::format_to(out, "ret");
        }
        auto operator()(ast::expression::Sizeof const& sizeof_) {
            std::format_to(out, "sizeof({})", sizeof_.inspected_type);
        }
        auto operator()(ast::expression::Unsafe const& unsafe) {
            std::format_to(out, "unsafe {}", unsafe.expression);
        }
        auto operator()(ast::expression::Variable const& variable) {
            std::format_to(out, "{}", variable.name);
        }
    };

    template <class Out>
    struct Pattern_format_visitor {
        Out out;
        explicit Pattern_format_visitor(Out&& out) noexcept : out { std::move(out) } {}
        auto operator()(ast::pattern::Slice const& slice) {
            std::format_to(out, "[{}]", slice.element_patterns);
        }
        auto operator()(ast::pattern::Tuple const& tuple) {
            std::format_to(out, "({})", tuple.field_patterns);
        }
        auto operator()(ast::pattern::Wildcard const&) {
            std::format_to(out, "_");
        }
        auto operator()(ast::pattern::Alias const& alias) {
            std::format_to(out, "{} as {} {}", alias.aliased_pattern, alias.alias_mutability, alias.alias_name);
        }
        auto operator()(ast::pattern::Constructor const& constructor) {
            std::format_to(out, "{}", constructor.constructor_name);
            if (constructor.payload_pattern.has_value())
                std::format_to(out, "({})", *constructor.payload_pattern);
        }
        auto operator()(ast::pattern::Abbreviated_constructor const& constructor) {
            std::format_to(out, "::{}", constructor.constructor_name);
            if (constructor.payload_pattern.has_value())
                std::format_to(out, "({})", *constructor.payload_pattern);
        }
        auto operator()(ast::pattern::Name const& name) {
            std::format_to(out, "{} {}", name.mutability, name.name);
        }
        auto operator()(ast::pattern::Guarded const& guarded) {
            std::format_to(out, "{} if {}", guarded.guarded_pattern, guarded.guard);
        }
        template <class T>
        auto operator()(ast::pattern::Literal<T> const& literal) {
            std::format_to(out, "{}", literal.value);
        }
        auto operator()(ast::pattern::Literal<utl::Pooled_string> const& literal) {
            std::format_to(out, "\"{}\"", literal.value);
        }
        auto operator()(ast::pattern::Literal<compiler::built_in_type::Character> const& literal) {
            std::format_to(out, "'{}'", literal.value);
        }
    };

    template <class Out>
    struct Type_format_visitor {
        Out out;
        explicit Type_format_visitor(Out&& out) noexcept : out { std::move(out) } {}
        auto operator()(compiler::built_in_type::Integer const integer) {
            std::format_to(out, "{}", compiler::built_in_type::integer_string(integer));
        }
        auto operator()(compiler::built_in_type::Floating const&) {
            std::format_to(out, "Float");
        }
        auto operator()(compiler::built_in_type::Character const&) {
            std::format_to(out, "Char");
        }
        auto operator()(compiler::built_in_type::Boolean const&) {
            std::format_to(out, "Bool");
        }
        auto operator()(compiler::built_in_type::String const&) {
            std::format_to(out, "String");
        }
        auto operator()(ast::type::Wildcard const&) {
            std::format_to(out, "_");
        }
        auto operator()(ast::type::Function const& function) {
            std::format_to(out, "fn({}): {}", function.argument_types, function.return_type);
        }
        auto operator()(ast::type::Self const&) {
            std::format_to(out, "Self");
        }
        auto operator()(ast::type::Tuple const& tuple) {
            std::format_to(out, "({})", tuple.field_types);
        }
        auto operator()(ast::type::Array const& array) {
            std::format_to(out, "[{}; {}]", array.element_type, array.array_length);
        }
        auto operator()(ast::type::Instance_of const& instance_of) {
            std::format_to(out, "inst {}", utl::formatting::delimited_range(instance_of.classes, " + "));
        }
        auto operator()(ast::type::Pointer const& pointer) {
            std::format_to(out, "*{} {}", pointer.mutability, pointer.pointed_to_type);
        }
        auto operator()(ast::type::Reference const& reference) {
            std::format_to(out, "&{} {}", reference.mutability, reference.referenced_type);
        }
        auto operator()(ast::type::Slice const& slice) {
            std::format_to(out, "[{}]", slice.element_type);
        }
        auto operator()(ast::type::Template_application const& application) {
            std::format_to(out, "{}[{}]", application.name, application.arguments);
        }
        auto operator()(ast::type::Typename const& name) {
            std::format_to(out, "{}", name.name);
        }
        auto operator()(ast::type::Typeof const& typeof_) {
            std::format_to(out, "typeof({})", typeof_.inspected_expression);
        }
    };

    template <class Out>
    struct Definition_format_visitor {
        Out out;
        explicit Definition_format_visitor(Out&& out) noexcept : out { std::move(out) } {}
        auto operator()(ast::definition::Function const& function) {
            std::format_to(out, "fn {}", function.signature.name);
            if (!function.signature.template_parameters.empty())
                std::format_to(out, "[{}]", function.signature.template_parameters);
            std::format_to(out, "({})", function.signature.function_parameters);
            if (function.signature.return_type.has_value())
                std::format_to(out, ": {}", *function.signature.return_type);
            assert(std::holds_alternative<ast::expression::Block>(function.body.value));
            std::format_to(out, " {}", function.body);
        }
        auto operator()(ast::definition::Struct const&) {
            utl::todo();
        }
        auto operator()(ast::definition::Enum const&) {
            utl::todo();
        }
        auto operator()(ast::definition::Alias const&) {
            utl::todo();
        }
        auto operator()(ast::definition::Typeclass const&) {
            utl::todo();
        }
        auto operator()(ast::definition::Implementation const&) {
            utl::todo();
        }
        auto operator()(ast::definition::Instantiation const&) {
            utl::todo();
        }
        auto operator()(ast::definition::Namespace const&) {
            utl::todo();
        }
        template <class T>
        auto operator()(ast::definition::Template<T> const&) {
            utl::todo();
        }
    };
}


DEFINE_FORMATTER(ast::Expression) {
    utl::match(value.value, Expression_format_visitor { context.out() });
    return context.out();
}
DEFINE_FORMATTER(ast::Pattern) {
    utl::match(value.value, Pattern_format_visitor { context.out() });
    return context.out();
}
DEFINE_FORMATTER(ast::Type) {
    utl::match(value.value, Type_format_visitor { context.out() });
    return context.out();
}
DEFINE_FORMATTER(ast::Definition) {
    utl::match(value.value, Definition_format_visitor { context.out() });
    return context.out();
}

DEFINE_FORMATTER(ast::Mutability) {
    utl::match(value.value,
        [&](ast::Mutability::Concrete const concrete) {
            std::format_to(context.out(), "{}", concrete.is_mutable ? "mut" : "immut");
        },
        [&](ast::Mutability::Parameterized const parameterized) {
            std::format_to(context.out(), "mut?{}", parameterized.name);
        });
    return context.out();
}

DEFINE_FORMATTER(ast::Qualified_name) {
    if (value.root_qualifier.has_value()) {
        utl::match(value.root_qualifier->value,
            [&](ast::Root_qualifier::Global) {
                std::format_to(context.out(), "global::");
            },
            [&](utl::Wrapper<ast::Type> const type) {
                std::format_to(context.out(), "{}::", type);
            });
    }
    for (ast::Qualifier const& qualifier : value.middle_qualifiers) {
        std::format_to(context.out(), "{}", qualifier.name);
        if (qualifier.template_arguments.has_value())
            std::format_to(context.out(), "[{}]", *qualifier.template_arguments);
        std::format_to(context.out(), "::");
    }
    return std::format_to(context.out(), "{}", value.primary_name);
}

DEFINE_FORMATTER(ast::Class_reference) {
    std::format_to(context.out(), "{}", value.name);
    if (value.template_arguments.has_value())
        std::format_to(context.out(), "[{}]", *value.template_arguments);
    return context.out();
}

DEFINE_FORMATTER(ast::Function_argument) {
    if (value.argument_name.has_value())
        std::format_to(context.out(), "{} = ", *value.argument_name);
    return std::format_to(context.out(), "{}", value.expression);
}

DEFINE_FORMATTER(ast::Function_parameter) {
    std::format_to(context.out(), "{}", value.pattern);
    if (value.type.has_value())
        std::format_to(context.out(), ": {}", *value.type);
    if (value.default_argument.has_value())
        std::format_to(context.out(), " = {}", *value.default_argument);
    return context.out();
}

DEFINE_FORMATTER(ast::Template_argument) {
    utl::match(value.value,
        [&](ast::Template_argument::Wildcard const&) {
            std::format_to(context.out(), "_");
        },
        [&](auto const& argument) {
            std::format_to(context.out(), "{}", argument);
        });
    return context.out();
}

DEFINE_FORMATTER(ast::Template_parameter) {
    utl::match(value.value,
        [&](ast::Template_parameter::Type_parameter const& type_parameter) {
            if (type_parameter.classes.empty()) return;
            std::format_to(context.out(), ": {}", utl::formatting::delimited_range(type_parameter.classes, " + "));
        },
        [&](ast::Template_parameter::Value_parameter const& value_parameter) {
            std::format_to(context.out(), "{}", value_parameter.name);
            if (value_parameter.type.has_value())
                std::format_to(context.out(), ": {}", *value_parameter.type);
        },
        [&](ast::Template_parameter::Mutability_parameter const& mutability_parameter) {
            std::format_to(context.out(), "{}: mut", mutability_parameter.name);
        });
    if (value.default_argument.has_value()) {
        std::format_to(context.out(), " = {}", *value.default_argument);
    }
    return context.out();
}
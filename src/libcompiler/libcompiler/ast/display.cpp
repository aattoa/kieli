#include <libutl/utilities.hpp>
#include <libcompiler/ast/display.hpp>

using namespace kieli::ast;

namespace {
    struct Display_state {
        std::string  output;
        std::string  indent;
        bool const   unicode = true;
        Arena const& arena;
    };

    enum class Last { no, yes };

    template <class Node>
    concept displayable = requires(Display_state& state, Node const& node) {
        { do_display(state, node) } -> std::same_as<void>;
    };

    template <class... Args>
    auto write_line(Display_state& state, std::format_string<Args...> const fmt, Args&&... args)
    {
        std::format_to(std::back_inserter(state.output), fmt, std::forward<Args>(args)...);
        state.output.push_back('\n');
    }

    auto write_node(Display_state& state, Last const last, std::invocable auto const& callback)
        -> void
    {
        std::size_t const previous_indent = state.indent.size();
        state.output.append(state.indent);
        if (last == Last::yes) {
            state.output.append(state.unicode ? "└─ " : "+- ");
            state.indent.append("   ");
        }
        else {
            state.output.append(state.unicode ? "├─ " : "|- ");
            state.indent.append(state.unicode ? "│  " : "|  ");
        }
        std::invoke(callback);
        state.indent.resize(previous_indent);
    }

    auto display_node(
        Display_state&          state,
        Last const              last,
        std::string_view const  description,
        displayable auto const& node) -> void
    {
        write_node(state, last, [&] {
            write_line(state, "{}", description);
            write_node(state, Last::yes, [&] { do_display(state, node); });
        });
    }

    template <displayable T>
    auto display_vector_node(
        Display_state&         state,
        Last const             last,
        std::string_view const description,
        std::vector<T> const&  vector) -> void
    {
        write_node(state, last, [&] {
            write_line(state, "{}", description);
            for (std::size_t i = 0; i != vector.size(); ++i) {
                write_node(state, i == vector.size() - 1 ? Last::yes : Last::no, [&] {
                    do_display(state, vector[i]);
                });
            }
        });
    }

    auto do_display(Display_state& state, Definition const& definition) -> void;
    auto do_display(Display_state& state, Expression const& expression) -> void;
    auto do_display(Display_state& state, Pattern const& pattern) -> void;
    auto do_display(Display_state& state, Type const& type) -> void;

    auto do_display(Display_state& state, Expression_id const id) -> void
    {
        do_display(state, state.arena.expressions[id]);
    }

    auto do_display(Display_state& state, Pattern_id const id) -> void
    {
        do_display(state, state.arena.patterns[id]);
    }

    auto do_display(Display_state& state, Type_id const id) -> void
    {
        do_display(state, state.arena.types[id]);
    }

    auto do_display(Display_state& state, kieli::Name const& name) -> void
    {
        write_line(state, "\"{}\"", name);
    }

    auto do_display(Display_state& state, Template_parameter const& parameter) -> void
    {
        auto const visitor = utl::Overload {
            [&](Template_type_parameter const& parameter) {
                write_line(state, "type parameter");
                display_node(state, Last::yes, "name", parameter.name);
            },
            [&](Template_value_parameter const& parameter) {
                write_line(state, "value parameter");
                display_node(state, Last::yes, "name", parameter.name);
            },
            [&](Template_mutability_parameter const& parameter) {
                write_line(state, "mutability parameter");
                display_node(state, Last::yes, "name", parameter.name);
            },
        };
        std::visit(visitor, parameter.variant); // TODO
    }

    auto do_display(Display_state& state, Path const& path) -> void
    {
        write_line(state, "path");
        if (path.root.has_value()) {
            cpputil::todo();
        }
        for (Path_segment const& _ : path.segments) {
            cpputil::todo();
        }
        display_node(state, Last::yes, "head", path.head);
    }

    auto display_template_parameters_node(
        Display_state& state, Last const last, Template_parameters const& parameters) -> void
    {
        if (parameters.has_value()) {
            display_vector_node(state, last, "template parameters", parameters.value());
        }
    }

    auto do_display(Display_state& state, definition::Field const& field) -> void
    {
        write_line(state, "field");
        display_node(state, Last::no, "name", field.name);
        display_node(state, Last::yes, "type", field.type);
    }

    auto do_display(Display_state& state, definition::Constructor_body const& body) -> void
    {
        auto const visitor = utl::Overload {
            [&](definition::Struct_constructor const& constructor) {
                write_line(state, "struct constructor");
                display_vector_node(state, Last::yes, "fields", constructor.fields);
            },
            [&](definition::Tuple_constructor const& constructor) {
                write_line(state, "tuple constructor");
                display_vector_node(state, Last::yes, "types", constructor.types);
            },
            [&](definition::Unit_constructor const&) { write_line(state, "unit constructor"); },
        };
        std::visit(visitor, body);
    }

    auto do_display(Display_state& state, definition::Constructor const& constructor) -> void
    {
        write_line(state, "constructor");
        display_node(state, Last::no, "name", constructor.name);
        display_node(state, Last::yes, "body", constructor.body);
    }

    auto do_display(Display_state& state, Concept_reference const& reference) -> void
    {
        write_line(state, "concept reference");
        if (reference.template_arguments.has_value()) {
            cpputil::todo();
        }
        display_node(state, Last::yes, "reference path", reference.path);
    }

    auto do_display(Display_state& state, Function_parameter const& parameter) -> void
    {
        write_line(state, "function parameter");
        if (parameter.type.has_value()) {
            display_node(state, Last::no, "type", parameter.type.value());
        }
        if (parameter.default_argument.has_value()) {
            display_node(state, Last::no, "default argument", parameter.default_argument.value());
        }
        display_node(state, Last::yes, "pattern", parameter.pattern);
    }

    auto do_display(Display_state& state, Function_signature const& signature) -> void
    {
        write_line(state, "function signature");
        display_node(state, Last::no, "name", signature.name);
        display_template_parameters_node(state, Last::no, signature.template_parameters);
        display_node(state, Last::no, "return type", signature.return_type);
        display_vector_node(state, Last::yes, "function parameters", signature.function_parameters);
    }

    auto do_display(Display_state& state, Type_signature const& signature) -> void
    {
        write_line(state, "type signature");
        display_node(state, Last::no, "name", signature.name);
        display_vector_node(state, Last::yes, "concepts", signature.concepts);
    }

    struct Definition_display_visitor {
        Display_state& state;

        auto operator()(definition::Function const& function)
        {
            write_line(state, "function");
            display_node(state, Last::no, "signature", function.signature);
            display_node(state, Last::yes, "body", function.body);
        }

        auto operator()(definition::Enumeration const& enumeration)
        {
            write_line(state, "enumeration");
            display_node(state, Last::no, "name", enumeration.name);
            display_template_parameters_node(state, Last::no, enumeration.template_parameters);
            display_vector_node(state, Last::yes, "constructors", enumeration.constructors);
        }

        auto operator()(definition::Alias const& alias)
        {
            write_line(state, "type alias");
            display_node(state, Last::no, "name", alias.name);
            display_template_parameters_node(state, Last::no, alias.template_parameters);
            display_node(state, Last::yes, "aliased type", alias.type);
        }

        auto operator()(definition::Concept const& concept_)
        {
            write_line(state, "concept");
            display_node(state, Last::no, "name", concept_.name);
            display_template_parameters_node(state, Last::no, concept_.template_parameters);
            display_vector_node(state, Last::no, "functions", concept_.function_signatures);
            display_vector_node(state, Last::yes, "types", concept_.type_signatures);
        }

        auto operator()(definition::Implementation const& implementation)
        {
            write_line(state, "implementation");
            display_template_parameters_node(state, Last::no, implementation.template_parameters);
            display_node(state, Last::no, "type", implementation.type);
            display_vector_node(state, Last::yes, "definitions", implementation.definitions);
        }

        auto operator()(definition::Submodule const& submodule)
        {
            write_line(state, "submodule");
            display_node(state, Last::no, "name", submodule.name);
            display_template_parameters_node(state, Last::no, submodule.template_parameters);
            display_vector_node(state, Last::yes, "definitions", submodule.definitions);
        }
    };

    struct Expression_display_visitor {
        Display_state& state;

        auto operator()(auto const&)
        {
            write_line(state, "expression todo");
        }
    };

    struct Pattern_display_visitor {
        Display_state& state;

        auto operator()(auto const&)
        {
            write_line(state, "pattern todo");
        }
    };

    struct Type_display_visitor {
        Display_state& state;

        auto operator()(kieli::type::Integer const& integer)
        {
            write_line(state, "{}", kieli::type::integer_name(integer));
        }

        auto operator()(kieli::type::Floating const&)
        {
            write_line(state, "Float");
        }

        auto operator()(kieli::type::Character const&)
        {
            write_line(state, "Char");
        }

        auto operator()(kieli::type::Boolean const&)
        {
            write_line(state, "Bool");
        }

        auto operator()(kieli::type::String const&)
        {
            write_line(state, "String");
        }

        auto operator()(Wildcard const&)
        {
            write_line(state, "Wildcard");
        }

        auto operator()(type::Self const&)
        {
            write_line(state, "Self");
        }

        auto operator()(auto const&)
        {
            cpputil::todo();
        }
    };

    auto do_display(Display_state& state, Definition const& definition) -> void
    {
        std::visit(Definition_display_visitor { state }, definition.variant);
    }

    auto do_display(Display_state& state, Expression const& expression) -> void
    {
        std::visit(Expression_display_visitor { state }, expression.variant);
    }

    auto do_display(Display_state& state, Pattern const& pattern) -> void
    {
        std::visit(Pattern_display_visitor { state }, pattern.variant);
    }

    auto do_display(Display_state& state, Type const& type) -> void
    {
        std::visit(Type_display_visitor { state }, type.variant);
    }
} // namespace

auto kieli::ast::display(Arena const& arena, Definition const& definition) -> std::string
{
    Display_state state { .arena = arena };
    do_display(state, definition);
    return std::move(state.output);
}

auto kieli::ast::display(Arena const& arena, Expression const& expression) -> std::string
{
    Display_state state { .arena = arena };
    do_display(state, expression);
    return std::move(state.output);
}

auto kieli::ast::display(Arena const& arena, Pattern const& pattern) -> std::string
{
    Display_state state { .arena = arena };
    do_display(state, pattern);
    return std::move(state.output);
}

auto kieli::ast::display(Arena const& arena, Type const& type) -> std::string
{
    Display_state state { .arena = arena };
    do_display(state, type);
    return std::move(state.output);
}

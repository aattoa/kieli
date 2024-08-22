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

    struct Node {
        Display_state& state;
        std::size_t    previous_indent {};

        explicit Node(Display_state& state, bool const last)
            : state(state)
            , previous_indent(state.indent.size())
        {
            state.output.append(state.indent);
            if (last) {
                state.output.append(state.unicode ? "└─ " : "+- ");
                state.indent.append("   ");
            }
            else {
                state.output.append(state.unicode ? "├─ " : "|- ");
                state.indent.append(state.unicode ? "│  " : "|  ");
            }
        }

        ~Node()
        {
            state.indent.resize(previous_indent);
        }
    };

    auto node(Display_state& state) -> Node
    {
        return Node(state, false);
    }

    auto last_node(Display_state& state) -> Node
    {
        return Node(state, true);
    }

    template <class... Args>
    auto write_line(Display_state& state, std::format_string<Args...> const fmt, Args&&... args)
    {
        std::format_to(std::back_inserter(state.output), fmt, std::forward<Args>(args)...);
        state.output.push_back('\n');
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

    template <class T>
    auto do_display(Display_state& state, std::vector<T> const& vector) -> void
    {
        for (std::size_t i = 0; i != vector.size(); ++i) {
            auto const _ = (i != vector.size() - 1) ? node(state) : last_node(state);
            do_display(state, vector[i]);
        }
    }

    auto do_display(Display_state& state, Template_parameter const& parameter) -> void
    {
        auto const visitor = utl::Overload {
            [&](Template_type_parameter const& parameter) {
                write_line(state, "type parameter {}", parameter.name);
            },
            [&](Template_value_parameter const& parameter) {
                write_line(state, "value parameter {}", parameter.name);
            },
            [&](Template_mutability_parameter const& parameter) {
                write_line(state, "mutability parameter {}", parameter.name);
            },
        };
        std::visit(visitor, parameter.variant); // TODO
    }

    auto do_display(Display_state& state, Path const& path) -> void
    {
        if (path.root.has_value()) {
            cpputil::todo();
        }
        for (Path_segment const& segment : path.segments) {
            (void)segment;
            cpputil::todo();
        }
        auto const _ = last_node(state);
        write_line(state, "head");
        {
            auto const _ = last_node(state);
            write_line(state, "{}", path.head);
        }
    }

    auto display_name_node(Display_state& state, kieli::Name const& name) -> void
    {
        auto const _ = node(state);
        write_line(state, "name");
        {
            auto const _ = last_node(state);
            write_line(state, "{}", name);
        }
    }

    auto display_template_parameters_node(
        Display_state& state, Template_parameters const& parameters) -> void
    {
        if (!parameters.has_value()) {
            return;
        }
        auto const _ = node(state);
        write_line(state, "template parameters");
        do_display(state, parameters.value());
    }

    auto do_display(Display_state& state, definition::Field const& field) -> void
    {
        write_line(state, "field");
        display_name_node(state, field.name);
        {
            auto const _ = last_node(state);
            write_line(state, "type");
            {
                auto const _ = last_node(state);
                do_display(state, field.type);
            }
        }
    }

    auto do_display(Display_state& state, definition::Constructor_body const& body) -> void
    {
        auto const visitor = utl::Overload {
            [&](definition::Struct_constructor const& constructor) {
                write_line(state, "struct constructor");
                auto const _ = last_node(state);
                write_line(state, "fields");
                do_display(state, constructor.fields);
            },
            [&](definition::Tuple_constructor const& constructor) {
                write_line(state, "tuple constructor");
                auto const _ = last_node(state);
                write_line(state, "types");
                do_display(state, constructor.types);
            },
            [&](definition::Unit_constructor const&) { write_line(state, "unit constructor"); },
        };
        std::visit(visitor, body);
    }

    auto do_display(Display_state& state, definition::Constructor const& constructor) -> void
    {
        write_line(state, "constructor");
        display_name_node(state, constructor.name);
        {
            auto const _ = last_node(state);
            write_line(state, "body");
            {
                auto const _ = last_node(state);
                do_display(state, constructor.body);
            }
        }
    }

    auto do_display(Display_state& state, Concept_reference const& reference) -> void
    {
        write_line(state, "concept reference");
        if (reference.template_arguments.has_value()) {
            cpputil::todo();
        }
        auto const _ = last_node(state);
        write_line(state, "path");
        do_display(state, reference.path);
    }

    auto do_display(Display_state& state, Function_parameter const& parameter) -> void
    {
        write_line(state, "function parameter");
        if (parameter.type.has_value()) {
            auto const _ = node(state);
            write_line(state, "type");
            {
                auto const _ = last_node(state);
                do_display(state, parameter.type.value());
            }
        }
        if (parameter.default_argument.has_value()) {
            auto const _ = node(state);
            write_line(state, "default argument");
            {
                auto const _ = last_node(state);
                do_display(state, parameter.default_argument.value());
            }
        }
        auto const _ = last_node(state);
        write_line(state, "pattern");
        {
            auto const _ = last_node(state);
            do_display(state, parameter.pattern);
        }
    }

    auto do_display(Display_state& state, Function_signature const& signature) -> void
    {
        write_line(state, "function signature");
        display_name_node(state, signature.name);
        display_template_parameters_node(state, signature.template_parameters);
        {
            auto const _ = node(state);
            write_line(state, "return type");
            {
                auto const _ = last_node(state);
                do_display(state, signature.return_type);
            }
        }
        {
            auto const _ = last_node(state);
            write_line(state, "function parameters");
            do_display(state, signature.function_parameters);
        }
    }

    auto do_display(Display_state& state, Type_signature const& signature) -> void
    {
        write_line(state, "type signature");
        display_name_node(state, signature.name);
        auto const _ = last_node(state);
        write_line(state, "concepts");
        do_display(state, signature.concepts);
    }

    struct Definition_display_visitor {
        Display_state& state;

        auto operator()(definition::Function const& function)
        {
            write_line(state, "function");
            {
                auto const _ = node(state);
                do_display(state, function.signature);
            }
            {
                auto const _ = last_node(state);
                write_line(state, "body");
                {
                    auto const _ = last_node(state);
                    do_display(state, function.body);
                }
            }
        }

        auto operator()(definition::Enumeration const& enumeration)
        {
            write_line(state, "enumeration");
            display_name_node(state, enumeration.name);
            display_template_parameters_node(state, enumeration.template_parameters);
            {
                auto const _ = last_node(state);
                write_line(state, "constructors");
                do_display(state, enumeration.constructors);
            }
        }

        auto operator()(definition::Alias const& alias)
        {
            write_line(state, "type alias");
            display_name_node(state, alias.name);
            display_template_parameters_node(state, alias.template_parameters);
            auto const _ = last_node(state);
            write_line(state, "aliased type");
            {
                auto const _ = last_node(state);
                do_display(state, alias.type);
            }
        }

        auto operator()(definition::Concept const& concept_)
        {
            write_line(state, "concept");
            display_name_node(state, concept_.name);
            display_template_parameters_node(state, concept_.template_parameters);
            {
                auto const _ = node(state);
                write_line(state, "function signatures");
                do_display(state, concept_.function_signatures);
            }
            {
                auto const _ = last_node(state);
                write_line(state, "type signatures");
                do_display(state, concept_.type_signatures);
            }
        }

        auto operator()(definition::Implementation const& implementation)
        {
            write_line(state, "implementation");
            display_template_parameters_node(state, implementation.template_parameters);
            {
                auto const _ = node(state);
                write_line(state, "type");
                {
                    auto const _ = last_node(state);
                    do_display(state, implementation.type);
                }
            }
            {
                auto const _ = last_node(state);
                write_line(state, "definitions");
                do_display(state, implementation.definitions);
            }
        }

        auto operator()(definition::Submodule const& submodule)
        {
            write_line(state, "submodule");
            display_name_node(state, submodule.name);
            display_template_parameters_node(state, submodule.template_parameters);
            {
                auto const _ = last_node(state);
                write_line(state, "definitions");
                do_display(state, submodule.definitions);
            }
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
            write_line(state, "_");
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

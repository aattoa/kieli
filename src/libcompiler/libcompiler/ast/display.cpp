#include <libutl/utilities.hpp>
#include <libcompiler/ast/display.hpp>

using namespace kieli::ast;

namespace {
    struct Display_state {
        std::string  output;
        std::string  indent;
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
                state.output.append("└─ ");
                state.indent.append("   ");
            }
            else {
                state.output.append("├─ ");
                state.indent.append("│  ");
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

    struct Definition_display_visitor {
        Display_state& state;

        auto operator()(auto const&)
        {
            auto const _ = last_node(state);
            write_line(state, "definition");
            {
                auto const _ = node(state);
                write_line(state, "aaa");
            }
            {
                auto const _ = node(state);
                write_line(state, "bbb");
                {
                    auto const _ = node(state);
                    write_line(state, "xxx");
                }
                {
                    auto const _ = node(state);
                    write_line(state, "yyy");
                }
                {
                    auto const _ = last_node(state);
                    write_line(state, "zzz");
                }
            }
            {
                auto const _ = last_node(state);
                write_line(state, "ccc");
            }
        }
    };

    struct Expression_display_visitor {
        Display_state& state;

        auto operator()(auto const&)
        {
            cpputil::todo();
        }
    };

    struct Pattern_display_visitor {
        Display_state& state;

        auto operator()(auto const&)
        {
            cpputil::todo();
        }
    };

    struct Type_display_visitor {
        Display_state& state;

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

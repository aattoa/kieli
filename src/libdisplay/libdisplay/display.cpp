#include <libutl/utilities.hpp>
#include <libparse/parse.hpp>
#include <libdesugar/desugar.hpp>
#include <libdisplay/display.hpp>

using namespace ki;
using namespace ki::ast;

namespace {
    struct Display_state {
        db::Database const& db;
        Arena const&        arena;
        std::ostream&       stream;
        std::string         indent;
        bool                unicode {};
    };

    enum struct Last : std::uint8_t { No, Yes };

    template <typename Node>
    concept displayable = requires(Display_state& state, Node const& node) {
        { do_display(state, node) } -> std::same_as<void>;
    };

    void write_indent(Display_state& state, Last last)
    {
        std::print(state.stream, "{}", state.indent);
        if (last == Last::Yes) {
            std::print(state.stream, "{}", state.unicode ? "└─ " : "+- ");
            state.indent.append("   ");
        }
        else {
            std::print(state.stream, "{}", state.unicode ? "├─ " : "|- ");
            state.indent.append(state.unicode ? "│  " : "|  ");
        }
    }

    void write_node(Display_state& state, Last last, std::invocable auto const& callback)
    {
        std::size_t previous_indent = state.indent.size();
        write_indent(state, last);
        std::invoke(callback);
        state.indent.resize(previous_indent);
    }

    void display_node(
        Display_state&          state,
        Last const              last,
        std::string_view const  description,
        displayable auto const& node)
    {
        write_node(state, last, [&] {
            std::println(state.stream, "{}", description);
            write_node(state, Last::Yes, [&] { do_display(state, node); });
        });
    }

    template <displayable T>
    void display_vector_node(
        Display_state&         state,
        Last const             last,
        std::string_view const description,
        std::vector<T> const&  vector)
    {
        write_node(state, last, [&] {
            std::println(state.stream, "{}", description);
            for (std::size_t i = 0; i != vector.size(); ++i) {
                write_node(state, i == vector.size() - 1 ? Last::Yes : Last::No, [&] {
                    do_display(state, vector[i]);
                });
            }
        });
    }

    void do_display(Display_state& state, Expression const& expression);
    void do_display(Display_state& state, Pattern const& pattern);
    void do_display(Display_state& state, Type const& type);

    void do_display(Display_state& state, Expression_id const id)
    {
        do_display(state, state.arena.expressions[id]);
    }

    void do_display(Display_state& state, Pattern_id const id)
    {
        do_display(state, state.arena.patterns[id]);
    }

    void do_display(Display_state& state, Type_id const id)
    {
        do_display(state, state.arena.types[id]);
    }

    void do_display(Display_state& state, Wildcard const&)
    {
        std::println(state.stream, "built-in wildcard");
    }

    void do_display(Display_state& state, db::Name const& name)
    {
        std::println(state.stream, "{:?}", state.db.string_pool.get(name.id));
    }

    void do_display(Display_state& state, Mutability const& mutability)
    {
        auto const visitor = utl::Overload {
            [&](db::Mutability concrete) {
                std::println(state.stream, "concrete {}", db::mutability_string(concrete));
            },
            [&](Parameterized_mutability const& m) {
                std::println(state.stream, "parameterized {}", state.db.string_pool.get(m.name.id));
            },
        };
        std::visit(visitor, mutability.variant);
    }

    void do_display(Display_state& state, Template_argument const& argument)
    {
        std::visit([&](auto const& alternative) { do_display(state, alternative); }, argument);
    }

    void do_display(Display_state& state, Path_segment const& segment)
    {
        std::println(state.stream, "path segment");
        if (segment.template_arguments.has_value()) {
            display_vector_node(
                state, Last::No, "template arguments", segment.template_arguments.value());
        }
        display_node(state, Last::Yes, "name", segment.name);
    }

    void do_display(Display_state& state, Path_root const& root)
    {
        auto const visitor = utl::Overload {
            [&](std::monostate) { std::println(state.stream, "none"); },
            [&](Path_root_global const&) { std::println(state.stream, "global"); },
            [&](Type_id const type) { do_display(state, type); },
        };
        std::visit(visitor, root);
    }

    void do_display(Display_state& state, Path const& path)
    {
        std::println(state.stream, "path");
        display_node(state, Last::No, "root", path.root);
        display_vector_node(state, Last::Yes, "segments", path.segments);
    }

    void do_display(Display_state& state, Template_parameter const& parameter)
    {
        auto display_default = [&](auto const& argument) {
            if (argument.has_value()) {
                write_node(state, Last::No, [&] {
                    std::println(state.stream, "default argument");
                    write_node(state, Last::Yes, [&] {
                        auto const visitor
                            = [&](auto const& alternative) { do_display(state, alternative); };
                        std::visit(visitor, argument.value());
                    });
                });
            }
        };
        auto const visitor = utl::Overload {
            [&](Template_type_parameter const& parameter) {
                std::println(state.stream, "type parameter");
                display_default(parameter.default_argument);
                display_node(state, Last::No, "name", parameter.name);
                display_vector_node(state, Last::Yes, "concepts", parameter.concepts);
            },
            [&](Template_value_parameter const& parameter) {
                std::println(state.stream, "value parameter");
                display_default(parameter.default_argument);
                display_node(state, Last::No, "name", parameter.name);
                display_node(state, Last::Yes, "type", parameter.type);
            },
            [&](Template_mutability_parameter const& parameter) {
                std::println(state.stream, "mutability parameter");
                display_default(parameter.default_argument);
                display_node(state, Last::Yes, "name", parameter.name);
            },
        };
        std::visit(visitor, parameter.variant);
    }

    void display_template_parameters_node(
        Display_state& state, Last const last, Template_parameters const& parameters)
    {
        if (parameters.has_value()) {
            display_vector_node(state, last, "template parameters", parameters.value());
        }
    }

    void do_display(Display_state& state, Loop_source const source)
    {
        std::println(state.stream, "loop source: {}", describe_loop_source(source));
    }

    void do_display(Display_state& state, Conditional_source const source)
    {
        std::println(state.stream, "conditional source: {}", describe_conditional_source(source));
    }

    void do_display(Display_state& state, Field const& field)
    {
        std::println(state.stream, "field");
        display_node(state, Last::No, "name", field.name);
        display_node(state, Last::Yes, "type", field.type);
    }

    void do_display(Display_state& state, Field_init const& field)
    {
        std::println(state.stream, "struct field initializer");
        display_node(state, Last::No, "name", field.name);
        display_node(state, Last::Yes, "expression", field.expression);
    }

    void do_display(Display_state& state, patt::Field const& field)
    {
        std::println(state.stream, "field");
        display_node(state, Last::No, "name", field.name);
        display_node(state, Last::Yes, "pattern", field.pattern);
    }

    void do_display(Display_state& state, Constructor_body const& body)
    {
        auto const visitor = utl::Overload {
            [&](Struct_constructor const& constructor) {
                std::println(state.stream, "struct constructor");
                display_vector_node(state, Last::Yes, "fields", constructor.fields);
            },
            [&](Tuple_constructor const& constructor) {
                std::println(state.stream, "tuple constructor");
                display_vector_node(state, Last::Yes, "types", constructor.types);
            },
            [&](Unit_constructor const&) { std::println(state.stream, "unit constructor"); },
        };
        std::visit(visitor, body);
    }

    void do_display(Display_state& state, patt::Constructor_body const& body)
    {
        auto const visitor = utl::Overload {
            [&](patt::Struct_constructor const& constructor) {
                std::println(state.stream, "struct constructor");
                display_vector_node(state, Last::Yes, "fields", constructor.fields);
            },
            [&](patt::Tuple_constructor const& constructor) {
                std::println(state.stream, "tuple constructor");
                display_vector_node(state, Last::Yes, "fields", constructor.fields);
            },
            [&](patt::Unit_constructor const&) { std::println(state.stream, "unit constructor"); },
        };
        std::visit(visitor, body);
    }

    void do_display(Display_state& state, Constructor const& constructor)
    {
        std::println(state.stream, "constructor");
        display_node(state, Last::No, "name", constructor.name);
        display_node(state, Last::Yes, "body", constructor.body);
    }

    void do_display(Display_state& state, Function_parameter const& parameter)
    {
        std::println(state.stream, "function parameter");
        display_node(state, Last::No, "type", parameter.type);
        if (parameter.default_argument.has_value()) {
            display_node(state, Last::No, "default argument", parameter.default_argument.value());
        }
        display_node(state, Last::Yes, "pattern", parameter.pattern);
    }

    void do_display(Display_state& state, Function_signature const& signature)
    {
        std::println(state.stream, "function signature");
        display_node(state, Last::No, "name", signature.name);
        display_template_parameters_node(state, Last::No, signature.template_parameters);
        display_node(state, Last::No, "return type", signature.return_type);
        display_vector_node(state, Last::Yes, "function parameters", signature.function_parameters);
    }

    void do_display(Display_state& state, Type_signature const& signature)
    {
        std::println(state.stream, "type signature");
        display_node(state, Last::No, "name", signature.name);
        display_vector_node(state, Last::Yes, "concepts", signature.concepts);
    }

    void do_display(Display_state& state, Match_arm const& arm)
    {
        std::println(state.stream, "arm");
        display_node(state, Last::No, "pattern", arm.pattern);
        display_node(state, Last::Yes, "handler", arm.expression);
    }

    void do_display(Display_state& state, Function const& function)
    {
        std::println(state.stream, "function");
        display_node(state, Last::No, "signature", function.signature);
        display_node(state, Last::Yes, "body", function.body);
    }

    void do_display(Display_state& state, Struct const& structure)
    {
        std::println(state.stream, "structure");
        display_template_parameters_node(state, Last::No, structure.template_parameters);
        display_node(state, Last::Yes, "constructor", structure.constructor);
    }

    void do_display(Display_state& state, Enum const& enumeration)
    {
        std::println(state.stream, "enumeration");
        display_node(state, Last::No, "name", enumeration.name);
        display_template_parameters_node(state, Last::No, enumeration.template_parameters);
        display_vector_node(state, Last::Yes, "constructors", enumeration.constructors);
    }

    void do_display(Display_state& state, Alias const& alias)
    {
        std::println(state.stream, "type alias");
        display_node(state, Last::No, "name", alias.name);
        display_template_parameters_node(state, Last::No, alias.template_parameters);
        display_node(state, Last::Yes, "aliased type", alias.type);
    }

    void do_display(Display_state& state, Concept const& concept_)
    {
        std::println(state.stream, "concept");
        display_node(state, Last::No, "name", concept_.name);
        display_template_parameters_node(state, Last::No, concept_.template_parameters);
        display_vector_node(state, Last::No, "functions", concept_.function_signatures);
        display_vector_node(state, Last::Yes, "types", concept_.type_signatures);
    }

    void display_literal(Display_state& state, db::Integer const& integer)
    {
        std::println(state.stream, "integer literal {}", integer.value);
    }

    void display_literal(Display_state& state, db::Floating const& floating)
    {
        std::println(state.stream, "floating point literal {}", floating.value);
    }

    void display_literal(Display_state& state, db::Boolean const& boolean)
    {
        std::println(state.stream, "boolean literal {}", boolean.value);
    }

    void display_literal(Display_state& state, db::String const& string)
    {
        std::println(state.stream, "string literal {:?}", state.db.string_pool.get(string.id));
    }

    struct Expression_display_visitor {
        Display_state& state;

        void operator()(
            utl::one_of<db::Integer, db::Floating, db::Boolean, db::String> auto const& literal)
        {
            display_literal(state, literal);
        }

        void operator()(Wildcard const& wildcard)
        {
            do_display(state, wildcard);
        }

        void operator()(Path const& path)
        {
            do_display(state, path);
        }

        void operator()(expr::Array const& array)
        {
            std::println(state.stream, "array literal");
            display_vector_node(state, Last::Yes, "elements", array.elements);
        }

        void operator()(expr::Tuple const& tuple)
        {
            std::println(state.stream, "tuple");
            display_vector_node(state, Last::Yes, "fields", tuple.fields);
        }

        void operator()(expr::Loop const& loop)
        {
            std::println(state.stream, "loop");
            display_node(state, Last::No, "body", loop.body);
            display_node(state, Last::Yes, "source", loop.source);
        }

        void operator()(expr::Break const& break_)
        {
            std::println(state.stream, "break");
            display_node(state, Last::Yes, "result", break_.result);
        }

        void operator()(expr::Continue const&)
        {
            std::println(state.stream, "continue");
        }

        void operator()(expr::Block const& block)
        {
            std::println(state.stream, "block");
            display_vector_node(state, Last::No, "side effects", block.effects);
            display_node(state, Last::Yes, "result", block.result);
        }

        void operator()(expr::Function_call const& call)
        {
            std::println(state.stream, "function call");
            display_node(state, Last::No, "invocable", call.invocable);
            display_vector_node(state, Last::Yes, "arguments", call.arguments);
        }

        void operator()(expr::Struct_init const& initializer)
        {
            std::println(state.stream, "struct initializer");
            display_node(state, Last::No, "constructor path", initializer.path);
            display_vector_node(state, Last::Yes, "field initializers", initializer.fields);
        }

        void operator()(expr::Infix_call const& application)
        {
            std::println(state.stream, "infix call");
            display_node(state, Last::No, "left operand", application.left);
            display_node(state, Last::No, "right operand", application.right);
            display_node(state, Last::Yes, "operator", application.op);
        }

        void operator()(expr::Struct_field const& field)
        {
            std::println(state.stream, "struct index");
            display_node(state, Last::No, "base expression", field.base);
            display_node(state, Last::Yes, "field name", field.name);
        }

        void operator()(expr::Tuple_field const& field)
        {
            std::println(state.stream, "tuple index");
            display_node(state, Last::No, "base expression", field.base);
            write_node(state, Last::Yes, [&] {
                std::println(state.stream, "field index {}", field.index);
            });
        }

        void operator()(expr::Array_index const& index)
        {
            std::println(state.stream, "array index");
            display_node(state, Last::No, "base expression", index.base);
            display_node(state, Last::Yes, "index expression", index.index);
        }

        void operator()(expr::Method_call const& call)
        {
            std::println(state.stream, "method call");
            display_node(state, Last::No, "method name", call.name);
            display_node(state, Last::No, "base expression", call.expression);
            if (call.template_arguments.has_value()) {
                display_vector_node(
                    state, Last::No, "template arguments", call.template_arguments.value());
            }
            display_vector_node(state, Last::Yes, "method arguments", call.function_arguments);
        }

        void operator()(expr::Conditional const& conditional)
        {
            std::println(state.stream, "conditional");
            display_node(state, Last::No, "condition", conditional.condition);
            display_node(state, Last::No, "true branch", conditional.true_branch);
            display_node(state, Last::No, "false branch", conditional.false_branch);
            display_node(state, Last::Yes, "source", conditional.source);
        }

        void operator()(expr::Match const& match)
        {
            std::println(state.stream, "match");
            display_node(state, Last::No, "scrutinee", match.scrutinee);
            display_vector_node(state, Last::Yes, "arms", match.arms);
        }

        void operator()(expr::Ascription const& ascription)
        {
            std::println(state.stream, "type ascription");
            display_node(state, Last::No, "expression", ascription.expression);
            display_node(state, Last::Yes, "ascribed type", ascription.type);
        }

        void operator()(expr::Let const& let)
        {
            std::println(state.stream, "let binding");
            if (let.type.has_value()) {
                display_node(state, Last::No, "type", let.type.value());
            }
            display_node(state, Last::No, "pattern", let.pattern);
            display_node(state, Last::Yes, "initializer", let.initializer);
        }

        void operator()(expr::Type_alias const& alias)
        {
            std::println(state.stream, "local type alias");
            display_node(state, Last::No, "name", alias.name);
            display_node(state, Last::Yes, "aliased type", alias.type);
        }

        void operator()(expr::Return const& ret)
        {
            std::println(state.stream, "ret");
            display_node(state, Last::Yes, "returned expression", ret.expression);
        }

        void operator()(expr::Sizeof const& sizeof_)
        {
            std::println(state.stream, "sizeof");
            display_node(state, Last::Yes, "inspected type", sizeof_.type);
        }

        void operator()(expr::Addressof const& addressof)
        {
            std::println(state.stream, "addressof");
            display_node(state, Last::No, "reference mutability", addressof.mutability);
            display_node(state, Last::Yes, "place expression", addressof.expression);
        }

        void operator()(expr::Deref const& dereference)
        {
            std::println(state.stream, "dereference");
            display_node(state, Last::Yes, "reference expression", dereference.expression);
        }

        void operator()(expr::Defer const& defer)
        {
            std::println(state.stream, "defer");
            display_node(state, Last::Yes, "effect", defer.expression);
        }

        void operator()(db::Error)
        {
            std::println(state.stream, "error");
        }
    };

    struct Pattern_display_visitor {
        Display_state& state;

        void operator()(
            utl::one_of<db::Integer, db::Floating, db::Boolean, db::String> auto const& literal)
        {
            display_literal(state, literal);
        }

        void operator()(Wildcard const& wildcard)
        {
            do_display(state, wildcard);
        }

        void operator()(patt::Name const& name)
        {
            std::println(state.stream, "name");
            display_node(state, Last::No, "name", name.name);
            display_node(state, Last::Yes, "mutability", name.mutability);
        }

        void operator()(patt::Constructor const& constructor)
        {
            std::println(state.stream, "constructor");
            display_node(state, Last::No, "constructor path", constructor.path);
            display_node(state, Last::Yes, "body", constructor.body);
        }

        void operator()(patt::Tuple const& tuple)
        {
            std::println(state.stream, "tuple");
            display_vector_node(state, Last::Yes, "field patterns", tuple.fields);
        }

        void operator()(patt::Slice const& slice)
        {
            std::println(state.stream, "slice");
            display_vector_node(state, Last::Yes, "element patterns", slice.elements);
        }

        void operator()(patt::Guarded const& guarded)
        {
            std::println(state.stream, "guarded");
            display_node(state, Last::No, "guarded pattern", guarded.pattern);
            display_node(state, Last::Yes, "guard expression", guarded.guard);
        }
    };

    struct Type_display_visitor {
        Display_state& state;

        void operator()(Path const& path)
        {
            do_display(state, path);
        }

        void operator()(type::Never const&)
        {
            std::println(state.stream, "built-in never");
        }

        void operator()(Wildcard const& wildcard)
        {
            do_display(state, wildcard);
        }

        void operator()(type::Tuple const& tuple)
        {
            std::println(state.stream, "tuple");
            display_vector_node(state, Last::Yes, "field types", tuple.fields);
        }

        void operator()(type::Array const& array)
        {
            std::println(state.stream, "tuple");
            display_node(state, Last::No, "length", array.length);
            display_node(state, Last::Yes, "element type", array.element_type);
        }

        void operator()(type::Slice const& slice)
        {
            std::println(state.stream, "slice");
            display_node(state, Last::Yes, "element type", slice.element_type);
        }

        void operator()(type::Function const& function)
        {
            std::println(state.stream, "function");
            display_vector_node(state, Last::No, "parameter types", function.parameter_types);
            display_node(state, Last::Yes, "return type", function.return_type);
        }

        void operator()(type::Typeof const& typeof_)
        {
            std::println(state.stream, "typeof");
            display_node(state, Last::Yes, "inspected expression", typeof_.expression);
        }

        void operator()(type::Reference const& reference)
        {
            std::println(state.stream, "reference");
            display_node(state, Last::No, "reference mutability", reference.mutability);
            display_node(state, Last::Yes, "referenced type", reference.referenced_type);
        }

        void operator()(type::Pointer const& pointer)
        {
            std::println(state.stream, "pointer");
            display_node(state, Last::No, "pointer mutability", pointer.mutability);
            display_node(state, Last::Yes, "pointee type", pointer.pointee_type);
        }

        void operator()(type::Impl const& implementation)
        {
            std::println(state.stream, "implementation");
            display_vector_node(state, Last::Yes, "concepts", implementation.concepts);
        }

        void operator()(db::Error const&)
        {
            std::println(state.stream, "error");
        }
    };

    void do_display(Display_state& state, Expression const& expression)
    {
        std::visit(Expression_display_visitor { state }, expression.variant);
    }

    void do_display(Display_state& state, Pattern const& pattern)
    {
        std::visit(Pattern_display_visitor { state }, pattern.variant);
    }

    void do_display(Display_state& state, Type const& type)
    {
        std::visit(Type_display_visitor { state }, type.variant);
    }

    struct Indent_info {
        std::size_t      width {};
        std::string_view description;
    };

    struct Definition_visitor {
        Display_state&           state;
        des::Context&            des_ctx;
        std::vector<Indent_info> indent_stack;

        void push_indent(std::string_view description)
        {
            indent_stack.emplace_back(state.indent.size(), description);
        }

        void operator()(auto const& def)

        {
            write_node(state, Last::No, [&] { do_display(state, des::desugar(des_ctx, def)); });
        }

        void operator()(cst::Impl_begin const& impl)
        {
            push_indent("impl");

            write_indent(state, Last::No);
            std::println(state.stream, "impl");

            auto const self = des::desugar(des_ctx, des_ctx.cst.types[impl.self_type]);
            display_node(state, Last::No, "self", self);

            write_indent(state, Last::Yes);
            std::println(state.stream, "definitions");
        }

        void operator()(cst::Submodule_begin const& submodule)
        {
            push_indent(state.db.string_pool.get(submodule.name.id));

            write_indent(state, Last::No);
            std::println(state.stream, "submodule");

            display_node(state, Last::No, "name", submodule.name);

            write_indent(state, Last::Yes);
            std::println(state.stream, "definitions");
        }

        void operator()(cst::Block_end const&)
        {
            cpputil::always_assert(not indent_stack.empty());
            auto indent = indent_stack.back();
            indent_stack.pop_back();

            write_node(state, Last::Yes, [&] {
                std::println(state.stream, "end of {}", indent.description);
            });

            state.indent.resize(indent.width);
        }
    };
} // namespace

void ki::dis::display_document(
    std::ostream& stream, db::Database& db, db::Document_id doc_id, db::Diagnostic_sink sink)
{
    auto par_ctx = par::context(db, doc_id, sink);

    auto des_ctx = des::Context {
        .cst            = par_ctx.arena,
        .ast            = ast::Arena {},
        .add_diagnostic = sink,
    };

    auto state = Display_state {
        .db      = db,
        .arena   = des_ctx.ast,
        .stream  = stream,
        .indent  = {},
        .unicode = true,
    };

    auto visitor = Definition_visitor {
        .state        = state,
        .des_ctx      = des_ctx,
        .indent_stack = {},
    };

    std::println(state.stream, "module");
    par::parse(par_ctx, visitor);
    write_node(state, Last::Yes, [&] { std::println(state.stream, "end of module"); });
}

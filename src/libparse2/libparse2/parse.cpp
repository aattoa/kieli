#include <libutl/common/utilities.hpp>
#include <libparse2/parse.hpp>
#include <libparse2/parser_internals.hpp>

auto libparse2::parse_template_parameters(Context& context)
    -> std::optional<cst::Template_parameters>
{
    (void)context;
    cpputil::todo();
}

auto libparse2::parse_template_parameter(Context& context) -> std::optional<cst::Template_parameter>
{
    (void)context;
    cpputil::todo();
}

auto libparse2::parse_template_argument(Context& context) -> std::optional<cst::Template_argument>
{
    (void)context;
    cpputil::todo();
}

auto libparse2::parse_template_arguments(Context& context) -> std::optional<cst::Template_arguments>
{
    (void)context;
    cpputil::todo();
}

auto libparse2::parse_function_parameters(Context& context)
    -> std::optional<cst::Function_parameters>
{
    (void)context;
    cpputil::todo();
}

auto libparse2::parse_function_parameter(Context& context) -> std::optional<cst::Function_parameter>
{
    (void)context;
    cpputil::todo();
}

auto libparse2::parse_function_argument(Context& context) -> std::optional<cst::Function_argument>
{
    (void)context;
    cpputil::todo();
}

auto libparse2::parse_function_arguments(Context& context) -> std::optional<cst::Function_arguments>
{
    (void)context;
    cpputil::todo();
}

auto kieli::parse2(utl::Source::Wrapper const source, Compile_info& compile_info) -> cst::Module
{
    cst::Node_arena    node_arena = cst::Node_arena::with_default_page_size();
    libparse2::Context context { node_arena, Lex2_state::make(source, compile_info) };

    std::vector<cst::Definition> definitions;
    while (auto definition = libparse2::parse_definition(context)) {
        definitions.push_back(std::move(definition.value()));
    }

    if (!context.is_finished()) {
        context.error_expected("a definition");
    }

    return cst::Module {
        .imports     = {},
        .definitions = std::move(definitions),
        .node_arena  = std::move(node_arena),
    };
}

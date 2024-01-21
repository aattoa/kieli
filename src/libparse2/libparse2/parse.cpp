#include <libutl/common/utilities.hpp>
#include <libparse2/parse.hpp>
#include <libparse2/parser_internals.hpp>

namespace {} // namespace

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

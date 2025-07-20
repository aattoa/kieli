#ifndef KIELI_LIBPARSE_PARSE
#define KIELI_LIBPARSE_PARSE

#include <libcompiler/db.hpp>
#include <libparse/internals.hpp>

namespace ki::par {

    void parse(Context& ctx, auto&& visitor)
    {
        for (;;) {
            try {
                switch (auto token = extract(ctx); token.type) {
                case lex::Type::End_of_input: return;
                case lex::Type::Fn:           visitor(extract_function(ctx, token)); break;
                case lex::Type::Struct:       visitor(extract_structure(ctx, token)); break;
                case lex::Type::Enum:         visitor(extract_enumeration(ctx, token)); break;
                case lex::Type::Concept:      visitor(extract_concept(ctx, token)); break;
                case lex::Type::Alias:        visitor(extract_alias(ctx, token)); break;
                case lex::Type::Impl:         visitor(extract_implementation(ctx, token)); break;
                case lex::Type::Module:       visitor(extract_submodule(ctx, token)); break;
                case lex::Type::Brace_close:  visitor(cst::Block_end { token.range }); break;
                default:                      error_expected(ctx, token.range, "a definition");
                }
            }
            catch (Failure const&) {
                skip_to_next_recovery_point(ctx);
            }
        }
    }

} // namespace ki::par

#endif // KIELI_LIBPARSE_PARSE

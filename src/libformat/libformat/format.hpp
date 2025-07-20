#ifndef KIELI_LIBFORMAT_FORMAT
#define KIELI_LIBFORMAT_FORMAT

#include <libutl/utilities.hpp>
#include <libcompiler/db.hpp>

namespace ki::fmt {

    enum struct Function_body : std::uint8_t {
        Leave_as_is,
        Normalize_to_block,
        Normalize_to_equals_sign
    };

    struct Options {
        std::size_t   tab_size                        = 4;
        bool          use_spaces                      = true;
        std::size_t   empty_lines_between_definitions = 1;
        Function_body function_body                   = Function_body::Leave_as_is;
    };

    struct Context {
        db::Database const& db;
        cst::Arena const&   arena;
        Options             options {};
        std::size_t         indentation {};
    };

    auto format(Context& ctx, cst::Function const& function) -> std::string;
    auto format(Context& ctx, cst::Struct const& structure) -> std::string;
    auto format(Context& ctx, cst::Enum const& enumeration) -> std::string;
    auto format(Context& ctx, cst::Alias const& alias) -> std::string;
    auto format(Context& ctx, cst::Concept const& concept_) -> std::string;
    auto format(Context& ctx, cst::Impl_begin const& impl) -> std::string;
    auto format(Context& ctx, cst::Submodule_begin const& submodule) -> std::string;
    auto format(Context& ctx, cst::Block_end const& block_end) -> std::string;

    auto format(Context& ctx, cst::Expression const& expression) -> std::string;
    auto format(Context& ctx, cst::Expression_id expression_id) -> std::string;
    auto format(Context& ctx, cst::Pattern const& pattern) -> std::string;
    auto format(Context& ctx, cst::Pattern_id pattern_id) -> std::string;
    auto format(Context& ctx, cst::Type const& type) -> std::string;
    auto format(Context& ctx, cst::Type_id type_id) -> std::string;

} // namespace ki::fmt

#endif // KIELI_LIBFORMAT_FORMAT

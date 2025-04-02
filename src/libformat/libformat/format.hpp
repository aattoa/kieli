#ifndef KIELI_LIBFORMAT_FORMAT
#define KIELI_LIBFORMAT_FORMAT

#include <libutl/utilities.hpp>
#include <libutl/string_pool.hpp>
#include <libcompiler/cst/cst.hpp>

namespace ki::format {

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

    auto format_module(
        cst::Module const& module, utl::String_pool const& pool, Options const& options)
        -> std::string;

    void format(
        utl::String_pool const& pool,
        cst::Arena const&       arena,
        Options const&          options,
        cst::Definition const&  definition,
        std::string&            output);

    void format(
        utl::String_pool const& pool,
        cst::Arena const&       arena,
        Options const&          options,
        cst::Expression const&  expression,
        std::string&            output);

    void format(
        utl::String_pool const& pool,
        cst::Arena const&       arena,
        Options const&          options,
        cst::Pattern const&     pattern,
        std::string&            output);

    void format(
        utl::String_pool const& pool,
        cst::Arena const&       arena,
        Options const&          options,
        cst::Type const&        type,
        std::string&            output);

    void format(
        utl::String_pool const& pool,
        cst::Arena const&       arena,
        Options const&          options,
        cst::Expression_id      expression_id,
        std::string&            output);

    void format(
        utl::String_pool const& pool,
        cst::Arena const&       arena,
        Options const&          options,
        cst::Pattern_id         pattern_id,
        std::string&            output);

    void format(
        utl::String_pool const& pool,
        cst::Arena const&       arena,
        Options const&          options,
        cst::Type_id            type_id,
        std::string&            output);

    template <typename T>
    concept formattable = requires(
        utl::String_pool const pool,
        cst::Arena const       arena,
        Options const          options,
        T const                object,
        std::string            output) {
        { ki::format::format(pool, arena, options, object, output) } -> std::same_as<void>;
    };

    auto to_string(
        utl::String_pool const  pool,
        cst::Arena const&       arena,
        Options const&          options,
        formattable auto const& object) -> std::string
    {
        std::string output;
        ki::format::format(pool, arena, options, object, output);
        return output;
    }

} // namespace ki::format

#endif // KIELI_LIBFORMAT_FORMAT

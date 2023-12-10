#include <libutl/common/utilities.hpp>
#include <liblex/context.hpp>

auto liblex::Context::source_view_for(std::string_view const view) const noexcept
    -> utl::Source_view
{
    utl::Source_position start_position;
    for (char const* ptr = m_source_begin; ptr != view.data(); ++ptr) {
        start_position.advance_with(*ptr);
    }
    utl::Source_position stop_position = start_position;
    for (char const c : view) {
        stop_position.advance_with(c);
    }
    return utl::Source_view { m_source, view, start_position, stop_position };
}

auto liblex::Context::remaining_input_size() const noexcept -> utl::Usize
{
    return utl::unsigned_distance(m_pointer, m_source_end);
}

liblex::Context::Context(
    utl::Source::Wrapper const source, kieli::Compile_info& compile_info) noexcept
    : m_compile_info { compile_info }
    , m_source { source }
    , m_source_begin { source->string().data() }
    , m_source_end { m_source_begin + source->string().size() }
    , m_pointer { m_source_begin }
{}

auto liblex::Context::source() const noexcept -> utl::Source::Wrapper
{
    return m_source;
}

auto liblex::Context::source_begin() const noexcept -> char const*
{
    return m_source_begin;
}

auto liblex::Context::source_end() const noexcept -> char const*
{
    return m_source_end;
}

auto liblex::Context::pointer() const noexcept -> char const*
{
    return m_pointer;
}

auto liblex::Context::position() const noexcept -> utl::Source_position
{
    return m_position;
}

auto liblex::Context::is_finished() const noexcept -> bool
{
    return m_pointer == m_source_end;
}

auto liblex::Context::current() const noexcept -> char
{
    assert(!is_finished());
    return *m_pointer;
}

auto liblex::Context::extract_current() noexcept -> char
{
    assert(!is_finished());
    return *m_pointer++;
}

auto liblex::Context::advance(utl::Usize const offset) noexcept -> void
{
    assert(offset <= remaining_input_size());
    for (utl::Usize i = 0; i != offset; ++i) {
        m_position.advance_with(*m_pointer++);
    }
}

auto liblex::Context::try_consume(char const c) noexcept -> bool
{
    assert(c != '\n');
    if (is_finished()) {
        return false;
    }
    if (current() == c) {
        ++m_position.column;
        ++m_pointer;
        return true;
    }
    return false;
}

auto liblex::Context::try_consume(std::string_view const string) noexcept -> bool
{
    if (ranges::starts_with(std::string_view { m_pointer, m_source_end }, string)) {
        advance(string.size());
        return true;
    }
    return false;
}

auto liblex::Context::make_string_literal(std::string_view const string) -> utl::Pooled_string
{
    return m_compile_info.string_literal_pool.make(string);
}

auto liblex::Context::make_operator(std::string_view const string) -> utl::Pooled_string
{
    assert(!string.empty());
    return m_compile_info.operator_pool.make(string);
}

auto liblex::Context::make_identifier(std::string_view const string) -> utl::Pooled_string
{
    assert(!string.empty());
    return m_compile_info.identifier_pool.make(string);
}

auto liblex::Context::error(std::string_view const position, std::string_view const message)
    -> std::unexpected<Token_extraction_failure>
{
    m_compile_info.diagnostics.emit(
        cppdiag::Severity::error, source_view_for(position), "{}", message);
    return std::unexpected { Token_extraction_failure {} };
}

auto liblex::Context::error(char const* const position, std::string_view const message)
    -> std::unexpected<Token_extraction_failure>
{
    return error({ position, position }, message);
}

auto liblex::Context::error(std::string_view const message)
    -> std::unexpected<Token_extraction_failure>
{
    return error(pointer(), message);
}

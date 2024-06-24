#include <libutl/utilities.hpp>
#include <libcompiler/filesystem.hpp>
#include <cpputil/io.hpp>

auto kieli::text_range(std::string_view const string, Range const range) -> std::string_view
{
    cpputil::always_assert(range.start <= range.stop);

    auto begin = string.begin();
    for (std::size_t i = 0; i != range.start.line; ++i) {
        begin = std::find(begin, string.end(), '\n');
        cpputil::always_assert(begin != string.end());
        ++begin; // Skip the line feed
    }
    begin += range.start.column;

    auto end = begin;
    auto pos = range.start;
    while (pos != range.stop) {
        cpputil::always_assert(end != string.end());
        advance_position(pos, *end++);
    }

    return { begin, end };
}

auto kieli::edit_text(std::string& text, Range const range, std::string_view const new_text) -> void
{
    auto const where = text_range(text, range);
    auto const index = static_cast<std::size_t>(where.data() - text.data());
    text.replace(index, where.size(), new_text);
}

auto kieli::advance_position(Position& position, char const character) -> void
{
    if (character == '\n') {
        ++position.line;
        position.column = 0;
    }
    else {
        ++position.column;
    }
}

auto kieli::find_source(std::filesystem::path const& path, Source_vector const& sources)
    -> std::optional<Source_id>
{
    auto const it    = std::ranges::find(sources.underlying, path, &Source::path);
    auto const index = static_cast<std::size_t>(it - sources.underlying.begin());
    return it != sources.underlying.end() ? std::optional(Source_id(index)) : std::nullopt;
}

auto kieli::read_source(std::filesystem::path path) -> std::expected<Source, Read_failure>
{
    if (auto file = cpputil::io::File::open_read(path.c_str())) {
        if (auto content = cpputil::io::read(file.get())) {
            return Source { std::move(content).value(), std::move(path) };
        }
        return std::unexpected(Read_failure::failed_to_read);
    }
    if (std::filesystem::exists(path)) {
        return std::unexpected(Read_failure::failed_to_open);
    }
    return std::unexpected(Read_failure::does_not_exist);
}

auto kieli::read_source(std::filesystem::path path, Source_vector& sources)
    -> std::expected<Source_id, Read_failure>
{
    return read_source(std::move(path)).transform([&](Source source) {
        return sources.push(std::move(source));
    });
}

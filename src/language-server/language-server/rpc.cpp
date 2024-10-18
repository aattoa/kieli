#include <libutl/utilities.hpp>
#include <language-server/rpc.hpp>

void kieli::lsp::rpc_write_message(std::ostream& out, std::string_view const message)
{
    std::format_to(
        std::ostreambuf_iterator(out), "Content-Length: {}\r\n\r\n{}", message.size(), message);
}

auto kieli::lsp::rpc_read_message(std::istream& in) -> std::expected<std::string, Read_failure>
{
    if (not in.ignore("Content-Length: "sv.size())) {
        return std::unexpected(Read_failure::no_header);
    }

    std::streamsize content_length {};
    if (in >> content_length; not in) {
        return std::unexpected(Read_failure::no_length);
    }

    if (not in.ignore("\r\n\r\n"sv.size())) {
        return std::unexpected(Read_failure::no_separator);
    }

    std::string content;
    content.resize(content_length);

    if (not in.read(&content.front(), content_length)) {
        return std::unexpected(Read_failure::no_content);
    }

    return content;
}

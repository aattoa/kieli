#include "utl/utilities.hpp"
#include "language/configuration.hpp"
#include "virtual_machine.hpp"


auto vm::Executable_program::serialize() const -> std::vector<std::byte> {
    utl::todo();
}

auto vm::Executable_program::deserialize(std::span<const std::byte>) -> Executable_program {
    utl::todo();
}


#if 0

auto vm::Executable_program::serialize() const -> std::vector<std::byte> {
    std::vector<std::byte> buffer;

    auto const write = [&](utl::trivially_copyable auto const... args) {
        utl::serialize_to(std::back_inserter(buffer), args...);
    };

    write(language::version, stack_capacity);

    {
        write(constants.string_buffer.size());
        buffer.insert(
            buffer.end(),
            reinterpret_cast<std::byte const*>(constants.string_buffer.data()),
            reinterpret_cast<std::byte const*>(constants.string_buffer.data() + constants.string_buffer.size()));

        write(constants.string_buffer_views.size());
        for (auto const pair : constants.string_buffer_views)
            write(pair);
    }

    {
        write(bytecode.bytes.size());
        buffer.insert(buffer.end(), bytecode.bytes.begin(), bytecode.bytes.end());
    }

    return buffer;
}


namespace {

    using Byte_span = std::span<std::byte const>;

    template <utl::trivial T>
    auto extract(Byte_span& span) -> T {
        assert(span.size() >= sizeof(T));

        T value;
        std::memcpy(&value, span.data(), sizeof(T));
        span = span.subspan(sizeof(T));
        return value;
    }

}


auto vm::Executable_program::deserialize(Byte_span bytes) -> Executable_program {
    if (auto const extracted_version = extract<utl::Usize>(bytes); language::version != extracted_version) {
        throw utl::exception(
            "Attempted to deserialize a program compiled with "
            "kieli version {}, but the current version is {}",
            extracted_version,
            language::version);
    }

    Executable_program program {
        .stack_capacity = extract<utl::Usize>(bytes)
    };

    {
        auto const string_buffer_size = extract<utl::Usize>(bytes);
        utl::always_assert(string_buffer_size <= bytes.size());

        program.constants.string_buffer.assign(
            reinterpret_cast<char const*>(bytes.data()),
            string_buffer_size
        );

        bytes = bytes.subspan(string_buffer_size);
        
        for (auto i = extract<utl::Usize>(bytes); i != 0; --i) {
            program.constants.string_buffer_views.push_back(extract<utl::Pair<utl::Usize>>(bytes));
        }
    }

    {
        auto const bytecode_size = extract<utl::Usize>(bytes);
        utl::always_assert(bytecode_size == bytes.size());

        program.bytecode.bytes.assign(bytes.data(), bytes.data() + bytecode_size);
    }

    return program;
}

#endif
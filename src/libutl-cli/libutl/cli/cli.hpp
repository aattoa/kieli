#pragma once

#include <libutl/common/utilities.hpp>
#include <libutl/common/flatmap.hpp>
#include <libutl/diagnostics/diagnostics.hpp>


namespace cli {

    template <class T>
    struct [[nodiscard]] Value {
        static_assert(std::is_trivially_copyable_v<T>);

        std::string_view name;

        tl::optional<T> default_value;
        tl::optional<T> minimum_value;
        tl::optional<T> maximum_value;

        auto default_to(T&&) noexcept -> Value;
        auto min       (T&&) noexcept -> Value;
        auto max       (T&&) noexcept -> Value;
    };

    namespace types {
        using Int   = utl::Isize;
        using Float = utl::Float;
        using Bool  = bool;
        using Str   = std::string_view;
    }

    inline auto integer  (std::string_view const name = {}) -> Value<types::Int  > { return { name }; }
    inline auto floating (std::string_view const name = {}) -> Value<types::Float> { return { name }; }
    inline auto boolean  (std::string_view const name = {}) -> Value<types::Bool > { return { name }; }
    inline auto string   (std::string_view const name = {}) -> Value<types::Str  > { return { name }; }


    struct [[nodiscard]] Parameter {
        struct Name {
            std::string        long_form;
            tl::optional<char> short_form;

            /*implicit*/ Name(char const* long_name, tl::optional<char> short_name = tl::nullopt) noexcept; // NOLINT
        };

        using Variant = std::variant<
            decltype(integer ()),
            decltype(floating()),
            decltype(boolean ()),
            decltype(string  ())
        >;

        Name                            name;
        std::vector<Variant>            values;
        tl::optional<std::string_view> description;
        bool                            defaulted = false;
    };


    struct [[nodiscard]] Named_argument {
        using Variant = std::variant<
            types::Int,
            types::Float,
            types::Bool,
            types::Str
        >;

        std::string          name; // short-form names are automatically converted to long-form, which is also why the string must be an owning one
        std::vector<Variant> values;
    };

    using Positional_argument = std::string_view;


    struct [[nodiscard]] Options_description {
        std::vector<Parameter>          parameters;
        utl::Flatmap<char, std::string> long_forms;

    private:

        struct Option_adder {
            Options_description* self;

            auto map_short_to_long(Parameter::Name const&) noexcept -> void;

            auto operator()(Parameter::Name&&               name,
                            tl::optional<std::string_view> description = tl::nullopt)
                noexcept -> Option_adder;

            template <class T>
            auto operator()(Parameter::Name&&               name,
                            Value<T>&&                      value,
                            tl::optional<std::string_view> description = tl::nullopt)
                noexcept -> Option_adder;

            auto operator()(Parameter::Name&&                 name,
                            std::vector<Parameter::Variant>&& values,
                            tl::optional<std::string_view>   description = tl::nullopt)
                noexcept -> Option_adder;
        };

    public:

        auto add_options() noexcept -> Option_adder {
            return { this };
        }
    };


    struct [[nodiscard]] Options {
        std::vector<Positional_argument> positional_arguments;
        std::vector<Named_argument>      named_arguments;
        std::string_view                 program_name_as_invoked;

        struct Argument_proxy {
            std::string_view         name;
            Named_argument::Variant* pointer = nullptr;
            utl::Usize               count   = 0;
            bool                     indexed = false;
            bool                     empty   = true;

            /* implicit */ operator types::Int   const*() const;
            /* implicit */ operator types::Float const*() const;
            /* implicit */ operator types::Bool  const*() const;
            /* implicit */ operator types::Str   const*() const;

            [[nodiscard]]
            explicit operator bool() const noexcept;

            auto operator[](utl::Usize) -> Argument_proxy;
        };

        auto operator[](std::string_view) noexcept -> Argument_proxy;

    };


    struct [[nodiscard]] Unrecognized_option : utl::diagnostics::Error {
        using Error::Error;
        using Error::operator=;
    };


    [[nodiscard]]
    auto to_string(cli::Options_description const&) -> std::string;

    [[nodiscard]]
    auto parse_command_line(int argc, char const* const* argv, Options_description const&)
        -> tl::expected<Options, Unrecognized_option>;

}

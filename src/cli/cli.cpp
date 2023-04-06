#include "utl/utilities.hpp"
#include "utl/diagnostics.hpp"
#include "cli.hpp"


namespace {

    constexpr auto to_lower(char const c) noexcept -> char {
        return ('A' <= c && c <= 'Z') ? c + 32 : c;
    }

    static_assert(to_lower('A') == 'a');
    static_assert(to_lower('z') == 'z');
    static_assert(to_lower('%') == '%');


    template <class T>
    constexpr auto type_description() noexcept -> std::string_view {
        if constexpr (std::same_as<T, cli::types::Int>)
            return "int";
        else if constexpr (std::same_as<T, cli::types::Float>)
            return "float";
        else if constexpr (std::same_as<T, cli::types::Bool>)
            return "bool";
        else if constexpr (std::same_as<T, cli::types::Str>)
            return "str";
        else
            static_assert(utl::always_false<T>);
    }

    template <class... Ts>
    constexpr auto type_description(std::variant<Ts...> const& variant) noexcept -> std::string_view {
        return std::visit([]<class T>(T const&) { return type_description<T>(); }, variant);
    }


    struct Parse_context {
        std::string_view* pointer;
        std::string_view* start;
        std::string_view* stop;

        explicit Parse_context(std::span<std::string_view> const span) noexcept
            : pointer { span.data()         }
            , start   { pointer             }
            , stop    { start + span.size() } {}

        [[nodiscard]]
        auto is_finished() const noexcept -> bool {
            return pointer == stop;
        }
        [[nodiscard]]
        auto current() const noexcept -> std::string_view& {
            return *pointer;
        }
        [[nodiscard]]
        auto extract() noexcept -> std::string_view& {
            return *pointer++;
        }
        auto advance() noexcept -> void {
            ++pointer;
        }
        auto retreat() noexcept -> void {
            --pointer;
        }

        [[nodiscard]]
        auto make_error(utl::diagnostics::Message_arguments const arguments) const
            -> utl::diagnostics::Builder
        {
            std::string command_line_string
                = std::span { start, stop }
                | ranges::views::join(' ')
                | ranges::to<std::string>();

            utl::disable_short_string_optimization(command_line_string);

            // The erroneous view must be a view into command_line_string
            std::string_view erroneous_view;

            if (pointer == stop) {
                auto const* const end = command_line_string.data() + command_line_string.size();
                erroneous_view = { end - 1, end };
            }
            else {
                char const* view_begin = command_line_string.data();
                for (auto const* view = start; view != pointer; ++view) {
                    view_begin += view->size() + 1; // +1 for the whitespace delimiter
                }
                erroneous_view = { view_begin, pointer->size() };
            }

            utl::Source const fake_source { utl::Source::Filename { "[command line]" }, std::move(command_line_string) };

            utl::diagnostics::Builder builder;
            builder.emit_simple_error(
                arguments.add_source_info(
                    fake_source,
                    utl::Source_view {
                        erroneous_view,
                        utl::Source_position {},
                        utl::Source_position { 1, 1 + utl::unsigned_distance(fake_source.string().data(), erroneous_view.data()) }
                    }
                ),
                utl::diagnostics::Type::recoverable // Prevent exception
            );
            return builder;
        }

        [[noreturn]]
        auto error(utl::diagnostics::Message_arguments const arguments) const -> void {
            throw utl::diagnostics::Error { make_error(arguments).string() };
        }

        [[noreturn]]
        auto expected(std::string_view const expectation) const -> void {
            error({
                .message           = "Expected {}",
                .message_arguments = fmt::make_format_args(expectation)
            });
        }

        [[nodiscard]]
        auto unrecognized_option() const -> cli::Unrecognized_option {
            return cli::Unrecognized_option { make_error({ "Unrecognized option" }).string() };
        }
    };


    template <class T>
    auto extract_value(Parse_context& context) -> tl::optional<T> {
        if (context.is_finished())
            return tl::nullopt;

        auto const view = context.extract();

        if constexpr (utl::one_of<T, cli::types::Int, cli::types::Float>) {
            auto const* const start = view.data();
            auto const* const stop  = start + view.size();

            T value;
            auto [ptr, ec] = std::from_chars(start, stop, value);

            switch (ec) {
            case std::errc {}:
            {
                if (ptr == stop) {
                    return value;
                }
                else {
                    context.retreat();
                    context.error({
                        .message           = "Unexpected suffix: '{}'",
                        .message_arguments = fmt::make_format_args(std::string_view { ptr, stop })
                    });
                }
            }
            case std::errc::result_out_of_range:
            {
                context.retreat();
                context.error({
                    .message           = "The given value is too large to be represented by a {}-bit value",
                    .message_arguments = fmt::make_format_args(sizeof(T) * CHAR_BIT)
                });
            }
            case std::errc::invalid_argument:
            {
                context.retreat();
                return tl::nullopt;
            }
            default:
                utl::todo();
            }
        }

        else if constexpr (std::same_as<T, cli::types::Bool>) {
            std::string input;
            input.reserve(view.size());
            ranges::copy(view | ranges::views::transform(to_lower), std::back_inserter(input));

            auto const is_one_of = [&](auto const&... args) {
                return ((input == args) || ...);
            };

            if (is_one_of("true", "yes", "1")) {
                return true;
            }
            else if (is_one_of("false", "no", "0")) {
                return false;
            }
            else {
                context.retreat();
                return tl::nullopt;
            }
        }

        else {
            static_assert(std::same_as<T, cli::types::Str>);
            return view;
        }
    }

    auto extract_arguments(Parse_context& context, cli::Parameter const& parameter)
        -> std::vector<cli::Named_argument::Variant>
    {
        std::vector<cli::Named_argument::Variant> arguments;
        arguments.reserve(parameter.values.size());

        for (auto const& value : parameter.values) {
            std::visit([&]<class T>(cli::Value<T> const& value) {
                auto argument = extract_value<T>(context);

                if (!argument) {
                    context.error({
                        .message           = "Expected an argument [{}]",
                        .message_arguments = fmt::make_format_args(type_description<T>())
                    });
                }

                if (value.minimum_value) {
                    if (*argument < *value.minimum_value) {
                        context.retreat();
                        context.error({
                            .message           = "The minimum allowed value is {}",
                            .message_arguments = fmt::make_format_args(*value.minimum_value)
                        });
                    }
                }
                if (value.maximum_value) {
                    if (*argument > *value.maximum_value) {
                        context.retreat();
                        context.error({
                            .message = "The maximum allowed value is {}",
                            .message_arguments = fmt::make_format_args(*value.maximum_value)
                        });
                    }
                }

                arguments.push_back(std::move(*argument));
            }, value);
        }

        return arguments;
    }

}


auto cli::parse_command_line(
    int                 const  argc,
    char const* const*  const  argv,
    Options_description const& description) -> tl::expected<Options, Unrecognized_option>
{
    std::vector<std::string_view> command_line(argv + 1, argv + argc);
    Options options { .program_name_as_invoked = *argv };

    Parse_context context { command_line };

    while (!context.is_finished()) {
        tl::optional<std::string> name;

        {
            auto view = context.extract();

            if (view.starts_with("--")) {
                view.remove_prefix(2);
                if (view.empty())
                    context.expected("a flag name");
                else
                    name = std::string(view);
            }
            else if (view.starts_with('-')) {
                view.remove_prefix(1);
                switch (view.size()) {
                case 0:
                    context.expected("a single-character flag name");
                case 1:
                    if (auto const* const long_form = description.long_forms.find(view.front())) {
                        name = *long_form;
                        break;
                    }
                    else {
                        context.retreat();
                        return tl::unexpected(context.unrecognized_option());
                    }
                default:
                    context.retreat();
                    context.expected("a single-character flag name; use '--' instead of '-' if this was intended");
                }
            }
            else {
                context.retreat();
            }
        }

        if (name) {
            auto it = ranges::find(
                description.parameters,
                name,
                utl::compose(&Parameter::Name::long_form, &Parameter::name)
            );

            if (it != description.parameters.end()) {
                options.named_arguments.emplace_back(
                    std::move(*name),
                    extract_arguments(context, *it));
            }
            else {
                context.retreat();
                return tl::unexpected(context.unrecognized_option());
            }
        }
        else {
            options.positional_arguments.push_back(context.extract());
        }
    }

    // Apply default arguments
    for (auto const& parameter : description.parameters) {
        if (parameter.defaulted && !options[parameter.name.long_form]) {
            std::vector<Named_argument::Variant> arguments;

            for (auto const& value : parameter.values) {
                std::visit([&]<class T>(Value<T> const& value) {
                    arguments.push_back(value.default_value.value());
                }, value);
            }

            options.named_arguments.emplace_back(
                parameter.name.long_form,
                std::move(arguments));
        }
    }

    return options;
}


template <class T>
auto cli::Value<T>::default_to(T&& value) noexcept -> Value {
    auto copy = *this;
    copy.default_value = std::move(value);
    return copy;
}

template <class T>
auto cli::Value<T>::min(T&& value) noexcept -> Value {
    auto copy = *this;
    copy.minimum_value = std::move(value);
    return copy;
}

template <class T>
auto cli::Value<T>::max(T&& value) noexcept -> Value {
    auto copy = *this;
    copy.maximum_value = std::move(value);
    return copy;
}

template struct cli::Value<cli::types::Int  >;
template struct cli::Value<cli::types::Float>;
template struct cli::Value<cli::types::Bool >;
template struct cli::Value<cli::types::Str  >;


cli::Parameter::Name::Name(
    char const*        const long_name,
    tl::optional<char> const short_name) noexcept
    : long_form  { long_name  }
    , short_form { short_name } {}


auto cli::Options_description::Option_adder::map_short_to_long(Parameter::Name const& name) noexcept -> void {
    if (name.short_form.has_value())
        self->long_forms.add_or_assign(*name.short_form, name.long_form);
}


auto cli::Options_description::Option_adder::operator()(
    Parameter::Name&&              name,
    tl::optional<std::string_view> description) noexcept -> Option_adder
{
    map_short_to_long(name);
    self->parameters.push_back({
        .name        = std::move(name),
        .description = description
    });
    return *this;
}

template <class T>
auto cli::Options_description::Option_adder::operator()(
    Parameter::Name&&              name,
    Value<T>&&                     value,
    tl::optional<std::string_view> description) noexcept -> Option_adder
{
    map_short_to_long(name);
    bool const is_defaulted = value.default_value.has_value();

    self->parameters.push_back({
        .name        = std::move(name),
        .values      { std::move(value) },
        .description = description,
        .defaulted   = is_defaulted
    });
    return *this;
}

auto cli::Options_description::Option_adder::operator()(
    Parameter::Name&&                 name,
    std::vector<Parameter::Variant>&& values,
    tl::optional<std::string_view>    description) noexcept -> Option_adder
{
    map_short_to_long(name);

    auto const has_default = [](auto const& variant) {
        return std::visit([](auto& alternative) {
            return alternative.default_value.has_value();
        }, variant);
    };

    bool is_defaulted = false;
    if (!values.empty()) {
        is_defaulted = has_default(values.front());
        auto rest = values | ranges::views::drop(1);

        if (is_defaulted)
            utl::always_assert(ranges::all_of(rest, has_default));
        else
            utl::always_assert(ranges::none_of(rest, has_default));
    }

    self->parameters.emplace_back(
        std::move(name),
        std::move(values),
        description,
        is_defaulted);
    return *this;
}

template auto cli::Options_description::Option_adder::operator()(Parameter::Name&&, Value<types::Int  >&&, tl::optional<std::string_view>) -> Option_adder;
template auto cli::Options_description::Option_adder::operator()(Parameter::Name&&, Value<types::Float>&&, tl::optional<std::string_view>) -> Option_adder;
template auto cli::Options_description::Option_adder::operator()(Parameter::Name&&, Value<types::Bool >&&, tl::optional<std::string_view>) -> Option_adder;
template auto cli::Options_description::Option_adder::operator()(Parameter::Name&&, Value<types::Str  >&&, tl::optional<std::string_view>) -> Option_adder;


namespace {
    template <class T>
    auto get_arg(cli::Options::Argument_proxy const& self) -> T const* {
        if (self.pointer) {
            switch (self.count) {
            case 0:
                utl::abort(
                    "Attempted to access value of non-existent "
                    "argument of nullary cli option --{}"_format(self.name));
            case 1:
                break;
            default:
                utl::abort(
                    "Attempted to access value of multi-argument "
                    "cli option --{} without indexing"_format(self.name));
            }

            if (T* const pointer = std::get_if<T>(self.pointer)) {
                return pointer;
            }
            else {
                utl::abort(
                    "Attempted to access a parameter of cli "
                    "option --{} as {}, but it is {}"_format(
                        self.name,
                        type_description<T>(),
                        type_description(*self.pointer)));
            }
        }
        else {
            return nullptr;
        }
    }
}


cli::Options::Argument_proxy::operator cli::types::Int const*() const {
    return get_arg<types::Int>(*this);
}
cli::Options::Argument_proxy::operator cli::types::Float const*() const {
    return get_arg<types::Float>(*this);
}
cli::Options::Argument_proxy::operator cli::types::Bool const*() const {
    return get_arg<types::Bool>(*this);
}
cli::Options::Argument_proxy::operator cli::types::Str const*() const {
    return get_arg<types::Str>(*this);
}


cli::Options::Argument_proxy::operator bool() const noexcept {
    return !empty;
}


auto cli::Options::Argument_proxy::operator[](utl::Usize const index) -> Argument_proxy {
    if (indexed)
        utl::abort("Attempted to index into an already indexed argument proxy");

    if (index < count) {
        return {
            .name    = name,
            .pointer = pointer + index,
            .count   = 1,
            .indexed = true,
            .empty   = false
        };
    }
    else {
        utl::abort(
            "The cli option --{} does not have a {} parameter"_format(
                name,
                utl::formatting::integer_with_ordinal_indicator(index + 1)));
    }
}


auto cli::Options::operator[](std::string_view const name) noexcept -> Argument_proxy {
    auto const it = ranges::find(named_arguments, name, &Named_argument::name);
    if (it != named_arguments.end()) {
        return {
            .name    = name,
            .pointer = it->values.data(),
            .count   = it->values.size(),
            .indexed = false,
            .empty   = false
        };
    }
    return { .empty = true };
}


DEFINE_FORMATTER_FOR(cli::Options_description) {
    std::vector<utl::Pair<std::string, tl::optional<std::string_view>>> lines;
    lines.reserve(value.parameters.size());
    utl::Usize max_length = 0;

    for (auto const& [name, arguments, description, _] : value.parameters) {
        std::string line;
        auto line_out = std::back_inserter(line);

        fmt::format_to(
            line_out,
            "--{}{}",
            name.long_form,
            name.short_form ? fmt::format(", -{}", *name.short_form) : "");

        for (auto const& argument : arguments) {
            fmt::format_to(line_out, " [{}]", std::visit([]<class T>(cli::Value<T> const& value) {
                return value.name.empty() ? type_description<T>() : value.name;
            }, argument));
        }

        max_length = std::max(max_length, line.size());
        lines.emplace_back(std::move(line), description);
    }

    auto out = context.out();

    for (auto& [names, description] : lines) {
        out = fmt::format_to(
            out,
            "\t{:{}}{}\n",
            names,
            max_length,
            description ? fmt::format(" : {}", *description) : "");
    }

    return out;
}

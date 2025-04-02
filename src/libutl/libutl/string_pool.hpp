#ifndef KIELI_LIBUTL_STRING_POOL
#define KIELI_LIBUTL_STRING_POOL

#include <libutl/utilities.hpp>
#include <libutl/index_vector.hpp>

namespace utl {

    struct String_id : utl::Vector_index<String_id, std::uint32_t> {
        using Vector_index::Vector_index;
    };

    class String_pool {
        using Hash = utl::Transparent_hash<std::string_view>;
        std::unordered_map<std::string, String_id, Hash, std::equal_to<>> m_map;
        utl::Index_vector<String_id, std::string>                         m_vec;
    public:
        [[nodiscard]] auto make(std::string owned) -> String_id;
        [[nodiscard]] auto make(std::string_view borrowed) -> String_id;
        [[nodiscard]] auto get(String_id id) const -> std::string_view;
    };

} // namespace utl

#endif // KIELI_LIBUTL_STRING_POOL

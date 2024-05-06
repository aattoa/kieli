#pragma once

namespace kieli::libhistory {
    auto add_to_history(char const*) -> void;
    auto read_history_file_to_active_history() -> void;
} // namespace kieli::libhistory

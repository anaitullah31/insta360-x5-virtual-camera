// Bilingual support: demos switch display language via --en/--cn (default Chinese).
// Usage: parse --en/--cn in the demo's main and set demo_common::lang(); wrap UI strings with L("中文","English").
// C++11 compatible: uses a function-local static instead of an inline variable.
#pragma once
#include <string>

namespace demo_common {

enum class Lang { CN, EN };

// Global language switch (function-local static, safe for a header-only inline).
inline Lang& lang() {
    static Lang l = Lang::CN;
    return l;
}

// Pick a word: returns cn in Chinese mode, en in English mode. Used for string literals.
inline const char* L(const char* cn, const char* en) {
    return lang() == Lang::CN ? cn : en;
}

// std::string version (for concatenation scenarios).
inline std::string Ls(const std::string& cn, const std::string& en) {
    return lang() == Lang::CN ? cn : en;
}

} // namespace demo_common

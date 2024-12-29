namespace detail {
    struct absolute_uri_rule_t {};
}

namespace regular {
    struct absolute_uri_rule_t {};
}

constexpr detail::absolute_uri_rule_t absolute_uri_rule = {};

constexpr regular::absolute_uri_rule_t regular_absolute_uri_rule = {};

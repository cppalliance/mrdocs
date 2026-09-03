// A comment whose first paragraph is only a styled span still has a brief:
// `\c char8_t.` is visible content even though none of it is plain text.

/// Kinds of character.
enum class CharKind {
    /// \c char8_t.
    Char8,
    /// Plain text.
    Plain,
    /// \c char16_t and more text.
    Char16,
    /// `code span` only.
    CodeOnly,
};

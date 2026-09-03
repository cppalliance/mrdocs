/// Hashes `Hash<unsigned char>` values.
///
/// Clang lexes `<u` followed by letters as an HTML start tag. With no end
/// tag it is prose, not markup, and the text is kept verbatim: names such
/// as "<unnamed>" or a comparison like `size <u VF * count` must render as
/// written.
void hash_unsigned();

/// A closed tag Mr.Docs does not render keeps its text, silently:
/// <u>underlined</u> words.
void unsupported_tag();

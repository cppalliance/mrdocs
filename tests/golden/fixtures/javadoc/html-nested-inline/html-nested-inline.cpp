/// Styled spans keep the inline markup they contain.
///
/// <em>If it returns a positive value, calls to @c underflow() will not
/// return @c traits::eof() until then.</em> Plain text follows.
///
/// <strong>Bold with <code>code</code> inside</strong>, then a
/// <a href="https://example.com">link to @c f() with code</a>, then
/// <sub>sub @c x</sub>, <sup>sup @c y</sup>, <mark>mark @c z</mark>
/// and <s>struck @c w</s>.
///
/// A tag Mr.Docs does not render keeps its content in place:
/// <u>under @c u() lined</u>.
void nested_inline();

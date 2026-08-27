// Merging distinguishes keyed sections from free-form prose. `@throws` names a
// symbol, so throws from different declarations are unioned (both kept). A
// `@return` is prose: a second one is not appended, the first is kept, since
// deduping open text is unreliable and appending would accumulate near
// duplicates.

/// Does something.
/// \return The first return description.
/// \throws error_a When A happens.
void act();

/// \return A different return description, not appended.
/// \throws error_b When B happens.
void act();

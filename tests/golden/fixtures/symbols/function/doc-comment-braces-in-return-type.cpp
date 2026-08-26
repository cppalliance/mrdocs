// Documentation comment association when the return type spells braces.
//
// Clang associates a doc comment with a declaration only when the source text
// between the end of the comment and the declaration's "representative
// location" contains none of the characters `;{}#@` (a crude heuristic meant to
// detect an intervening declaration or preprocessor directive):
//
//   clang/lib/AST/ASTContext.cpp, getRawCommentNoCacheImpl():
//       StringRef Text(Buffer + CommentEndOffset,
//                      LocDecomp.second - CommentEndOffset);
//       if (Text.find_last_of(";{}#@") != StringRef::npos)
//         return nullptr;
//
// For a FunctionDecl the representative location is the *identifier* location
// (getLocsForCommentSearch(), ASTContext.cpp: `BaseLocation = D->getLocation()`),
// so the scanned text includes the whole leading return type. A return type
// that spells `{}` -- e.g. `decltype(int{})`, or a dependent type such as
// `reference_t<Q{}, U{}>` -- trips the heuristic and Clang discards the comment.
// (Clang already special-cases TypedefDecl at that spot to use getBeginLoc()
// "to allow association across {} in `typedef struct X {} Y`", but does not do
// the same for functions.)
//
// MrDocs recovers the comment in getDocumentation() (src/mrdocs/AST/ClangHelpers.cpp)
// by retrying the lookup anchored at the declaration's begin location, before
// the return type. Both functions below are therefore documented; if
// `braces_in_return_type` becomes undocumented again, that workaround regressed.

/// Documented. Clang drops this comment (braces in the return type); MrDocs
/// recovers it via the begin-location fallback.
decltype(int{}) braces_in_return_type();

/// Documented. Nothing but the return type `int` sits between the comment and
/// the name, so Clang associates it without help.
int plain_return_type();

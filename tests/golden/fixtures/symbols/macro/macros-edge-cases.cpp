#define FIRST_THING_IN_FILE 1

// Edge cases for macro extraction.
//
//  - `FIRST_THING_IN_FILE`'s definition starts with the
//    first token in the file: there is no preceding comment
//    of any kind.
//
//  - `REDEFINED_MACRO` is defined twice (with an `#undef`
//    in between) so the corpus ends up with two macro
//    symbols that share a name.
//
//  - `DOC_THEN_BLANK` is preceded by a doc comment with
//    one blank line in between.
//
//  - `SAME_LINE` has its doc comment on the same line as
//    the directive.

#define REDEFINED_MACRO 100
#undef REDEFINED_MACRO
#define REDEFINED_MACRO 200

/// Documentation for `DOC_THEN_BLANK`; one blank line
/// separates this comment from the macro definition.

#define DOC_THEN_BLANK 1

/** Doc-comment that ends on the macro's line. */ #define SAME_LINE 2

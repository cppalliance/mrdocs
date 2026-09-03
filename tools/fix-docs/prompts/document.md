# Documentation batch: one file

Mr.Docs extracted this project and produced a work list of symbols whose
documentation is missing or broken. This batch covers every problem in one
file, listed below. Fix all of them in a single pass over the file. Don't run
Mr.Docs. The list is already complete for this file.

## File

`{file}`

## Rules

- Write doc comments in the project's existing comment style. Look at a
  well-documented symbol in the same file or a sibling file first and imitate
  it: comment markers, tag vocabulary, indentation, line width.
- A brief is one sentence, roughly 4 to 160 characters, that says what the
  symbol is or does. Details, parameter lists, return values, and examples go
  after the brief, not inside it.
- "undocumented" on a function or enum means write the whole comment: brief,
  every parameter, and every enum value. "missing param doc" and
  "undocumented enum value" call out the specific gaps on a symbol that is
  otherwise documented.
- "anomalous-brief" means the text picked up as the brief was never meant as
  one, or it's just a bad brief: a description paragraph, a banner, a stray
  fragment. Write a real one-sentence brief. Keep the original text as the
  description when it carries information, and delete it when it's decoration.
- "missing return doc" means the function returns a value and the comment has
  no `@return`. Add one sentence saying what is returned. Prose such as
  "Return the ..." in the description does not count; Mr.Docs only sees the
  tag.
- "documented parameter(s) that do not exist" means an `@param` names
  something that is not a parameter of the declaration. Rename it to the
  real parameter, or remove it when the parameter is gone.
- Macro tasks work like function tasks: every named macro parameter needs an
  `@param`, the variadic list is optional.
- A namespace task is about the namespace as a whole, not this file. The
  namespace is reopened in many files, Mr.Docs merges the comment attached to
  every reopening, and the first one wins. So the brief must describe what
  the whole namespace holds across the project (look at sibling directories
  if you need to), never what this one file adds to it. When a task marks a
  reopening as non-canonical, do not write a namespace brief there: if the
  comment attached to it describes the declaration right after the opening
  brace, move it inside the namespace onto that declaration; if it is a
  section banner or decoration, delete it. Only the canonical reopening keeps
  the namespace's own one-sentence brief.
- "also documented at" lists the other declarations of the same symbol that
  carry a comment. Mr.Docs merges them, first one wins, so fixing only this
  copy leaves the stale text free to win again. Fix the copy in this file as
  the task says; the other files get their own batch.
- Line numbers below are hints from before any edits, so locate each symbol by
  its name and signature. Working from the bottom of the file upward keeps
  the remaining hints accurate.
- Only edit documentation comments. The one exception: when a task says a
  function has unnamed parameters, give each one a name in the declaration so
  it can be documented, then document it. That is the only code edit allowed.
- When you are done, reply with a one-line summary: how many symbols you
  documented and how many briefs you rewrote.

## Work-list for this file

{tasks}

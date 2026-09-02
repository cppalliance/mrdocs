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

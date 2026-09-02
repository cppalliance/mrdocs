# Documentation repair batch: Mr.Docs warnings

Mr.Docs ran a strict documentation check and produced the diagnostics below.
Fix every problem in this batch so these diagnostics go away. Don't run
Mr.Docs yourself. The driver re-runs it after all batches are done.

## How the check was invoked

```text
{command}
```

## Rules

- Each block starts with `path:line:col:` and lists numbered problems at that
  location, usually with a source snippet.
- Match the project's existing documentation style: comment markers, tag
  vocabulary, indentation, line width.
- Only edit documentation comments and, when a diagnostic points at the
  Mr.Docs configuration, the config. Never change code.
- This is batch {batch_index} of {batch_count} from one check run. Other
  batches cover other locations, so stay inside the ones listed here.
- When you are done, reply with a one-line summary of what you fixed.

## Diagnostics

{report}

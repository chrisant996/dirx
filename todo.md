# FEATURES

### BUGS

### Rethink and simplify
- Keep CMD DIR options for compatibility.
- Fix default date format to match CMD DIR.
- Built in date formats like eza (since they're mostly ISO based).
- Remove or simplify some niche options.
- Maybe `/t` shouldn't default to showing ALL attributes; maybe `/tt` (or `/t /t`) can show ALL attributes (or vice versa)?
- Be more user friendly like eza.

### Git integration
- Ignore files per `.gitignore`.
- Git status column for files.  Be very careful about performance!

### Maybe
- Ability to choose which file attributes to list, without needing to use format pictures?
- Attribute colors?
- Combine "d" and "l" attributes into same column?
- Configurable icon mappings?
- Tree view?  The vertical extender lines can be tracked with a "more" bit per directory component.
- Option to show column headers?  Format pictures make it difficult.

# DOCUMENT

- `--color-mode` and `--color-mode=WHEN` (_WHEN_ is `never`, `always`, or `auto`).
- `--hide-dot-files` and `--no-hide-dot-files`, and also their interaction with `-a` and `-a-`.
- `--hyperlinks` and `--no-hyperlinks`.
- `%DIRX_COLORS%`.


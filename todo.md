# FEATURES

### BUGS
- `dirx ~\ -a --classify` asserts because the width didn't properly account for the classification character; maybe it's a mismatch between truncation and classify?

### Rethink and simplify
- Be more user friendly like eza.
- Maybe `/t` shouldn't default to showing ALL attributes; maybe `/tt` (or `/t /t`) can show ALL attributes (or vice versa)?
- Built in date formats like eza (since they're mostly ISO based).
- Need a single-letter flag way to turn on `--color-scale`.

### Git integration
- Ignore files per `.gitignore`.
- Git status column for files.  Be very careful about performance!

### Maybe
- The `readme*` underline might be annoying to some people; try to find a simple way to supersede it, while still making it able to automatically attach to other colors.  I may want to use a similar principle in other cases as well, so ideally there needs to be a generalized way to supersede "attaching" color effects.
- Something like `%EZA_GRID_ROWS%`?
- Ability to choose which file attributes to list, without needing to use format pictures?
- Attribute colors?
- Combine "d" and "l" attributes into same column?
- Configurable icon mappings?
- Tree view?  The vertical extender lines can be tracked with a "more" bit per directory component.
- Option to show column headers?  Format pictures make it difficult.


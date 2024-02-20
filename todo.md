# FEATURES

### BUGS

### Rethink and simplify
- `-L`, `--level` to limit depth of recursion.
- Be more user friendly like eza; but CMD DIR familiarity competes with that goal.
- Built in date formats like eza (since they're mostly ISO based).
- Document the `--nix` flag once it's more polished.
- Default colors.

### Git integration
- Ignore files per `.gitignore`.
- Git status column for files.  Be very careful about performance!

### Maybe
- Maybe a couple different presets of default colors to choose from?  E.g. attribute-based, and file-type-based?
- Post-process column computation to reduce rows by shifting items into a new
  column?  `ls` does not, but `eza` does.
- Configurable icon mappings?
- Tree view?  The vertical extender lines can be tracked with a "more" bit per directory component.
- Option to show column headers?  Format pictures make it difficult.


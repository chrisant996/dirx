# FEATURES

### BUGS

### Rethink and simplify
- `-L`, `--level` to limit depth of recursion.
- Be more user friendly like eza; but CMD DIR familiarity competes with that goal.
- Built in date formats like eza (since they're mostly ISO based).
- Messy:  `-Z --no-size` and `--size -Z-` don't have an intuitive outcome.
- Document the `--nix` flag once it's more polished.

### Git integration
- Ignore files per `.gitignore`.
- Git status column for files.  Be very careful about performance!

### Maybe
- Post-process column computation to reduce rows by shifting items into a new
  column?  `ls` does not, but `eza` does.
- Configurable icon mappings?
- Tree view?  The vertical extender lines can be tracked with a "more" bit per directory component.
- Option to show column headers?  Format pictures make it difficult.


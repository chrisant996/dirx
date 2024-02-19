# FEATURES

### BUGS
- Why is dirx using 3 columns in \wbin\clink but eza uses 4?
- In nix mode, group adjacent patterns in same directory AND sort them all together.
- In Win mode, show separate dir header per pattern, and don't group them.
  - And have an option to group them like nix mode does.

### Rethink and simplify
- Be more user friendly like eza; but CMD DIR familiarity competes with that goal.
- Built in date formats like eza (since they're mostly ISO based).
- Messy:  `-Z --no-size` and `--size -Z-` don't have an intuitive outcome.
- Document the `--nix` flag once it's more polished.
- `-L`, `--level` to limit depth of recursion.

### Git integration
- Ignore files per `.gitignore`.
- Git status column for files.  Be very careful about performance!

### Maybe
- Configurable icon mappings?
- Tree view?  The vertical extender lines can be tracked with a "more" bit per directory component.
- Option to show column headers?  Format pictures make it difficult.


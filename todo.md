# FEATURES

### BUGS
- The git (or git-repo) column should be omitted if the directory isn't in a repo (or isn't a repo root).

### Next
- Size unit colors.
- Short directory header color.
- An option to use short directory header (without needing to use --nix).
- Write README.md file as documentation.
- Document the `--nix` flag once it's more polished (also find a better name?).

### Back burner
- Debugging output.
- Variable width relative time format with longer unit names?
- Default colors.  Maybe a few different preset options; basic, attributes, file types?
- Tree view?  The vertical extender lines can be tracked with a "more" bit per directory component.
- Unit tests.
- Post-process column computation to reduce rows by shifting items into a new column?  `ls` does not, but `eza` does.  I'm not sure I like the effect on the list; it makes it easy to lose track of the final column since it can end up having only a few files instead of at least ((N/COLS)-COLS) files.

### Future; maybe
- Configurable icon mappings?
- Option to show column headers?


# FEATURES

### BUGS

### Next
- Option to autofit all columns to their content (even size).
  - Rewrite to autofit the field itself, simply based on its width being zero.
  - Then it can work for relative dates and also any other field.
  - Caveat: if a field pads with spaces then the spaces count toward the width, so fields that want to crop spaces will need to specially handle formatting differently during the measure phase and the format phase.
  - I don't like the --compact-columns option name.
- Write README.md file as documentation.
- Document the `--nix` flag once it's more polished (also find a better name?).

### Back burner
- Debugging output.
- Default colors.  Maybe a few different preset options; basic, attributes, file types?
- Tree view?  The vertical extender lines can be tracked with a "more" bit per directory component.
- Unit tests.
- Post-process column computation to reduce rows by shifting items into a new column?  `ls` does not, but `eza` does.  I'm not sure I like the effect on the list; it makes it easy to lose track of the final column since it can end up having only a few files instead of at least ((N/COLS)-COLS) files.

### Future; maybe
- Configurable icon mappings?
- Option to show column headers?


# FEATURES

### BUGS

### Back burner
- Default colors.  Maybe a few different preset options; basic, attributes, file types?
- Unit tests.  Especially things like,
  - `dirx \\\\` --> The specified path is invalid.
  - `dirx 9:\` --> The system cannot find the path specified.
  - `dirx \\` --> The filename, directory name, or volume label syntax is incorrect.
  - etc.

### Future; maybe
- Configurable icon mappings?
- Option to show column headers?
- Post-process column computation to reduce rows by shifting items into a new column?  `ls` does not, but `eza` does.  I'm not sure I like the effect on the list; it makes it easy to lose track of the final column since it can end up having only a few files instead of at least ((N/COLS)-COLS) files.
- Refactor width fitting so it can be deferred, and create an (MAXCOLS^2)/2 triangle of picture formatters?  It would use around 600KB of memory.  I'm not convinced the layout benefits are worth the performance costs or development costs.


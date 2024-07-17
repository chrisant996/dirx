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

### Punt; known issues

- Wrapping with a background color can end up with the rest of the line filled with the background color, because of how the Windows console subsystem works.  E.g. can repro using `dirx --nix --icons=never \Windows\WinSxS\ --width 1` with a background color set for `di` in `%DIRX_COLORS%`.  Currently the field format functions don't return whether they printed any background colors, but if they did then `CSI K` could be appended to the line to compensate.  But I don't think that scenario is worth the general case performance cost!

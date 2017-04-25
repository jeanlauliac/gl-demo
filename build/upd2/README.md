# upd

Update files, fast.

## Description

`upd` is a command-line utility that automatically determines what files in
a directory need to be updated as resulting from the transformation of other
files, and issue the commands to update them. In that regard it is similar
to `make(1)` or `ninja` utilities. `upd` focuses on convenience and
performance. It can automatically identify sets of files based on
globbing patterns. It can automatically track transitive dependencies of a
file to be updated.

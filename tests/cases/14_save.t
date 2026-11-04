# save() and transcript() write files; see tests/cli_tests.py for their
# contents. Here: the errors.
=== strings are only file names
"abc"
"abc" + 1
x = "abc"
---
error: line 1: strings can only be used as file names in save() and transcript()
error: line 2: strings can only be used as file names in save() and transcript()
error: line 3: strings can only be used as file names in save() and transcript()

=== save needs a string
save(work)
save()
1 + save("a")
---
error: line 1: save: the file name must be a string, like save("work.xyz")
error: line 2: save: expected 1 argument
error: line 3: save() can only be used as a statement on its own

=== unterminated string
save("abc
2
---
error: line 1: unterminated string
2

=== cannot write
save("/nonexistent-dir/x.xyz")
---
error: line 1: cannot write the file '/nonexistent-dir/x.xyz'

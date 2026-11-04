=== syntax errors are reported and processing continues
1+
2
(3
)
4)
5
---
error: line 1: syntax error: expected a number, name or '(', found end of statement
2
3
error: line 5: syntax error: expected end of statement, found ')'
5

=== exit status is 1 after an error
1/0
2
--- exit 1
error: line 1: division by zero
2

=== exit status is 0 without errors
1+1
--- exit 0
2

=== bad characters
3 $ 4
5
---
error: line 1: unexpected character '$'
5

=== unterminated comment
1
/* never closed
2
---
1
error: line 2: unterminated comment

=== assignment needs a name
3=4
x+1=2
---
error: line 1: can only assign to a name, or define a function such as f(x)
error: line 2: can only assign to a name, or define a function such as f(x)

# latex(e) prints the value of e as LaTeX math.
=== fractions, powers and roots
latex(x^2/4+x/3+1/9)
latex(3/(4*(x+y)^2))
latex(-1/x^2)
latex(2^(1/2)/3)
latex(sqrt(8))
latex(x^(1/3))
latex(x^(2/3))
latex(2*3^x)
latex((x-1)*(x+1)^2)
latex(-x/2)
---
\frac{x^{2}}{4}+\frac{x}{3}+\frac{1}{9}
\frac{3}{4 \left(x+y\right)^{2}}
-\frac{1}{x^{2}}
\frac{\sqrt{2}}{3}
2 \sqrt{2}
\sqrt[3]{x}
x^{\frac{2}{3}}
2 \cdot 3^{x}
\left(x-1\right) \left(x+1\right)^{2}
-\frac{x}{2}

=== names and functions
latex(alpha_1^2 + beta*gamma + abc + x_max)
latex(Re(x^(1/2)) + abs(x) + conj(z^(1/2)))
latex(1/2-im/2)
latex(num(1/3+2im/3, 3))
---
\alpha_{1}^{2}+\beta \gamma+\mathrm{abc}+x_{\mathrm{max}}
\operatorname{Re}\left(\sqrt{x}\right)+\left|x\right|+\overline{\sqrt{z}}
\frac{1}{2}-\frac{i}{2}
0.333+0.666\,i

=== stored formulas with comparisons
c := x < 3 && y != 2;
latex(1)
---
1

=== latex is a statement
1 + latex(x)
---
error: line 1: latex() can only be used as a statement on its own

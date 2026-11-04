# Licenses

xyz is licensed under the GNU General Public License, version 3 or (at
your option) any later version. See `../LICENSE` for the full text.

The `xyz` binary is statically linked with **bcl**, the math library of
Gavin D. Howard's `bc` (https://github.com/gavinhoward/bc). bc is
not part of this repository; it is built from a separate checkout
(`BC_DIR`, default `../bc`).

The parts of bc that are linked into `xyz` (`src/data.c`, `src/library.c`,
`src/num.c`, `src/rand.c`, `src/vector.c`, `src/vm.c`) are under:

* the BSD 2-Clause License, Copyright (c) 2018-2026 Gavin D. Howard and
  contributors, and
* `src/rand.c` also under the MIT License, Copyright (c) 2014-2017 Melissa
  O'Neill and PCG Project contributors.

When GNU readline is available at build time, `xyz` is also linked with
it (dynamically) for line editing. GNU readline is licensed under the GNU
GPL version 3 or later, the same as xyz. On systems where the readline
interface is provided by libedit (BSD license), that is used instead.

The bc licenses are compatible with the GPL. They require their copyright
notices and license texts to be kept with source and binary
distributions. The full texts are in `bc-LICENSE.md` (a verbatim copy of
bc's `LICENSE.md`), and bc's contributor list is in `bc-NOTICE.md`.
Anyone who distributes `xyz` binaries must ship this directory with them.

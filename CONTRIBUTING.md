# Contributing

Thanks for helping! This project prioritises **physical and numerical correctness**.

1. Open an issue before large changes.
2. Every new force term, integrator or scenario needs a validation test against an analytic
   result, a published benchmark, or a conservation law (cite the source in `docs/physics.md`).
3. Build and test: `cmake -S . -B build && cmake --build build && ctest --test-dir build`.
4. Never enable `-ffast-math`; the tests depend on IEEE semantics.
5. Keep `core/` free of any web or graphics dependency.
6. Add a CHANGELOG entry and keep the units (AU, day, solar mass) explicit in code and docs.

By contributing you agree your work is released under the MIT license.

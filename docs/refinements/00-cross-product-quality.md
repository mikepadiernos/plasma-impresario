# Cross-product quality refinement

This section covers the overall quality bar for the cross-product refinement milestone. It defines the product-level acceptance criteria that apply across all mail workflows and companion services.

## Focus

- Review end-to-end mail and available productivity workflows and prioritize remaining friction
- Refine cross-feature usability, accessibility, keyboard navigation, and visual consistency
- Tune performance, reliability, and resource usage across realistic workloads
- Validate provider, desktop, Nextcloud, and any available Plasma Opus integration behavior together
- Resolve refinement findings and document remaining limitations and acceptance results
- Compare supported workflows with the inspiration principles and prioritize gaps without assuming feature parity
- Record reproducible acceptance results and regression coverage for each refinement area
- Inventory implemented workflows and map their entry points to automated or reproducible manual checks
- Benchmark cold and warm startup, first interactive window, and settings readiness; record the environment and compare improvements against the baseline
- Exercise successful operations, cancellation, partial failure, recovery, and persistence with isolated test data and synthetic credentials
- Review accessibility, translations, display scaling, optional dependencies, and representative desktop configurations; record environments not tested
- Run suitable static analysis and dependency checks and document security findings and residual risks without claiming testing proves security
- Use C++ Core Guidelines and project-specific KDE/Qt conventions for findings
- Evaluate open static analyzers and PVS-Studio only where licensing and host support allow
- Establish reproducible formatting, warning, test coverage, and dynamic-analysis commands
- Use AddressSanitizer and UndefinedBehaviorSanitizer for supported desktop configurations
- Use ThreadSanitizer for targeted concurrency investigations where practical

## Acceptance criteria

- The product feels coherent across the entire desktop workflow, not just in isolated features.
- Cross-feature friction is reduced and comparable across primary mail tasks.
- Quality is measured through realistic workloads, not just compile and smoke test success.
- Remaining limitations are documented with clear acceptance and validation notes.
- The app remains honest about what has and has not been verified across environments.

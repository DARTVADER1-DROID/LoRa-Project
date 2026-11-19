# Firmware code formatting

The repository uses **clang-format** with the root `.clang-format` policy.

## Format all firmware sources

```sh
sh firmware/campus_assistance/scripts/format.sh
```

## Verify without changing files

```sh
sh firmware/campus_assistance/scripts/check-format.sh
```

## Format automatically before every local commit

Install clang-format, then enable the tracked hook once per clone:

```sh
git config core.hooksPath .githooks
chmod +x .githooks/pre-commit
```

The pre-commit hook formats staged firmware C++ headers, sources and Arduino sketches,
then re-stages the formatted files. GitHub Actions independently rejects unformatted
changes and compiles the public C++11 contract headers.

The hook is opt-in per clone because Git does not automatically activate repository
hooks for security reasons. Use two-space indentation, one declaration per line and
the configured 88-column limit. Do not hand-align code with spaces; let clang-format
own layout.

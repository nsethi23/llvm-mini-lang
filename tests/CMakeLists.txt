# Smoke test: the driver binary builds and runs.
add_test(NAME mlang_smoke_test COMMAND mlang)

# Unit tests (GoogleTest) for individual pipeline stages -- lexer/parser/etc.
add_subdirectory(unit)

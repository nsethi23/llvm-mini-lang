# M0 scope: a single smoke test that the driver binary builds and runs.
# Unit tests (Catch2/GoogleTest) for the lexer/parser/etc. land starting M1,
# alongside the components they cover -- see tests/unit/ and tests/golden/.
add_test(NAME mlang_smoke_test COMMAND mlang)

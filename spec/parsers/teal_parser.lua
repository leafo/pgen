-- the Teal grammar lives in examples/, this shim lets the test suite and the
-- spec parser Makefile rules pick it up like any other test grammar
return require "examples.teal_parser"

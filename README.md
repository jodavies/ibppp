# ibppp

[![Test](https://github.com/jodavies/ibppp/actions/workflows/tests.yml/badge.svg)](https://github.com/jodavies/ibppp/actions/workflows/tests.yml)

Post process IBP reduction tables, for use with FORM.

Currently, `ibppp` can:
 - read gzipped FIRE tables, in the original format
 - write out gzipped FORM fill statements for a tablebase

TODO
 - unit tests, malformed input tests
 - read Kira tables
 - read FIRE tables in reversed format
 - read/write uncompressed files (based on extension?)
 - more customisation re: output format

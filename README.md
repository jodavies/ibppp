# ibppp

[![Test](https://github.com/jodavies/ibppp/actions/workflows/tests.yml/badge.svg)](https://github.com/jodavies/ibppp/actions/workflows/tests.yml)

Post process IBP reduction tables, for use with FORM.

Currently, `ibppp` can:
 - read gzipped FIRE tables, in the original format
 - read gzipped Kira tables, produced with Fermat or Firefly
 - write out gzipped FORM fill statements for a tablebase
 - expand master-integral coefficients in ep

TODO
 - unit tests, malformed input tests
 - read FIRE tables in reversed format
 - read/write uncompressed files (based on extension?)
 - more customisation re: output format

# Third-party dependencies

This project vendors these upstream libraries:

- mbedTLS: `mbedtls-3.6.4` (`c765c83`)
  - source: https://github.com/Mbed-TLS/mbedtls
  - local path: `third_party/mbedtls`
- usrsctp: default branch snapshot (`fd070e0`)
  - source: https://github.com/sctplab/usrsctp
  - local path: `third_party/usrsctp`

Notes:
- Both libraries are built as static libraries through `add_subdirectory()`.
- Upstream licenses are kept in each library directory.

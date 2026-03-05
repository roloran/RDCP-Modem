# SchnorrSig library on nRF52480

How-to by @mgrabatin

- Download [mbedtls](https://github.com/Mbed-TLS/mbedtls/releases) version 3.6.5 (or most recent 3.x.y release) and extract files to `lib/mbedtls`.
- Create `lib/mbedtls/library.json` with the contents below.
- Create `include/mbedtls_config.h` with the contents below.
- Change `#error` to `#warning` in `lib/mbedtls/library/entropy_poll.c:35` and `lib/mbedtls/library/platform_util.c:261`.

`include/mbedtls_config.h`

```h 
#pragma once
#define MBEDTLS_AES_C
#define MBEDTLS_BIGNUM_C
#define MBEDTLS_CTR_DRBG_C
#define MBEDTLS_ECP_C
#define MBEDTLS_ECP_CURVE_LIST
#define MBEDTLS_ECP_DP_SECP256R1_ENABLED
#define MBEDTLS_ECP_NIST_OPTIM
#define MBEDTLS_ENTROPY_C
#define MBEDTLS_ENTROPY_HARDWARE_ALT
#define MBEDTLS_MD5_C
#define MBEDTLS_MD_C
#define MBEDTLS_MD_CAN_MD5
#define MBEDTLS_MD_CAN_RIPEMD160
#define MBEDTLS_MD_CAN_SHA1
#define MBEDTLS_MD_CAN_SHA256
#define MBEDTLS_MD_CAN_SHA3_256
#define MBEDTLS_MD_CAN_SHA512
#define MBEDTLS_MD_WRAP_C
#define MBEDTLS_NO_PLATFORM_ENTROPY
#define MBEDTLS_PLATFORM_C
#define MBEDTLS_RIPEMD160_C
#define MBEDTLS_SHA1_C
#define MBEDTLS_SHA256_C
#define MBEDTLS_SHA3_C
#define MBEDTLS_SHA512_C
#define MBEDTLS_TIMING_ALT
```

`lib/mbedtls/library.json`
```json
{
  "name": "mbedtls-vendored",
  "version": "0.0.1",
  "build": {
    "includeDir": "include",
    "srcDir": "library",
    "srcFilter": [
      "+<aes.c>",
      "+<bignum.c>",
      "+<bignum_core.c>",
      "+<constant_time.c>",
      "+<ecp.c>",
      "+<ecp_curves.c>",
      "+<ctr_drbg.c>",
      "+<entropy.c>",
      "+<md.c>",
      "+<md_wrap.c>",
      "+<sha256.c>",
      "+<sha512.c>",
      "+<sha1.c>",
      "+<md5.c>",
      "+<ripemd160.c>",
      "+<sha3.c>",
      "+<platform.c>",
      "+<platform_util.c>",
      "+<entropy_poll.c>"
    ]
  }
}
```

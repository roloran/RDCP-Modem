#include "rdcp-modem-crypto.h"
#include "rdcp-modem-serial.h"
#include "rdcp-modem-lora-settings.h"

SchnorrSigCtx ssc;
bool ssc_initialized = false;
SchnorrSigVerify ssv = SchnorrSigVerify();
bool ssv_initialized = false;

extern device_config cfg; 

bool encrypt_aes256gcm(uint8_t *plaintext, size_t ptsize, uint8_t *adata, size_t adatasize, uint8_t *key, size_t keysize, uint8_t *iv, size_t ivsize, uint8_t *out_ciphertext, uint8_t *out_tag, size_t tagsize) 
{
  GCM<AES256> gcm;
  gcm.setKey(key, keysize);
  gcm.setIV(iv, ivsize);
  gcm.addAuthData(adata, adatasize);
  gcm.encrypt(out_ciphertext, plaintext, ptsize);
  gcm.computeTag(out_tag, tagsize);
  return true;
}

bool decrypt_aes256gcm(uint8_t *ciphertext, size_t csize, uint8_t *adata, size_t adatasize, uint8_t *key, size_t keysize, uint8_t *iv, size_t ivsize, uint8_t *tag, size_t tagsize, uint8_t *out_plaintext) 
{
  GCM<AES256> gcm;
  gcm.setKey(key, keysize);
  gcm.setIV(iv, ivsize);
  gcm.addAuthData(adata, adatasize);
  gcm.decrypt(out_plaintext, ciphertext, csize);
  if (!gcm.checkTag(tag, tagsize)) return false;
  return true;
}

bool schnorr_init_ctx(void)
{
  if (ssc_initialized) return true;

  size_t seed_len = MBEDTLS_CTR_DRBG_MAX_SEED_INPUT - MBEDTLS_CTR_DRBG_ENTROPY_LEN;
  uint8_t seed[seed_len];    
#if defined(ESP32)
  esp_fill_random(seed, seed_len);
#else
  for (size_t i=COUNT_ZERO; i<seed_len; i++) seed[i] = radio_random_byte();
#endif

  ssc = SchnorrSigCtx();
  int res = ssc.init(seed, seed_len);
  if (res != RESULT_OK)
  {
    serial_writeln("ERROR: schnorr_init_ctx() failed");
    return false;
  }

  ssc_initialized = true;
  return true;
}

int schnorr_create_signature(uint8_t *data, uint8_t datalen, uint8_t *targetbuffer)
{
  schnorr_init_ctx();

  SchnorrSigSign sss = SchnorrSigSign();
  int res = sss.init(&ssc, (char*) cfg.rdcp_myprivkey);
  if (res != RESULT_OK)
  {
    char buf[LONGINFOLEN];
    snprintf(buf, LONGINFOLEN, "ERROR: schnorr_create_signature() could not initialize with private key %s", cfg.rdcp_myprivkey);
    serial_writeln(buf);
    return RESULT_FAIL;
  }

  struct SchnorrSigCtx::signature new_sig;
  res = sss.sign((const unsigned char*)data, datalen, &new_sig);
  if (res != RESULT_OK)
  {
    serial_writeln("ERROR: schnorr_create_signature() signing failed");
    return RESULT_FAIL;
  }

  int tbi = COUNT_ZERO;
  for (int i=COUNT_ZERO; i < new_sig.point_len; i++) targetbuffer[tbi++] = new_sig.point[i];
  for (int i=COUNT_ZERO; i < new_sig.sig_len  ; i++) targetbuffer[tbi++] = new_sig.sig[i];

  return tbi;
}

bool schnorr_verify_signature(uint8_t *data, uint8_t datalen, uint8_t *signature)
{
  schnorr_init_ctx();
  int res;

  if (ssv_initialized == false)
  {
    res = ssv.init(&ssc, (char*) cfg.rdcp_hqpubkey);
                               
    if (res != RESULT_OK)
    {
      char msg[LONGINFOLEN];
      snprintf(msg, LONGINFOLEN, "ERROR: schnorr_verify_signature() could not initialize (res %d) with HQ public key %s", res, cfg.rdcp_hqpubkey);
      serial_writeln(msg);
      return false;
    }

    ssv_initialized = true;
  }

  struct SchnorrSigCtx::signature sig;
  sig.point_len = POINT_LEN;
  for (int i=COUNT_ZERO; i<POINT_LEN; i++) sig.point[i] = signature[i];
  sig.sig_len = SIG_LEN;
  for (int i=COUNT_ZERO; i<SIG_LEN; i++) sig.sig[i] = signature[i+POINT_LEN];

  res = ssv.verify((const unsigned char*)data, datalen, &sig);
  if (res == RESULT_OK) return true;

  return false;
}

/* EOF */
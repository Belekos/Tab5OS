/* Tab5OS compatibility backport, 0.8.28.
 * Base: ESP-IDF 735507283d/components/esp_security/src/esp_crypto_lock.c
 * Source SHA-256: 5ec2287139419de96af04c40afdff39829655d3b67893f2bdef8096070068c2b
 * Backport: ECDSA SHA/AES lock hunk from espressif/esp-idf 7fc2aaf.
 * Complete lock module: all crypto users share the same lock instances.
 * The sketch object satisfies SDK lock symbols, so the archive copy is not
 * extracted. No custom linker option is required in Arduino IDE builds.
 * Restrict this compatibility code to the verified ESP32-P4 / IDF 5.5.4 SDK;
 * newer SDKs and other targets retain their own implementation unchanged.
 */
#include <sdkconfig.h>
#include <esp_idf_version.h>
#if defined(CONFIG_IDF_TARGET_ESP32P4) && CONFIG_IDF_TARGET_ESP32P4 && \
    ESP_IDF_VERSION == ESP_IDF_VERSION_VAL(5, 5, 4)
/*
 * SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <sys/lock.h>

#include "esp_crypto_lock.h"

/* Lock overview:
SHA: peripheral independent, but DMA is shared with AES
AES: peripheral independent, but DMA is shared with SHA
MPI/RSA: independent
ECC: independent
HMAC: needs SHA
DS: needs HMAC (which needs SHA), AES and MPI
ECDSA: needs ECC, SHA/AES and MPI; its reset also holds SHA in reset
*/

#if !NON_OS_BUILD
#ifdef SOC_DIG_SIGN_SUPPORTED
/* Lock for DS peripheral */
static _lock_t s_crypto_ds_lock;
#endif /* SOC_DIG_SIGN_SUPPORTED */

#ifdef SOC_HMAC_SUPPORTED
/* Lock for HMAC peripheral */
static _lock_t s_crypto_hmac_lock;
#endif /* SOC_HMAC_SUPPORTED */

#ifdef SOC_MPI_SUPPORTED
/* Lock for the MPI/RSA peripheral, also used by the DS peripheral */
static _lock_t s_crypto_mpi_lock;
#endif /* SOC_MPI_SUPPORTED */

#if defined(SOC_SHA_SUPPORTED) || defined(SOC_AES_SUPPORTED)
/* Single lock for SHA and AES, sharing a reserved GDMA channel */
static _lock_t s_crypto_sha_aes_lock;
#endif /* defined(SOC_SHA_SUPPORTED) || defined(SOC_AES_SUPPORTED) */

#ifdef SOC_ECC_SUPPORTED
/* Lock for ECC peripheral */
static _lock_t s_crypto_ecc_lock;
#endif /* SOC_ECC_SUPPORTED */

#ifdef SOC_ECDSA_SUPPORTED
/* Lock for ECDSA peripheral */
static _lock_t s_crypto_ecdsa_lock;
#endif /* SOC_ECDSA_SUPPORTED */

#ifdef SOC_KEY_MANAGER_SUPPORTED
/* Lock for Key Manager peripheral */
static _lock_t s_crypto_key_manager_lock;
#endif /* SOC_KEY_MANAGER_SUPPORTED */

#ifdef SOC_HMAC_SUPPORTED
void esp_crypto_hmac_lock_acquire(void)
{
    _lock_acquire(&s_crypto_hmac_lock);
    esp_crypto_sha_aes_lock_acquire();
}

void esp_crypto_hmac_lock_release(void)
{
    esp_crypto_sha_aes_lock_release();
    _lock_release(&s_crypto_hmac_lock);
}
#endif /* SOC_HMAC_SUPPORTED */

#ifdef SOC_DIG_SIGN_SUPPORTED
void esp_crypto_ds_lock_acquire(void)
{
    _lock_acquire(&s_crypto_ds_lock);
    esp_crypto_hmac_lock_acquire();
    esp_crypto_mpi_lock_acquire();
}

void esp_crypto_ds_lock_release(void)
{
    esp_crypto_mpi_lock_release();
    esp_crypto_hmac_lock_release();
    _lock_release(&s_crypto_ds_lock);
}
#endif /* SOC_DIG_SIGN_SUPPORTED */

#if defined(SOC_SHA_SUPPORTED) || defined(SOC_AES_SUPPORTED)
void esp_crypto_sha_aes_lock_acquire(void)
{
    _lock_acquire(&s_crypto_sha_aes_lock);
}

void esp_crypto_sha_aes_lock_release(void)
{
    _lock_release(&s_crypto_sha_aes_lock);
}
#endif /* defined(SOC_SHA_SUPPORTED) || defined(SOC_AES_SUPPORTED) */

#if defined(SOC_SHA_CRYPTO_DMA) || defined(SOC_AES_CRYPTO_DMA)
void esp_crypto_dma_lock_acquire(void)
{
    _lock_acquire(&s_crypto_sha_aes_lock);
}

void esp_crypto_dma_lock_release(void)
{
    _lock_release(&s_crypto_sha_aes_lock);
}
#endif /* defined(SOC_SHA_CRYPTO_DMA) || defined(SOC_AES_CRYPTO_DMA) */

#ifdef SOC_MPI_SUPPORTED
void esp_crypto_mpi_lock_acquire(void)
{
    _lock_acquire(&s_crypto_mpi_lock);
}

void esp_crypto_mpi_lock_release(void)
{
    _lock_release(&s_crypto_mpi_lock);
}
#endif /* SOC_MPI_SUPPORTED */

#ifdef SOC_ECC_SUPPORTED
void esp_crypto_ecc_lock_acquire(void)
{
    _lock_acquire(&s_crypto_ecc_lock);
}

void esp_crypto_ecc_lock_release(void)
{
    _lock_release(&s_crypto_ecc_lock);
}
#endif /* SOC_ECC_SUPPORTED */

#ifdef SOC_ECDSA_SUPPORTED
void esp_crypto_ecdsa_lock_acquire(void)
{
    _lock_acquire(&s_crypto_ecdsa_lock);
    esp_crypto_ecc_lock_acquire();
#if defined(SOC_SHA_SUPPORTED) || defined(SOC_AES_SUPPORTED)
    /* ECDSA reset also holds SHA in reset, and SHA shares DMA with AES.
       Match upstream 7fc2aaf: SHA/AES before MPI, as in the DS lock order. */
    esp_crypto_sha_aes_lock_acquire();
#endif
    /* The reset coupling exists even on a revision that does not use MPI. */
#if (SOC_MPI_SUPPORTED && SOC_ECDSA_USES_MPI)
    esp_crypto_mpi_lock_acquire();
#endif /* (SOC_MPI_SUPPORTED && SOC_ECDSA_USES_MPI) */
}

void esp_crypto_ecdsa_lock_release(void)
{
#if (SOC_MPI_SUPPORTED && SOC_ECDSA_USES_MPI)
    esp_crypto_mpi_lock_release();
#endif /* (SOC_MPI_SUPPORTED && SOC_ECDSA_USES_MPI) */
#if defined(SOC_SHA_SUPPORTED) || defined(SOC_AES_SUPPORTED)
    esp_crypto_sha_aes_lock_release();
#endif
    esp_crypto_ecc_lock_release();
    _lock_release(&s_crypto_ecdsa_lock);
}
#endif /* SOC_ECDSA_SUPPORTED */

#ifdef SOC_KEY_MANAGER_SUPPORTED
void esp_crypto_key_manager_lock_acquire(void)
{
    _lock_acquire(&s_crypto_key_manager_lock);
}

void esp_crypto_key_manager_lock_release(void)
{
    _lock_release(&s_crypto_key_manager_lock);
}
#endif /* SOC_KEY_MANAGER_SUPPORTED */
#else /* NON_OS_BUILD */
#ifdef SOC_HMAC_SUPPORTED
void esp_crypto_hmac_lock_acquire(void) {}

void esp_crypto_hmac_lock_release(void) {}
#endif /* SOC_HMAC_SUPPORTED */

#ifdef SOC_DIG_SIGN_SUPPORTED
void esp_crypto_ds_lock_acquire(void) {}

void esp_crypto_ds_lock_release(void) {}
#endif /* SOC_DIG_SIGN_SUPPORTED */

#if defined(SOC_SHA_SUPPORTED) || defined(SOC_AES_SUPPORTED)
void esp_crypto_sha_aes_lock_acquire(void) {}

void esp_crypto_sha_aes_lock_release(void) {}
#endif /* defined(SOC_SHA_SUPPORTED) || defined(SOC_AES_SUPPORTED) */

#if defined(SOC_SHA_CRYPTO_DMA) || defined(SOC_AES_CRYPTO_DMA)
void esp_crypto_dma_lock_acquire(void) {}

void esp_crypto_dma_lock_release(void) {}
#endif /* defined(SOC_SHA_CRYPTO_DMA) || defined(SOC_AES_CRYPTO_DMA) */

#ifdef SOC_MPI_SUPPORTED
void esp_crypto_mpi_lock_acquire(void) {}

void esp_crypto_mpi_lock_release(void) {}
#endif /* SOC_MPI_SUPPORTED */

#ifdef SOC_ECC_SUPPORTED
void esp_crypto_ecc_lock_acquire(void) {}

void esp_crypto_ecc_lock_release(void) {}
#endif /* SOC_ECC_SUPPORTED */

#ifdef SOC_ECDSA_SUPPORTED
void esp_crypto_ecdsa_lock_acquire(void) {}

void esp_crypto_ecdsa_lock_release(void) {}
#endif /* SOC_ECDSA_SUPPORTED */

#ifdef SOC_KEY_MANAGER_SUPPORTED
void esp_crypto_key_manager_lock_acquire(void) {}

void esp_crypto_key_manager_lock_release(void) {}
#endif /* SOC_KEY_MANAGER_SUPPORTED */
#endif /* !NON_OS_BUILD */

#endif /* ESP32-P4, ESP-IDF 5.5.4 */

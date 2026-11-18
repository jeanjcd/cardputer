#pragma once
#include <stddef.h>
#include <stdint.h>

// AES-256-CCM (RFC 3610) with L = 2, a 13-byte nonce and no associated data,
// matching the Meshtastic PKI scheme. Tag length M must be even, 4..16.

/// Encrypts plain[len] into crypt[len] and writes the M-byte tag; false on bad params.
bool aesCcmEncrypt(const uint8_t key[32], const uint8_t nonce[13], size_t M,
                   const uint8_t* plain, size_t len, uint8_t* crypt, uint8_t* tag);

/// Decrypts crypt[len] into plain[len] and verifies the M-byte tag in constant time; false on failure (plain zeroed).
bool aesCcmDecrypt(const uint8_t key[32], const uint8_t nonce[13], size_t M,
                   const uint8_t* crypt, size_t len, const uint8_t* tag, uint8_t* plain);

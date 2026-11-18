// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stddef.h>
#include <stdint.h>

/// AES-128-CTR in place (encrypt == decrypt); counter block = id u64 LE | from u32 LE | u32 BE counter from 0.
void ctrCrypt(const uint8_t key[16], uint32_t id, uint32_t from, uint8_t* buf, size_t len);

/// Generates an X25519 keypair: priv = rng bytes clamped per RFC 7748, pub = X25519(priv, 9).
void x25519Keypair(void (*rng)(uint8_t* buf, size_t len), uint8_t priv[32], uint8_t pub[32]);

/// outKey = SHA256(X25519(priv, peerPub)); false if the peer key is weak/invalid.
bool x25519Shared(const uint8_t priv[32], const uint8_t peerPub[32], uint8_t outKey[32]);

/// PKI encrypt (AES-256-CCM, tag 8); out = cipher | tag8 | extraNonce LE; returns len + 12, or 0 on error.
size_t pkiEncrypt(const uint8_t key[32], uint32_t id, uint32_t from, uint32_t extraNonce,
                  const uint8_t* plain, size_t len, uint8_t* out);

/// PKI decrypt of cipher | tag8 | extraNonce LE into out (writes at most wireLen-12 bytes); returns plain length (0 for wireLen 12 with a valid tag), or -1 if wireLen < 12 or auth fails.
int pkiDecrypt(const uint8_t key[32], uint32_t id, uint32_t from, const uint8_t* wire, size_t wireLen,
               uint8_t* out);

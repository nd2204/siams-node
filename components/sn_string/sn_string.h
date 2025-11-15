#pragma once
#include <stdint.h>
#include <stddef.h>

typedef struct {
  uint32_t hash;
  const char *str; // canonical pointer in registry
} StringId;

StringId sid_intern(const char *str);
const char *sid_lookup(uint32_t hash);
uint32_t sid_hash(const char *str);

// Convenience macro to create interned StringId at runtime
#define SID(str_literal) sid_intern(str_literal)

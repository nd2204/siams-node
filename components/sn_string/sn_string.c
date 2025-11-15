// string_id.c
#include "sn_string.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define STRING_TABLE_CAP 4096

typedef struct {
  uint32_t hash;
  const char *str;
} Entry;

static Entry g_string_table[STRING_TABLE_CAP];
static size_t g_string_count = 0;

// Very fast hash (you can swap with fnv1a, xxhash, etc.)
uint32_t sid_hash(const char *str) {
  uint32_t h = 2166136261u;
  for (; *str; str++)
    h = (h ^ (unsigned char)(*str)) * 16777619u;
  return h;
}

StringId sid_intern(const char *str) {
  uint32_t hash = sid_hash(str);

  // Check if already in table
  for (size_t i = 0; i < g_string_count; i++) {
    if (g_string_table[i].hash == hash && strcmp(g_string_table[i].str, str) == 0) {
      return (StringId){hash, g_string_table[i].str};
    }
  }

  // Add new entry
  if (g_string_count >= STRING_TABLE_CAP) {
    fprintf(stderr, "StringId table full\n");
    abort();
  }

  char *copy = strdup(str);
  g_string_table[g_string_count] = (Entry){hash, copy};
  g_string_count++;

  return (StringId){hash, copy};
}

const char *sid_lookup(uint32_t hash) {
  for (size_t i = 0; i < g_string_count; i++)
    if (g_string_table[i].hash == hash) return g_string_table[i].str;
  return NULL;
}

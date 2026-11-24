// SPDX-License-Identifier: MIT
#include "ota.h"
#include "json.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
namespace ss { namespace ota {

bool parse_manifest(const char* j, Manifest& m) {
  memset(&m, 0, sizeof m); double size = 0;
  if (!json::str(json::find(j, "version"), m.version, sizeof m.version)) return false;
  if (!json::str(json::find(j, "url"), m.url, sizeof m.url)) return false;
  if (!json::str(json::find(j, "sha256"), m.sha256, sizeof m.sha256) || strlen(m.sha256) != 64) return false;
  for (const char* p = m.sha256; *p; p++) if (!isxdigit((unsigned char)*p)) return false;
  if (!json::num(json::find(j, "size"), size) || size <= 0) return false;
  m.size = (uint32_t)size;
  if (!json::str(json::find(j, "channel"), m.channel, sizeof m.channel)) strcpy(m.channel, "stable");
  if (!json::str(json::find(j, "min_version"), m.min_version, sizeof m.min_version)) strcpy(m.min_version, "0.0.0");
  json::str(json::find(j, "notes"), m.notes, sizeof m.notes);
  return strncmp(m.url, "https://", 8) == 0; // never fetch firmware over plain HTTP
}
int semver_cmp(const char* a, const char* b) {
  for (int i = 0; i < 3; i++) {
    long x = strtol(a, (char**)&a, 10), y = strtol(b, (char**)&b, 10);
    if (x != y) return x < y ? -1 : 1;
    if (*a == '.') a++;
    if (*b == '.') b++;
  }
  return 0;
}
bool should_offer(const Manifest& m, const char* cur) { return semver_cmp(m.version, cur) > 0 && semver_cmp(cur, m.min_version) >= 0; }
}}

// Minimal read-only JSON scanner for small, trusted-shape documents
// (weather API, OTA manifest). No allocation, no recursion limit issues:
// values are located by key inside one object level and read in place.
#pragma once
namespace ss { namespace json {
// v points at an object ('{'); returns a pointer to the value for key at that level, or nullptr.
const char* find(const char* v, const char* key);
// Follow a path of nested object keys, e.g. path(doc, {"current", "temperature_2m"}).
const char* path(const char* v, const char* const* keys, int n);
bool num(const char* v, double& out);
bool str(const char* v, char* out, int cap);            // handles \" \\ \/ \n escapes
int nums(const char* v, double* out, int max);           // v points at '[': numbers (null -> NAN)
const char* skip(const char* v);                         // past one value
}}

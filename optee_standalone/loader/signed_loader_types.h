#ifndef SIGNED_LOADER_TYPES_H
#define SIGNED_LOADER_TYPES_H
#include <stdint.h>
#define SIGNED_LOADER_RECORDS 32U
struct signed_loader_entry { uint8_t uuid[16]; uint32_t flags, signature, digest, reserved; };
struct signed_loader_audit { uint32_t version, used, overflow, reserved; struct signed_loader_entry entries[SIGNED_LOADER_RECORDS]; };
extern volatile struct signed_loader_audit signed_loader_audit;
#endif

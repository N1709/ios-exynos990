#pragma once
#include <stdint.h>

void ufs_probe(void (*kv)(const char *, uint64_t), void (*out)(const char *),
	       uintptr_t dma_base);

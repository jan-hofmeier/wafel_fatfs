#include "fatfs/ff.h"
#include <types.h>
#include <wafel/ios/memory.h>

void* (*memcpy)(void *dst, const void *src, size_t size) = (void*)0x107f4f7c;
void* (*memset)(void *dst, int fill, size_t size) = (void*)0x107f5018;

typedef struct BlkCacheBlockInfo {
  LBA_t lba;
  bool valid;
  bool dirty;
  UINT age;
} BlkCacheBlockInfo;


typedef struct BlkCache {
  BYTE *buffer;
  size_t sector_size; // size of sector in bytes
  size_t cache_blocks; // in sectors
  BlkCacheDev device;
  BlkCacheBlockInfo block_info[];
} BlkCache;

typedef struct BlkCacheDev {
  int dev_handle;
  int (*read_func)(int handle, LBA_t sector, UINT count, BYTE* buff);
  int (*write_func)(int handle, LBA_t sector, UINT count, const BYTE* buff);
} BlkCacheDev;

BlkCache* blkcache_allocate(size_t cache_size, size_t sector_size, size_t alignment, BlkCacheDev* dev){
  BYTE *buffer = iosAllocAligned(HEAPID_LOCAL, cache_size, alignment);
  size_t block_count = cache_size / sector_size;
  BlkCache *cache = malloc_local(sizeof(BlkCache) + block_count * sizeof(BlkCacheBlockInfo));
  cache->buffer = buffer;
  cache->sector_size = sector_size;
  cache->cache_blocks = block_count;
  cache->device = *dev;
  for(int i=0; i<block_count; i++){
    cache->block_info[i].valid = false;
  } 
  return cache;
}

int blkcache_read(BlkCache *cache, LBA_t sector, UINT count, BYTE* buff) {
  UINT hit_idx[count];
  memset(hit_idx, 0xFF, size_of(hit_idx));

  bool any_dirty = false;
  UINT hitcount = 0;
  for(int i=0, off=0; i<cache->cache_blocks; i++, off+=cache->sector_size) {
    if(cache->block_info[i].valid){
      int cache_lba = cache->block_info[i].lba;
      if(cache_lba>=sector && cache_lba<sector+count){
        hitcount++;
        hit_idx[cache_lba-sector] = i;
        if(cache->block_info[i].dirty){
          any_dirty = true;
        }
      }
    }
  }

  if(!hitcount){
    // read
    // unalligned
    //install if small
  }

  if(!any_dirty){
    int i;
    for(i=0; i<count && hit_idx[i] != 0xFFFF; i++){
      UINT cpy_start_idx = i;
      size_t cpy_len = 0;
      while(hit_idx[i] == hit_idx[i+1]){
        i++;
        cpy_len+=cache->sector_size;
      }
      void *dst = buff + cpy_start_idx * cache->sector_size;
      void *src = cache->buffer + hit_idx[cpy_start_idx] * cache->sector_size;
      memcpy(dst, src, cpy_len);
    }
    int start_read = i;
    // now from the end
  }

}
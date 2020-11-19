/*
 * Basic general purpose allocator for managing special purpose
 * memory, for example, memory that is not managed by the regular
 * kmalloc/kfree interface.  Uses for this includes on-device special
 * memory, uncached memory etc.
 *
 * It is safe to use the allocator in NMI handlers and other special
 * unblockable contexts that could otherwise deadlock on locks.  This
 * is implemented by using atomic operations and retries on any
 * conflicts.  The disadvantage is that there may be livelocks in
 * extreme cases.  For better scalability, one allocator can be used
 * for each CPU.
 *
 * The lockless operation only works if there is enough memory
 * available.  If new memory is added to the pool a lock has to be
 * still taken.  So any user relying on locklessness has to ensure
 * that sufficient memory is preallocated.
 *
 * The basic atomic operation of this allocator is cmpxchg on long.
 * On architectures that don't have NMI-safe cmpxchg implementation,
 * the allocator can NOT be used in NMI handler.  So code uses the
 * allocator in NMI handler should depend on
 * CONFIG_ARCH_HAVE_NMI_SAFE_CMPXCHG.
 *
 * Copyright 2005 (C) Jes Sorensen <jes@trained-monkey.org>
 *
 * This source code is licensed under the GNU General Public License,
 * Version 2.  See the file COPYING for more details.
 */

#include <linux/slab.h>
#include <linux/export.h>
#include <linux/bitmap.h>
#include <linux/rculist.h>
#include <linux/interrupt.h>
#include <linux/of_device.h>
#include <linux/vmalloc.h>
#include <linux/version.h>
#include "bm_genalloc.h"
static inline size_t chunk_size(const struct bm_gen_pool_chunk *chunk)
{
	return chunk->end_addr - chunk->start_addr + 1;
}

static int set_bits_ll(unsigned long *addr, unsigned long mask_to_set)
{
	unsigned long val, nval;

	nval = *addr;
	do {
		val = nval;
		if (val & mask_to_set)
			return -EBUSY;
		cpu_relax();
	} while ((nval = cmpxchg(addr, val, val | mask_to_set)) != val);

	return 0;
}

static int clear_bits_ll(unsigned long *addr, unsigned long mask_to_clear)
{
	unsigned long val, nval;

	nval = *addr;
	do {
		val = nval;
		if ((val & mask_to_clear) != mask_to_clear)
			return -EBUSY;
		cpu_relax();
	} while ((nval = cmpxchg(addr, val, val & ~mask_to_clear)) != val);

	return 0;
}

/*
 * bitmap_set_ll - set the specified number of bits at the specified position
 * @map: pointer to a bitmap
 * @start: a bit position in @map
 * @nr: number of bits to set
 *
 * Set @nr bits start from @start in @map lock-lessly. Several users
 * can set/clear the same bitmap simultaneously without lock. If two
 * users set the same bit, one user will return remain bits, otherwise
 * return 0.
 */
static int bitmap_set_ll(unsigned long *map, int start, int nr)
{
	unsigned long *p = map + BIT_WORD(start);
	const int size = start + nr;
	int bits_to_set = BITS_PER_LONG - (start % BITS_PER_LONG);
	unsigned long mask_to_set = BITMAP_FIRST_WORD_MASK(start);

	while (nr - bits_to_set >= 0) {
		if (set_bits_ll(p, mask_to_set))
			return nr;
		nr -= bits_to_set;
		bits_to_set = BITS_PER_LONG;
		mask_to_set = ~0UL;
		p++;
	}
	if (nr) {
		mask_to_set &= BITMAP_LAST_WORD_MASK(size);
		if (set_bits_ll(p, mask_to_set))
			return nr;
	}

	return 0;
}

/*
 * bitmap_clear_ll - clear the specified number of bits at the specified position
 * @map: pointer to a bitmap
 * @start: a bit position in @map
 * @nr: number of bits to set
 *
 * Clear @nr bits start from @start in @map lock-lessly. Several users
 * can set/clear the same bitmap simultaneously without lock. If two
 * users clear the same bit, one user will return remain bits,
 * otherwise return 0.
 */
static int bitmap_clear_ll(unsigned long *map, int start, int nr)
{
	unsigned long *p = map + BIT_WORD(start);
	const int size = start + nr;
	int bits_to_clear = BITS_PER_LONG - (start % BITS_PER_LONG);
	unsigned long mask_to_clear = BITMAP_FIRST_WORD_MASK(start);

	while (nr - bits_to_clear >= 0) {
		if (clear_bits_ll(p, mask_to_clear))
			return nr;
		nr -= bits_to_clear;
		bits_to_clear = BITS_PER_LONG;
		mask_to_clear = ~0UL;
		p++;
	}
	if (nr) {
		mask_to_clear &= BITMAP_LAST_WORD_MASK(size);
		if (clear_bits_ll(p, mask_to_clear))
			return nr;
	}

	return 0;
}
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 18, 69)
/**
 * bitmap_find_next_zero_area_off - find a contiguous aligned zero area
 * @map: The address to base the search on
 * @size: The bitmap size in bits
 * @start: The bitnumber to start searching at
 * @nr: The number of zeroed bits we're looking for
 * @align_mask: Alignment mask for zero area
 * @align_offset: Alignment offset for zero area.
 *
 * The @align_mask should be one less than a power of 2; the effect is that
 * the bit offset of all zero areas this function finds plus @align_offset
 * is multiple of that power of 2.
 */
unsigned long bitmap_find_next_zero_area_off(unsigned long *map,
					     unsigned long size,
					     unsigned long start,
					     unsigned int nr,
					     unsigned long align_mask,
					     unsigned long align_offset)
{
	unsigned long index, end, i;
again:
	index = find_next_zero_bit(map, size, start);

	/* Align allocation */
	index = __ALIGN_MASK(index + align_offset, align_mask) - align_offset;

	end = index + nr;
	if (end > size)
		return end;
	i = find_next_bit(map, end, index);
	if (i < end) {
		start = i + 1;
		goto again;
	}
	return index;
}
#endif
/**
 * bm_gen_pool_create - create a new special memory pool
 * @min_alloc_order: log base 2 of number of bytes each bitmap bit represents
 * @nid: node id of the node the pool structure should be allocated on, or -1
 *
 * Create a new special memory pool that can be used to manage special purpose
 * memory not managed by the regular kmalloc/kfree interface.
 */
struct bm_gen_pool *bm_gen_pool_create(int min_alloc_order, int nid)
{
	struct bm_gen_pool *pool;

	pool = kmalloc_node(sizeof(struct bm_gen_pool), GFP_KERNEL, nid);
	if (pool != NULL) {
		spin_lock_init(&pool->lock);
		INIT_LIST_HEAD(&pool->chunks);
		pool->min_alloc_order = min_alloc_order;
		pool->algo = bm_gen_pool_first_fit;
		pool->data = NULL;
		pool->name = NULL;
	}
	return pool;
}

/**
 * bm_gen_pool_add_virt - add a new chunk of special memory to the pool
 * @pool: pool to add new memory chunk to
 * @virt: virtual starting address of memory chunk to add to pool
 * @phys: physical starting address of memory chunk to add to pool
 * @size: size in bytes of the memory chunk to add to pool
 * @nid: node id of the node the chunk structure and bitmap should be
 *       allocated on, or -1
 *
 * Add a new chunk of special memory to the specified pool.
 *
 * Returns 0 on success or a -ve errno on failure.
 */
int bm_gen_pool_add_virt(struct bm_gen_pool *pool, unsigned long virt, phys_addr_t phys,
		 size_t size, int nid)
{
	struct bm_gen_pool_chunk *chunk;
	int nbits = size >> pool->min_alloc_order;
	int nbytes = sizeof(struct bm_gen_pool_chunk) +
				BITS_TO_LONGS(nbits) * sizeof(long);

	chunk = vzalloc_node(nbytes, nid);
	if (unlikely(chunk == NULL))
		return -ENOMEM;

	chunk->phys_addr = phys;
	chunk->start_addr = virt;
	chunk->end_addr = virt + size - 1;
	atomic_long_set(&chunk->avail, size);

	spin_lock(&pool->lock);
	list_add_rcu(&chunk->next_chunk, &pool->chunks);
	spin_unlock(&pool->lock);

	return 0;
}

/**
 * bm_gen_pool_virt_to_phys - return the physical address of memory
 * @pool: pool to allocate from
 * @addr: starting address of memory
 *
 * Returns the physical address on success, or -1 on error.
 */
phys_addr_t bm_gen_pool_virt_to_phys(struct bm_gen_pool *pool, unsigned long addr)
{
	struct bm_gen_pool_chunk *chunk;
	phys_addr_t paddr = -1;

	rcu_read_lock();
	list_for_each_entry_rcu(chunk, &pool->chunks, next_chunk) {
		if (addr >= chunk->start_addr && addr <= chunk->end_addr) {
			paddr = chunk->phys_addr + (addr - chunk->start_addr);
			break;
		}
	}
	rcu_read_unlock();

	return paddr;
}

/**
 * bm_gen_pool_destroy - destroy a special memory pool
 * @pool: pool to destroy
 *
 * Destroy the specified special memory pool. Verifies that there are no
 * outstanding allocations.
 */
void bm_gen_pool_destroy(struct bm_gen_pool *pool)
{
	struct list_head *_chunk, *_next_chunk;
	struct bm_gen_pool_chunk *chunk;
	int order = pool->min_alloc_order;
	int bit, end_bit;

	list_for_each_safe(_chunk, _next_chunk, &pool->chunks) {
		chunk = list_entry(_chunk, struct bm_gen_pool_chunk, next_chunk);
		list_del(&chunk->next_chunk);

		end_bit = chunk_size(chunk) >> order;
		bit = find_next_bit(chunk->bits, end_bit, 0);
		BUG_ON(bit < end_bit);

		vfree(chunk);
	}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 0, 0)
	kfree_const(pool->name);
#else
	kfree((char *)(pool->name));
#endif
	kfree(pool);
}

/**
 * bm_gen_pool_alloc - allocate special memory from the pool
 * @pool: pool to allocate from
 * @size: number of bytes to allocate from the pool
 *
 * Allocate the requested number of bytes from the specified pool.
 * Uses the pool allocation function (with first-fit algorithm by default).
 * Can not be used in NMI handler on architectures without
 * NMI-safe cmpxchg implementation.
 */
unsigned long bm_gen_pool_alloc(struct bm_gen_pool *pool, size_t size)
{
	return bm_gen_pool_alloc_algo(pool, size, pool->algo, pool->data);
}

/**
 * bm_gen_pool_alloc_algo - allocate special memory from the pool
 * @pool: pool to allocate from
 * @size: number of bytes to allocate from the pool
 * @algo: algorithm passed from caller
 * @data: data passed to algorithm
 *
 * Allocate the requested number of bytes from the specified pool.
 * Uses the pool allocation function (with first-fit algorithm by default).
 * Can not be used in NMI handler on architectures without
 * NMI-safe cmpxchg implementation.
 */
unsigned long bm_gen_pool_alloc_algo(struct bm_gen_pool *pool, size_t size,
		bm_genpool_algo_t algo, void *data)
{
	struct bm_gen_pool_chunk *chunk;
	unsigned long addr = 0;
	int order = pool->min_alloc_order;
	int nbits, start_bit, end_bit, remain;

#ifndef CONFIG_ARCH_HAVE_NMI_SAFE_CMPXCHG
	BUG_ON(in_nmi());
#endif

	if (size == 0)
		return 0;

	nbits = (size + (1UL << order) - 1) >> order;
	rcu_read_lock();
	list_for_each_entry_rcu(chunk, &pool->chunks, next_chunk) {
		if (size > atomic_long_read(&chunk->avail))
			continue;

		start_bit = 0;
		end_bit = chunk_size(chunk) >> order;
retry:
		start_bit = algo(chunk->bits, end_bit, start_bit,
				 nbits, data, pool, chunk->start_addr);
		if (start_bit >= end_bit)
			continue;
		remain = bitmap_set_ll(chunk->bits, start_bit, nbits);
		if (remain) {
			remain = bitmap_clear_ll(chunk->bits, start_bit,
						 nbits - remain);
			BUG_ON(remain);
			goto retry;
		}

		addr = chunk->start_addr + ((unsigned long)start_bit << order);
		size = (size_t)nbits << order;
		atomic_long_sub(size, &chunk->avail);
		break;
	}
	rcu_read_unlock();
	return addr;
}

/**
 * bm_gen_pool_dma_alloc - allocate special memory from the pool for DMA usage
 * @pool: pool to allocate from
 * @size: number of bytes to allocate from the pool
 * @dma: dma-view physical address return value.  Use NULL if unneeded.
 *
 * Allocate the requested number of bytes from the specified pool.
 * Uses the pool allocation function (with first-fit algorithm by default).
 * Can not be used in NMI handler on architectures without
 * NMI-safe cmpxchg implementation.
 */
void *bm_gen_pool_dma_alloc(struct bm_gen_pool *pool, size_t size, dma_addr_t *dma)
{
	unsigned long vaddr;

	if (!pool)
		return NULL;

	vaddr = bm_gen_pool_alloc(pool, size);
	if (!vaddr)
		return NULL;

	if (dma)
		*dma = bm_gen_pool_virt_to_phys(pool, vaddr);

	return (void *)vaddr;
}

/**
 * bm_gen_pool_free - free allocated special memory back to the pool
 * @pool: pool to free to
 * @addr: starting address of memory to free back to pool
 * @size: size in bytes of memory to free
 *
 * Free previously allocated special memory back to the specified
 * pool.  Can not be used in NMI handler on architectures without
 * NMI-safe cmpxchg implementation.
 */
void bm_gen_pool_free(struct bm_gen_pool *pool, unsigned long addr, size_t size)
{
	struct bm_gen_pool_chunk *chunk;
	int order = pool->min_alloc_order;
	int start_bit, nbits, remain;

#ifndef CONFIG_ARCH_HAVE_NMI_SAFE_CMPXCHG
	BUG_ON(in_nmi());
#endif

	nbits = (size + (1UL << order) - 1) >> order;
	rcu_read_lock();
	list_for_each_entry_rcu(chunk, &pool->chunks, next_chunk) {
		if (addr >= chunk->start_addr && addr <= chunk->end_addr) {
			BUG_ON(addr + size - 1 > chunk->end_addr);
			start_bit = (addr - chunk->start_addr) >> order;
			remain = bitmap_clear_ll(chunk->bits, start_bit, nbits);
			BUG_ON(remain);
			size = (size_t)nbits << order;
			atomic_long_add(size, &chunk->avail);
			rcu_read_unlock();
			return;
		}
	}
	rcu_read_unlock();
	BUG();
}

/**
 * bm_gen_pool_for_each_chunk - call func for every chunk of generic memory pool
 * @pool:	the generic memory pool
 * @func:	func to call
 * @data:	additional data used by @func
 *
 * Call @func for every chunk of generic memory pool.  The @func is
 * called with rcu_read_lock held.
 */
void bm_gen_pool_for_each_chunk(struct bm_gen_pool *pool,
	void (*func)(struct bm_gen_pool *, struct bm_gen_pool_chunk *, void *),
	void *data) {
	struct bm_gen_pool_chunk *chunk;

	rcu_read_lock();
	list_for_each_entry_rcu(chunk, &(pool)->chunks, next_chunk)
		func(pool, chunk, data);
	rcu_read_unlock();
}

/**
 * addr_in_bm_gen_pool - checks if an address falls within the range of a pool
 * @pool:	the generic memory pool
 * @start:	start address
 * @size:	size of the region
 *
 * Check if the range of addresses falls within the specified pool. Returns
 * true if the entire range is contained in the pool and false otherwise.
 */
bool addr_in_bm_gen_pool(struct bm_gen_pool *pool, unsigned long start,
			size_t size)
{
	bool found = false;
	unsigned long end = start + size - 1;
	struct bm_gen_pool_chunk *chunk;

	rcu_read_lock();
	list_for_each_entry_rcu(chunk, &(pool)->chunks, next_chunk) {
		if (start >= chunk->start_addr && start <= chunk->end_addr) {
			if (end <= chunk->end_addr) {
				found = true;
				break;
			}
		}
	}
	rcu_read_unlock();
	return found;
}

/**
 * bm_gen_pool_avail - get available free space of the pool
 * @pool: pool to get available free space
 *
 * Return available free space of the specified pool.
 */
size_t bm_gen_pool_avail(struct bm_gen_pool *pool)
{
	struct bm_gen_pool_chunk *chunk;
	size_t avail = 0;

	rcu_read_lock();
	list_for_each_entry_rcu(chunk, &pool->chunks, next_chunk)
		avail += atomic_long_read(&chunk->avail);
	rcu_read_unlock();
	return avail;
}

/**
 * bm_gen_pool_size - get size in bytes of memory managed by the pool
 * @pool: pool to get size
 *
 * Return size in bytes of memory managed by the pool.
 */
size_t bm_gen_pool_size(struct bm_gen_pool *pool)
{
	struct bm_gen_pool_chunk *chunk;
	size_t size = 0;

	rcu_read_lock();
	list_for_each_entry_rcu(chunk, &pool->chunks, next_chunk)
		size += chunk_size(chunk);
	rcu_read_unlock();
	return size;
}

/**
 * bm_gen_pool_set_algo - set the allocation algorithm
 * @pool: pool to change allocation algorithm
 * @algo: custom algorithm function
 * @data: additional data used by @algo
 *
 * Call @algo for each memory allocation in the pool.
 * If @algo is NULL use bm_gen_pool_first_fit as default
 * memory allocation function.
 */
void bm_gen_pool_set_algo(struct bm_gen_pool *pool, bm_genpool_algo_t algo, void *data)
{
	rcu_read_lock();

	pool->algo = algo;
	if (!pool->algo)
		pool->algo = bm_gen_pool_first_fit;

	pool->data = data;

	rcu_read_unlock();
}

/**
 * bm_gen_pool_first_fit - find the first available region
 * of memory matching the size requirement (no alignment constraint)
 * @map: The address to base the search on
 * @size: The bitmap size in bits
 * @start: The bitnumber to start searching at
 * @nr: The number of zeroed bits we're looking for
 * @data: additional data - unused
 * @pool: pool to find the fit region memory from
 */
unsigned long bm_gen_pool_first_fit(unsigned long *map, unsigned long size,
		unsigned long start, unsigned int nr, void *data,
		struct bm_gen_pool *pool, unsigned long start_addr)
{
	return bitmap_find_next_zero_area(map, size, start, nr, 0);
}
/**
 * bm_gen_pool_first_fit_align - find the first available region
 * of memory matching the size requirement (alignment constraint)
 * @map: The address to base the search on
 * @size: The bitmap size in bits
 * @start: The bitnumber to start searching at
 * @nr: The number of zeroed bits we're looking for
 * @data: data for alignment
 * @pool: pool to get order from
 */
unsigned long bm_gen_pool_first_fit_align(unsigned long *map, unsigned long size,
		unsigned long start, unsigned int nr, void *data,
		struct bm_gen_pool *pool, unsigned long start_addr)
{
	struct bm_genpool_data_align *alignment;
	unsigned long align_mask, align_off;
	int order;

	alignment = data;
	order = pool->min_alloc_order;
	align_mask = ((alignment->align + (1UL << order) - 1) >> order) - 1;
	align_off = (start_addr & (alignment->align - 1)) >> order;

	return bitmap_find_next_zero_area_off(map, size, start, nr,
					      align_mask, align_off);
}

/**
 * bm_gen_pool_fixed_alloc - reserve a specific region
 * @map: The address to base the search on
 * @size: The bitmap size in bits
 * @start: The bitnumber to start searching at
 * @nr: The number of zeroed bits we're looking for
 * @data: data for alignment
 * @pool: pool to get order from
 */
unsigned long bm_gen_pool_fixed_alloc(unsigned long *map, unsigned long size,
		unsigned long start, unsigned int nr, void *data,
		struct bm_gen_pool *pool, unsigned long start_addr)
{
	struct bm_genpool_data_fixed *fixed_data;
	int order;
	unsigned long offset_bit;
	unsigned long start_bit;

	fixed_data = data;
	order = pool->min_alloc_order;
	offset_bit = fixed_data->offset >> order;
	if (WARN_ON(fixed_data->offset & ((1UL << order) - 1)))
		return size;

	start_bit = bitmap_find_next_zero_area(map, size,
			start + offset_bit, nr, 0);
	if (start_bit != offset_bit)
		start_bit = size;
	return start_bit;
}

/**
 * bm_gen_pool_first_fit_order_align - find the first available region
 * of memory matching the size requirement. The region will be aligned
 * to the order of the size specified.
 * @map: The address to base the search on
 * @size: The bitmap size in bits
 * @start: The bitnumber to start searching at
 * @nr: The number of zeroed bits we're looking for
 * @data: additional data - unused
 * @pool: pool to find the fit region memory from
 */
unsigned long bm_gen_pool_first_fit_order_align(unsigned long *map,
		unsigned long size, unsigned long start,
		unsigned int nr, void *data, struct bm_gen_pool *pool,
		unsigned long start_addr)
{
	unsigned long align_mask = roundup_pow_of_two(nr) - 1;

	return bitmap_find_next_zero_area(map, size, start, nr, align_mask);
}

/**
 * bm_gen_pool_best_fit - find the best fitting region of memory
 * macthing the size requirement (no alignment constraint)
 * @map: The address to base the search on
 * @size: The bitmap size in bits
 * @start: The bitnumber to start searching at
 * @nr: The number of zeroed bits we're looking for
 * @data: additional data - unused
 * @pool: pool to find the fit region memory from
 *
 * Iterate over the bitmap to find the smallest free region
 * which we can allocate the memory.
 */
unsigned long bm_gen_pool_best_fit(unsigned long *map, unsigned long size,
		unsigned long start, unsigned int nr, void *data,
		struct bm_gen_pool *pool, unsigned long start_addr)
{
	unsigned long start_bit = size;
	unsigned long len = size + 1;
	unsigned long index;

	index = bitmap_find_next_zero_area(map, size, start, nr, 0);

	while (index < size) {
		int next_bit = find_next_bit(map, size, index + nr);

		if ((next_bit - index) < len) {
			len = next_bit - index;
			start_bit = index;
			if (len == nr)
				return start_bit;
		}
		index = bitmap_find_next_zero_area(map, size,
						   next_bit + 1, nr, 0);
	}

	return start_bit;
}

static void devm_bm_gen_pool_release(struct device *dev, void *res)
{
	bm_gen_pool_destroy(*(struct bm_gen_pool **)res);
}

static int devm_bm_gen_pool_match(struct device *dev, void *res, void *data)
{
	struct bm_gen_pool **p = res;

	/* NULL data matches only a pool without an assigned name */
	if (!data && !(*p)->name)
		return 1;

	if (!data || !(*p)->name)
		return 0;

	return !strcmp((*p)->name, data);
}

/**
 * bm_gen_pool_get - Obtain the bm_gen_pool (if any) for a device
 * @dev: device to retrieve the bm_gen_pool from
 * @name: name of a bm_gen_pool or NULL, identifies a particular bm_gen_pool on device
 *
 * Returns the bm_gen_pool for the device if one is present, or NULL.
 */
struct bm_gen_pool *bm_gen_pool_get(struct device *dev, const char *name)
{
	struct bm_gen_pool **p;

	p = devres_find(dev, devm_bm_gen_pool_release, devm_bm_gen_pool_match,
			(void *)name);
	if (!p)
		return NULL;
	return *p;
}

/**
 * devm_bm_gen_pool_create - managed bm_gen_pool_create
 * @dev: device that provides the bm_gen_pool
 * @min_alloc_order: log base 2 of number of bytes each bitmap bit represents
 * @nid: node selector for allocated bm_gen_pool, %NUMA_NO_NODE for all nodes
 * @name: name of a bm_gen_pool or NULL, identifies a particular bm_gen_pool on device
 *
 * Create a new special memory pool that can be used to manage special purpose
 * memory not managed by the regular kmalloc/kfree interface. The pool will be
 * automatically destroyed by the device management code.
 */
struct bm_gen_pool *devm_bm_gen_pool_create(struct device *dev, int min_alloc_order,
				      int nid, const char *name)
{
	struct bm_gen_pool **ptr, *pool;
	const char *pool_name = NULL;

	/* Check that bm_genpool to be created is uniquely addressed on device */
	if (bm_gen_pool_get(dev, name))
		return ERR_PTR(-EINVAL);

	if (name) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 0, 0)
		pool_name = kstrdup_const(name, GFP_KERNEL);
#else
		pool_name = (const char *)(kstrdup(name, GFP_KERNEL));
#endif
		if (!pool_name)
			return ERR_PTR(-ENOMEM);
	}

	ptr = devres_alloc(devm_bm_gen_pool_release, sizeof(*ptr), GFP_KERNEL);
	if (!ptr)
		goto free_pool_name;

	pool = bm_gen_pool_create(min_alloc_order, nid);
	if (!pool)
		goto free_devres;

	*ptr = pool;
	pool->name = pool_name;
	devres_add(dev, ptr);

	return pool;

free_devres:
	devres_free(ptr);
free_pool_name:
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 0, 0)
	kfree_const(pool_name);
#else
	kfree((char *)pool_name);
#endif

	return ERR_PTR(-ENOMEM);
}

#ifdef CONFIG_OF
/**
 * of_bm_gen_pool_get - find a pool by phandle property
 * @np: device node
 * @propname: property name containing phandle(s)
 * @index: index into the phandle array
 *
 * Returns the pool that contains the chunk starting at the physical
 * address of the device tree node pointed at by the phandle property,
 * or NULL if not found.
 */
struct bm_gen_pool *of_bm_gen_pool_get(struct device_node *np,
	const char *propname, int index)
{
	struct platform_device *pdev;
	struct device_node *np_pool, *parent;
	const char *name = NULL;
	struct bm_gen_pool *pool = NULL;

	np_pool = of_parse_phandle(np, propname, index);
	if (!np_pool)
		return NULL;

	pdev = of_find_device_by_node(np_pool);
	if (!pdev) {
		/* Check if named bm_gen_pool is created by parent node device */
		parent = of_get_parent(np_pool);
		pdev = of_find_device_by_node(parent);
		of_node_put(parent);

		of_property_read_string(np_pool, "label", &name);
		if (!name)
			name = np_pool->name;
	}
	if (pdev)
		pool = bm_gen_pool_get(&pdev->dev, name);
	of_node_put(np_pool);

	return pool;
}
#endif /* CONFIG_OF */

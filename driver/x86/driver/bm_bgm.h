#ifndef _BM_BGM_H_
#define _BM_BGM_H_
#include <linux/device.h>
#include <linux/dma-direction.h>
#include <linux/kref.h>
#include <linux/mm_types.h>
#include <linux/mutex.h>
#include <linux/rbtree.h>
#include <linux/sched.h>
#include <linux/shrinker.h>
#include <linux/types.h>

typedef enum {
  BM_MEM_TYPE_DEVICE  = 0,
  BM_MEM_TYPE_HOST    = 1,
  BM_MEM_TYPE_SYSTEM  = 2,
  BM_MEM_TYPE_INT8_DEVICE  = 3,
  BM_MEM_TYPE_INVALID = 4
} bm_mem_type_t;

typedef union {
	struct {
		bm_mem_type_t        mem_type : 3;
		unsigned int         gmem_heapid : 3;
		unsigned int         reserved : 26;
	} u;
	unsigned int           rawflags;
} bm_mem_flags_t;

typedef struct bm_mem_desc {
	union {
		struct {
			unsigned long         device_addr;
			unsigned int         reserved0;
			int         dmabuf_fd;
		} device;
		struct {
			void *      system_addr;
			unsigned int reserved;
			int         reserved1;
		} system;
	} u;

	bm_mem_flags_t         flags;
	unsigned int                    size;
} bm_mem_desc_t;

typedef struct bm_mem_desc   bm_device_mem_t;

enum ion_heap_type {
	ION_HEAP_TYPE_SYSTEM,
	ION_HEAP_TYPE_SYSTEM_CONTIG,
	ION_HEAP_TYPE_CARVEOUT,
	ION_HEAP_TYPE_CHUNK,
	ION_HEAP_TYPE_DMA,
	ION_HEAP_TYPE_CUSTOM, /*
			       * must be last so device specific heaps always
			       * are at the end of this enum
			       */
};

struct ion_platform_heap {
	enum ion_heap_type type;
	unsigned int id;
	const char *name;
	phys_addr_t base;
	size_t size;
	phys_addr_t align;
	void *priv;
};

struct ion_buffer {
	union {
		struct rb_node node;
		struct list_head list;
	};
	struct ion_device *dev;
	struct ion_heap *heap;
	unsigned long flags;
	unsigned long private_flags;
	size_t size;
	void *priv_virt;
	struct mutex lock;
	int kmap_cnt;
	void *vaddr;
	struct sg_table *sg_table;
	struct list_head attachments;
};

struct ion_heap_ops {
	int (*allocate)(struct ion_heap *heap,
			struct ion_buffer *buffer, unsigned long len,
			unsigned long flags);
	void (*free)(struct ion_buffer *buffer);
	void * (*map_kernel)(struct ion_heap *heap, struct ion_buffer *buffer);
	void (*unmap_kernel)(struct ion_heap *heap, struct ion_buffer *buffer);
	int (*map_user)(struct ion_heap *mapper, struct ion_buffer *buffer,
			struct vm_area_struct *vma);
	int (*shrink)(struct ion_heap *heap, gfp_t gfp_mask, int nr_to_scan);
};

struct ion_heap {
	struct list_head node;
	struct ion_device *dev;
	enum ion_heap_type type;
	struct ion_heap_ops *ops;
	unsigned long flags;
	unsigned int id;
	const char *name;
	struct shrinker shrinker;
	struct list_head free_list;
	size_t free_list_size;
	spinlock_t free_lock;
	wait_queue_head_t waitqueue;
	struct task_struct *task;
};

struct ion_carveout_heap {
	struct ion_heap heap;
	struct bm_gen_pool *pool;
	phys_addr_t base;
};

struct ion_device {
	struct rb_root buffers;
	struct mutex buffer_lock;
	struct rw_semaphore lock;
	struct list_head heaps;
	int heap_cnt;
};

void ion_device_add_heap(struct bm_device_info *bmdi, struct ion_heap *heap);
void ion_device_create(struct bm_device_info *bmdi);
struct ion_heap *ion_carveout_heap_create(struct ion_platform_heap *heap_data);
int ion_alloc(struct bm_device_info *bmdi, struct bm_mem_desc* device_mem, unsigned int heap_id_mask, unsigned int flags);
void ion_carveout_heap_destroy(struct ion_carveout_heap *carveout_heap);

#endif

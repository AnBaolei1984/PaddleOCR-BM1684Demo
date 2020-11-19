#ifndef __VMM_TYPE_H__
#define __VMM_TYPE_H__

#include <linux/vmalloc.h>

typedef struct _video_mm_info_struct {
    unsigned long   total_pages;
    unsigned long   alloc_pages;
    unsigned long   free_pages;
    unsigned long   page_size;
} vmem_info_t;

typedef unsigned long long  vmem_key_t;

#define VMEM_PAGE_SIZE           (16*1024)
#define MAKE_KEY_VPU(_a, _b)        (((vmem_key_t)_a)<<32 | _b)
#define KEY_TO_VALUE(_key)      (_key>>32)

typedef struct page_struct {
    int             pageno;
    unsigned long   addr;
    int             used;
    int             alloc_pages;
    int             first_pageno;
} page_t;

typedef struct avl_node_struct {
    vmem_key_t   key;
    int     height;
    page_t *page;
    struct avl_node_struct *left;
    struct avl_node_struct *right;
} avl_node_t;

typedef struct _video_mm_struct {
    avl_node_t *free_tree;
    avl_node_t *alloc_tree;
    page_t        *page_list;
    int            num_pages;
    unsigned long   base_addr;
    unsigned long   mem_size;
    int             free_page_count;
    int             alloc_page_count;
} video_mm_t;

typedef struct _video_mm_struct jpu_mm_t;
#endif

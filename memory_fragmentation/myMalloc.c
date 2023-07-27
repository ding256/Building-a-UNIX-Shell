#include <errno.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "myMalloc.h"
#include "printing.h"

/* Due to the way assert() prints error messges we use out own assert function
 * for deteminism when testing assertions
 */
#ifdef TEST_ASSERT
  inline static void assert(int e) {
    if (!e) {
      const char * msg = "Assertion Failed!\n";
      write(2, msg, strlen(msg));
      exit(1);
    }
  }
#else
  #include <assert.h>
#endif

/*
 * Mutex to ensure thread safety for the freelist
 */
static pthread_mutex_t mutex;

/*
 * Array of sentinel nodes for the freelists
 */
header freelistSentinels[N_LISTS];

/*
 * Pointer to the second fencepost in the most recently allocated chunk from
 * the OS. Used for coalescing chunks
 */
header * lastFencePost;

/*
 * Pointer to maintian the base of the heap to allow printing based on the
 * distance from the base of the heap
 */ 
void * base;

/*
 * List of chunks allocated by  the OS for printing boundary tags
 */
header * osChunkList [MAX_OS_CHUNKS];
size_t numOsChunks = 0;

/*
 * direct the compiler to run the init function before running main
 * this allows initialization of required globals
 */
static void init (void) __attribute__ ((constructor));

// Helper functions for manipulating pointers to headers
static inline header * get_header_from_offset(void * ptr, ptrdiff_t off);
static inline header * get_left_header(header * h);
static inline header * ptr_to_header(void * p);

// Helper functions for allocating more memory from the OS
static inline void initialize_fencepost(header * fp, size_t left_size);
static inline void insert_os_chunk(header * hdr);
static inline void insert_fenceposts(void * raw_mem, size_t size);
static header * allocate_chunk(size_t size);

// Helper functions for freeing a block
static inline void deallocate_object(void * p);

// Helper functions for allocating a block
static inline header * allocate_object(size_t raw_size);

// Helper functions for verifying that the data structures are structurally 
// valid
static inline header * detect_cycles();
static inline header * verify_pointers();
static inline bool verify_freelist();
static inline header * verify_chunk(header * chunk);
static inline bool verify_tags();

static void init();

static bool isMallocInitialized;

/**
 * @brief Helper function to retrieve a header pointer from a pointer and an 
 *        offset
 *
 * @param ptr base pointer
 * @param off number of bytes from base pointer where header is located
 *
 * @return a pointer to a header offset bytes from pointer
 */
static inline header * get_header_from_offset(void * ptr, ptrdiff_t off) {
	return (header *)((char *) ptr + off);
}

/**
 * @brief Helper function to get the header to the right of a given header
 *
 * @param h original header
 *
 * @return header to the right of h
 */
header * get_right_header(header * h) {
	return get_header_from_offset(h, get_size(h));
}

/**
 * @brief Helper function to get the header to the left of a given header
 *
 * @param h original header
 *
 * @return header to the right of h
 */
inline static header * get_left_header(header * h) {
  return get_header_from_offset(h, -h->left_size);
}

/**
 * @brief Fenceposts are marked as always allocated and may need to have
 * a left object size to ensure coalescing happens properly
 *
 * @param fp a pointer to the header being used as a fencepost
 * @param left_size the size of the object to the left of the fencepost
 */
inline static void initialize_fencepost(header * fp, size_t left_size) {
	set_state(fp,FENCEPOST);
	set_size(fp, ALLOC_HEADER_SIZE);
	fp->left_size = left_size;
}

/**
 * @brief Helper function to maintain list of chunks from the OS for debugging
 *
 * @param hdr the first fencepost in the chunk allocated by the OS
 */
inline static void insert_os_chunk(header * hdr) {
  if (numOsChunks < MAX_OS_CHUNKS) {
    osChunkList[numOsChunks++] = hdr;
  }
}

/**
 * @brief given a chunk of memory insert fenceposts at the left and 
 * right boundaries of the block to prevent coalescing outside of the
 * block
 *
 * @param raw_mem a void pointer to the memory chunk to initialize
 * @param size the size of the allocated chunk
 */
inline static void insert_fenceposts(void * raw_mem, size_t size) {
  // Convert to char * before performing operations
  char * mem = (char *) raw_mem;

  // Insert a fencepost at the left edge of the block
  header * leftFencePost = (header *) mem;
  initialize_fencepost(leftFencePost, ALLOC_HEADER_SIZE);

  // Insert a fencepost at the right edge of the block
  header * rightFencePost = get_header_from_offset(mem, size - ALLOC_HEADER_SIZE);
  initialize_fencepost(rightFencePost, size - 2 * ALLOC_HEADER_SIZE);
}

/**
 * @brief Allocate another chunk from the OS and prepare to insert it
 * into the free list
 *
 * @param size The size to allocate from the OS
 *
 * @return A pointer to the allocable block in the chunk (just after the 
 * first fencpost)
 */
static header * allocate_chunk(size_t size) {
  void * mem = sbrk(size);
  
  insert_fenceposts(mem, size);
  header * hdr = (header *) ((char *)mem + ALLOC_HEADER_SIZE);
  set_state(hdr, UNALLOCATED);
  set_size(hdr, size - 2 * ALLOC_HEADER_SIZE);
  hdr->left_size = ALLOC_HEADER_SIZE;
  return hdr;
}

/**
 *
 */
void insert(header *h) {
  int index = (get_size(h) - ALLOC_HEADER_SIZE) / MIN_ALLOCATION - 1;
  if (index == 0) index = 1;
  if (index > N_LISTS - 1) index = N_LISTS - 1;
  header *dummy = &freelistSentinels[index];
  if (dummy->next == dummy) {
    dummy->next = h;
    dummy->prev = h;
    h->next = dummy;
    h->prev = dummy;
  } else {
    dummy->next->prev = h;
    h->next = dummy->next;
    h->prev = dummy;
    dummy->next = h;
  }
}

/*
 *
 */
header *no_split_alloc(header *ptr) {
  ptr->prev->next = ptr->next;
  ptr->next->prev = ptr->prev;
  ptr->prev = NULL;
  ptr->next = NULL;
  set_state(ptr, ALLOCATED);
  return (header *)(ptr->data);
}

/*
 *
 */
header *split_alloc(header *ptr, header *ptr2, int actual_size) {
  header *right = get_right_header(ptr);
  ptr2 = get_header_from_offset(ptr2, ptr->size_state - actual_size);
  set_size(ptr, ptr->size_state - actual_size);
  set_size(ptr2, actual_size);
  ptr2->left_size = ptr->size_state;
  right->left_size = ptr2->size_state;
  ptr2->prev = NULL;
  ptr2->next = NULL;
  set_state(ptr2, ALLOCATED);

  int new_size = get_size(ptr) - ALLOC_HEADER_SIZE;
  if (new_size / 8 < N_LISTS) {
    ptr->prev->next = ptr->next;
    ptr->next->prev = ptr->prev;
    ptr->prev = NULL;
    ptr->next = NULL;
    insert(ptr);
  }

  return (header *)(ptr2->data);
}

/*
 *
 */
void isolate(header *h) {
  h->prev->next = h->next;
  h->next->prev = h->prev;
  h->next = NULL;
  h->prev = NULL;
}

/**
 * @brief Helper allocate an object given a raw request size from the user
 *
 * @param raw_size number of bytes the user needs
 *
 * @return A block satisfying the user's request
 */
static inline header *allocate_object(size_t raw_size) {
  // TODO implement allocation
  if (raw_size == 0) return NULL;

  // Calculate the rounded alloc size
  int alloc_size = ((int)raw_size + 7) & (-MIN_ALLOCATION); // 7 is the largest remainder of MIN_ALLOCATION
  int actual_size = 0;
  if (alloc_size <= 2 * sizeof(header *)) {
    actual_size = sizeof(header);
  } else {
    actual_size = ALLOC_HEADER_SIZE + alloc_size;
  }

  // Use alloc size to calculate row number and check if row contains free block
  int row = (actual_size - ALLOC_HEADER_SIZE) / MIN_ALLOCATION - 1;
  if (row > N_LISTS - 1) row = N_LISTS - 1;
  header *ptr = NULL;
  header *ptr2 = NULL;
  int split;
  for (int i = row; i < N_LISTS; i++) {
    // Case: enters last row
    if (i == N_LISTS - 1) {
      ptr = &freelistSentinels[i];
      while (ptr->next != &freelistSentinels[i]) {
        ptr = ptr->next;
        if (actual_size <= ptr->size_state) {
          split = ptr->size_state - actual_size;
          if (split < sizeof(header)) {
            // Case: no split
            return no_split_alloc(ptr);
          } else {
            //Case: split
            ptr2 = ptr;
            return split_alloc(ptr, ptr2, actual_size);
          }
        }
      }
      break;
    }
    // Case: finds free block before last row
    if (freelistSentinels[i].next != &freelistSentinels[i]) {
      ptr = freelistSentinels[i].next;
      split = ptr->size_state - actual_size;
      if (split < sizeof(header)) {
        return no_split_alloc(ptr);
      } else {
        ptr2 = ptr;
        return split_alloc(ptr, ptr2, actual_size);
      }
    }
  }
  
  // Task 3: ptr did not find appropriate block in entire freelist
  header *first_header = allocate_chunk(ARENA_SIZE);
  if ((header *)((char *)lastFencePost + 2 * ALLOC_HEADER_SIZE) == first_header) {
    first_header = lastFencePost;
    set_size_and_state(first_header, ARENA_SIZE, UNALLOCATED);
    memset((void *)((char *)first_header + ALLOC_HEADER_SIZE), 0, ARENA_SIZE - ALLOC_HEADER_SIZE);
    header *last_header = get_left_header(first_header);
    if (get_state(last_header) == UNALLOCATED) {
      int last_header_size = get_size(last_header) - ALLOC_HEADER_SIZE;
      set_size(last_header, get_size(last_header) + get_size(first_header));
      memset((void *)first_header, 0, ALLOC_HEADER_SIZE);
      get_right_header(last_header)->left_size = get_size(last_header);
      if (last_header_size / 8 < N_LISTS) {
        isolate(last_header);
        insert(last_header);
      }
      lastFencePost = get_right_header(last_header);
    } else {
      insert(first_header);
      lastFencePost = get_right_header(first_header);
    }
    last_header = NULL;
  } else {
    insert(first_header);
    lastFencePost = get_right_header(first_header);
    insert_os_chunk(get_left_header(first_header));
  }
  first_header = NULL;
  return allocate_object(raw_size);
}

/**
 * @brief Helper to get the header from a pointer allocated with malloc
 *
 * @param p pointer to the data region of the block
 *
 * @return A pointer to the header of the block
 */
static inline header * ptr_to_header(void * p) {
  return (header *)((char *) p - ALLOC_HEADER_SIZE); //sizeof(header));
}

/**
 * @brief Helper to manage deallocation of a pointer returned by the user
 *
 * @param p The pointer returned to the user by a call to malloc
 */
static inline void deallocate_object(void * p) {
  // TODO implement deallocation
  // Freeing a null pointer
  if (!p) return;
  header *ptr = ptr_to_header(p);
  if (get_state(ptr) != ALLOCATED) {
    puts("Double Free Detected");
    assert(false);
  }
  header *left = get_left_header(ptr);
  header *right = get_right_header(ptr);

  if ((get_state(left) != UNALLOCATED) && (get_state(right) != UNALLOCATED)) {
    set_state(ptr, UNALLOCATED);
    memset(p, 0, get_size(ptr) - ALLOC_HEADER_SIZE);
    insert(ptr);
    return;
  }

  if ((get_state(left) == UNALLOCATED) && (get_state(right) != UNALLOCATED)) {
    int left_index = (get_size(left) - ALLOC_HEADER_SIZE) / 8 - 1;
    right->left_size = get_size(left) + get_size(ptr);
    set_size(left, get_size(left) + get_size(ptr));
    memset((void *)((char *)p - ALLOC_HEADER_SIZE), 0, get_size(ptr));
    if (left_index < N_LISTS - 1) {
      isolate(left);
      insert(left);
    }
    return;
  }

  if ((get_state(left) != UNALLOCATED) && (get_state(right) == UNALLOCATED)) {
    int right_index = (get_size(right) - ALLOC_HEADER_SIZE) / 8 - 1;
    memset(p, 0, get_size(ptr) - ALLOC_HEADER_SIZE);
    right->prev->next = ptr;
    right->next->prev = ptr;
    ptr->next = right->next;
    ptr->prev = right->prev;
    header *right_of_right = get_right_header(right);
    right_of_right->left_size = get_size(ptr) + get_size(right);
    set_state(ptr, UNALLOCATED);
    set_size(ptr, get_size(ptr) + get_size(right));
    memset((void *)right, 0, get_size(right));
    if (right_index < N_LISTS - 1) {
      isolate(ptr);
      insert(ptr);
    }
    return;
  }

  if ((get_state(left) == UNALLOCATED) && (get_state(right) == UNALLOCATED)) {
    int left_index = (get_size(left) - ALLOC_HEADER_SIZE) / 8 - 1;
    header* right_of_right = get_right_header(right);
    right_of_right->left_size = get_size(left) + get_size(ptr) + get_size(right);
    isolate(right);
    set_size(left, get_size(left) + get_size(ptr) + get_size(right));
    memset((void *)((char *)p - ALLOC_HEADER_SIZE), 0, get_size(ptr) + sizeof(header));
    if (left_index < N_LISTS - 1) {
      isolate(left);
      insert(left);
    }
    return;
  }
}

/**
 * @brief Helper to detect cycles in the free list
 * https://en.wikipedia.org/wiki/Cycle_detection#Floyd's_Tortoise_and_Hare
 *
 * @return One of the nodes in the cycle or NULL if no cycle is present
 */
static inline header * detect_cycles() {
  for (int i = 0; i < N_LISTS; i++) {
    header * freelist = &freelistSentinels[i];
    for (header * slow = freelist->next, * fast = freelist->next->next; 
         fast != freelist; 
         slow = slow->next, fast = fast->next->next) {
      if (slow == fast) {
        return slow;
      }
    }
  }
  return NULL;
}

/**
 * @brief Helper to verify that there are no unlinked previous or next pointers
 *        in the free list
 *
 * @return A node whose previous and next pointers are incorrect or NULL if no
 *         such node exists
 */
static inline header * verify_pointers() {
  for (int i = 0; i < N_LISTS; i++) {
    header * freelist = &freelistSentinels[i];
    for (header * cur = freelist->next; cur != freelist; cur = cur->next) {
      if (cur->next->prev != cur || cur->prev->next != cur) {
        return cur;
      }
    }
  }
  return NULL;
}

/**
 * @brief Verify the structure of the free list is correct by checkin for 
 *        cycles and misdirected pointers
 *
 * @return true if the list is valid
 */
static inline bool verify_freelist() {
  header * cycle = detect_cycles();
  if (cycle != NULL) {
    fprintf(stderr, "Cycle Detected\n");
    print_sublist(print_object, cycle->next, cycle);
    return false;
  }

  header * invalid = verify_pointers();
  if (invalid != NULL) {
    fprintf(stderr, "Invalid pointers\n");
    print_object(invalid);
    return false;
  }

  return true;
}

/**
 * @brief Helper to verify that the sizes in a chunk from the OS are correct
 *        and that allocated node's canary values are correct
 *
 * @param chunk AREA_SIZE chunk allocated from the OS
 *
 * @return a pointer to an invalid header or NULL if all header's are valid
 */
static inline header * verify_chunk(header * chunk) {
	if (get_state(chunk) != FENCEPOST) {
		fprintf(stderr, "Invalid fencepost\n");
		print_object(chunk);
		return chunk;
	}
	
	for (; get_state(chunk) != FENCEPOST; chunk = get_right_header(chunk)) {
		if (get_size(chunk)  != get_right_header(chunk)->left_size) {
			fprintf(stderr, "Invalid sizes\n");
			print_object(chunk);
			return chunk;
		}
	}
	
	return NULL;
}

/**
 * @brief For each chunk allocated by the OS verify that the boundary tags
 *        are consistent
 *
 * @return true if the boundary tags are valid
 */
static inline bool verify_tags() {
  for (size_t i = 0; i < numOsChunks; i++) {
    header * invalid = verify_chunk(osChunkList[i]);
    if (invalid != NULL) {
      return invalid;
    }
  }

  return NULL;
}

/**
 * @brief Initialize mutex lock and prepare an initial chunk of memory for allocation
 */
static void init() {
  // Initialize mutex for thread safety
  pthread_mutex_init(&mutex, NULL);

#ifdef DEBUG
  // Manually set printf buffer so it won't call malloc when debugging the allocator
  setvbuf(stdout, NULL, _IONBF, 0);
#endif // DEBUG

  // Allocate the first chunk from the OS
  header * block = allocate_chunk(ARENA_SIZE);

  header * prevFencePost = get_header_from_offset(block, -ALLOC_HEADER_SIZE);
  insert_os_chunk(prevFencePost);

  lastFencePost = get_header_from_offset(block, get_size(block));

  // Set the base pointer to the beginning of the first fencepost in the first
  // chunk from the OS
  base = ((char *) block) - ALLOC_HEADER_SIZE; //sizeof(header);

  // Initialize freelist sentinels
  for (int i = 0; i < N_LISTS; i++) {
    header * freelist = &freelistSentinels[i];
    freelist->next = freelist;
    freelist->prev = freelist;
  }

  // Insert first chunk into the free list
  header * freelist = &freelistSentinels[N_LISTS - 1];
  freelist->next = block;
  freelist->prev = block;
  block->next = freelist;
  block->prev = freelist;
}

/* 
 * External interface
 */
void * my_malloc(size_t size) {
  pthread_mutex_lock(&mutex);
  header * hdr = allocate_object(size); 
  pthread_mutex_unlock(&mutex);
  return hdr;
}

void * my_calloc(size_t nmemb, size_t size) {
  return memset(my_malloc(size * nmemb), 0, size * nmemb);
}

void * my_realloc(void * ptr, size_t size) {
  void * mem = my_malloc(size);
  memcpy(mem, ptr, size);
  my_free(ptr);
  return mem; 
}

void my_free(void * p) {
  pthread_mutex_lock(&mutex);
  deallocate_object(p);
  pthread_mutex_unlock(&mutex);
}

bool verify() {
  return verify_freelist() && verify_tags();
}

/*
 * Buddy Page Allocation Algorithm
 * SKELETON IMPLEMENTATION -- TO BE FILLED IN FOR TASK (2)
 */

/*
 * STUDENT NUMBER: s1746788
 */
#include <infos/mm/page-allocator.h>
#include <infos/mm/mm.h>
#include <infos/kernel/kernel.h>
#include <infos/kernel/log.h>
#include <infos/util/math.h>
#include <infos/util/printf.h>

using namespace infos::kernel;
using namespace infos::mm;
using namespace infos::util;

#define MAX_ORDER	17




/**
 * A buddy page allocation algorithm.
 */
class BuddyPageAllocator : public PageAllocatorAlgorithm
{
private:


	// My own helper Functions declared
	
	/**
	 * Checks if block contains the desired page
	 * @param block A pointer the array of page descriptors
	 * @param order The order of pages in the block
	 * @param pgd The desired page
	 * @return Returns TRUE if the page is inside the block, FALSE otherwise
	 */
	bool page_contained(PageDescriptor* block, int order, PageDescriptor* pgd) {
		// size is the power of 2 and shifting by x is the same as multiplying by 2^x
		int size = pages_per_block(order);
		// retrieves last page
		PageDescriptor* final_page = block + size;
		// finds if addres is between first and last page of block
		return (pgd >= block) && (pgd < final_page);
	}

	/**
	 * Checks if page is free
	 * @param pgd Page descriptor of page to be checked
	 * @param order The power of two number of contiguous pages to be checked
	 * @return If page found return the pointer of the free page, else nullptr
	 */
	PageDescriptor** page_is_free(PageDescriptor* pgd, int order) {

		auto point = &_free_areas[order];

		while (*point != nullptr) {
			if (*point == pgd) {
				return point;
			}
			// use to get next available
			point = &(*point)->next_free;
		}

		return nullptr;
	}


	/**
	 * recursive method that merges as many pages as it can with reference to the order
	 * @param pgd the array of page descriptors.
	 * @param order The power of two of number of contiguous pages to be merged.
	 */
	struct PageMerge {
		int order;
		PageDescriptor* pgd;
	};
	PageMerge page_merge(PageDescriptor *pgd, int order) {
		//checks
		assert(is_correct_alignment_for_order(pgd, order));
		assert(order >= 0 && order <= MAX_ORDER);
		// Can't merge further since nothing above maximum
		if(order == MAX_ORDER) return PageMerge{order,pgd};

		// recursively merge till limit is hit or buddy isn't free(to keep contiguous)
		auto to_be_merged = buddy_of(pgd,order);

		while((page_is_free(to_be_merged,order) != nullptr)) {
			// new pgd formed of higher order
			pgd = *merge_block(&pgd, order);
			order++;
			// get next buddy for merging
			to_be_merged = buddy_of(pgd, order);

			if (order == MAX_ORDER) {
				break;
			}
		}
		return PageMerge{order,pgd};
	}

	/**
	 * Returns the number of pages that comprise a 'block', in a given order.
	 * @param order The order to base the calculation off of.
	 * @return Returns the number of pages in a block, in the order.
	 */
	static inline constexpr uint64_t pages_per_block(int order)
	{
		/* The number of pages per block in a given order is simply 1, shifted left by the order number.
		 * For example, in order-2, there are (1 << 2) == 4 pages in each block.
		 */
		return (1 << order);
	}
	
	/**
	 * Returns TRUE if the supplied page descriptor is correctly aligned for the 
	 * given order.  Returns FALSE otherwise.
	 * @param pgd The page descriptor to test alignment for.
	 * @param order The order to use for calculations.
	 */
	static inline bool is_correct_alignment_for_order(const PageDescriptor *pgd, int order)
	{
		// Calculate the page-frame-number for the page descriptor, and return TRUE if
		// it divides evenly into the number pages in a block of the given order.
		return (sys.mm().pgalloc().pgd_to_pfn(pgd) % pages_per_block(order)) == 0;
	}
	
	/** Given a page descriptor, and an order, returns the buddy PGD.  The buddy could either be
	 * to the left or the right of PGD, in the given order.
	 * @param pgd The page descriptor to find the buddy for.
	 * @param order The order in which the page descriptor lives.
	 * @return Returns the buddy of the given page descriptor, in the given order.
	 */
	PageDescriptor *buddy_of(PageDescriptor *pgd, int order)
	{
		// (1) Make sure 'order' is within range
		if (order >= MAX_ORDER) {
			return NULL;
		}

		// (2) Check to make sure that PGD is correctly aligned in the order
		if (!is_correct_alignment_for_order(pgd, order)) {
			return NULL;
		}
				
		// (3) Calculate the page-frame-number of the buddy of this page.
		// * If the PFN is aligned to the next order, then the buddy is the next block in THIS order.
		// * If it's not aligned, then the buddy must be the previous block in THIS order.
		uint64_t buddy_pfn = is_correct_alignment_for_order(pgd, order + 1) ?
			sys.mm().pgalloc().pgd_to_pfn(pgd) + pages_per_block(order) : 
			sys.mm().pgalloc().pgd_to_pfn(pgd) - pages_per_block(order);
		
		// (4) Return the page descriptor associated with the buddy page-frame-number.
		return sys.mm().pgalloc().pfn_to_pgd(buddy_pfn);
	}
	
	/**
	 * Inserts a block into the free list of the given order.  The block is inserted in ascending order.
	 * @param pgd The page descriptor of the block to insert.
	 * @param order The order in which to insert the block.
	 * @return Returns the slot (i.e. a pointer to the pointer that points to the block) that the block
	 * was inserted into.
	 */
	PageDescriptor **insert_block(PageDescriptor *pgd, int order)
	{
		// Starting from the _free_area array, find the slot in which the page descriptor
		// should be inserted.
		PageDescriptor** slot = &_free_areas[order];
		
		// Iterate whilst there is a slot, and whilst the page descriptor pointer is numerically
		// greater than what the slot is pointing to.
		while (*slot && pgd > *slot) {
			slot = &(*slot)->next_free;
		}
		
		// Insert the page descriptor into the linked list.
		pgd->next_free = *slot;
		*slot = pgd;
		
		// Return the insert point (i.e. slot)
		return slot;
	}
	
	/**
	 * Removes a block from the free list of the given order.  The block MUST be present in the free-list, otherwise
	 * the system will panic.
	 * @param pgd The page descriptor of the block to remove.
	 * @param order The order in which to remove the block from.
	 */
	void remove_block(PageDescriptor *pgd, int order)
	{
		// Starting from the _free_area array, iterate until the block has been located in the linked-list.
		PageDescriptor **slot = &_free_areas[order];
		while (*slot && pgd != *slot) {
			slot = &(*slot)->next_free;
		}

		// Make sure the block actually exists.  Panic the system if it does not.
		assert(*slot == pgd);
		
		// Remove the block from the free list.
		*slot = pgd->next_free;
		pgd->next_free = NULL;
	}
	
	/**
	 * Given a pointer to a block of free memory in the order "source_order", this function will
	 * split the block in half, and insert it into the order below.
	 * @param block_pointer A pointer to a pointer containing the beginning of a block of free memory.
	 * @param source_order The order in which the block of free memory exists.  Naturally,
	 * the split will insert the two new blocks into the order below.
	 * @return Returns the left-hand-side of the new block.
	 */
	PageDescriptor *split_block(PageDescriptor **block_pointer, int source_order) {
		// Make sure there is an incoming pointer.
		assert(*block_pointer);
		
		// Make sure the block_pointer is correctly aligned.
		assert(is_correct_alignment_for_order(*block_pointer, source_order));
		
		// Ensure source_order is positive
		assert(source_order > 0);

		// Get the order of the target
		int target = source_order-1;

		// Blocks of interest
		PageDescriptor* left_half = *block_pointer;
		PageDescriptor* right_half = buddy_of(left_half, target);

		// Remove original block and insert into new
		remove_block(*block_pointer, source_order);
		insert_block(left_half, target);
		insert_block(right_half, target);

		// return left-hand-side of new block
		return left_half;
	}
	
	/**
	 * Takes a block in the given source order, and merges it (and it's buddy) into the next order.
	 * This function assumes both the source block and the buddy block are in the free list for the
	 * source order.  If they aren't this function will panic the system.
	 * @param block_pointer A pointer to a pointer containing a block in the pair to merge.
	 * @param source_order The order in which the pair of blocks live.
	 * @return Returns the new slot that points to the merged block.
	 */
	PageDescriptor **merge_block(PageDescriptor **block_pointer, int source_order) {
		assert(*block_pointer);
		
		// Make sure the area_pointer is correctly aligned.
		assert(is_correct_alignment_for_order(*block_pointer, source_order));

		// Since our target is above we have to be at least 1 below the Maximum
		assert(source_order < MAX_ORDER);

		// Get target and blocks of interest
		int target = source_order+1;
		auto left_buddy = *block_pointer;
		auto right_buddy = buddy_of(left_buddy, source_order);
		// Make sue ordering is correct
		if (left_buddy > right_buddy) {
			auto temp = left_buddy;
			left_buddy = right_buddy;
			right_buddy = temp;
		}

		// Remove old blocks and add new one
		remove_block(left_buddy, source_order);
		remove_block(right_buddy, source_order);
		return insert_block(left_buddy, target);
	}
	
public:
	/**
	 * Constructs a new instance of the Buddy Page Allocator.
	 */
	BuddyPageAllocator() {
		// Iterate over each free area, and clear it.
		for (unsigned int i = 0; i < ARRAY_SIZE(_free_areas); i++) {
			_free_areas[i] = NULL;
		}
	}
	
	/**
	 * Allocates 2^order number of contiguous pages
	 * @param order The power of two, of the number of contiguous pages to allocate.
	 * @return Returns a pointer to the first page descriptor for the newly allocated page range, or NULL if
	 * allocation failed.
	 */
	PageDescriptor *alloc_pages(int order) override {
		// Order checks
		assert(order >= 0 && order <= MAX_ORDER);/*************/

		// Iterate through free blocks
		int index_order = order;
		auto free_block = _free_areas[order];
		while(!free_block || index_order > order) {
			// if invalid order cannot allocate pages
			if(index_order > MAX_ORDER || index_order < 0) return nullptr;
			// If the current order is splittable then split and decrement
			if(_free_areas[index_order]) {
				free_block = split_block(&_free_areas[index_order], index_order);
				index_order--;
			}
			// If not then incremet till splittable
			else index_order++;
		}

		// Remove resultant lock and return it
		remove_block(free_block, order);
		return free_block;

	}
	
	/**
	 * Frees 2^order contiguous pages.
	 * @param pgd A pointer to an array of page descriptors to be freed.
	 * @param order The power of two number of contiguous pages to free.
	 */
	void free_pages(PageDescriptor *pgd, int order) override {
		// Make sure that the incoming page descriptor is correctly aligned
		// for the order on which it is being freed, for example, it is
		// illegal to free page 1 in order-1.
		assert(is_correct_alignment_for_order(pgd, order));

		// Verify order
		assert(order >= 0 && order <= MAX_ORDER);

		// Free starting pages 
		insert_block(pgd, order); 

		// Merge as many pages as possible 
		page_merge(pgd, order);
		
	}
	

	/**
	 * Reserves a specific page, so that it cannot be allocated.
	 * @param pgd The page descriptor of the page to reserve.
	 * @return Returns TRUE if the reservation was successful, FALSE otherwise.
	 */
	bool reserve_page(PageDescriptor *pgd) {
		// Exploration parameters
		int order = MAX_ORDER;
		PageDescriptor* current_block = nullptr;

		// Go through possible orders starting with largest
		while(order >= 0) {

			// Once order reaches 0 we have found the block
			if(order == 0 && current_block) {
				auto point = page_is_free(pgd, order);
				// if valid reserve and return true
				if(point == nullptr) return false;
				remove_block(*point, 0);
				return true; 
			}

			// If block containing the page has been found split further to search for specific page
			if(current_block != nullptr) {
				auto left_buddy = split_block(&current_block, order);
				int temp_order = order -1;

				// If its not in LHS then it must be in the RHS
				// Either way analyze block that contains it
				if(page_contained(left_buddy, temp_order, pgd)) current_block = left_buddy;
				else current_block = buddy_of(left_buddy, temp_order);

				// go on to next fraction of blocks until we eventually hit the page
				order = temp_order;
				continue;
			}

			// Block containing page not found so keep searching through free areas
			current_block = _free_areas[order];
			while(current_block != nullptr) {
				// If page is between base address of the current block and last page  of current_block 
				// we've found the bock of interest
				if(page_contained(current_block, order, pgd)) break;

				// Use next free to get next block in our search 
				current_block = current_block->next_free;
			}

			// check next order
			if(current_block == nullptr) order--;
		}

		// page not found withtin ay block
		return false;
	}

	
	/**
	 * Initialises the allocation algorithm.
	 * @return Returns TRUE if the algorithm was successfully initialised, FALSE otherwise.
	 */
	bool init(PageDescriptor *page_descriptors, uint64_t nr_page_descriptors) override {
		mm_log.messagef(LogLevel::DEBUG, "Buddy Allocator Initialising pd=%p, nr=0x%lx", page_descriptors, nr_page_descriptors);

		// Initialise the free area linked list for the maximum order
		// to initialise the allocation algorithm.
		
		auto order = MAX_ORDER;
		uint64_t remaining_pages = nr_page_descriptors;

		// Go through pages and checks
		do {

			auto block_size = pages_per_block(order);
			auto remainder_pages = remaining_pages % block_size;
			auto block_num = (remaining_pages - remainder_pages) / block_size;
			auto final_block = page_descriptors + (block_num * block_size);

			while(page_descriptors < final_block) {
				insert_block(page_descriptors, order);
				page_descriptors += block_size;
				remaining_pages -= block_size;
			} 

			order--;
			
		} while(remaining_pages > 0); 

		return true;
	}

	/**
	 * Returns the friendly name of the allocation algorithm, for debugging and selection purposes.
	 */
	const char* name() const override { return "buddy"; }
	
	/**
	 * Dumps out the current state of the buddy system
	 */
	void dump_state() const override
	{
		// Print out a header, so we can find the output in the logs.
		mm_log.messagef(LogLevel::DEBUG, "BUDDY STATE:");
		
		// Iterate over each free area.
		for (unsigned int i = 0; i < ARRAY_SIZE(_free_areas); i++) {
			char buffer[256];
			snprintf(buffer, sizeof(buffer), "[%d] ", i);
						
			// Iterate over each block in the free area.
			PageDescriptor *pg = _free_areas[i];
			while (pg) {
				// Append the PFN of the free block to the output buffer.
				snprintf(buffer, sizeof(buffer), "%s%lx ", buffer, sys.mm().pgalloc().pgd_to_pfn(pg));
				pg = pg->next_free;
			}
			
			mm_log.messagef(LogLevel::DEBUG, "%s", buffer);
		}
	}

	
private:
	PageDescriptor *_free_areas[MAX_ORDER];
};

/* --- DO NOT CHANGE ANYTHING BELOW THIS LINE --- */

/*
 * Allocation algorithm registration framework
 */
RegisterPageAllocator(BuddyPageAllocator);
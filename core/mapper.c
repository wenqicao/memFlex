#include <linux/slab.h>
#include "mapper.h"

extern unsigned long memswap_size;
size_t cur, cap;

unsigned long *init_mapper(size_t size){

	unsigned long *mapper = kmalloc(size*sizeof(unsigned long), GFP_KERNEL);

	if(mapper == NULL){
		printk(KERN_ERR "mapper initialization fails\n");
	}else{
		cur = 0;
		cap = size;
	}
	return mapper;
}

int insert_mapper(unsigned long *mapper, unsigned long offset){
	size_t tmp_cap;
	unsigned long *tmp_mapper;
	
	if(cur == cap){
		/*mapper is full, realloc before insert*/
		tmp_cap = (cap<<1);
		tmp_mapper = kmalloc(tmp_cap*sizeof(unsigned long), GFP_KERNEL);
	
		if(tmp_mapper == NULL){
			printk(KERN_ERR "mapper realloc fails\n");
			return -1;
		}	

		memcpy(tmp_mapper, mapper, cap*sizeof(unsigned long));
		kfree(mapper);
		mapper = tmp_mapper;
		cap = tmp_cap;
	}
	
	*(mapper + cur) = offset;
	cur++;

	return 0;
}

int delete_mapper(unsigned long *mapper, unsigned long offset){
	return 0;
}

unsigned long get_offset(unsigned long *mapper, unsigned long voffset){
	unsigned long idx = voffset/memswap_size;
	unsigned long offset = voffset%memswap_size;
	unsigned long start = *(mapper + idx);

	return start + offset;	
}

int destroy_mapper(unsigned long *mapper){
	kfree(mapper);
	return 1;
}

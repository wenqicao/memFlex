#ifndef __MAPPER_H__
#define __MAPPER_H__

unsigned long *init_mapper(size_t size);
int insert_mapper(unsigned long **mapper, unsigned long offset);
unsigned long get_offset(unsigned long *mapper, unsigned long offset);

#endif

#ifndef __BITMAP_H__
#define __BITMAP_H__

void setbit(char *p, unsigned long idx);
void clearbit(char *p, unsigned long idx);
void clear_all_bits(char *p, unsigned long width);
int first_zero_bit(char *p, unsigned long width);

#endif

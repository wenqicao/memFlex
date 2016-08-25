#define BITS_PER_BYTE 8
#include <linux/printk.h>
/*
*Starting from the first bit of @*p, set the @idx bit (to 1). 
*/
void setbit(char *p, unsigned long idx){
	unsigned long k1, k2;

	k1 = idx/(BITS_PER_BYTE*sizeof(char)); /*How many bytes*/
	k2 = idx%(BITS_PER_BYTE*sizeof(char)); /*How many bits left*/

	*(p + k1) |= 1<<k2;
		
}

/*
*Starting from the first bit of @*p, clear the @idx bit (to 0). 
*/
void clearbit(char *p, unsigned long idx){
	unsigned long k1, k2;

        k1 = idx/(BITS_PER_BYTE*sizeof(char)); /*How many bytes*/
        k2 = idx%(BITS_PER_BYTE*sizeof(char)); /*How many bits left*/

	*(p + k1) &= ~(1 << k2);
}

/*
*Starting from the first bit of @*p, clear all the @width bits
*/
void clear_all_bits(char *p, unsigned long width){
	unsigned long i;

	for(i = 0; i < width; i++){
		clearbit(p, i);
	}

}

/*
*Starting from the first bit of @*p, find the first zero bit
*At most @width bits will be checked
*Return the index of that bit
*/
int first_zero_bit(char *p, unsigned long width){
	unsigned long k1, k2, i, j, bit;
	char c;

	k1 = width/(BITS_PER_BYTE*sizeof(char)); /*How many bytes*/
	k2 = width%(BITS_PER_BYTE*sizeof(char)); /*How many bits left*/



	for(i = 0; i < k1; i++){
		c = *(p + i);
		for(j = 0; j < BITS_PER_BYTE; j++){
			bit = (c>>j)&0x01;
			if( bit == 0 )
				return i*BITS_PER_BYTE + j;
		}		
	}

	for(i = 0; i < k2; i++){
		c = *(p + k1);
		bit = (c>>i)&0x01;
		if( bit == 0){
			return k1*BITS_PER_BYTE + i;
		}
	}
	return -1;
}

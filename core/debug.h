#ifndef _DEBUG_H_
#define _DEBUG_H_


#include <linux/netdevice.h>

//#define DEBUG

#ifdef DEBUG
#define TRACE_ENTRY printk(KERN_CRIT "Entering %s\n", __func__)
#define TRACE_ENTRY_ONCE do{ static int once = 1; if (once){ TRACE_ENTRY; once = 0; } }while(0)
#define TRACE_EXIT  printk(KERN_CRIT "Exiting %s\n", __func__)
#define DUMP_STACK_ONCE do{ static int once = 1; if (once){ dump_stack(); once = 0; } }while(0)
#define DB( x, args... ) printk(KERN_CRIT "DEBUG: %s: line %d: " x, __FUNCTION__ , __LINE__ , ## args ); 
#else

#define TRACE_ENTRY do {} while (0)
#define TRACE_ENTRY_ONCE do {} while (0)
#define TRACE_EXIT  do {} while (0)
#define DUMP_STACK_ONCE do{} while(0)
#define DB(x, args...) do{} while(0)
#endif

#define TRACE_ERROR printk(KERN_CRIT "ERROR: Exiting %s\n", __func__)
#define EPRINTK( x, args... ) printk(KERN_CRIT "ERROR %s: line %d: " x, __FUNCTION__ , __LINE__ , ## args ); 
#define DPRINTK( x, args... ) printk(KERN_CRIT "%s: line %d: " x, __FUNCTION__ , __LINE__ , ## args );/*'## args' means ignor the comma before if args is null*/ 

/* DB is for all DEBUG info that only needed at debug time
 * EPRINTK is for all ERROR messages
 * DPRINTK is for all necessary init/end status milestone for user
 */



#define MAC_FMT				"%02x:%02x:%02x:%02x:%02x:%02x"
#define MAC_NTOA(addr) 			addr[0], \
					addr[1], \
					addr[2], \
					addr[3], \
					addr[4], \
					addr[5]



#endif /* _DEBUG_H_ */

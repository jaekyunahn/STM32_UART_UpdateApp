/*
 *  Include
 */

#include <pthread.h>
#include <stdio.h> 
#include <stdlib.h>
#include <string.h> 
#include <stdint.h>
#include <unistd.h>
#include <termios.h> 
#include <fcntl.h>    
#include <errno.h>
#include <time.h>
#include <signal.h>

#include <linux/fb.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/poll.h>

/*
 *  function prototypes
 */
void fPtheradStart(void);





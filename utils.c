#include <stdarg.h>
#include <unistd.h>

#include "utils.h"


int debug=FALSE;
void exit_error(char *format,...)
{
   va_list ap;

   va_start(ap,format);
   vfprintf(stderr,format,ap);
   va_end(ap);
   exit(-1);
}


void *myalloc(char *str,size_t size)
{
   void *retval=(void *)malloc(size);
   if(!retval)
      exit_error("ran out of memory allocating %s\n",str);
//   memset(retval,0,size);
   return(retval);
}




void debug_printf(char *format,...)
{
   va_list ap;

   if(debug)
   {
      va_start(ap,format);
      vfprintf(stdout,format,ap);
      va_end(ap);
   }
}

void myread(int fd,void *buf, size_t count)
{
	size_t currlen;
	long curr_buff=(long)buf;
	while(count>0)
	{
		currlen=read(fd,(void *)curr_buff,count);
		if(currlen==-1)
			exit_error("myread failed");
		

		count-=currlen;
		curr_buff=curr_buff+currlen;
	}

}



void mywrite(int fd,void *buf, size_t count)
{
	size_t currlen;
	long curr_buff=(long)buf;
	while(count>0)
	{
		currlen=write(fd,(void *)curr_buff,count);
		if(currlen==-1)
			exit_error("mywrite failed");
		

		count-=currlen;
		curr_buff=curr_buff+currlen;
	}

}

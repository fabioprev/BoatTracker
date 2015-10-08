#ifndef LIVEGRABBER_H_
#define LIVEGRABBER_H_

#include "grabber.h"

//time
//#ifdef LINUX
#include <sys/timeb.h>
#include <sys/time.h>
#include <time.h>
//#else
//#include <sys/timeb.h>
//#include <sys/types.h>
//#endif

class LiveGrabber: public Grabber
{	
	public:
		LiveGrabber(int camera_index=-1, int prev_offset=1);
		LiveGrabber(const char *rtsp_address, int prev_offset=1);
		virtual ~LiveGrabber();
		virtual bool getData();
		int getWidth(){return width;};
		int getHeight(){return height;};
		virtual unsigned long getTimeStamp();
		double getFps(){return fps;};
};


#endif /*LIVEGRABBER_H_*/

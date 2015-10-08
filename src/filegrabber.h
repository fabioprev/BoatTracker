#ifndef FILEGRABBER_H_
#define FILEGRABBER_H_

#include "grabber.h"
#include <unistd.h>

class FileGrabber: public Grabber
{
	private:
		double time_scale;

	public:
		FileGrabber(const char *fn, int prev_offset = 1);
		virtual ~FileGrabber();
		virtual bool getData();
		int getWidth(){return width;};
		int getHeight(){return height;};
		virtual unsigned long getTimeStamp();
		double getFps(){return fps;};
};


#endif /*FILEGRABBER_H_*/


#ifndef GRABBER_H_
#define GRABBER_H_

//C
#include <stdio.h>
#include <stdlib.h>

#include <iostream>
using namespace std;


// OpenCV
#include <opencv/cv.h>
#include <opencv/highgui.h>

class Grabber
{
	public:
		int prev_counter;
		int prev_offset;

		int frameNumber; // frame number
		int width, height;

		IplImage *frame;
		IplImage *previousFrame;

		unsigned long timestamp;

		CvCapture* capture;

		double fps;

	public:
		Grabber(){};
		virtual ~Grabber(){};
		virtual bool getData() = 0;
		virtual unsigned long getTimeStamp() = 0;
		virtual int getWidth() = 0;
		virtual int getHeight() = 0;
		virtual double getFps() = 0;
};


#endif /*GRABBER_H_*/


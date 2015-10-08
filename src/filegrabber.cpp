#include "filegrabber.h"

FileGrabber::FileGrabber(const char *fn, int prev_offset)
{
	prev_counter = 0;
	this->prev_offset = prev_offset;

    capture = cvCaptureFromFile(fn);
	if(!capture) {
		cout<<"unable to capture from "<<fn<<endl;
		cout<<"aborting..."<<endl;
		abort();
    }

	cvQueryFrame(capture);
    height = (int) cvGetCaptureProperty(capture, CV_CAP_PROP_FRAME_HEIGHT);
    width = (int) cvGetCaptureProperty(capture, CV_CAP_PROP_FRAME_WIDTH);
    fps = cvGetCaptureProperty(capture, CV_CAP_PROP_FPS);
	CvSize frameSize = cvSize(width, height);

	time_scale = 1000 / fps;

	frameNumber = 0;

	frame = cvCreateImage(frameSize, IPL_DEPTH_8U, 3);
	previousFrame = cvCreateImage(frameSize, IPL_DEPTH_8U, 3);
}

FileGrabber::~FileGrabber()
{
	cvReleaseImage(&frame);
	cvReleaseImage(&previousFrame);
	cvReleaseCapture(&capture);
}

bool FileGrabber::getData()
{
	prev_counter++;
	if(prev_counter == prev_offset) {
		cvCopy(frame, previousFrame);
		prev_counter = 0;
	}

    if(!cvGrabFrame( capture )) {
			return false;
    }

    cvCopy(cvRetrieveFrame(capture), frame);


	timestamp = getTimeStamp();

	frameNumber++;

	//simulating real data
	//Sleep(time_scale - 7.7);
    usleep(time_scale * 2.16);
    //Sleep(time_scale * 2.16);
	//Sleep(3000);

	return true;
}

unsigned long FileGrabber::getTimeStamp()
{
	return (long)(frameNumber * time_scale);
}



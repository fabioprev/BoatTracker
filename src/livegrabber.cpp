#include "livegrabber.h"

using namespace std;

LiveGrabber::LiveGrabber(int camera_index, int prev_offset)
{
	prev_counter = 0;
	this->prev_offset = prev_offset;

	capture = cvCaptureFromCAM(camera_index);
	if(!capture) {
		cout<<"unable to capture from camera."<<endl;
		cout<<"aborting..."<<endl;
		abort();
	}

	cvQueryFrame(capture);
	width = (int) cvGetCaptureProperty(capture, CV_CAP_PROP_FRAME_WIDTH);
	height = (int) cvGetCaptureProperty(capture, CV_CAP_PROP_FRAME_HEIGHT);

	CvSize frameSize = cvSize(width, height);
	frame = cvCreateImage(frameSize, IPL_DEPTH_8U, 3);
	previousFrame = cvCreateImage(frameSize, IPL_DEPTH_8U, 3);

	fps = -1;
	frameNumber = 0;
}

LiveGrabber::LiveGrabber(const char* rtsp_address, int prev_offset)
{
	prev_counter = 0;
	this->prev_offset = prev_offset;

	capture = cvCaptureFromFile(rtsp_address);
	if(!capture) {
		cout<<"unable to capture from camera."<<endl;
		cout<<"aborting..."<<endl;
		abort();
	}

	cvQueryFrame(capture);
	width = (int) cvGetCaptureProperty(capture, CV_CAP_PROP_FRAME_WIDTH);
	height = (int) cvGetCaptureProperty(capture, CV_CAP_PROP_FRAME_HEIGHT);
	fps = -1;
	CvSize frameSize = cvSize(width, height);

	frameNumber = 0;

	frame = cvCreateImage(frameSize, IPL_DEPTH_8U, 3);
	previousFrame = cvCreateImage(frameSize, IPL_DEPTH_8U, 3);
}

LiveGrabber::~LiveGrabber()
{
	cvReleaseImage(&frame);
	cvReleaseImage(&previousFrame);
	cvReleaseCapture(&capture);
}

bool LiveGrabber::getData()
{
	prev_counter++;
	if(prev_counter == prev_offset)
	{
		cvCopy(frame, previousFrame);
		prev_counter = 0;
	}

	if(!cvGrabFrame( capture )) {
		return false;
	}

	cvCopy(cvRetrieveFrame(capture), frame);

	timestamp = getTimeStamp();

	frameNumber++;
	return true;
}

// return timestamp in ms from midnight
unsigned long LiveGrabber::getTimeStamp()
{
#ifdef __linux__
        struct timeval tv;
        gettimeofday(&tv,0);
        return (unsigned long)(tv.tv_sec%86400)*1000+(unsigned long)tv.tv_usec/1000;
#else
        struct timeb timebuffer;
        ftime( &timebuffer );
        unsigned short  millitm = timebuffer.millitm;
        time_t sectm = timebuffer.time;
        return (unsigned long)(sectm%86400)*1000+(unsigned long)millitm;
#endif
}

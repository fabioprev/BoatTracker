#include "track.h"

using namespace std;

Track::Track(IplImage* frame, CvRect* r, int id)
{
	cvSetImageROI( frame, *r );
	hsvImage = cvCreateImage(cvSize(r->width, r->height), frame->depth, 3);
	cvCvtColor( frame, hsvImage, CV_BGR2HSV );
	
	cvResetImageROI( frame );

	for(int k = 0; k < 256; k++){
		HueHistogram[k] = 0;
		SaturationHistogram[k] = 0;
		ValueHistogram[k] = 0;
	}

	int counter = 0;

	uchar hue, saturation, value;

	for(int y = 0; y < hsvImage->height; y++) {
		for(int x = 0; x < hsvImage->width; x++)	{

			hue = (uchar)hsvImage->imageData[y*hsvImage->widthStep + x];
			saturation = (uchar)hsvImage->imageData[y*hsvImage->widthStep + x + 1];
			value = (uchar)hsvImage->imageData[y*hsvImage->widthStep + x + 2];
			HueHistogram[hue]++;
			SaturationHistogram[saturation]++;
			ValueHistogram[value]++;
			counter++;
		}
	}

	for(int i = 0; i < 256; i++){
		HueHistogram[i] /= counter;
		SaturationHistogram[i] /= counter;
		ValueHistogram[i] /= counter;
	}

	cvReleaseImage(&hsvImage);

	this->id = id;
	this->color = CV_RGB( rand()&255, rand()&255, rand()&255 );
	this->hit = true;
	this->miss = 0;
	this->cnt = 1;

	this->v_x = 0;
	this->v_y = 0;

	this->Rect.x = r->x;
	this->Rect.y = r->y;
	this->Rect.width = r->width;
	this->Rect.height = r->height;

	this->imageSize.width = frame->width;
	this->imageSize.height = frame->height;
}

Track::~Track()
{

}

double Track::calcDist(IplImage* frame, CvRect* r)
{
	double NewHueHistogram[256];
	double NewSaturationHistogram[256];
	double NewValueHistogram[256];

	cvSetImageROI( frame, *r );
	hsvImage = cvCreateImage(cvSize(r->width, r->height), frame->depth, 3);
	cvCvtColor( frame, hsvImage, CV_BGR2HSV );
	
	cvResetImageROI( frame );

	for(int k = 0; k < 256; k++){
		NewHueHistogram[k] = 0;
		NewSaturationHistogram[k] = 0;
		NewValueHistogram[k] = 0;
	}

	int counter = 0;

	uchar hue, saturation, value;

	for(int y = 0; y < hsvImage->height; y++) {
		for(int x = 0; x < hsvImage->width; x++)	{

			hue = (uchar)hsvImage->imageData[y*hsvImage->widthStep + x];
			saturation = (uchar)hsvImage->imageData[y*hsvImage->widthStep + x + 1];
			value = (uchar)hsvImage->imageData[y*hsvImage->widthStep + x + 2];
			NewHueHistogram[hue]++;
			NewSaturationHistogram[saturation]++;
			NewValueHistogram[value]++;
			counter++;
		}
	}

	for(int i = 0; i < 256; i++){
		NewHueHistogram[i] /= counter;
		NewSaturationHistogram[i] /= counter;
		NewValueHistogram[i] /= counter;
	}

	cvReleaseImage(&hsvImage);

	double bcoeff[3] = {0,0,0};
	for(int i = 0; i < 256; i++) {
		bcoeff[0] += sqrt(HueHistogram[i] * NewHueHistogram[i]);
		bcoeff[1] += sqrt(SaturationHistogram[i] * NewSaturationHistogram[i]);
		bcoeff[2] += sqrt(ValueHistogram[i] * NewValueHistogram[i]);
	}
	dist = 0;
	for(int i = 0; i < 3; i++){
		dist += ((3.0 - i)/6.0) * sqrt(1 - bcoeff[i]);
	}

	//cout << "distance " << dist << endl;

	/*
	if(bdist < 0.25) {
		prevRect.x = Rect.x;
		prevRect.y = Rect.y;
		prevRect.width = Rect.width;
		prevRect.height = Rect.height;

		Rect.x = (r->x + prevRect.x) / 2;
		Rect.y = (r->y + prevRect.y) / 2;
		Rect.width = (r->width + prevRect.width) / 2;
		Rect.height = (r->height + prevRect.height) / 2;	

		this->hit = true;
		this->miss = 0;
		this->cnt++;

		this->v_x = Rect.x - prevRect.x;
		this->v_y = Rect.y - prevRect.y;

		cvSetImageROI( frame, Rect );
		hsvImage = cvCreateImage(cvSize(Rect.width, Rect.height), frame->depth, 3);
		cvCvtColor( frame, hsvImage, CV_BGR2HSV );
	
		cvResetImageROI( frame );

		for(int k = 0; k < 256; k++){
			HueHistogram[k] = 0;
			SaturationHistogram[k] = 0;
			ValueHistogram[k] = 0;
		}

		counter = 0;

		for(int y = 0; y < hsvImage->height; y++) {
			for(int x = 0; x < hsvImage->width; x++)	{

				hue = (uchar)hsvImage->imageData[y*hsvImage->widthStep + x];
				saturation = (uchar)hsvImage->imageData[y*hsvImage->widthStep + x + 1];
				value = (uchar)hsvImage->imageData[y*hsvImage->widthStep + x + 2];
				HueHistogram[hue]++;
				SaturationHistogram[saturation]++;
				ValueHistogram[value]++;
				counter++;
			}
		}

		for(int i = 0; i < 256; i++){
			HueHistogram[i] /= counter;
			SaturationHistogram[i] /= counter;
			ValueHistogram[i] /= counter;
		}

		cvReleaseImage(&hsvImage);

		return true;
	}
	else {
		hit = false;
		return false;
	}
	*/

	return dist;

}

void Track::associate(IplImage* frame, CvRect* r, double dist, bool swap) {
	prevRect.x = Rect.x;
	prevRect.y = Rect.y;
	prevRect.width = Rect.width;
	prevRect.height = Rect.height;

	Rect.x = (r->x + prevRect.x * 3) / 4;
	Rect.y = (r->y + prevRect.y * 3) / 4;
	Rect.width = (r->width + prevRect.width * 3) / 4;
	Rect.height = (r->height + prevRect.height * 3) / 4;

	this->hit = true;
	this->miss = 0;
	if(!swap) {
		this->cnt++;
	}
	this->dist = dist;

	this->v_x = Rect.x - prevRect.x;
	this->v_y = Rect.y - prevRect.y;

	cvSetImageROI( frame, Rect );
	hsvImage = cvCreateImage(cvSize(Rect.width, Rect.height), frame->depth, 3);
	cvCvtColor( frame, hsvImage, CV_BGR2HSV );
	
	cvResetImageROI( frame );

	for(int k = 0; k < 256; k++){
		HueHistogram[k] = 0;
		SaturationHistogram[k] = 0;
		ValueHistogram[k] = 0;
	}

	int counter = 0;

	uchar hue, saturation, value;

	for(int y = 0; y < hsvImage->height; y++) {
		for(int x = 0; x < hsvImage->width; x++)	{

			hue = (uchar)hsvImage->imageData[y*hsvImage->widthStep + x];
			saturation = (uchar)hsvImage->imageData[y*hsvImage->widthStep + x + 1];
			value = (uchar)hsvImage->imageData[y*hsvImage->widthStep + x + 2];
			HueHistogram[hue]++;
			SaturationHistogram[saturation]++;
			ValueHistogram[value]++;
			counter++;
		}
	}

	for(int i = 0; i < 256; i++){
		HueHistogram[i] /= counter;
		SaturationHistogram[i] /= counter;
		ValueHistogram[i] /= counter;
	}

	cvReleaseImage(&hsvImage);
}



void Track::join(Track* bt) {

	//cout << "join " << this->id;

	if(this->id > bt->getId()) {

		this->id = bt->getId();
		this->setColor(*(bt->getColor()));
		this->Rect.x = (this->Rect.x + bt->getRect()->x*3) / 4;
		this->Rect.y = (this->Rect.y + bt->getRect()->y*3) / 4;
		this->Rect.width = (this->Rect.width + bt->getRect()->width*3) / 4;
		this->Rect.height = (this->Rect.height + bt->getRect()->height*3) / 4;
		//cout << " with " << this->id << endl;
	}
	else {
		this->Rect.x = (this->Rect.x*3 + bt->getRect()->x) / 4;
		this->Rect.y = (this->Rect.y*3 + bt->getRect()->y) / 4;
		this->Rect.width = (this->Rect.width*3 + bt->getRect()->width) / 4;
		this->Rect.height = (this->Rect.height*3 + bt->getRect()->height) / 4;
		//cout << " with " << bt->getId() << endl;
	}
	bt->setId(-1);
	this->miss = 0;
	this->cnt += bt->getCnt();
	this->hit = true;

	/*
	for(int k = 0; k < 256; k++){
		HueHistogram[k] = bt->getHueHistogram(k);
		SaturationHistogram[k] = bt->getSaturationHistogram(k);
		ValueHistogram[k] = bt->getValueHistogram(k);
	}

	this->v_y = bt->getVy();
	this->v_x = bt->getVx();
	*/

}

void Track::update(IplImage* frame) {

	/*
	prevRect.x = Rect.x;
	prevRect.y = Rect.y;
	prevRect.width = Rect.width;
	prevRect.height = Rect.height;

	Rect.x = prevRect.x + v_x;
	Rect.y = prevRect.y + v_y;
	

	if(Rect.x < 0) {
		Rect.x = 0;
	}
	if(Rect.x >= frame->width) {
		Rect.x = frame->width-2;
	}
	if(Rect.y < 0) {
		Rect.y = 0;
	}
	if(Rect.y >= frame->height) {
		Rect.y = frame->height-2;
	}
	if(Rect.x + Rect.width >= frame->width) {
		Rect.width = frame->width - Rect.x - 1;
	}
	if(Rect.y + Rect.height >= frame->height) {
		Rect.height = frame->height - Rect.y - 1;
	}

	cout << "Rect.x " << Rect.x << " Rect.y " << Rect.y << endl;
	cout << "Rect.width " << Rect.width << " Rect.height " << Rect.height << endl;

	*/

	cvSetImageROI( frame, Rect );
	hsvImage = cvCreateImage(cvSize(Rect.width, Rect.height), frame->depth, 3);
	cvCvtColor( frame, hsvImage, CV_BGR2HSV );

	cvResetImageROI( frame );

	for(int k = 0; k < 256; k++){
		HueHistogram[k] = 0;
		SaturationHistogram[k] = 0;
		ValueHistogram[k] = 0;
	}

	int counter = 0;
	uchar hue, saturation, value;

	for(int y = 0; y < hsvImage->height; y++) {
		for(int x = 0; x < hsvImage->width; x++)	{

			hue = (uchar)hsvImage->imageData[y*hsvImage->widthStep + x];
			saturation = (uchar)hsvImage->imageData[y*hsvImage->widthStep + x + 1];
			value = (uchar)hsvImage->imageData[y*hsvImage->widthStep + x + 2];
			HueHistogram[hue]++;
			SaturationHistogram[saturation]++;
			ValueHistogram[value]++;
			counter++;
		}
	}

	for(int i = 0; i < 256; i++){
		HueHistogram[i] /= counter;
		SaturationHistogram[i] /= counter;
		ValueHistogram[i] /= counter;
	}

	cvReleaseImage(&hsvImage);

}
	

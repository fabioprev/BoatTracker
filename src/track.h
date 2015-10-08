#ifndef _TRACK_H
#define _TRACK_H

#include <opencv/cv.h>
#include <opencv/highgui.h>
#include <iostream>

using namespace std;

class Track {

private:	
	//color histograms
	double HueHistogram[256];
	double SaturationHistogram[256];
	double ValueHistogram[256];

	IplImage* hsvImage;

	CvRect prevRect;	// location of the boat in the previous frame
	CvRect Rect;		// current boat-location estimate
	int miss;				// miss counter
	long id;				// id number
	int cnt;				// counter
	
	bool hit;				//last hit state

	int v_x;				//velocity along x axis
	int v_y;				//velocity along y axis

	CvScalar color;

	double dist;			//histogram distance

	CvSize imageSize;

public:
	Track(IplImage* frame, CvRect* r, int id);
	~Track();	
	double calcDist(IplImage* frame, CvRect* r);
	void join(Track* bt);
	void update(IplImage* frame);
	void associate(IplImage* frame, CvRect* r, double dist, bool swap);

	inline CvRect* getRect() {
		return &Rect;
	};

	inline bool getHit() {
		return hit;
	};

	inline void setHit(bool hit) {
		this->hit = hit;
	};

	inline int getMiss() {
		return miss;
	};

	inline void setMiss(int miss) {
		this->miss = miss;
	};

	inline void increaseMiss() {
		miss++;
	};

	inline long getId() {
		return id;
	};

	inline void setId(long id) {
		this->id = id;
	};

	inline CvScalar* getColor() {
		return &color;
	};

	inline void setColor(CvScalar color) {
		this->color.val[0] = color.val[0];
		this->color.val[1] = color.val[1];
		this->color.val[2] = color.val[2];
	};

	inline long getCnt() {
		return cnt;
	};

	inline void setCnt(long cnt) {
		this->cnt = cnt;
	};

	inline void increaseCnt() {
		cnt++;
	};

	inline double getHueHistogram(int k) {
		return HueHistogram[k];
	};

	inline double getSaturationHistogram(int k) {
		return SaturationHistogram[k];
	};

	inline double getValueHistogram(int k) {
		return ValueHistogram[k];
	};

	inline int getVx() {
		return v_x;
	};

	inline int getVy() {
		return v_y;
	};

	inline double getDist() {
		return dist;
	};

	inline CvSize getImageSize() {
		return imageSize;
	};

private:
	void cloneTrack(Track* bt);
};

#endif /*_TRACK_H*/

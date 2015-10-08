#ifndef _TRACKER_H
#define _TRACKER_H

#include <opencv/cv.h>
#include <opencv/highgui.h>
#include <iostream>

#include "boatdetector.h"
#include "track.h"

#include <vector>

using namespace std;

class Tracker {
private:
	vector<Track*>* tracks;
	vector<Track*>::iterator tracks_it;
	vector<Track*>* new_tracks;
	vector<Track*>::iterator new_it;
	vector<Track*>* old_tracks;
	vector<Track*>::iterator old_it;
	vector<Track*>* hit_tracks;
	vector<Track*>::iterator hit_it;
	long n_tracks;
	int associateThreshold;
	int missThreshold;
	int validThreshold;
	int limit;
	CvFont trackIDFont;
	vector<CvRect>* obs;
	vector<CvRect>::iterator obs_it;

public:
	Tracker(int associateThreshold = 10, int missThreshold = 10, int validThreshold = 10);
	~Tracker();
	void process(IplImage* image, IplImage* edgeImage, BoatDetector* detector);
	void draw(IplImage* img);
	void printTracks();
	inline vector<Track*>* getTracks() {
		return tracks;
	}

private:

	void merge();
	void update(IplImage* image, IplImage* edgeImage, int limit);
};

#endif /*_TRACKER_H_*/

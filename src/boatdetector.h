#ifndef _BOAT_DETECTOR_H_
#define _BOAT_DETECTOR_H_

#include <opencv/cv.h>
#include <opencv/highgui.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <float.h>
#include <limits.h>
#include <time.h>
#include <ctype.h>

#include <iostream>
#include <fstream>
#include <vector>

/*
#include <Windows.h>
#include <malloc.h>
#include <tchar.h> 
#include <wchar.h> 
#include <strsafe.h>
#include <direct.h>
*/

using namespace std;

class BoatDetector {
private:
	double scale;
	double scale_factor;
	int min_neighbors;
	int flags;
	CvSize min_size;
	char* cascade_name;
	CvMemStorage* storage;
	CvMemStorage* hough_storage;
	CvHaarClassifierCascade* cascade;

	vector<CvAvgComp>* result_vec;
	vector<CvRect>* boats;

    std::vector< std::vector<CvRect> > m_rectangles;

	int sea_limit;
	bool coastline;

public:
	BoatDetector(	double scale,
					double scale_factor,
					int min_neighbors,
					int flags,
					CvSize min_size,
					CvHaarClassifierCascade* cascade);
	~BoatDetector();
	void detect(IplImage* image, IplImage* edgeImage, bool coastline);
	void draw(IplImage* res);
	void printSettings();
	vector<CvRect>* getBoats();

    inline std::vector< std::vector<CvRect> > rectangles() const
    {
        return m_rectangles;
    }

	void houghline(IplImage* edgeImage, IplImage* image, IplImage* lineImage);

	inline int getSeaLimit() {
		return sea_limit;
	};
	inline bool getCoastline() {
		return coastline;
	};
};

#endif /*_BOAT_DETECTOR_H_*/

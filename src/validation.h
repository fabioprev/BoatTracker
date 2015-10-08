/*
 * validation.h
 *
 *  Created on: 23/nov/2010
 *      Author: Domenico
 */

#ifndef VALIDATION_H_
#define VALIDATION_H_

#include <opencv/cv.h>
#include <opencv/highgui.h>
#include <iostream>

#include "vtsreader.h"
#include "tracker.h"

using namespace std;

class Validation {

private:

	struct VTSLabel {
		string name;
		int x;
		int y;
	};

	struct VisualLabel {
		int id;
		CvRect r;
		int x;
		int y;
		double history_x;
		double history_y;
	};

	IplImage* validationImage;
	double w;
	double h;
	CvFont font;

	vector<VTSLabel>* vtsLabels;
	vector<VisualLabel>* visualLabelVector;
	vector<VisualLabel>* prev_visualLabelVector;

public:
	Validation();
	~Validation();
	void process(vector<Track*>* visualTracks, vector<VTSReader::FOVTrack>* vtsTracks,
			IplImage* frame, int validThreshold);
	inline IplImage* getValidationImage() {
		return validationImage;
	};
};


#endif /* VALIDATION_H_ */

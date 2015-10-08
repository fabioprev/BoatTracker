#include "boatdetector.h"

extern "C" {

extern void HaarDetectObjects( const CvArr* _img,
				const CvArr* _edgeImg,
				CvHaarClassifierCascade* cascade,
                /*CvMemStorage* storage,*/ double scaleFactor,
                int minNeighbors, int flags, CvSize minSize,
                //CvSeq* result_seq);
                vector<CvAvgComp>* result_vec);
}

BoatDetector::BoatDetector(	double scale,
				double scale_factor,
				int min_neighbors,
				int flags,
				CvSize min_size,
				CvHaarClassifierCascade* cascade) {
	this->scale = scale;
	this->scale_factor = scale_factor;
	this->min_neighbors = min_neighbors;
	this->flags = flags;
	this->min_size = min_size;
	this->cascade = cascade;
	this->storage = cvCreateMemStorage(0);
	this->boats = new vector<CvRect>();
	this->result_vec = new vector<CvAvgComp>();
	this->sea_limit = 0;
	this->hough_storage = cvCreateMemStorage(0);
}

BoatDetector::~BoatDetector() {
	cvReleaseMemStorage(&storage);
	cvReleaseMemStorage(&hough_storage);
	delete boats;
	delete result_vec;
}

void BoatDetector::detect( IplImage* image, IplImage* edgeImage, bool coastline )
{
	this->coastline = coastline;

	//cvClearMemStorage( storage );

    double t = (double)cvGetTickCount();

    //HAAR DETECTION

    result_vec->clear();

    HaarDetectObjects(	image,
								edgeImage,
								cascade,
								/*storage,*/
								scale_factor,
								min_neighbors,
								flags,
								min_size,
								result_vec);
    //END HAAR DETECTION

    //MANAGE OBSERVATIONS

	boats->clear();
	int numBoats = result_vec->size();
	CvRect* r = 0;
	CvRect* s = 0;

	for( int i = 0; i < numBoats; i++ )	{
		r = (CvRect*)(&result_vec->at(i).rect);

		//check if the observation is below the coast limit
		if(coastline && (r->y + r->height/2) < sea_limit) {
			continue;
		}

		bool add = true;
		bool swap = false;
		for(int j = 0; j < i; j++) {
			s = (CvRect*)(&result_vec->at(j).rect);
			if(r->x <= s->x && r->y <= s->y &&
					(r->x + r->width) >= (s->x + s->width) &&
					(r->y + r->height) >= (s->y + s->height) )
			{
				add = false;
				break;
			}
			else if(r->x > s->x && r->y > s->y &&
					(r->x + r->width) < (s->x + s->width) &&
					(r->y + r->height) < (s->y + s->height) )
			{
				swap = true;
				break;
			}
		}

		if(add) {
			boats->push_back(cvRect(r->x, r->y, r->width, r->height));
		}
		if(swap) {
			vector<CvRect>::iterator it;
			CvRect* t = 0;
			for(it = boats->begin(); it != boats->end(); ++it) {
				t = &(*it);
				if(s->x == t->x && s->y == t->y && s->width == t->width && s->height == t->height)
				{
					t->x = r->x;
					t->y = r->y;
					t->width = r->width;
					t->height = r->height;
				}
				break;
			}
		}
	}

	t = (double)cvGetTickCount() - t;
	//cout << "detection time = " << t/((double)cvGetTickFrequency()*1000.) << " ms" << endl;
}

void BoatDetector::draw(IplImage* res) {
	vector<CvRect>::iterator it;
    vector<CvRect> rects;
	CvRect* r = 0;
	for(it = boats->begin(); it != boats->end(); ++it) {
	    r = &(*it);
        cvRectangle(res, cvPoint(r->x, r->y), cvPoint(r->x + r->width, r->y + r->height), CV_RGB(255,0,0), 2);
        rects.push_back(*r);
    }
	if(sea_limit) {
		cvLine(	res, cvPoint(0, sea_limit), cvPoint(res->width, sea_limit),	CV_RGB(0, 0, 255), 3);
		//cvLine(	res, cvPoint(res->width/2, 0), cvPoint(res->width/2, res->height), CV_RGB(255, 255, 255), 1);
	}
    m_rectangles.push_back(rects);

}

vector<CvRect>* BoatDetector::getBoats() {
	return boats;
}

void BoatDetector::houghline(IplImage* edgeImage, IplImage* image, IplImage* lineImage) {

	//validation
	int points = 50; // points per row
	int rows = 3; // number of rows
	int ver_dist = 10; // vertical distance between points
	int hor_dist = image->width / points; // horizontal distance between points

	cvCopy(edgeImage, lineImage);

	CvSeq* hough_lines = 0;

	CvScalar line_color = cvScalar(120);

	hough_lines = cvHoughLines2( edgeImage, hough_storage, CV_HOUGH_STANDARD, 1, CV_PI/180, 100, 0, 0 );


	if(hough_lines->total == 0) {
		return;
	}

	bool find = false;

	CvPoint pt1, pt2;
	float* line;
	float theta;
	float rho;
	double a, b, x0, y0;
	for( int i = 0; i < min(hough_lines->total, 100); i++ )
	{
		line = (float*)cvGetSeqElem(hough_lines, i);
		theta = line[1];
		if(theta < 1.50 || theta > 1.60) {
			continue;
		}
		rho = line[0];

		a = cos(theta);
		b = sin(theta);
		x0 = a*rho;
		y0 = b*rho;
		pt1.x = cvRound(x0 + 1000*(-b));
		pt1.y = cvRound(y0 + 1000*(a));
		pt2.x = cvRound(x0 - 1000*(-b));
		pt2.y = cvRound(y0 - 1000*(a));		

		cvLine( lineImage, pt1, pt2, line_color, 2, CV_AA, 0 );
		find = true;
	}
	if(!find) {
		return;
	}

	bool run = true;
	int search_limit = lineImage->height - (ver_dist * rows);
	int line_step = lineImage->widthStep/sizeof(char);
	int img_step = image->widthStep/sizeof(char);
	int max_left, max_right;
	int tmp_limit;
	double count;
	while(run) {
		max_left = 0;
		max_right = 0;
		for(int i = ver_dist * rows; i < search_limit; i++) {
			if(((uchar)lineImage->imageData[i*line_step+3]) == line_color.val[0]) {
				if(i > max_left) {
					max_left = i;
				}
			}
			if(((uchar)lineImage->imageData[i*line_step + (lineImage->width-3)]) == line_color.val[0]) {
				if(i > max_right) {
					max_right = i;
				}
			}		
		}
		if(max_left == 0 || max_right == 0) {
			run = false;
			continue;
		}

		tmp_limit = (max_left + max_right) / 2;

		//limit validation
		count = 0;
		if(abs(max_left - max_right) < 10) {

			for(int i = tmp_limit - (ver_dist * rows), k = 0, t = rows*2; k < rows; i+=ver_dist, k++, t-=2) {
				for(int j = hor_dist; j < image->width; j+=hor_dist) {
					if(abs(image->imageData[i*img_step + j] -
						image->imageData[(i+t*ver_dist)*img_step + j] ) > 10 )
					{
						count++;
					}
				}
			}
		}
		if((count / (points * rows)) > 0.9 ) {

			sea_limit = tmp_limit;

			/*
			IplImage* ao = cvCloneImage(image);
			for(int i = tmp_limit - (ver_dist * rows), k = 0, t = rows*2; k < rows; i+=ver_dist, k++, t-=2) {
				for(int j = hor_dist; j < image->width; j+=hor_dist) {
					if(abs(image->imageData[i*img_step + j] -
							image->imageData[(i+t*ver_dist)*img_step + j] ) > 10 )
					{
						cvLine(ao, cvPoint(j,i), cvPoint(j,i), CV_RGB(0,0,0), 3);
						cvLine(ao, cvPoint(j,i+t*ver_dist), cvPoint(j,i+t*ver_dist), CV_RGB(255,255,255), 3);
					}
				}
			}

			cvShowImage("ao",ao);
			cvWaitKey(0);
			*/

			run = false;
		}
		else {
			search_limit = max(max_left, max_right);
		}
	}
}

void BoatDetector::printSettings() {
	cout << endl;
	cout << "DETECTOR SETTINGS" << endl;
	cout << "-------------------" << endl;
	cout << "SCALE: " << scale << endl;
	cout << "SCALE FACTOR: " << scale_factor << endl;
	cout << "MIN NEIGHBORS: "<< min_neighbors << endl;
	cout << "MIN WINDOW SIZE: " << min_size.width << " X " << min_size.height << endl;
	cout << endl;
}

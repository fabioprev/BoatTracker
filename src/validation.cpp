/*
 * validation.cpp
 *
 *  Created on: 23/nov/2010
 *      Author: Domenico
 */

#include "validation.h"

using namespace std;

Validation::Validation()
{
	w = 640;
	h = 480;
	validationImage = cvCreateImage(cvSize(w, h), 8, 3);
	cvInitFont(&font, CV_FONT_HERSHEY_SIMPLEX, 0.8, 0.8, 2);

	vtsLabels = new vector<VTSLabel>();
	visualLabelVector = new vector<VisualLabel>();
	prev_visualLabelVector = new vector<VisualLabel>();
}

Validation::~Validation()
{
	cvReleaseImage(&validationImage);
}

void Validation::process(vector<Track*>* visualTracks, vector<VTSReader::FOVTrack>* vtsTracks,
		IplImage* frame, int validThreshold) {

	vtsLabels->clear();

	prev_visualLabelVector->clear();
	vector<VisualLabel>::iterator vl_it;
	VisualLabel* vl = NULL;
	for(vl_it = visualLabelVector->begin(); vl_it != visualLabelVector->end(); ++vl_it) {
		prev_visualLabelVector->push_back(*vl_it);
	}
	visualLabelVector->clear();

	cvSet(validationImage, cvScalar(255,255,255));
	cvLine(validationImage, cvPoint(w/2, 0), cvPoint(w/2, h), CV_RGB(0,0,0), 1);
	cvLine(validationImage, cvPoint(0, 0), cvPoint(0, h), CV_RGB(255,0,0), 3);
	cvLine(validationImage, cvPoint(w-1, 0), cvPoint(w-1, h), CV_RGB(0,255,0), 3);

	//VTS TRACKS
	int vtstracks_size = (int)vtsTracks->size();
	if(vtstracks_size) {
		vector<VTSReader::FOVTrack>::iterator it;
		VTSReader::FOVTrack* t = 0;

		double min_camera_d = -1;
		double max_camera_d = -1;
		for(it = vtsTracks->begin(); it != vtsTracks->end(); ++it) {
			t = &(*it);
			//camera distance
			if(min_camera_d == -1) {
				min_camera_d = t->camera_distance;
			}
			else if(t->camera_distance < min_camera_d) {
				min_camera_d = t->camera_distance;
			}
			if(t->camera_distance > max_camera_d) {
				max_camera_d = t->camera_distance;
			}
		}

		double k_cam = (h-50) / (max_camera_d - min_camera_d);

		for(it = vtsTracks->begin(); it != vtsTracks->end(); ++it) {
			t = &(*it);

			double total_distance = t->right_distance + t->left_distance;
			double fov_d = t->left_distance / total_distance;

			if(min_camera_d == max_camera_d) {
				cvCircle(validationImage, cvPoint(w * fov_d, h-50), 5, CV_RGB(255,0,0), -1);
				cvPutText(validationImage, t->name.c_str(),
					cvPoint(w * fov_d, h-50), &font, CV_RGB(0, 0, 0));

				VTSLabel vtslabel;
				vtslabel.name.assign(t->name);
				vtslabel.x = w * fov_d;
				vtslabel.y = h-50;
				vtsLabels->push_back(vtslabel);
			}
			else {
				double s_cam = (t->camera_distance - min_camera_d) * k_cam + 20;

				cvCircle(validationImage, cvPoint(w * fov_d, h - s_cam),
					5, CV_RGB(255,0,0), -1);
				cvPutText(validationImage, t->name.c_str(),
					cvPoint(w * fov_d + 5, h - s_cam), &font, CV_RGB(0, 0, 0));

				VTSLabel vtslabel;
				vtslabel.name.assign(t->name);
				vtslabel.x = w * fov_d;
				vtslabel.y = h - s_cam;
				vtsLabels->push_back(vtslabel);
			}
		}

		//VISUAL TRACKS
		int visualtracks_size = (int)visualTracks->size();
		if(visualtracks_size) {
			vector<Track*>::iterator v_it;
			Track* vt = 0;

			double min_bottom_d = -1;
			double max_bottom_d = -1;

			for(v_it = visualTracks->begin(); v_it != visualTracks->end(); ++v_it) {
				vt = *v_it;
				if(vt->getCnt() <= validThreshold) {
					continue;
				}

				int bottom_d = vt->getImageSize().height - (vt->getRect()->y + vt->getRect()->height);

				if(min_bottom_d == -1) {
					min_bottom_d = bottom_d;
				}
				else if(bottom_d < min_bottom_d) {
					min_bottom_d = bottom_d;
				}
				if(bottom_d > max_bottom_d) {
					max_bottom_d = bottom_d;
				}
			}

			double norm_unit_bottom = (h - 100) / (max_bottom_d - min_bottom_d);

			double bottom_d;
			double left_d;
			double x;
			double y;
			VisualLabel visuallabel;

			for(v_it = visualTracks->begin(); v_it != visualTracks->end(); ++v_it) {
				vt = *v_it;
				if(vt->getCnt() <= validThreshold) {
					continue;
				}

				bottom_d = -1;
				left_d = -1;

				bottom_d = vt->getImageSize().height - (vt->getRect()->y + vt->getRect()->height);
				left_d = (vt->getRect()->x + vt->getRect()->width/2) / (double)frame->width;

				x = w * left_d;

				if(min_bottom_d == max_bottom_d) {
					//se vedo solo una nave assumo che sia la più vicina alla camera
					//per occlusione
					y = h - 50;
				}
				else {
					y = h - 50 - ((bottom_d - min_bottom_d) * norm_unit_bottom);
				}

				bool vl_find = false;
				bool x_disagree = false;
				bool y_disagree = false;
				for(vl_it = prev_visualLabelVector->begin(); vl_it != prev_visualLabelVector->end(); ++vl_it) {
					vl = &(*vl_it);
					if(vl->id == vt->getId()) {
						if(vl->history_x/x > 1.5 || vl->history_x/x < 0.67) {
							x_disagree = true;
						}

						if(vl->history_y/y > 1.5 || vl->history_y/y < 0.67) {
							y_disagree = true;
						}

						vl_find = true;
						break;
					}
				}

				visuallabel.id = vt->getId();
				visuallabel.r.x = vt->getRect()->x;
				visuallabel.r.y  = vt->getRect()->y;
				visuallabel.r.width = vt->getRect()->width;
				visuallabel.r.height = vt->getRect()->height;

				if(vl_find) {
					if(x_disagree) {
						x = x * 0.2 + vl->history_x * 0.8;
					}
					visuallabel.history_x = 0.2 * x + vl->history_x * 0.8;

					if(y_disagree) {
						y = y * 0.2 + vl->history_y * 0.8;
					}
					visuallabel.history_y = 0.2 * y + vl->history_y * 0.8;
				}
				else {
					visuallabel.history_x = x;
					visuallabel.history_y = y;
				}

				visuallabel.x = x;
				visuallabel.y = y;

				visualLabelVector->push_back(visuallabel);

				cvCircle(validationImage, cvPoint(x, y), 5, CV_RGB(0,0,0), -1);
				ostringstream oss;
				oss << vt->getId();
				cvPutText(validationImage, oss.str().c_str(), cvPoint(x + 5, y), &font, CV_RGB(0, 0, 0));

			}
		}


		// ASSOCIATION

		vector<VTSLabel>::iterator vts_label_it;
		VTSLabel* vts_label = 0;
		vector<VisualLabel>::iterator visual_label_it;
		VisualLabel* visual_label = 0;
		float dist;
		float min_dist;
		float max_dist = w/3;
		string* associated_name = 0;
		int* associated_x = 0;
		int* associated_y = 0;
		for(visual_label_it = visualLabelVector->begin(); visual_label_it != visualLabelVector->end(); ++visual_label_it) {
			visual_label = &(*visual_label_it);

			associated_name = 0;
			dist = -1;
			min_dist = -1;

			for(vts_label_it = vtsLabels->begin(); vts_label_it != vtsLabels->end(); ++vts_label_it) {
				vts_label = &(*vts_label_it);

				dist = sqrt(
						((visual_label->x - vts_label->x) * (visual_label->x - vts_label->x)) +
						((visual_label->y - vts_label->y) * (visual_label->y - vts_label->y)) );
				if(dist > max_dist) {
					continue;
				}
				if(min_dist < 0 || dist < min_dist)
				{
					min_dist = dist;
					associated_name = &(vts_label->name);
					associated_x = &(vts_label->x);
					associated_y = &(vts_label->y);
				}
			}

			if(associated_name) {
				cvPutText(frame, associated_name->c_str(),
							cvPoint(visual_label->r.x, visual_label->r.y - 5), &font, CV_RGB(0, 0, 0));
				cvLine(validationImage, cvPoint(visual_label->x, visual_label->y),
						cvPoint(*associated_x, *associated_y), CV_RGB(120, 120, 120), 2);
			}
		}
	}
	//else {
		//cout << "no vts tracks" << endl;
	//}
}


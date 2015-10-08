#include "tracker.h"

using namespace std;

Tracker::Tracker(int associateThreshold, int missThreshold, int validThreshold) {
	this->tracks = new vector<Track*>();
	this->new_tracks = new vector<Track*>();
	this->old_tracks = new vector<Track*>();
	this->hit_tracks = new vector<Track*>();
	this->n_tracks = -1;
	this->associateThreshold = associateThreshold;
	this->missThreshold = missThreshold;
	this->validThreshold = validThreshold;
	cvInitFont(&trackIDFont, CV_FONT_HERSHEY_SIMPLEX, 0.7, 0.7 );
}

Tracker::~Tracker() {
	delete this->tracks;
	delete this->new_tracks;
	delete this->old_tracks;
	delete this->hit_tracks;
}

void Tracker::process(IplImage* image, IplImage* edgeImage, BoatDetector* detector) {
	this->limit = detector->getSeaLimit();

	//check if the track is above the coastline
	bool discardCoastline = detector->getCoastline();

	this->obs = detector->getBoats();

	//reset hit label for each track
	Track* track = 0;
	for(tracks_it = tracks->begin(); tracks_it != tracks->end(); ++tracks_it) {
		track = *tracks_it;
		track->setHit(false);
	}

	//clear auxiliary vectors
	hit_tracks->clear();
	old_tracks->clear();
	new_tracks->clear();

	//scan boat observations
	CvRect* obsRect = 0;
	CvRect* trackRect = 0;
	Track* best_track = 0;
	for(obs_it = obs->begin(); obs_it != obs->end(); ++obs_it) {
		obsRect = &(*obs_it);
		if(tracks->empty()) {
			track = new Track(image, obsRect, ++n_tracks);
			new_tracks->push_back(track);
		}
		else {
			double best_dist = 0.4;
			int best_index = -2;
			int index = 0;
			double dist = 0;

			for(tracks_it = tracks->begin(); tracks_it != tracks->end(); ++tracks_it, index++) {
				track = *tracks_it;
				if(track->getId() == -1) {
					//the track is not valid
					continue;
				}
				trackRect = track->getRect();
				if(discardCoastline) {
					//the track is above the coastline
					int y = trackRect->y + trackRect->height/2;
					if(y < limit) {
						track->setId(-1);
						continue;
					}
				}
				//calculate histogram distance between the observation and the track
				//cout << "distance T: " << bt->getId() << " rect " << r->x << " " << r->y << endl;
				dist = track->calcDist(image, obsRect);

				if (dist < best_dist) {
					int c_x_track = trackRect->x + trackRect->width/2;
					int c_y_track = trackRect->y + trackRect->height/2;

					int c_x_obs = obsRect->x + obsRect->width/2;
					int c_y_obs = obsRect->y + obsRect->height/2;

					if(sqrt( ((c_x_track - c_x_obs) * (c_x_track - c_x_obs)) +
							 ((c_y_track - c_y_obs) * (c_y_track - c_y_obs)) ) < associateThreshold)
					{
						best_index = index;
						best_dist = dist;
					}
				}
			}//for tracks
			if(best_index == -2) {
				track = new Track(image, obsRect, ++n_tracks);
				new_tracks->push_back(track);
			}
			else {
				best_track = tracks->at(best_index);
				if(best_track->getHit()) {
					//best track already hit
					if(best_track->getDist() > best_dist) {
						//best track needs to be associated with current observation
						Track* new_track = new Track(image, best_track->getRect(), ++n_tracks);
						new_tracks->push_back(new_track);

						//associate the best track with the best observation
						//SWAP
						best_track->associate(image, obsRect, best_dist, true);
					}
					else {
						//best track is already associated with a nearer observation
						Track* new_track = new Track(image, obsRect, ++n_tracks);
						new_tracks->push_back(new_track);
					}
				}
				else {
					//best track not hit
					//NO SWAP
					best_track->associate(image, obsRect, best_dist, false);
				}
			}
		}
	}
	
	for(tracks_it = tracks->begin(); tracks_it != tracks->end(); ++tracks_it) {
		track = *tracks_it;
		if(track->getId() != -1) {
			if(track->getHit()) {
				hit_tracks->push_back(track);
			}
			else {
				old_tracks->push_back(track);
			}
		}
	}

	//DEBUG
	/*
	//add hit tracks
	cout << "HIT" << endl;
	for(hit_it = hit_tracks->begin(); hit_it != hit_tracks->end(); ++hit_it) {
		track = *hit_it;
		cout << track->getId() << endl;
	}

	//add new tracks
	cout << "NEW" << endl;
	for(new_it = new_tracks->begin(); new_it != new_tracks->end(); ++new_it) {
		track = *new_it;
		cout << track->getId() << endl;
	}

	//add old tracks
	cout << "OLD" << endl;
	for(old_it = old_tracks->begin(); old_it != old_tracks->end(); ++old_it) {
		track = *old_it;
		cout << track->getId() << endl;
	}
	*/
	//END DEBUG

	//clear tracks
	tracks->clear();

	//update old tracks
	update(image, edgeImage, limit);

	//merge new and old
	merge();

	//add hit tracks
	for(hit_it = hit_tracks->begin(); hit_it != hit_tracks->end(); ++hit_it) {
		track = *hit_it;
		tracks->push_back(track);
	}

	//add new tracks
	for(new_it = new_tracks->begin(); new_it != new_tracks->end(); ++new_it) {
		track = *new_it;
		tracks->push_back(track);
	}

	//add old tracks
	for(old_it = old_tracks->begin(); old_it != old_tracks->end(); ++old_it) {
		track = *old_it;
		if(track->getId() != -1) {
			tracks->push_back(track);
		}
	}
}

void Tracker::update(IplImage* image, IplImage* edgeImage, int limit) {

	Track* old_track = 0;

	for(old_it = old_tracks->begin(); old_it != old_tracks->end(); ++old_it) {
		old_track = *old_it;

		if(old_track->getId() == -1) {
			continue;
		}

		old_track->increaseMiss();

		if(old_track->getMiss() < missThreshold) {
			old_track->update(image);
		}
		else {

			//tenta di associarla ad una hit track
			cout << old_track->getId() << " prova di associazione con hit tracks" << endl;

			old_track->setId(-1);
		}
	}
}

void Tracker::merge() {
	Track* old_track = 0;
	CvRect* oldRect = 0;
	Track* new_track = 0;
	CvRect* newRect = 0;
	Track* hit_track = 0;
	CvRect* hitRect = 0;

	bool merged = false;
	for(old_it = old_tracks->begin(); old_it != old_tracks->end(); ++old_it) {
		old_track = *old_it;
		oldRect = old_track->getRect();

		if(old_track->getId() == -1) {
			continue;
		}

		for(new_it = new_tracks->begin(); new_it != new_tracks->end(); ++new_it) {
			new_track = *new_it;
			newRect = new_track->getRect();

			if(abs(oldRect->x - newRect->x) < associateThreshold &&
					abs(oldRect->y - newRect->y) < associateThreshold &&
					abs((oldRect->x + oldRect->width) - (newRect->x + newRect->width)) < associateThreshold &&
					abs((oldRect->y + oldRect->height) - (newRect->y + newRect->height)) < associateThreshold )
			{
				//le due tracce vanno fuse

				cout << "new track join old track" << endl;

				new_track->join(old_track);

				merged = true;
				break;
			}
		}

		if(merged) {
			continue;
		}

		for(hit_it = hit_tracks->begin(); hit_it != hit_tracks->end(); ++hit_it) {
			hit_track = *hit_it;
			hitRect = hit_track->getRect();

			if(abs(oldRect->x - hitRect->x) < associateThreshold &&
					abs(oldRect->y - hitRect->y) < associateThreshold &&
					abs((oldRect->x + oldRect->width) - (hitRect->x + hitRect->width)) < associateThreshold &&
					abs((oldRect->y + oldRect->height) - (hitRect->y + hitRect->height)) < associateThreshold )
			{
				//le due tracce vanno fuse

				cout << "hit track join old track" << endl;

				hit_track->join(old_track);
				break;
			}
		}
	}
}

void Tracker::draw(IplImage* img) {
	Track* bt = 0;
	int id = 0;
	for(tracks_it = tracks->begin(); tracks_it != tracks->end(); ++tracks_it) {
		bt = *tracks_it;
		id = bt->getId();

		if(bt->getCnt() > validThreshold && id != -1)
		{

			cvRectangle(img,
				cvPoint(bt->getRect()->x, bt->getRect()->y),
				cvPoint(bt->getRect()->x+bt->getRect()->width,
					bt->getRect()->y+bt->getRect()->height),
					*(bt->getColor()), 2);
			
			/*
			cvRectangle(img,
				cvPoint(bt->getRect()->x, bt->getRect()->y),
				cvPoint(bt->getRect()->x+30,
					bt->getRect()->y+20),
					CV_RGB(255,255,255), -1);
			*/

			ostringstream oss;
			oss << bt->getId();

			cvPutText(img, oss.str().c_str(),
					cvPoint(bt->getRect()->x+2, bt->getRect()->y+18),
					&trackIDFont, CV_RGB(0,0,0) );
		}
	}
}

void Tracker::printTracks() {
	cout << "---------------" << endl;
	cout << "PRINT TRACKS" << endl;
	cout << "---------------" << endl;
	Track* track = 0;
	for(tracks_it = tracks->begin(); tracks_it != tracks->end(); ++tracks_it) {
		track = *tracks_it;
		cout << "ID: " << track->getId() << endl;
		cout << "CNT: " << track->getCnt() << endl;
		cout << "MISS: " << track->getMiss() << endl;
		cout << "HIT: " << track->getHit() << endl;
		cout << endl;
	}
	cout << "---------------" << endl;
	cout << "END TRACKS" << endl;
	cout << "---------------" << endl;
	cout << endl;
}

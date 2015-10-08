/*
 * vtsreader.cpp
 *
 *  Created on: 09/nov/2010
 *      Author: Domenico
 */

#include "vtsreader.h"

using namespace std;

VTSReader::VTSReader(bool datacollection, char* dcVideoName)
{
	camera_heading = -1;

	camera_heading_user = -1;

	camera_fov = 8;

	camera_fov_user = -1;

	FOVTrackVector = new vector<FOVTrack>();
	FOVTrackVector_stable = new vector<FOVTrack>();

	if(datacollection){
		this->datacollection = true;
		dc_prefix.assign(dcVideoName);
		size_t found;
		found = dc_prefix.find_last_of(".");
		if(found != string::npos) {
			dc_prefix.substr(0, found);
			dc_prefix += "_";
		}
		else {
			cout << "ERROR: DATACOLLECTION FILE not valid." << endl;
			cout << "exiting..." << endl;
			exit(EXIT_FAILURE);
		}
		dc_counter = 0;
	}
	else {
		this->datacollection = false;
	}
}

VTSReader::~VTSReader()
{
	cvReleaseImage(&vtsImage);

	cvReleaseImage(&messageImage);

	delete FOVTrackVector;

	delete FOVTrackVector_stable;

	cvDestroyWindow("VTS");
}

bool VTSReader::init(const char* vts_cfg, CvSize &vts_image_size) {

	if(!readCfgFile(vts_cfg, vts_image_size)) {
		return false;
	}

	reference_latitude = reference_lat_degrees * 60000 +
								reference_lat_minutes * 1000 +
								reference_lat_seconds;
	reference_longitude = reference_lon_degrees * 60000 +
								reference_lon_minutes * 1000 +
								reference_lon_seconds;

	camera_latitude = camera_lat_degrees * 60000 +
								camera_lat_minutes * 1000 +
								camera_lat_seconds;
	camera_longitude = camera_lon_degrees * 60000 +
								camera_lon_minutes * 1000 +
								camera_lon_seconds;

	//posizione della camera nell'immagine
	camera_x = cvRound(getXImage(camera_longitude));
	camera_y = cvRound(getYImage(camera_latitude));

	cvInitFont(&cameraFont, CV_FONT_HERSHEY_SIMPLEX, 0.5, 0.5, 2);
	cvInitFont(&trackFont, CV_FONT_HERSHEY_PLAIN, 0.8, 0.8, 2);

	vtsTrackVector = new vector<VTSTrack>();

	current_time = -1;
	current_time_hours = -1;
	current_time_minutes = -1;
	current_time_seconds = -1;
	current_time_milliseconds = -1;

	return true;
}

bool VTSReader::getData(long &time) {
	int res = pcap_next_ex( adhandle, &header, &pkt_data );
	if(res == 0) {
		cout << "PCAP: Timeout elapsed" << endl;
		time = current_time;
		return true;
	}
	if(res == -1){
		cout << "PCAP: Error reading the packets: " << pcap_geterr(adhandle) << endl;
		time = current_time;
		return true;
	}
	if(res == -2) {
		cout << "PCAP: EOF reached." << endl;
		time = current_time;
		return false;
	}
	// define/compute ip header offset
	ip = (struct sniff_ip*)(pkt_data + SIZE_ETHERNET);
	size_ip = IP_HL(ip)*4;
	if (size_ip < 20) {
		cout << "PCAP:   * Invalid IP header length: " << size_ip << " bytes" << endl;
		time = current_time;
		return true;
	}
	// define/compute tcp header offset
	udp = (struct sniff_udp*)(pkt_data + SIZE_ETHERNET + SIZE_UDP);
	// define/compute udp payload (segment) offset
	payload = (u_char *)(pkt_data + SIZE_ETHERNET + size_ip + SIZE_UDP);
	// compute udp payload (segment) size
	size_payload = ntohs(ip->ip_len) - (size_ip + SIZE_UDP);
	if( size_payload > ntohs(udp->uh_ulen) ) {
		size_payload = ntohs(udp->uh_ulen);
	}
	pckstr.assign(checkmessagecatandtype(payload, size_payload));
	if(pckstr == "MSFST" && checkmessagelength(payload, size_payload))
	{
		msg.assign(decodeST(payload, size_payload, 1));

		if(msg == "") {
			time = current_time;
			return true;
		}
		while(msg.size() > 0) {

			index_lat = msg.find("lat:,");
			lat = msg.substr(index_lat + 5, msg.size() - (index_lat + 5));
			index_lat = lat.find_first_of(",");
			lat = lat.substr(0, index_lat);

			lat_degrees = atoi(lat.substr(0, lat.find(" ")).c_str());
			lat = lat.substr(lat.find(" ")+1);

			lat_minutes = atoi(lat.substr(0, lat.find(" ")).c_str());
			lat = lat.substr(lat.find(" ")+1);

			lat_seconds = atoi(lat.c_str());

			index_lon = msg.find("lon:,");
			lon = msg.substr(index_lon + 5, msg.size() - (index_lon + 5));
			index_lon = lon.find_first_of(",");
			lon = lon.substr(0, index_lon);

			lon_degrees = atoi(lon.substr(0, lon.find(" ")).c_str());
			lon = lon.substr(lon.find(" ")+1);

			lon_minutes = atoi(lon.substr(0, lon.find(" ")).c_str());
			lon = lon.substr(lon.find(" ")+1);

			lon_seconds = atoi(lon.c_str());

			index_timestamp = msg.find("time:,");
			timestamp = msg.substr(index_timestamp + 6, msg.size() - (index_timestamp + 6));
			index_timestamp = timestamp.find_first_of(",");
			timestamp = timestamp.substr(0, index_timestamp);
			time_hours = atoi(timestamp.substr(0, timestamp.find(" ")).c_str());
			timestamp = timestamp.substr(timestamp.find(" ")+1);
			time_minutes = atoi(timestamp.substr(0, timestamp.find(" ")).c_str());
			timestamp = timestamp.substr(timestamp.find(" ")+1);
			time_seconds = atoi(timestamp.substr(0, timestamp.find(" ")).c_str());
			timestamp = timestamp.substr(timestamp.find(" ")+1);
			time_milliseconds = atoi(timestamp.c_str());

			index_name = msg.find("name:,");
			name = msg.substr(index_name + 6, msg.size() - (index_name + 6));
			index_name = name.find_first_of(",");
			name = name.substr(0, index_name);

			pos = name.find_last_not_of(' ');
			if(pos != string::npos) {
				name.erase(pos + 1);
				pos = name.find_first_not_of(' ');
				if(pos != string::npos)
					name.erase(0, pos);
			}
			else {
				name = "-";
			}

			index_stn = msg.find("stn:,");
			stn = msg.substr(index_stn + 5, msg.size() - (index_stn + 6));
			index_stn = stn.find_first_of(",");
			stn = stn.substr(0, index_stn);

			track_time = time_hours * 3600000 + time_minutes * 60000 +
							time_seconds * 1000 + time_milliseconds;

			track_latitude = lat_degrees * 60000 +
							lat_minutes * 1000 + lat_seconds;
			track_longitude = lon_degrees * 60000 +
							lon_minutes * 1000 + lon_seconds;

			//posizione della traccia nell'immagine
			track_x = getXImage(track_longitude);
			track_y = getYImage(track_latitude);

			vtsTrack.name.assign(name);
			vtsTrack.stn.assign(stn);
			vtsTrack.latitude = track_latitude;
			vtsTrack.longitude = track_latitude;
			vtsTrack.x = track_x;
			vtsTrack.y = track_y;

			if(track_time - current_time > refresh_time || current_time < 0)
			{
				if(current_time < 0) {
					cvCopy(backgroundImage, vtsImage);
				}
				current_time = track_time;
				current_time_hours = time_hours;
				current_time_minutes = time_minutes;
				current_time_seconds = time_seconds;
				current_time_milliseconds = time_milliseconds;

				//CAMERA POSITION
				cvLine(vtsImage, cvPoint(camera_x, camera_y),
						cvPoint(camera_x, camera_y - 50), CV_RGB(0,0,255), 1);
				cvLine(vtsImage, cvPoint(camera_x, camera_y - 50),
						cvPoint(camera_x + 5, camera_y - 50 + 5), CV_RGB(0,0,255), 1);
				cvLine(vtsImage, cvPoint(camera_x, camera_y - 50),
						cvPoint(camera_x - 5, camera_y - 50 + 5), CV_RGB(0,0,255), 1);
				cvCircle(vtsImage, cvPoint(camera_x, camera_y), 5, CV_RGB(0,0,255), CV_FILLED);
				cvPutText(vtsImage, "CAMERA", cvPoint(camera_x + 5,
						camera_y), &cameraFont, CV_RGB(0,0,0));

				//CURRENT TIME
				ostringstream oss;
				if(current_time_hours < 10) {
					oss << "0" << current_time_hours << ":";
				}
				else {
					oss << current_time_hours << ":";
				}
				if(current_time_minutes < 10) {
					oss << "0" << current_time_minutes << ":";
				}
				else {
					oss << current_time_minutes << ":";
				}
				if(current_time_seconds < 10) {
					oss << "0" << current_time_seconds << ":";
				}
				else {
					oss << current_time_seconds << ":";
				}
				if(current_time_milliseconds < 10 ) {
					oss << "00" << current_time_milliseconds;
				}
				else if(current_time_milliseconds < 100 ) {
					oss << "0" << current_time_milliseconds;
				}
				else {
					oss << current_time_milliseconds;
				}
				cvPutText(vtsImage, oss.str().c_str(), cvPoint(10, 20), &cameraFont, CV_RGB(0,0,0));

				FOVTrackVector_stable->clear();
				vector<VTSReader::FOVTrack>::iterator it;
				for(it = FOVTrackVector->begin(); it != FOVTrackVector->end(); ++it) {
					FOVTrackVector_stable->push_back(*it);
				}

				if(datacollection) {
					string dc_name(dc_prefix);
					ostringstream oss;
					oss << dc_counter << ".jpg";
					dc_name += oss.str();
					cvSaveImage(dc_name.c_str(), vtsImage);
					dc_counter++;
				}

				cvShowImage("VTS", vtsImage);
				cvWaitKey(1);

				vtsTrackVector->clear();

				FOVTrackVector->clear();

				cvCopy(backgroundImage, vtsImage);

				camera_heading = camera_heading_user;
				camera_fov = camera_fov_user;
				left_heading = camera_heading - camera_fov/2;
				if(left_heading < 0) {
					left_heading += 360;
				}
				right_heading = camera_heading + camera_fov/2;
				if(right_heading >= 360) {
					right_heading -= 360;
				}
			}

			if(camera_heading >= 0) {
				ostringstream oss;
				oss << "HEADING " << camera_heading;
				cvPutText(vtsImage, oss.str().c_str(), cvPoint(130, 20), &cameraFont, CV_RGB(0,0,0));
				//central
				getCameraLine(camera_line, camera_heading);
				drawLine(&camera_line, CV_RGB(255, 255, 255), vtsImage->width);
				//left
				getCameraLine(leftside_line, left_heading);
				drawLine(&leftside_line, CV_RGB(255, 0, 0), vtsImage->width);
				//right
				getCameraLine(rightside_line, right_heading);
				drawLine(&rightside_line, CV_RGB(0, 255, 0), vtsImage->width);
			}
			else {
				cvPutText(vtsImage, "NO HEADING", cvPoint(130, 20), &cameraFont, CV_RGB(0,0,0));
			}

			vtsTrackVector->push_back(vtsTrack);

			if(name == "-") {
				cvPutText(vtsImage, stn.c_str(), cvPoint(cvRound(track_x)+5,
					cvRound(track_y)), &trackFont, CV_RGB(0,0,0));
				//cout << stn << endl;
			}
			else {
				cvPutText(vtsImage, name.c_str(), cvPoint(cvRound(track_x)+5,
					cvRound(track_y)), &trackFont, CV_RGB(0,0,0));
				//cout << name << endl;
			}

			if( camera_heading >= 0 &&
				cvRound(track_x) > 0 && cvRound(track_y) > 0 &&
				cvRound(track_x) < vtsImage->width && cvRound(track_y) < vtsImage->height) {

				bool left_sign = false; //false --> '<'  true --> '>'
				bool right_sign = false;//false --> '<'  true --> '>'
				bool check = false;
				bool add = false;

				if(camera_line.undefinedslope) {
					if(camera_line.northward) {
						if(track_y < camera_y) {
							check = true;
						}
					}
					else {
						if(track_y >= camera_y) {
							check = true;
						}
					}
				}
				else {
					if(camera_line.westward) {
						if(track_x < camera_x) {
							check = true;
						}
					}
					else {
						if(track_x >= camera_x) {
							check = true;
						}
					}
				}

				if(check) {

					//ATTENZIONE (0,0) TOP LEFT IMMAGINE
					//pertanto expression < 0 --> true
					double expression = 0;
					if(leftside_line.undefinedslope) {

						expression = track_x - leftside_line.q;
						if(expression < 0) {
							left_sign = true; // a destra
						}
					}
					else {

						expression = track_y - (leftside_line.m * track_x) - leftside_line.q;
						if(expression < 0) {
							left_sign = true;
						}
					}

					if(rightside_line.undefinedslope) {

						expression = track_x - rightside_line.q;
						if(expression < 0) {
							right_sign = true; //a destra
						}
					}
					else {
						expression = track_y - (rightside_line.m * track_x) - rightside_line.q;
						if(expression < 0) {
							right_sign = true;
						}
					}

					//find if the track is in the FOV
					if(left_heading > 0 && left_heading < 180) {
						if(right_heading < 180) {
							if(!left_sign && right_sign) {
								add = true;
							}
						}
						else {
							if(!left_sign && !right_sign) {
								add = true;
							}
						}
					}
					else if(left_heading > 180 && left_heading < 360) {
						if(right_heading < 90) {
							if(left_sign && right_sign) {
								add = true;
							}
						}
						else {
							if(left_sign && !right_sign) {
								add = true;
							}
						}
					}
					else if(left_heading == 0) {
						if(left_sign && right_sign) {
							add = true;
						}
					}
					else if(left_heading == 180) {
						if(left_sign && !right_sign) {
							add = true;
						}
					}
				}//check

				if(add) {
					FOVTrack fovTrack;
					if(name=="-") {
						fovTrack.name.assign(stn);
					}
					else {
						fovTrack.name.assign(name);
					}
					fovTrack.p = cvPoint2D64f(track_x, track_y);

					if(camera_line.undefinedslope) {
						if(track_x < camera_x) {
							fovTrack.leftside = true;
						}
						else {
							fovTrack.leftside = false;
						}
					}
					else if(camera_line.m == 0) {
						if(track_x < camera_x) {
							fovTrack.leftside = true;
						}
						else {
							fovTrack.leftside = false;
						}
					}
					else {
						double d = track_y - (camera_line.m * track_x) - camera_line.q;
						if(camera_line.westward) {
							if(d > 0) {
								fovTrack.leftside = true;
							}
							else {
								fovTrack.leftside = false;
							}
						}
						else {
							if(d < 0) {
								fovTrack.leftside = true;
							}
							else {
								fovTrack.leftside = false;
							}
						}
					}
					//camera_distance
					fovTrack.camera_distance =
							sqrt(((track_x - camera_x) * (track_x - camera_x)) +
									((track_y - camera_y) * (track_y - camera_y)));

					//left_distance
					if(leftside_line.undefinedslope) {
						fovTrack.left_distance = abs(track_x - leftside_line.q);
					}
					else if(leftside_line.m == 0) {
						fovTrack.left_distance = abs(track_y - leftside_line.q);
					}
					else {
						double p_x = (track_y - leftside_line.q) * leftside_line.m + track_x;
						p_x /= (leftside_line.m * leftside_line.m) + 1;
						double p_y = p_x * leftside_line.m + leftside_line.q;

						fovTrack.left_distance =
							sqrt(((track_x - p_x) * (track_x - p_x)) +
								((track_y - p_y) * (track_y - p_y)));
					}
					//right_distance
					if(rightside_line.undefinedslope) {
						fovTrack.right_distance = abs(track_x - rightside_line.q);
					}
					else if(rightside_line.m == 0) {
						fovTrack.right_distance = abs(track_y - rightside_line.q);
					}
					else {
						double p_x = (track_y - rightside_line.q) * rightside_line.m + track_x;
						p_x /= (rightside_line.m * rightside_line.m) + 1;
						double p_y = p_x * rightside_line.m + rightside_line.q;

						fovTrack.right_distance =	sqrt(
								((track_x - p_x) * (track_x - p_x)) +
								((track_y - p_y) * (track_y - p_y)) );
					}
					FOVTrackVector->push_back(fovTrack);

					int p1_x = cvRound(fovTrack.p.x) + 5;
					int p1_y = cvRound(fovTrack.p.y);
					int p2_x = cvRound(fovTrack.p.x) + fovTrack.name.size() * 10;
					int p2_y = cvRound(fovTrack.p.y) - 10;

					cvRectangle(vtsImage, cvPoint(p1_x, p1_y),
							cvPoint(p2_x, p2_y), CV_RGB(255,255,255), -1);

					cvPutText(vtsImage, fovTrack.name.c_str(), cvPoint(p1_x, p1_y),
							&trackFont, CV_RGB(0,0,0));
				}
			}

			cvCircle(vtsImage, cvPoint(cvRound(track_x), cvRound(track_y)), 3,
					CV_RGB(255,0,0),CV_FILLED);

			msg.erase(0,1);
			pos = msg.find("MSFST,");
			if(pos != string::npos) {
				msg.erase(0, pos);
			}
			else {
				msg = "";
			}
		} //while(msg.size() > 0)
		time = current_time;
		return true;
	}// if MSFST
	else {
		time = current_time;
		return true;
	}
}

double VTSReader::getYImage(int latitude) {
	double y = reference_y;
    bool up = true;

    int diff_lat = latitude - reference_latitude;

    if(diff_lat < 0) {
    	up = false;
		diff_lat *= -1;
	}
	int diff_lat_degrees = diff_lat / 60000;

	diff_lat -= diff_lat_degrees * 60000;
	int diff_lat_minutes = diff_lat / 1000;

	diff_lat -= diff_lat_minutes * 1000;
	double diff_lat_seconds = diff_lat;

	if(up) {
		y -= (diff_lat_degrees * 60 + diff_lat_minutes +
				diff_lat_seconds / 1000) * lat_pixel_per_minute;
	}
	else {
		y += (diff_lat_degrees * 60 + diff_lat_minutes +
				diff_lat_seconds / 1000) * lat_pixel_per_minute;
	}
	return y;
}

double VTSReader::getXImage(int longitude) {
	double x = reference_x;
    bool right = true;
	int diff_lon = longitude - reference_longitude;

	if(diff_lon < 0) {
		right = false;
		diff_lon *= -1;
	}
	int diff_lon_degrees = diff_lon / 60000;

	diff_lon -= diff_lon_degrees * 60000;
	int diff_lon_minutes = diff_lon / 1000;

	diff_lon -= diff_lon_minutes * 1000;
	double diff_lon_seconds = diff_lon;

	if(right) {
		x += (diff_lon_degrees * 60 + diff_lon_minutes +
				(diff_lon_seconds / 1000)) * lon_pixel_per_minute;
	}
	else {
		x -= (diff_lon_degrees * 60 + diff_lon_minutes +
				(diff_lon_seconds / 1000)) * lon_pixel_per_minute;
	}
	return x;
}

void VTSReader::drawLine(Line* line, CvScalar color, int length) {
	if(line->undefinedslope) {
		if(line->northward) {
			cvLine(vtsImage, cvPoint(camera_x, camera_y),
				cvPoint(camera_x, camera_y - length), color, 1);
		}
		else {
			cvLine(vtsImage, cvPoint(camera_x, camera_y),
				cvPoint(camera_x, camera_y + length), color, 1);
		}
	}
	else {
		if(line->m == 0) {
			if(line->westward) {
				cvLine(vtsImage, cvPoint(camera_x, camera_y),
					cvPoint(camera_x - length, camera_y ), color, 1);
			}
			else {
				cvLine(vtsImage, cvPoint(camera_x, camera_y),
					cvPoint(camera_x + length, camera_y ), color, 1);
			}
		}
		else {
			if(line->westward) {
				for(int x = max(0, camera_x - length); x < camera_x; x++) {
					double y = line->m * x + line->q;
					cvLine(vtsImage, cvPoint(x, cvRound(y)), cvPoint(x, cvRound(y)), color, 1);
				}
			}
			else {
				int limit = min(camera_x+length, vtsImage->width);
				for(int x = camera_x; x < limit; x++) {
					double y = line->m * x + line->q;
					cvLine(vtsImage, cvPoint(x, cvRound(y)),
							cvPoint(x, cvRound(y)), color, 1);
				}
			}
		}
	}
}

void VTSReader::getCameraLine(Line &line, float heading_deg) {
	double heading_rad = heading_deg * (CV_PI/180) - CV_PI/2;
	double heading_x = camera_x + cos(heading_rad);
	double heading_y = camera_y + sin(heading_rad);

	if(heading_x == camera_x) {
		line.undefinedslope = true;
		line.q = heading_x;
		if(heading_y < camera_y) {
			line.northward = true;
		}
		else {
			line.northward = false;
		}
	}
	else if(heading_y == camera_y) {
		line.undefinedslope = false;
		line.m = 0;
		line.q = heading_y;
		if(heading_x < camera_x) {
			line.westward = true;
		}
		else {
			line.westward = false;
		}
	}
	else {
		line.undefinedslope = false;
		line.m = (heading_y - camera_y) / (heading_x - camera_x);
		line.q = camera_y - (line.m * camera_x);
		if(heading_x < camera_x) {
			line.westward = true;
		}
		else {
			line.westward = false;
		}
	}
}

void VTSReader::drawVTSView(IplImage* vtsView) {

	if(camera_heading < 0) {
		cvZero(vtsView);
		cvPutText(vtsView, "NO HEADING",
			cvPoint(vtsView->width/2-30, vtsView->height/2),
			&cameraFont, CV_RGB(255, 255, 255));
		return;
	}

	int r_x, r_y;

	int FOVTrackVector_size = (int)FOVTrackVector->size();
	if(FOVTrackVector_size) {
		vector<VTSReader::FOVTrack>::iterator it;
		VTSReader::FOVTrack* t = 0;

		int sum_x = 0;
		int sum_y = 0;
		double min_distance = -1;
		double max_distance = -1;

		CvPoint fov_p1;
		CvPoint fov_p2;
		for(it = FOVTrackVector->begin(); it != FOVTrackVector->end(); ++it) {
			t = &(*it);

			fov_p1.x = cvRound(t->p.x)+5;
			fov_p1.y = cvRound(t->p.y);
			fov_p2.x = cvRound(t->p.x)+t->name.size()*10;
			fov_p2.y = cvRound(t->p.y)-10;

			cvRectangle(vtsImage, fov_p1, fov_p2, CV_RGB(255,255,255), -1);

			cvPutText(vtsImage, t->name.c_str(), fov_p1, &trackFont, CV_RGB(0,0,0));

			sum_x += t->p.x;
			sum_y += t->p.y;
			if(min_distance == -1) {
				min_distance = t->camera_distance;
			}
			else if(t->camera_distance < min_distance) {
				min_distance = t->camera_distance;
			}
			if(t->camera_distance > max_distance) {
				max_distance = t->camera_distance;
			}
		}

		r_x = max(0,(sum_x / FOVTrackVector_size) - (vtsView->width / 2));
		r_y = max(0,(sum_y / FOVTrackVector_size) - (vtsView->height / 2));

		//cvCircle(vtsImage, cvPoint(max(0,(sum_x / FOVTrackVector_size)), max(0,(sum_y / FOVTrackVector_size))),
		//		120, CV_RGB(255,0,0), 2);

		CvRect rect = cvRect(r_x, r_y, vtsView->width, vtsView->height);
		cvSetImageROI(vtsImage, rect);
		cvResize(vtsImage, vtsView, CV_INTER_CUBIC);
		cvResetImageROI(vtsImage);
	}
	else {
		//cvZero(vtsView);
		r_x = camera_x - (vtsView->width / 2);
		r_y = camera_y - (vtsView->height / 2);
		cvPutText(vtsImage, "NO TRACKS IN THE FIELD OF VIEW",
				cvPoint(camera_x-100, camera_y),
				&cameraFont, CV_RGB(255, 255, 255));
	}
	CvRect rect = cvRect(r_x, r_y, vtsView->width, vtsView->height);
	cvSetImageROI(vtsImage, rect);
	cvResize(vtsImage, vtsView, CV_INTER_CUBIC);
	cvResetImageROI(vtsImage);
}

string VTSReader::checkmessagecatandtype(u_char buf[], int buf_length)
{
	if(buf_length < 4) {
		return "UKN";
	}
	if((buf[0]==0x00)&&(buf[1]==0x08)&&(buf[2]==0x00)&&(buf[3]==0x06))
		return "BYPST";    //Bypass System Track
	else if((buf[0]==0x00)&&(buf[1]==0x08)&&(buf[2]==0x00)&&(buf[3]==0x03))
		return "DRKST";    //Dead Reckoning System Track
	else if((buf[0]==0x00)&&(buf[1]==0x08)&&(buf[2]==0x00)&&(buf[3]==0x01))
		return "MSFST";    //Bypass System Track
	else if((buf[0]==0x00)&&(buf[1]==0x06)&&(buf[2]==0x00)&&(buf[3]==0x07))
		return "VTHLT";    //Bypass System Track
	else if((buf[0]==0x00)&&(buf[1]==0x06)&&(buf[2]==0x00)&&(buf[3]==0x09))
		return "RDPLT";    //Bypass System Track
	else {
		return "UKN";
	}
}

bool VTSReader::checkmessagelength(u_char buf[], int buf_length)
{
	if(buf_length < 8) {
		return false;
	}
	int pcklength =
			(int)( buf[7] | ((buf[6] & 0x7F) << 8) ) + 12;
	if(pcklength == buf_length) {
		return true;
	}
	else {
		return false;
	}
}

string VTSReader::decodeVTSmsg(u_char buf[], int buf_length, string pckstring)
{
	if(pckstring == "BYPST")
	{
		return decodeST(buf, buf_length, 6);
	}
	else if(pckstring == "DRKST")
	{
		return decodeST(buf, buf_length, 3);
	}
	else if(pckstring == "MSFST")
	{
		return decodeST(buf, buf_length, 1);
	}
	else if(pckstring == "VTHLT")
	{
		return decodelocaltrack(buf);
	}
	else if(pckstring == "RDPLT")
	{
		return decodeplot(buf);
	}
	else return "";
}

string VTSReader::decodeST(u_char buf[], int buf_length, int STtype)
{
	int Ntrack = (int)buf[15]; //NUMBER OF TRACKS
	int sendnode = (int)(buf[3]);
	int Nsect = (int)(buf[13] & 0x0F);

	string tempstr = "";

	//latitudine del radar
	//int radar_lat = (int)(buf[16] << 24 | buf[17] << 16 | buf[18] <<8 | buf[19]);
	//int radar_lat_degrees = radar_lat/60000;
	//int radar_lat_minutes = (radar_lat - (radar_lat_degrees * 60000)) / 1000;
	//int radar_lat_seconds = radar_lat - (radar_lat_degrees * 60000) - (radar_lat_minutes * 1000);

    //longitudine del radar
	//int radar_lon = (int)(buf[20] << 24 | buf[21] << 16 | buf[22] <<8 | buf[23]);
	//int radar_lon_degrees = radar_lon/60000;
	//int radar_lon_minutes = (radar_lon - (radar_lon_degrees * 60000)) / 1000;
	//int radar_lon_seconds = radar_lon - (radar_lon_degrees * 60000) - (radar_lon_minutes * 1000);

	int offset = 0;
    if(Ntrack > 0) {
    	offset = (buf_length - 24) / Ntrack;
    }

    for(int j = 0; j < Ntrack; j++)
    {
    	int stracknum = (int)(buf[25 + offset*j] | (buf[24 + offset * j])<<8);
        //int gtracknum = (int)(buf[27 + offset*j] | (buf[26 + offset * j])<<8);
        //int btracknum = (int)(buf[29 + offset*j] | (buf[28 + offset * j])<<8);
		//int sh_width = (int)buf[32] * 6;
		//int sh_length = (int)(buf[33] & 0x7F) * 6;
		float sh_heading = (int)(buf[35 + offset*j] | (buf[34 + offset * j])<<8);
		sh_heading = sh_heading * 90/128;
        //int refresh_time = (int)(buf[37 + offset*j] | (buf[36 + offset * j])<<8);
		int elaboration_time = (int)(buf[38 + offset * j] << 24 | buf[39 + offset*j] << 16 | buf[40 + offset * j] <<8 | buf[41 + offset*j]);
		int elaboration_hours = elaboration_time /3600000;
		int elaboration_minutes =
				(elaboration_time - (elaboration_hours * 3600000)) / 60000;
		int elaboration_seconds =
				(elaboration_time - (elaboration_hours * 3600000) -
				(elaboration_minutes * 60000)) / 1000;
		int elaboration_milliseconds =
				elaboration_time - (elaboration_hours * 3600000) -
				(elaboration_minutes * 60000) - elaboration_seconds * 1000;
		ostringstream elabStream;
		elabStream << elaboration_hours << " " <<
				elaboration_minutes << " " <<
				elaboration_seconds << " " <<
				elaboration_milliseconds;
		string elabString = elabStream.str();
		//int iX = (int)(buf[42 + offset * j] << 24 | buf[43 + offset*j] << 16 | buf[44 + offset * j] <<8 | buf[45 + offset*j]);
        //float X = (float)iX/1852;  //NM
        //int iY = (int)(buf[46 + offset * j] << 24 | buf[47 + offset*j] << 16 | buf[48 + offset * j] <<8 | buf[49 + offset*j]);
        //float Y = (float)iY/1852; //Nm

		//int ivX = (int)(buf[50 + offset * j] << 24 | buf[51 + offset*j] << 16 | buf[52 + offset * j] <<8 | buf[53 + offset*j]);
		//float vX = (float)ivX/1852;  //NM

		//int ivY = (int)(buf[54 + offset * j] << 24 | buf[55 + offset*j] << 16 | buf[56 + offset * j] <<8 | buf[57 + offset*j]);
		//float vY = (float)ivY/1852; //Nm

        string flag = "";
		for(int k = 0; k < 2; k++) {
			flag += (char)(buf[166 + k + offset*j]);
		}
        string callsign = "";
		for(int k = 0; k < 10; k++)
			callsign = callsign+(char)(buf[168 + k + offset*j]);

		string name = "";
		for(int k = 0; k < 40; k++) {
			name += (char)(buf[178 + k + offset*j]);
		}

		string mmsi = "";
		for(int k = 0; k < 9; k++) {
			mmsi += (char)(buf[218 + k + offset*j]);
		}

		string imo = "";
		for(int k = 0; k < 10; k++) {
			imo += (char)(buf[228 + k + offset*j]);
		}

		//int shiptype = (int)buf[239 + offset*j];
		//int length = (int)(buf[243 + offset*j] | (buf[242 + offset * j])<<8);
		//int width = (int)(buf[245 + offset*j] | (buf[244 + offset * j])<<8);
		//int height = (int)(buf[247 + offset*j] | (buf[246 + offset * j])<<8);

		//int draught = (int)(buf[249 + offset*j] | (buf[248 + offset * j])<<8);
		//int line = (int)(buf[255 + offset*j] | (buf[254 + offset * j])<<8);

		string vts_ship_id = "";
		for(int k = 0; k < 13; k++) {
			vts_ship_id += (char)(buf[256 + k + offset*j]);
		}

		//int rot = (int)(buf[270 + offset*j]);

		int lat = (int)(buf[272 + offset * j] << 24 | buf[273 + offset*j] << 16 | buf[274 + offset * j] <<8 | buf[275 + offset*j]);
		int lat_degrees = lat/60000;
		int lat_minutes = (lat - (lat_degrees * 60000)) / 1000;
		int lat_seconds = lat - (lat_degrees * 60000) - (lat_minutes * 1000);
		ostringstream latStream;
		latStream << lat_degrees << " " <<
				lat_minutes << " " << lat_seconds;
		string latString = latStream.str();

		int lon = (int)(buf[276 + offset * j] << 24 | buf[277 + offset*j] << 16 | buf[278 + offset * j] <<8 | buf[279 + offset*j]);
		int lon_degrees = lon/60000;
		int lon_minutes = (lon - (lon_degrees * 60000)) / 1000;
		int lon_seconds = lon - (lon_degrees * 60000) - (lon_minutes * 1000);
		ostringstream lonStream;
		lonStream << lon_degrees << " " <<
				lon_minutes << " " << lon_seconds;
		string lonString = lonStream.str();

		//int threat_type = (int)(buf[271 + offset * j] & 0x0F);

		if(STtype==1) tempstr = tempstr + "MSFST,";
		if(STtype==3) tempstr = tempstr + "DRKST,";
		if(STtype==6) tempstr = tempstr + "BYPST,";

		ostringstream tempStream;
		tempStream << "sn:," << sendnode << "," <<
				"sct:," << Nsect << "," <<
				"stn:," << stracknum << "," <<
				//"gtn:," << gtracknum << "," <<
				//"btn:," << btracknum << "," <<
				//"s_wi:," << sh_width << "," <<
				//"s_le:," << sh_length << "," <<
				//"s_he:," << sh_heading << "," <<
				//"X(NM):," << X << "," <<
				//"Y(NM):," << Y << "," <<
				//"VX(KN):," << vX << "," <<
				//"VY(KN):," << vY << "," <<
				//"calls:," << callsign << "," <<
				"name:," << name << "," <<
				"mmsi:," << mmsi << "," <<
				"imo:," << imo << "," <<
				//"shtype:," << shiptype << "," <<
				//"length:," << length << "," <<
				//"width:," << width << "," <<
				//heigth:," << height << "," <<
				//"draught:," << draught <<  "," <<
				"lat:," << latString <<  "," <<
				"lon:," << lonString <<  "," <<
				"time:," << elabString << "," <<
				"\n";

		tempstr += tempStream.str();

	}
	return tempstr;
}

string VTSReader::decodelocaltrack(u_char buf[])
{
	string tempstr = "";

	int Ntrack = (int)(buf[15] | (buf[14])<<8);
	int sendnode = (int)(buf[5]);

	short word = (short)(buf[13] | (buf[12])<<8);
	int Nsect = (word & 0x01E0)>>5;

	int offset = 24;        //24 byte per ogni traccia
	for(int j = 0; j < Ntrack; j++)
	{
		int tracknum = (int)(buf[17 + offset*j] | (buf[16 + offset * j])<<8);

		int trkstatus = (int)((buf[18] & 0x38)>>3);

		float pltrange = (int)(buf[21 + offset*j] | (buf[20 + offset * j])<<8);
		pltrange = (pltrange * 3)/1852;

		float trkrange = (int)(buf[25 + offset*j] | (buf[24 + offset * j])<<8);
		trkrange = (trkrange * 3)/1852;

		float pltazimuth = (int)(buf[23 + offset*j] | (buf[22 + offset * j])<<8);
		pltazimuth = pltazimuth * 360 / 65536;

		float trkazimuth = (int)(buf[27 + offset*j] | (buf[26 + offset * j])<<8);
		trkazimuth = trkazimuth * 360 / 65536;

		float trkspeedrng = (int)(buf[29 + offset*j] | (buf[28 + offset * j])<<8);
		trkspeedrng = trkspeedrng / 80;

		float trkspeedaz = (int)(buf[31 + offset*j] | (buf[30 + offset * j])<<8);
		trkspeedaz = trkspeedaz / 80;

		ostringstream tempStream;
		tempStream << "VTHLT," <<
				"sn:," << sendnode << "," <<
				"sct:," << Nsect << "," <<
				"tn:," << tracknum << "," <<
				"stat:," << trkstatus << "," <<
				"plrg:," << pltrange << "," <<
				"pltaz:," << pltazimuth << "," <<
				"ltrg:," << trkrange << "," <<
				"ltaz:," << trkazimuth << "," <<
				"sprg:," << trkspeedrng << "," <<
				"spaz:," << trkspeedaz <<
				"\n";

		tempstr += tempStream.str();
	}
	return tempstr;
}

string VTSReader::decodeplot(u_char buf[])
{
	string tempstr = "";

	int Nplot = (int)(buf[15] | (buf[14])<<8);
	int sendnode = (int)(buf[5]);

	short word = (short)(buf[13] | (buf[12])<<8);
	int Nsect = (word & 0x01E0)>>5;

	int offset = 42;        //42 byte per ogni plot
	for(int j = 0; j < Nplot; j++)
	{
		float range = (int)(buf[17 + offset*j] | (buf[16 + offset * j])<<8);
		range = (range * 3)/1852;

		float azimuth = (int)(buf[19 + offset*j] | (buf[18 + offset * j])<<8);
		azimuth = azimuth * 360 / 16384;

		int video_mean = (int)(buf[26 + offset*j]);
		int video_std = (int)(buf[27 + offset*j]);

		ostringstream tempStream;
		tempStream << "RDPLT," <<
				"sn:," << sendnode << "," <<
				"sct:," << Nsect << "," <<
				"rng:," << range << "," <<
				"azm:," << azimuth << "," <<
				"vdm:," << video_mean << "," <<
				"vds:," << video_std <<
				"\n";

		tempstr += tempStream.str();
	}
	return tempstr;
}

bool VTSReader::readCfgFile(const char* vts_cfg, CvSize &vts_image_size) {
	ifstream cfgStream;
	cfgStream.open(vts_cfg);
	if(cfgStream.is_open()) {
		string line;
		while (!cfgStream.eof()) {
			getline(cfgStream, line);
			if(line.compare(0, 1, "%") == 0) {
				continue;
			}
			else if(line.compare(0, 18, "[background image]") == 0) {
				getline(cfgStream, line);
				backgroundImage = cvLoadImage(line.c_str());
				if(backgroundImage) {
					vtsImage = cvCreateImage(cvSize(backgroundImage->width, backgroundImage->height),
							backgroundImage->depth, backgroundImage->nChannels);
					vts_image_size.width = vtsImage->width;
					vts_image_size.height = vtsImage->height;
				}
				else {
					cerr << "Unable to load VTS background image." << endl;
					return false;
				}
			}
			else if(line.compare(0, 13, "[reference x]") == 0) {
				getline(cfgStream, line);
				int i;
				istringstream stream(line);
				if (stream >> i) {
					reference_x = i;
				}
				else {
					cerr << "Unable to decode VTS CFG line " << line << endl;
					return false;
				}
			}
			else if(line.compare(0, 13, "[reference y]") == 0) {
				getline(cfgStream, line);
				int i;
				istringstream stream(line);
				if (stream >> i) {
					reference_y = i;
				}
				else {
					cerr << "Unable to decode VTS CFG line " << line << endl;
					return false;
				}
			}
			else if(line.compare(0, 23, "[reference lat degrees]") == 0) {
				getline(cfgStream, line);
				int i;
				istringstream stream(line);
				if (stream >> i) {
					reference_lat_degrees = i;
				}
				else {
					cerr << "Unable to decode VTS CFG line " << line << endl;
					return false;
				}
			}
			else if(line.compare(0, 23, "[reference lat minutes]") == 0) {
				getline(cfgStream, line);
				int i;
				istringstream stream(line);
				if (stream >> i) {
					reference_lat_minutes = i;
				}
				else {
					cerr << "Unable to decode VTS CFG line " << line << endl;
					return false;
				}
			}
			else if(line.compare(0, 23, "[reference lat seconds]") == 0) {
				getline(cfgStream, line);
				int i;
				istringstream stream(line);
				if (stream >> i) {
					reference_lat_seconds = i;
				}
				else {
					cerr << "Unable to decode VTS CFG line " << line << endl;
					return false;
				}
			}
			else if(line.compare(0, 23, "[reference lon degrees]") == 0) {
				getline(cfgStream, line);
				int i;
				istringstream stream(line);
				if (stream >> i) {
					reference_lon_degrees = i;
				}
				else {
					cerr << "Unable to decode VTS CFG line " << line << endl;
					return false;
				}
			}
			else if(line.compare(0, 23, "[reference lon minutes]") == 0) {
				getline(cfgStream, line);
				int i;
				istringstream stream(line);
				if (stream >> i) {
					reference_lon_minutes = i;
				}
				else {
					cerr << "Unable to decode VTS CFG line " << line << endl;
					return false;
				}
			}
			else if(line.compare(0, 23, "[reference lon seconds]") == 0) {
				getline(cfgStream, line);
				int i;
				istringstream stream(line);
				if (stream >> i) {
					reference_lon_seconds = i;
				}
				else {
					cerr << "Unable to decode VTS CFG line " << line << endl;
					return false;
				}
			}
			else if(line.compare(0, 20, "[camera lat degrees]") == 0) {
				getline(cfgStream, line);
				int i;
				istringstream stream(line);
				if (stream >> i) {
					camera_lat_degrees = i;
				}
				else {
					cerr << "Unable to decode VTS CFG line " << line << endl;
					return false;
				}
			}
			else if(line.compare(0, 20, "[camera lat minutes]") == 0) {
				getline(cfgStream, line);
				int i;
				istringstream stream(line);
				if (stream >> i) {
					camera_lat_minutes = i;
				}
				else {
					cerr << "Unable to decode VTS CFG line " << line << endl;
					return false;
				}
			}
			else if(line.compare(0, 20, "[camera lat seconds]") == 0) {
				getline(cfgStream, line);
				int i;
				istringstream stream(line);
				if (stream >> i) {
					camera_lat_seconds = i;
				}
				else {
					cerr << "Unable to decode VTS CFG line " << line << endl;
					return false;
				}
			}
			else if(line.compare(0, 20, "[camera lon degrees]") == 0) {
				getline(cfgStream, line);
				int i;
				istringstream stream(line);
				if (stream >> i) {
					camera_lon_degrees = i;
				}
				else {
					cerr << "Unable to decode VTS CFG line " << line << endl;
					return false;
				}
			}
			else if(line.compare(0, 20, "[camera lon minutes]") == 0) {
				getline(cfgStream, line);
				int i;
				istringstream stream(line);
				if (stream >> i) {
					camera_lon_minutes = i;
				}
				else {
					cerr << "Unable to decode VTS CFG line " << line << endl;
					return false;
				}
			}
			else if(line.compare(0, 20, "[camera lon seconds]") == 0) {
				getline(cfgStream, line);
				int i;
				istringstream stream(line);
				if (stream >> i) {
					camera_lon_seconds = i;
				}
				else {
					cerr << "Unable to decode VTS CFG line " << line << endl;
					return false;
				}
			}
			else if(line.compare(0, 16, "[camera heading]") == 0) {
				getline(cfgStream, line);
				double d;
				istringstream stream(line);
				if (stream >> d) {
					camera_heading = d;
				}
				else {
					cerr << "Unable to decode VTS CFG line " << line << endl;
					return false;
				}
			}
			else if(line.compare(0, 22, "[lat pixel per minute]") == 0) {
				getline(cfgStream, line);
				double i;
				istringstream stream(line);
				if (stream >> i) {
					lat_pixel_per_minute = i;
				}
				else {
					cerr << "Unable to decode VTS CFG line " << line << endl;
					return false;
				}
			}
			else if(line.compare(0, 22, "[lon pixel per minute]") == 0) {
				getline(cfgStream, line);
				double i;
				istringstream stream(line);
				if (stream >> i) {
					lon_pixel_per_minute = i;
				}
				else {
					cerr << "Unable to decode VTS CFG line " << line << endl;
					return false;
				}
			}
			else if(line.compare(0, 14, "[refresh time]") == 0) {
				getline(cfgStream, line);
				long i;
				istringstream stream(line);
				if (stream >> i) {
					refresh_time = i;
				}
				else {
					cerr << "Unable to decode VTS CFG line " << line << endl;
					return false;
				}
			}
			else {
				continue;
			}
		}
		cfgStream.close();
		return true;
	}
	else {
		cout << "ERROR: cannot read from cfg file " << vts_cfg << endl;
		return false;
	}
}

bool VTSReader::open() {
	int inum;
	int i = 0;
	// Retrieve the device list on the local machine    
    if (pcap_findalldevs(&alldevs, errbuf) == -1)
	{
		cout << "Error in pcap_findalldevs: " << errbuf << endl;
		return false;
	}

	/* Print the list */
	for(d=alldevs; d; d=d->next)
	{
		printf("%d. %s", ++i, d->name);
		if (d->description)
			printf(" (%s)\n", d->description);
		else
			printf(" (No description available)\n");
	}

	if(i==0)
	{
		cout << "\nNo interfaces found! Make sure WinPcap is installed.\n" << endl;
		return false;
	}

	printf("Enter the interface number (1-%d):",i);
	scanf("%d", &inum);

	if(inum < 1 || inum > i)
	{
		cout << "\nInterface number out of range." << endl;

		pcap_freealldevs(alldevs);
		return false;
	}

	/* Jump to the selected adapter */
	for(d=alldevs, i=0; i < inum-1; d=d->next, i++);

	/* Open the device */
    if ( (adhandle= pcap_open_live(d->name,          // name of the device
							  65536,            // portion of the packet to capture
												// 65536 guarantees that the whole packet will be captured on all the link layers
							  PCAP_OPENFLAG_PROMISCUOUS,    // promiscuous mode
							  1000,             // read timeout
                              errbuf            // error buffer
							  ) ) == NULL)
	{
		cout << "\nUnable to open the adapter. " << d->name << " is not supported by WinPcap" << endl;

		pcap_freealldevs(alldevs);
		return false;
	}

	cout << "\nlistening on " << d->description << "...\n" << endl;

	pcap_freealldevs(alldevs);

	cvNamedWindow("VTS", 1);

	messageImage = cvCloneImage(vtsImage);
	cvZero(messageImage);
	cvPutText(messageImage, "Waiting for", cvPoint(30,50), &cameraFont, CV_RGB(255, 255, 255));
	cvPutText(messageImage, "VTS data...", cvPoint(30,70), &cameraFont, CV_RGB(255, 255, 255));
	cvShowImage("VTS", messageImage);
	cvWaitKey(1);

	return true;
}

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

#include <pthread.h>

#include "filegrabber.h"
#include "livegrabber.h"
#include "boatdetector.h"
#include "track.h"
#include "tracker.h"
#include "vtsreader.h"
#include "validation.h"
#include "xmlwriter.h"

#include <PTracker/PTracker.h>
#include <sys/stat.h>

// Uncomment if you want to write the results in a xml file.
#define RESULTS_ENABLED

#define EBUSY 16	

using namespace std;
using namespace cv;
using namespace PTracking;

map<int,pair<int,pair<int,int> > > colorMap;

//VIDEO THREAD DATA
typedef struct VideoData{
	Grabber *grabber;
	IplImage* frame;
	string input_name;
	bool terminate;
} VideoData;

//VTS THREAD DATA
typedef struct VTSData{
	VTSReader* vtsReader;
	vector<VTSReader::FOVTrack>* vtsTrackVector;
	IplImage* vtsView;
	long current_time;
	bool terminate;
} VTSData;

/*
* Command Argument structure
*/
typedef struct ArgParam {
	//VIDEO INPUT
	string input_name;
	//DETECTOR
	string cascade_name;
	double scale;
	double scale_factor;
	int min_neighbors;
	int flags;
	int width;
	int height;
	//VTS
	string vts_cfg;
	//VIDEO OUTPUT
	bool saveVideo;
	char* outVideoName;
	//GUI
	bool slow;
	bool coastline;
	//DEBUG
	bool print_tracks;
	bool save_detection;
	char* save_detection_folder;
	//DATA COLLECTION
	bool datacollection;
	char* dcVideoName;
} ArgParam;

//MUTEX
pthread_mutex_t frame_mutex;
pthread_mutex_t vts_mutex;

//trackbars
int max_heading = 360;
int max_fov = 60;
int heading_pos = 0;
int fov_pos = 0;


/************************* Function Prototypes *********************************/
void arg_parse( int argc, char** argv, ArgParam* arg = NULL );
IplImage* imageMosaic(int rows, int cols, int width, int height, IplImage** images);
void headingControl(int pos);
void fovControl(int pos);

float NewHueHistogram[ObjectSensorReading::Model::HISTOGRAM_VECTOR_LENGTH];
float NewSaturationHistogram[ObjectSensorReading::Model::HISTOGRAM_VECTOR_LENGTH];
float NewValueHistogram[ObjectSensorReading::Model::HISTOGRAM_VECTOR_LENGTH];

void updateBoatHistograms(IplImage* frame, CvRect* r)
{
	cvSetImageROI(frame,*r);
	IplImage* hsvImage = cvCreateImage(cvSize(r->width,r->height),frame->depth,3);
	cvCvtColor(frame,hsvImage,CV_BGR2HSV);
	
	cvResetImageROI(frame);
	
	for (int k = 0; k < ObjectSensorReading::Model::HISTOGRAM_VECTOR_LENGTH; k++)
	{
		NewHueHistogram[k] = 0;
		NewSaturationHistogram[k] = 0;
		NewValueHistogram[k] = 0;
	}
	
	int counter = 0;
	
	uchar hue, saturation, value;
	
	for (int y = 0; y < hsvImage->height; y++)
	{
		for (int x = 0; x < hsvImage->width; x++)
		{
			hue = (uchar) hsvImage->imageData[y*hsvImage->widthStep + x];
			saturation = (uchar) hsvImage->imageData[y*hsvImage->widthStep + x + 1];
			value = (uchar) hsvImage->imageData[y*hsvImage->widthStep + x + 2];
			
			NewHueHistogram[hue]++;
			NewSaturationHistogram[saturation]++;
			NewValueHistogram[value]++;
			
			counter++;
		}
	}
	
	for (int i = 0; i < ObjectSensorReading::Model::HISTOGRAM_VECTOR_LENGTH; i++)
	{
		NewHueHistogram[i] /= counter;
		NewSaturationHistogram[i] /= counter;
		NewValueHistogram[i] /= counter;
	}
	
	cvReleaseImage(&hsvImage);
}

/************************* Main ************************************************/
int main( int argc, char** argv )
{
	// initialization
	ArgParam init_arg = {
		//VIDEO INPUT
		"0", //input_name
		//DETECTOR
		"haarcascade.xml", //cascade_name
		1, //scale
		1.1, //scale_factor
		5, //min_neighbors
		CV_HAAR_DO_CANNY_PRUNING, //flags
		60, //width
		30, //height
		//VTS
		"vts.cfg", //vts_cfg
		//VIDEO OUTPUT
		false, //saveVideo
		0, //outVideoName
		//GUI
		false, //slow
		false, //coastline
		//DEBUG
		false, //print_tracks
		false, //save detection
		0, //save detection folder
		//DATA COLLECTION
		false, //datacollection
		0 //dcVideoName
	};

	ArgParam *arg = &init_arg;
	// parse arguments
	arg_parse( argc, argv, arg );

	//DETECTOR
    IplImage *frame = 0, *frame_copy = 0;
	IplImage *rawImage = 0;
	IplImage *grayImage = 0;
    IplImage *edgeImage = 0;
    IplImage *detectionImage = 0;
	IplImage *trackingImage = 0;
	IplImage *sealineImage = 0;
	IplImage *seaEdgeImage = 0;

	//time
	long detector_time = 0;
	int detector_hours = 0;
	int detector_minutes = 0;
	int detector_seconds = 0;
	int detector_milliseconds = 0;

    //TRACKING
    int associateThreshold = 50;
    int missThreshold = 10;
    int validThreshold = 15;

    //VALIDATION
    IplImage* validationImage = 0;
    vector<VTSReader::FOVTrack>::iterator it;
    vector<VTSReader::FOVTrack>* FOVTrackVector = new vector<VTSReader::FOVTrack>();

    //VIDEO INPUT
    int frame_number = 0;
    pthread_t videoThread;
    VideoData videoData;
    Grabber* grabber;
    string dim_s;
    int dim_s_size;
    ostringstream dim_oss;

    //VIDEO OUTPUT
	CvVideoWriter *writer = 0;
	int isColor = 1;
	int out_fps = 10;

	//GUI
	string gui_title = "Boat Detector - Keys: <p> Play  <s> Stop  <f> Fast  <ESC> Quit";
	bool fastForward = false;
	CvFont font;
	cvInitFont(&font, CV_FONT_HERSHEY_SIMPLEX, 0.5, 0.5, 2);
	IplImage* images[4];
	IplImage* mosaic = 0;
	int mosaic_width = 900;
	int mosaic_height = 600;

	//DATA COLLECTION
	CvVideoWriter *dc_writer = 0;
	int dc_isColor = 1;
	int dc_out_fps = 10;

	//DETECTION DEBUG
	int save_detection_counter = 0;
	string save_detection_folder;

	//FPS calculation
	clock_t fps_start, fps_stop;
	int fps_counter = 0;
	clock_t fps_t = 0;
	double fps_value = -1;
	string fps_s;
	int fps_s_size;
	ostringstream fps_oss;
	
	int centerX, centerY;

	//pthread
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	cout << endl;
    cout << "BOAT DETECTOR -- ver. 0" << endl;
    cout << endl;

    //CLASSIFIER
	cout << "Loading classifier " << arg->cascade_name << "...";
    cout.flush();

    CvHaarClassifierCascade* cascade =
    		cvLoadHaarClassifierCascade(arg->cascade_name.c_str(), cvSize(arg->width, arg->height));

    if( cascade == NULL )
	{
    	cout << endl;
		cout << "Unable to load classifier." << endl;
		return -1;
	}
    else {
    	cout << "done." <<endl;
    }

    //DETECTOR

	BoatDetector* detector = new BoatDetector(	arg->scale,
												arg->scale_factor,
												arg->min_neighbors,
												arg->flags,
												cvSize(arg->width, arg->height),
												cascade );

	detector->printSettings();

	//TRACKER
	//Tracker* tracker =
	//		new Tracker(associateThreshold, missThreshold, validThreshold);
	
	// PTracker
	PTracker* pTracker = new PTracker(1);

	//VALIDATION
	Validation* validation = new Validation();
	validationImage = validation->getValidationImage();

	//VIDEO GRABBER
    grabber = new FileGrabber(arg->input_name.c_str());


	if(grabber) {
		cout << "video grabber initialized." << endl;
		cout << "GRABBER - FRAME WIDTH " << grabber->width << " HEIGHT " << grabber->height << endl;
		cout << "GRABBER - FPS " << grabber->getFps() << endl;
		cout << endl;
	}
	else {
		cout << "video grabber NOT initialized." << endl;
		cout << "exiting..." << endl;
		return EXIT_FAILURE;
	}


    //VIDEO THREAD
//    videoData.input_name.assign(arg->input_name);
//    videoData.frame = 0;
//    videoData.grabber = grabber;
//	videoData.terminate = false;

//	pthread_mutex_init(&frame_mutex, NULL);

//	void *video_thread_status;
//	int video_thread_rc;
//	video_thread_rc = pthread_create(&videoThread, &attr, videoManager, (void *)&videoData);
//	if (video_thread_rc){
//		cout << "VIDEO THREAD ERROR; return code from pthread_create() is " << video_thread_rc << endl;
//		return EXIT_FAILURE;
//	}

	//GUI
	cvNamedWindow(gui_title.c_str(), 1);
	cvCreateTrackbar("Heading", gui_title.c_str(),
			&heading_pos, max_heading, headingControl);
	cvCreateTrackbar("FOV", gui_title.c_str(),
			&fov_pos, max_fov, fovControl);

	//MAIN LOOP
	bool run = true;
	int keyboard = 0;
	double t = 0;

	//STARTING TIME
	detector_hours = detector_time /3600000;
	detector_minutes = (detector_time - (detector_hours * 3600000)) / 60000;
	detector_seconds = (detector_time - (detector_hours * 3600000) -
					(detector_minutes * 60000)) / 1000;
	detector_milliseconds = detector_time - (detector_hours * 3600000) -
					(detector_minutes * 60000) - detector_seconds * 1000;

	int counter = 1;
	
	for (int i = 0; i < 256; i += 63)
	{
		for (int j = 0; j < 256; j += 63)
		{
			for (int k = 0; k < 256; k += 128, ++counter)
			{
				colorMap.insert(make_pair(counter,make_pair(i,make_pair(j,k))));
			}
		}
	}
	
#ifdef RESULTS_ENABLED
	ofstream results;
	
	struct stat temp;
	
	if (stat("../results",&temp) == -1)
	{
		mkdir("../results",0700);
	}
	
	stringstream s;
	
	int startIndex, endIndex;
	
	startIndex = string(arg->input_name).rfind("/") + 1;
	endIndex = string(arg->input_name).rfind(".");
	
	s << "../results/PTracker-" << string(arg->input_name).substr(startIndex,endIndex - startIndex) << ".xml";
	
	results.open(s.str().c_str());
	
	results << "<?xml version=\"1.0\" encoding=\"utf-8\"?>" << endl;
	results << "<dataset>" << endl;
	
	int resultsIteration;
	
	resultsIteration = 0;
#endif

    while(run) {

        detector_hours = detector_time /3600000;
        detector_minutes = (detector_time - (detector_hours * 3600000)) / 60000;
        detector_seconds = (detector_time - (detector_hours * 3600000) -
                        (detector_minutes * 60000)) / 1000;
        detector_milliseconds = detector_time - (detector_hours * 3600000) -
                        (detector_minutes * 60000) - detector_seconds * 1000;

        if(!grabber->getData()) {
            cout << "NO INPUT VIDEO DATA" << endl;
            run = false;
            continue;
        }

        if(grabber->frame)
        {
            if(!frame) {
                frame = cvCloneImage(grabber->frame);
            }
            else {
                cvCopy(grabber->frame, frame);
            }

            if( !frame_copy ) {

                frame_copy = cvCreateImage( cvSize(frame->width, frame->height), frame->depth, frame->nChannels );
                rawImage = cvCreateImage( cvSize((int)(frame->width/arg->scale), (int)(frame->height/arg->scale)),
                                    frame->depth, frame->nChannels );
                detectionImage = cvCreateImage( cvSize(rawImage->width, rawImage->height),
                        rawImage->depth, rawImage->nChannels );

                images[0] = detectionImage;

                trackingImage = cvCreateImage( cvSize(rawImage->width, rawImage->height), rawImage->depth, rawImage->nChannels );

                images[1] = trackingImage;

                grayImage = cvCreateImage( cvSize(rawImage->width, rawImage->height), rawImage->depth, 1 );

                edgeImage = cvCreateImage( cvSize(rawImage->width, rawImage->height), rawImage->depth, 1 );

                sealineImage = cvCreateImage( cvSize(rawImage->width, rawImage->height), rawImage->depth, 1 );

                images[2] = sealineImage;

                seaEdgeImage = cvCreateImage( cvSize(rawImage->width, rawImage->height), rawImage->depth, 1 );

                images[3] = validationImage;

                if(arg->saveVideo) {
                    writer = cvCreateVideoWriter(arg->outVideoName, CV_FOURCC('D', 'I', 'V', 'X'), out_fps,
                                cvSize(mosaic_width, mosaic_height), isColor);
                }

                if(arg->datacollection) {
                    dc_writer = cvCreateVideoWriter(arg->dcVideoName, CV_FOURCC('D', 'I', 'V', 'X'),
                                dc_out_fps,	cvSize(rawImage->width, rawImage->height), dc_isColor);
                }

                cout << "DETECTOR - FRAME WIDTH " << grayImage->width <<
                        " HEIGHT " << grayImage->height << endl;
            }

            //FPS calculation
            fps_counter++;
            fps_start = clock();

            //TIME
            t = (double)cvGetTickCount();

            if( frame->origin == IPL_ORIGIN_TL ) {
                cvCopy( frame, frame_copy, 0 );
            }
            else {
                cvFlip( frame, frame_copy, 0 );
            }

            cvResize( frame_copy, rawImage, CV_INTER_CUBIC );

            cvCopy(rawImage, detectionImage);
            
            pair<map<int,pair<pair<ObjectSensorReading::Observation,PTracking::Point2f>,pair<string,int> > >,map<int,pair<ObjectSensorReading::Observation,PTracking::Point2f> > > estimatedTargetModelsWithIdentity;

            if(!fastForward) {

                cvCopy(rawImage, trackingImage);

                //gray_image
                cvCvtColor( rawImage, grayImage, CV_BGR2GRAY );

                //seaEdgeImage
                cvCanny( grayImage, seaEdgeImage, 50, 70, 3);
                cvRectangle(seaEdgeImage, cvPoint(0,0), cvPoint(seaEdgeImage->width, 5), cvScalar(0), -1);
                cvRectangle(seaEdgeImage, cvPoint(0, seaEdgeImage->height-5), cvPoint(seaEdgeImage->width, seaEdgeImage->height-1), cvScalar(0), -1);

                //smoothing
                cvSmooth(grayImage, grayImage, CV_MEDIAN);
                cvSmooth(grayImage, grayImage, CV_GAUSSIAN);

                //edgeImage
                cvCanny( grayImage, edgeImage, 30, 100, 3);
                cvRectangle(edgeImage, cvPoint(0,0), cvPoint(edgeImage->width, 5), cvScalar(0), -1);
                cvRectangle(edgeImage, cvPoint(0, edgeImage->height-5), cvPoint(edgeImage->width, edgeImage->height-1), cvScalar(0), -1);

                //DETECTION

                //detect sea line
                //TODO colorata
                detector->houghline(seaEdgeImage, grayImage, sealineImage);

                //detect boats
                detector->detect(grayImage, edgeImage, arg->coastline);

                detector->draw(detectionImage);

                //TRACKING
                //tracker->process(rawImage, edgeImage, detector);

                //tracker->draw(trackingImage);

                //if(arg->print_tracks) {
                //    tracker->printTracks();
                //}
                
                /// The coast line has not been computed yet.
                if (detector->getSeaLimit() == 0) continue;
                
                vector<CvRect>* boats = detector->getBoats();
				
				vector<ObjectSensorReading::Observation> obs;
				ObjectSensorReading visualReading;
				
				for(vector<CvRect>::iterator it = boats->begin(); it != boats->end(); ++it)
				{
					//if (it->y < detector->getSeaLimit()) continue;
					
					centerX = it->x + (it->width / 2);
					centerY = it->y + it->height;
					
					updateBoatHistograms(rawImage, &(*it));
					
					ObjectSensorReading::Observation observation;
					
					observation.observation.x = centerX;
					observation.observation.y = centerY;
					observation.model.barycenter = centerX;
					observation.model.boundingBox = make_pair(PTracking::Point2f(-it->width / 2,-it->height),PTracking::Point2f(it->width / 2,0));
					observation.model.height = it->height;
					observation.model.width = it->width;
					
					for (int i = 0; i < ObjectSensorReading::Model::HISTOGRAM_VECTOR_LENGTH; ++i)
					{
						observation.model.histograms[0][i] = NewHueHistogram[i];
						observation.model.histograms[1][i] = NewSaturationHistogram[i];
						observation.model.histograms[2][i] = NewValueHistogram[i];
					}
					
					obs.push_back(observation);
				}
				
				visualReading.setObservations(obs);
				visualReading.setObservationsAgentPose(Point2of(0.0,0.0,0.0));
				
				if (resultsIteration > 400) pTracker->exec(visualReading);
				
				estimatedTargetModelsWithIdentity = pTracker->getAgentEstimations();
				//groupEstimatedTargetModelsWithIdentity = pTracker->getAgentGroupEstimations();
				
				Mat matTrackingImage(trackingImage);
				
#ifdef RESULTS_ENABLED
				//if (estimatedTargetModelsWithIdentity.first.size() > 0)
				{
					results << "   <frame number=\"" << resultsIteration++ << "\">" << endl;
					results << "      <objectlist>" << endl;
				}
#endif

				/// Global estimations.
				for (map<int,pair<pair<ObjectSensorReading::Observation,PTracking::Point2f>,pair<string,int> > >::const_iterator it = estimatedTargetModelsWithIdentity.first.begin(); it != estimatedTargetModelsWithIdentity.first.end(); ++it)
				{
					cv::Point2f p;
					
					p.x = it->second.first.first.observation.x;
					p.y = it->second.first.first.observation.y;
					
					map<int,pair<int,pair<int,int> > >::iterator colorTrack = colorMap.find(it->first);
					
					rectangle(matTrackingImage,cvPoint(p.x - (it->second.first.first.model.width / 2), p.y - it->second.first.first.model.height),
							  cvPoint(p.x + (it->second.first.first.model.width / 2), p.y),
							  cvScalar(colorTrack->second.first,colorTrack->second.second.first,colorTrack->second.second.second), 3);
					
					stringstream text;
					
					text << it->first;
					
					int fontFace = FONT_HERSHEY_SIMPLEX;
					double fontScale = 1;
					int thickness = 3;
					Point textOrg(p.x - (it->second.first.first.model.width / 2), p.y - it->second.first.first.model.height - 7);
					putText(matTrackingImage,text.str(),textOrg,fontFace,fontScale,Scalar::all(0),thickness,8);
					
					fontScale = 1;
					thickness = 2;
					Point textOrg2(p.x - (it->second.first.first.model.width / 2) + 2, p.y - it->second.first.first.model.height - 9);
					putText(matTrackingImage,text.str(),textOrg2,fontFace,fontScale,Scalar::all(255),thickness,8);
					
	#ifdef RESULTS_ENABLED
						results << "         <object id=\"" << it->first << "\">" << endl;
						results << "            <box h=\"" << (it->second.first.first.model.height * arg->scale) << "\" w=\"" << (it->second.first.first.model.width * arg->scale) << "\" xc=\"" << (p.x * arg->scale)
								<< "\" yc=\"" << ((p.y * arg->scale) - ((it->second.first.first.model.height * arg->scale) / 2)) << "\"/>" << endl;
						results << "         </object>" << endl;
#endif
				}
			
#ifdef RESULTS_ENABLED
				//if (estimatedTargetModelsWithIdentity.first.size() > 0)
				{
					results << "      </objectlist>" << endl;
					results << "   </frame>" << endl;
				}
#endif

                //print time
                ostringstream oss;
                if(detector_hours < 10) {
                    oss << "0" << detector_hours << ":";
                }
                else {
                    oss << detector_hours << ":";
                }
                if(detector_minutes < 10) {
                    oss << "0" << detector_minutes << ":";
                }
                else {
                    oss << detector_minutes << ":";
                }
                if(detector_seconds < 10) {
                    oss << "0" << detector_seconds << ":";
                }
                else {
                    oss << detector_seconds << ":";
                }
                if(detector_milliseconds < 10 ) {
                    oss << "00" << detector_milliseconds;
                }
                else if(detector_milliseconds < 100 ) {
                    oss << "0" << detector_milliseconds;
                }
                else {
                    oss << detector_milliseconds;
                }
                cvPutText(trackingImage, oss.str().c_str(), cvPoint(10, 20), &font, CV_RGB(0,0,0));

            } //if ! FAST FORWARD



            if(arg->datacollection) {
                cvWriteFrame(dc_writer, rawImage);
            }
            if(arg->save_detection) {
                vector<CvRect>::iterator it;
                CvRect* r = 0;
                for(it = detector->getBoats()->begin(); it != detector->getBoats()->end(); ++it) {
                    r = &(*it);

                    string det_name(arg->save_detection_folder);
                    det_name += "\\";
                    ostringstream oss;
                    oss << save_detection_counter << ".png";
                    det_name += oss.str();

                    IplImage* img = cvCreateImage(cvSize(r->width, r->height),
                                                rawImage->depth, rawImage->nChannels);
                    cvSetImageROI(rawImage, *r);
                    cvCopy(rawImage, img);
                    cvResetImageROI(rawImage);

                    if(!cvSaveImage(det_name.c_str(), img)) {
                        cout << "ERROR: cannot save image with savedetector option.";
                        cout << "exiting..." << endl;
                        exit(EXIT_FAILURE);
                    }
                    cvReleaseImage(&img);
                    save_detection_counter++;
                }
            }

            //print frame number
            string s;
            ostringstream oss2;
            oss2 << frame_number;
            s.assign(oss2.str());
            int s_size = s.size();
            cvRectangle(detectionImage, cvPoint(10, 5), cvPoint(8 + s_size * 13, 22), cvScalar(255, 255, 255), -1);
            cvPutText(detectionImage, s.c_str(), cvPoint(10,20), &font, CV_RGB(0, 0, 0));
            frame_number++;

            //FPS calculation
            fps_stop = clock();
            fps_t += (fps_stop - fps_start);

            if( (fps_counter % 20) == 0 )
            {
                //cout << "\n\n\n"  << endl;
                fps_value = (double)20 / ((double)fps_t / (long)CLOCKS_PER_SEC);
                //cout << "\nFPS calculation: " << fps_value <<" frame/sec" << endl;
                //cout << "\n\n\n"  << endl;
                fps_t = 0;
                fps_counter = 0;
            }
            fps_s.assign("FPS: ");
            fps_oss.str(std::string());
            fps_oss << fps_value;
            fps_s += fps_oss.str();
            fps_s_size = fps_s.size();
            cvRectangle(detectionImage, cvPoint(70, 5), cvPoint(78 + fps_s_size * 9, 22), cvScalar(255, 255, 255), -1);
            cvPutText(detectionImage, fps_s.c_str(), cvPoint(70,20), &font, CV_RGB(0, 0, 0));

            dim_s.assign("W: ");
            dim_oss.str(std::string());
            dim_oss << grayImage->width << " H: " << grayImage->height;
            dim_s += dim_oss.str();
            dim_s_size = dim_s.size();
            cvRectangle(detectionImage, cvPoint(200, 5), cvPoint(208 + dim_s_size * 9, 22), cvScalar(255, 255, 255), -1);
            cvPutText(detectionImage, dim_s.c_str(), cvPoint(200,20), &font, CV_RGB(0, 0, 0));

            //MOSAIC GUI
            if(mosaic) {
                cvReleaseImage(&mosaic);
            }
            mosaic = imageMosaic(2, 2, mosaic_width, mosaic_height, images);
            cvShowImage("Boat Detector - Keys: <p> Play  <s> Stop  <f> Fast  <ESC> Quit", mosaic);

            if(arg->saveVideo) {
                cvWriteFrame(writer, mosaic);
            }


            if(arg->slow) {
                keyboard = cvWaitKey(0);
            }
            else {
                keyboard = cvWaitKey(30);
            }

            if(keyboard == 27) {//ESC key
                videoData.terminate = true;
                run = false;
                continue;
            }
            else if(keyboard == 's') {
                arg->slow = true;
                fastForward = false;
                cout << "SLOW MODE" << endl;
            }
            else if(keyboard == 'p') {
                arg->slow = false;
                fastForward = false;
                cout << "PLAY MODE" << endl;
            }
            else if(keyboard == 'f') {
                arg->slow = false;
                fastForward = true;
                cout << "FAST FORWARD MODE" << endl;
            }

            //increase time in ms
            t = (double)cvGetTickCount() - t;
            detector_time += (t / ((double)cvGetTickFrequency()*1000.));

        }
    }
    
#ifdef RESULTS_ENABLED
	results << "</dataset>";
	
	results.close();
#endif

    cvDestroyAllWindows();

    XmlWriter xmlwriter("detection.xml", detector->rectangles());

    return EXIT_SUCCESS;
}

IplImage* imageMosaic(int rows, int cols, int width, int height, IplImage** images) {
	IplImage* mosaic = cvCreateImage(cvSize(width, height), IPL_DEPTH_8U, 3);
	cvZero(mosaic);
	int image_width = width / cols;
	int image_height = height / rows;
	int x = 0;
	int y = 0;
	int mosaic_step = mosaic->widthStep/sizeof(uchar);
	uchar* mosaic_data = (uchar *)mosaic->imageData;
	int mosaic_channels = mosaic->nChannels;
	for(int r = 0; r < rows; r++, y+=image_height) {
		x = 0;
		for(int c = 0; c < cols; c++, x+=image_width) {
			IplImage* image = images[r*cols + c];

			IplImage* r_image =
					cvCreateImage(cvSize(image_width, image_height), image->depth, image->nChannels);

			cvResize(image, r_image, CV_INTER_CUBIC);

			int image_step = r_image->widthStep/sizeof(uchar);
			uchar* image_data = (uchar *)r_image->imageData;
			int image_channels = r_image->nChannels;

			for(int i = y, ii = 0; ii < image_height; i++, ii++) {
				for(int j = x, jj = 0; jj < image_width; j++, jj++) {

					for(int k = 0; k < mosaic_channels; k++) {
						if(image_channels == mosaic_channels) {
							mosaic_data[i*mosaic_step+j*mosaic_channels+k] =
									image_data[ii*image_step+jj*image_channels+k];
						}
						else {
							mosaic_data[i*mosaic_step+j*mosaic_channels+k] =
									image_data[ii*image_step+jj];
						}
					}
				}
			}
			cvReleaseImage(&r_image);
		}
	}
	return mosaic;
}


/*
 * Arguments Processing
 */
void arg_parse( int argc, char** argv, ArgParam *arg )
{
	if( argc < 3 ) {
		cout <<  "Usage: boatdetector\n"
				 "  -c <cascade_xml_file_name>\n"
				 "  filename | camera_index\n"
				 "  [ -vtscfg <vts_cfg_file_name> ]\n"
				 "  [ -out <avi_video_name> ]\n"
				 "  [ -sc < scale = " << arg->scale<< " > ]\n"
				 "  [ -sf < scale_factor = " << arg->scale_factor << " > ]\n"
				 "  [ -mn < min_neighbors = " << arg->min_neighbors << " > ]\n"
				 "  [ -fl < flags = " << arg->flags << " > ]\n"
				 "  [ -w < width = " << arg->width << " > ]\n"
				 "  [ -h < height = " << arg->height << " > ]\n"
				 "  [ -slow ]\n"
				 "  [ -coastline ]\n"
				 "  [ -printtracks ]\n"
				 "  [ -datacollection <avi_video_name> ]\n"
				 "  [ -savedetection <folder_path> ]\n";

		cout << "See also: cvHaarDetectObjects() about option parameters." << endl;
		cout << "exiting..." << endl;
		exit(EXIT_FAILURE);
	}

	for(int i = 1; i < argc; i++ )
	{
		if( !strcmp( argv[i], "-c" ) )
		{
			arg->cascade_name.assign(argv[++i]);
		}
		else if( !strcmp( argv[i], "-vtscfg" ) )
		{
			arg->vts_cfg.assign(argv[++i]);
		}
		else if( !strcmp( argv[i], "-out" ) )
		{
			arg->saveVideo = true;
			arg->outVideoName = argv[++i];
		}
		else if( !strcmp( argv[i], "-sc" ) )
		{
			arg->scale = (float) atof( argv[++i] );
		}
		else if( !strcmp( argv[i], "-sf" ) )
		{
			arg->scale_factor = (float) atof( argv[++i] );
		}
		else if( !strcmp( argv[i], "-mn" ) )
		{
			arg->min_neighbors = atoi( argv[++i] );
		}
		else if( !strcmp( argv[i], "-fl" ) )
		{
			arg->flags = atoi( argv[++i] );
		}
		else if( !strcmp( argv[i], "-w" ) )
		{
			arg->width = atoi( argv[++i] );
		}
		else if( !strcmp( argv[i], "-h" ) )
		{
			arg->height = atoi( argv[++i] );
		}
		else if( !strcmp( argv[i], "-slow" ) )
		{
			arg->slow = true;
		}
		else if( !strcmp( argv[i], "-coastline" ) )
		{
			arg->coastline = true;
		}
		else if( !strcmp( argv[i], "-printtracks" ) )
		{
			arg->print_tracks = true;
		}
		else if( !strcmp( argv[i], "-datacollection" ) )
		{
			arg->datacollection = true;
			arg->dcVideoName = argv[++i];
		}
		else if( !strcmp( argv[i], "-savedetection" ) )
		{
			arg->save_detection = true;
			arg->save_detection_folder = argv[++i];
		}
		else
		{
			arg->input_name.assign(argv[i]);
		}
	}
}

// callback function for heading trackbar
void headingControl(int pos)
{

}

// callback function for fov trackbar
void fovControl(int pos)
{

}

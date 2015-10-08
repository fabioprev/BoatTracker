/*
 * vtsreader.h
 *
 *  Created on: 09/nov/2010
 *      Author: Domenico
 */

#ifndef VTSREADER_H_
#define VTSREADER_H_

#include <opencv/cv.h>
#include <opencv/highgui.h>

#include <stdio.h>
#include <string>
#include <iostream>
#include <sstream>
#include <fstream>

//#include <winsock2.h>
#include <netinet/in.h>
#define WPCAP
#define HAVE_REMOTE
#include "pcap.h"

/* Ethernet addresses are 6 bytes */
#define ETHER_ADDR_LEN	6

#define PCAP_BUF_SIZE 1024
#define PCAP_SRC_IF_STRING "rpcap://"
#define PCAP_OPENFLAG_PROMISCUOUS 1

/* Ethernet header */
struct sniff_ethernet {
	u_char ether_dhost[ETHER_ADDR_LEN]; /* Destination host address */
	u_char ether_shost[ETHER_ADDR_LEN]; /* Source host address */
	u_short ether_type; /* IP? ARP? RARP? etc */
};

/* IP header */
struct sniff_ip {
	u_char ip_vhl;		/* version << 4 | header length >> 2 */
	u_char ip_tos;		/* type of service */
	u_short ip_len;		/* total length */
	u_short ip_id;		/* identification */
	u_short ip_off;		/* fragment offset field */
#define IP_RF 0x8000		/* reserved fragment flag */
#define IP_DF 0x4000		/* dont fragment flag */
#define IP_MF 0x2000		/* more fragments flag */
#define IP_OFFMASK 0x1fff	/* mask for fragmenting bits */
	u_char ip_ttl;		/* time to live */
	u_char ip_p;		/* protocol */
	u_short ip_sum;		/* checksum */
	struct in_addr ip_src,ip_dst; /* source and dest address */
};
#define IP_HL(ip)		(((ip)->ip_vhl) & 0x0f)
#define IP_V(ip)		(((ip)->ip_vhl) >> 4)

/* UDP header */
struct sniff_udp {
		 u_short uh_sport;               /* source port */
		 u_short uh_dport;               /* destination port */
		 u_short uh_ulen;                /* udp length */
		 u_short uh_sum;                 /* udp checksum */

};

#define SIZE_ETHERNET 	14
#define SIZE_UDP		8    /* length of UDP header */

using namespace std;

class VTSReader {

public:
	typedef struct FOVTrack {
		string name;
		CvPoint2D64f p;
		bool leftside;
		double camera_distance;
		double left_distance;
		double right_distance;
	} FOVTrack;

	typedef struct VTSTrack {
		string name;
		string stn;
		int latitude;
		int longitude;
		double x;
		double y;
	} VTSTrack;

private:

	typedef struct Line {
		double m;
		double q;
		bool northward;
		bool westward;
		bool undefinedslope;
	} Line;

	int current_time_hours;
	int current_time_minutes;
	int current_time_seconds;
	int current_time_milliseconds;
	long current_time;

	long refresh_time;

	//posizione della camera nell'immagine
	int camera_x;
	int camera_y;

	//fonts
	CvFont cameraFont;
	CvFont trackFont;

	//immagine google earth di background
	IplImage* backgroundImage;
	IplImage* vtsImage;
	IplImage* messageImage;

	//punto di riferimento nell'immagine background
	int reference_x;
	int reference_y;

	//punto di riferimento in lat
	int reference_lat_degrees;
	int reference_lat_minutes;
	int reference_lat_seconds;
	int reference_latitude;

	//punto di riferimento in lon
	int reference_lon_degrees;
	int reference_lon_minutes;
	int reference_lon_seconds;
	int reference_longitude;

	//scala dell'immagine background
	double lat_pixel_per_minute;
	double lon_pixel_per_minute;

	//coordinate lat della camera
	int camera_lat_degrees;
	int camera_lat_minutes;
	int camera_lat_seconds;
	int camera_latitude;

	//coordinate lon della camera
	int camera_lon_degrees;
	int camera_lon_minutes;
	int camera_lon_seconds;
	int camera_longitude;

	//camera heading in gradi georeferenziati (0-360)
	double camera_heading;
	double camera_heading_user;
	double left_heading;
	double right_heading;

	//CAMERA FOV
	double camera_fov;
	double camera_fov_user;
	Line camera_line;
	Line leftside_line;
	Line rightside_line;

	//tracks in the field of view of the camera
	vector<FOVTrack>* FOVTrackVector;
	vector<FOVTrack>* FOVTrackVector_stable;

	//VTS TRACKS
	vector<VTSTrack>* vtsTrackVector;

	VTSTrack vtsTrack;

	string line;
	string lat;
	string lon;
	string timestamp;
	string name;
	string stn;

	int index_lat;
	int index_lon;
	int index_timestamp;
	int index_name;
	int index_stn;
	string::size_type pos;

	int lat_degrees;
	int lat_minutes;
	int lat_seconds;

	int lon_degrees;
	int lon_minutes;
	int lon_seconds;

	int time_hours;
	int time_minutes;
	int time_seconds;
	int time_milliseconds;

	int track_latitude;
	int track_longitude;
	long track_time;

	double track_x;
	double track_y;

	string pckstr;
	string msg;

	//libpcap
	pcap_t *adhandle;
	char errbuf[PCAP_ERRBUF_SIZE];
	char source[PCAP_BUF_SIZE];
	const struct sniff_ip *ip;		// The IP header
	const struct sniff_udp *udp;  	// The UDP header
	struct pcap_pkthdr *header;		// packet header
	const u_char *pkt_data;			// packet data (header included)
	u_char *payload;              	// Packet payload
	int size_ip;
	int size_payload;

	pcap_if_t *alldevs;
	pcap_if_t *d;

	//DATA COLLECTION
	bool datacollection;
	string dc_prefix;
	int dc_counter;

public:
	VTSReader(bool datacollection, char* dcVideoName);
	~VTSReader();
	bool init(const char* vts_cfg, CvSize &vts_image_size);
	bool open();
	bool getData(long &time);

	void drawVTSView(IplImage* vtsView);
	inline IplImage* getVTSImage() {
		return vtsImage;
	};
	inline long getTime() {
		return current_time;
	};
	inline long getRefreshTime() {
		return refresh_time;
	};
	inline vector<FOVTrack>* getFOVTrackVector() {
		return FOVTrackVector_stable;
	};
	inline double getCameraHeading(){
		return camera_heading;
	};
	inline void setCameraHeading(double heading){
		camera_heading_user = heading;
	};
	inline double getCameraFov(){
		return camera_fov;
	};
	inline void setCameraFov(double fov){
		camera_fov_user = fov;
	};

private:
	double getYImage(int latitude);
	double getXImage(int longitude);
	void getCameraLine(Line &line, float heading_deg);
	void drawLine(Line* line, CvScalar color, int length);

	string checkmessagecatandtype(u_char buf[], int buf_length);
	bool checkmessagelength(u_char buf[], int buf_length);
	string decodeVTSmsg(u_char buf[], int buf_length, string pckstring);
	string decodeST(u_char buf[], int buf_length, int STtype);
	string decodelocaltrack(u_char buf[]);
	string decodeplot(u_char buf[]);

	bool readCfgFile(const char* vts_cfg, CvSize &vts_image_size);
};

#endif /* VTSREADER_H_ */

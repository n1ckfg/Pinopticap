#pragma once

#include "ofMain.h"
#include "ofxCv.h"
#include "ofxOsc.h"
#include "ofxXmlSettings.h"
#include "ofxHTTP.h"
#include "ofxJSONElement.h"
#include "ofxCrypto.h"

#define NUM_MESSAGES 30 // how many past ws messages we want to keep

class ofApp : public ofBaseApp {

    public:	
	void setup();
	void update();
	void draw();
		
	int width, height, appFramerate, camFramerate;
	
	string uniqueId, hostName; 
	string oscHost;
	int oscPort, streamPort, wsPort, postPort;

	bool debug; // draw to local screen, default true

	ofFile file;
	ofxXmlSettings settings;

	ofFbo fbo;
	ofPixels pixels;
	int rpiCamVersion; // 0 for not an RPi cam, 1, 2, or 3
	string lastPhotoTakenName;
	int stillCompression;
	int timestamp;
	
	void createResultHtml(string fileName);
	void takePhoto();

	ofxHTTP::SimpleIPVideoServer streamServer;
	ofxHTTP::SimpleIPVideoServerSettings streamSettings;
	vector<string> photoFiles;

	ofxHTTP::SimplePostServer postServer;
	ofxHTTP::SimplePostServerSettings postSettings;
	void onHTTPPostEvent(ofxHTTP::PostEventArgs& evt);
	void onHTTPFormEvent(ofxHTTP::PostFormEventArgs& evt);
	void onHTTPUploadEvent(ofxHTTP::PostUploadEventArgs& evt);
	    	       
    ofxHTTP::SimpleWebSocketServer wsServer;  
	ofxHTTP::SimpleWebSocketServerSettings wsSettings;
	void onWebSocketOpenEvent(ofxHTTP::WebSocketEventArgs& evt);
	void onWebSocketCloseEvent(ofxHTTP::WebSocketCloseEventArgs& evt);
	void onWebSocketFrameReceivedEvent(ofxHTTP::WebSocketFrameEventArgs& evt);
	void onWebSocketFrameSentEvent(ofxHTTP::WebSocketFrameEventArgs& evt);
	void onWebSocketErrorEvent(ofxHTTP::WebSocketErrorEventArgs& evt);
    
	// ~ ~ ~ ~ ~ ~ ~     

	bool sendOsc;  // send osc, default true
	bool sendWs;  // send websockets, default true
	bool sendHttp;  // serve web control panel, default true
	bool sendMjpeg;  // send mjpeg stream, default true	
	bool syncVideo;  // send video image over osc, default false
	bool brightestPixel;  // send brightest pixel, default false
	bool blobs;  // send blob tracking, default true
	bool contours; // send contours, default false

	ofBuffer videoBuffer;
	ofBuffer photoBuffer;
	ofBuffer contourColorBuffer;
	ofBuffer contourPointsBuffer;

	cv::Mat frame, frameProcessed;
	ofImage gray;
	int syncVideoQuality; // 5 best to 1 worst, default 3 medium
	bool videoColor;

	//string oscAddress;
	int thresholdValue; // default 127
	int thresholdKeyCounter;
	bool thresholdKeyFast;

	ofxOscSender sender;
	void sendOscVideo();
	void sendOscBlobs(int index, float x, float y);
	void sendOscContours(int index);
	void sendOscPixel(float x, float y);
	void sendWsVideo();
	void sendWsBlobs(int index, float x, float y);
	void sendWsContours(int index);
	void sendWsPixel(float x, float y);

	ofxCv::ContourFinder contourFinder;
	float contourThreshold;  // default 127
	float contourMinAreaRadius; // default 10
	float contourMaxAreaRadius; // default 150
	int contourSlices; // default 20
	ofxCv::TrackingColorMode trackingColorMode; // RGB, HSV, H, HS; default RGB

};

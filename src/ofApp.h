#pragma once

#include "ofMain.h"
#include "ofxCv.h"
#include "ofxOsc.h"
#include "ofxXmlSettings.h"

#define NUM_MESSAGES 30 // how many past ws messages we want to keep

class ofApp : public ofBaseApp {

    public:	
	void setup();
	void update();
	void draw();
		
	int width, height, appFramerate, camFramerate;
	
	string uniqueId, hostName; 
	string oscHost;
	int oscPort, oscReceivePort, streamPort, wsPort, postPort;

	bool debug; // draw to local screen, default true

	ofFile file;
	ofxXmlSettings settings;

	ofFbo fbo;
	ofPixels pixels;
	int rpiCamVersion; // 0 for not an RPi cam, 1, 2, or 3
	string lastPhotoTakenName;
	int stillCompression;
	int timestamp;
   
	// ~ ~ ~ ~ ~ ~ ~     

	ofBuffer videoBuffer;
	ofBuffer photoBuffer;
	ofBuffer contourColorBuffer;
	ofBuffer contourPointsBuffer;

	cv::Mat frame, frameProcessed;
	ofImage gray;
	int syncVideoQuality; // 5 best to 1 worst, default 3 medium
    
	ofxOscSender sender;
    ofxOscReceiver receiver;
	void sendOscBlobs(int index, float x, float y, float z);

};

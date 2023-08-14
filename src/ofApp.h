#pragma once

#include "ofMain.h"
#include "ofxCv.h"
#include "ofxOsc.h"
#include "ofxXmlSettings.h"
#include "Eye.h"

class ofApp : public ofBaseApp {

    public:	
        void setup();
        void update();
        void draw();
            
        int width, height, appFramerate, camFramerate;
		int eyeWidth, eyeHeight, imageWidth, imageHeight, numColumns, numRows;

		string sessionId, hostName;

		string oscHost;
        int oscPort, oscReceivePort;
		int timestamp;
		int dotSize;

        bool debug; // draw to local screen, default true
		bool videoEnabled = false;

        vector<Eye> eyes;
    
        ofFile file;
        ofxXmlSettings settings;

        ofxOscSender sender;
        ofxOscReceiver receiver;
        void sendOscBlobs(int index, float x, float y, float z);
        
        int checkSessionId(string _sessionId);
    
};

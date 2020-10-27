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
        
        string uniqueId, hostName;
        string oscHost;
        int oscPort, oscReceivePort;
        
        int timestamp;
        
        bool debug; // draw to local screen, default true

        vector<Eye> eyes;
    
        ofFile file;
        ofxXmlSettings settings;

        ofxOscSender sender;
        ofxOscReceiver receiver;
        void sendOscBlobs(int index, float x, float y, float z);

};

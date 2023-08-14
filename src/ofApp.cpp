#include "ofApp.h"
#include "../../common/src/PinopticonUtils.hpp"

using namespace cv;
using namespace ofxCv;
using namespace PinopticonUtils;

//--------------------------------------------------------------
void ofApp::setup() {
    settings.loadFile("settings.xml");
    
    ofSetVerticalSync(false);
    ofHideCursor();

    appFramerate = settings.getValue("settings:app_framerate", 60);
    camFramerate = settings.getValue("settings:cam_framerate", 30);
    ofSetFrameRate(appFramerate);   

	numColumns = 3;
	numRows = 3;
	imageWidth = 640;
	imageHeight = 480;
	eyeWidth = imageWidth / 2;
	eyeHeight = imageHeight / 2;
	width = eyeWidth * numColumns; // settings.getValue("settings:width", 1280);
	height = eyeHeight * numRows; // settings.getValue("settings:height", 720);
	ofSetWindowShape(width, height);

    debug = (bool) settings.getValue("settings:debug", 1);

    oscHost = settings.getValue("settings:osc_host", "127.0.0.1");
    oscPort = settings.getValue("settings:osc_port", 7114);
    oscReceivePort = settings.getValue("settings:osc_receive_port", 7110);
 
    debug = (bool) settings.getValue("settings:debug", 1);

	dotSize = 10;

	// ~ ~ ~   get a persistent name for this computer   ~ ~ ~
#ifdef TARGET_LINUX_ARM
    ofSystem("cp /etc/hostname " + ofToDataPath("DocumentRoot/js/"));
    hostName = getHostName();
#else
	hostName = "unknown";
#endif
    sessionId = getSessionId();

    sender.setup(oscHost, oscPort);
    receiver.setup(oscReceivePort);
    cout << "Using OSC." << endl;
}

//--------------------------------------------------------------
void ofApp::update() {
    timestamp = (int) ofGetSystemTimeMillis();
    
    // check for waiting messages
    while (receiver.hasWaitingMessages()) {
        // get the next message
        ofxOscMessage msg;
        receiver.getNextMessage(msg);

		//cout << "New OSC message received." << endl;

        string newHostName = msg.getArgAsString(0);
        string newsessionId = msg.getArgAsString(1);
        
        int whichOne = checkSessionId(newsessionId);
        
        if (whichOne == -1) {
			cout << "New Eye detected: " << newHostName << " " << newsessionId << endl;
			whichOne = eyes.size();
			Eye eye = Eye(newHostName, newsessionId, whichOne);
			eyes.push_back(eye);
        }
        
        if (msg.getAddress() == "/blob") {
            int index = msg.getArgAsInt32(2);
            float x = msg.getArgAsFloat(3);
            float y = msg.getArgAsFloat(4);
            int msg_timestamp = msg.getArgAsInt32(5);
            
            eyes[whichOne].addBlob(index, x, y, msg_timestamp, timestamp);
        } else if (msg.getAddress() == "/video" && videoEnabled) {
            ofBuffer buffer = msg.getArgAsBlob(2);
			ofImage image;
			bufferToImage(buffer, image, imageWidth, imageHeight, true);
            int msg_timestamp = msg.getArgAsInt32(3);
            
            eyes[whichOne].addVideo(image, msg_timestamp, timestamp);
        }
    }
}

//--------------------------------------------------------------
void ofApp::draw() {
	if (debug) {
		ofBackground(0);

        cam.begin();
        ofEnableDepthTest();
        ofDrawGrid(20, 10, true, true, true, true);
        ofDisableDepthTest();
        cam.end();

		for (int i=0; i<eyes.size(); i++) {
			int originX = eyeWidth * (i % numColumns);
			int originY = eyeHeight * (i / numRows);

			ofSetColor(eyes[i].bgColor);
			ofRect(eyeWidth * (i % numColumns), eyeHeight * (i / numRows), eyeWidth, eyeHeight);

			if (videoEnabled) {
				for (int j = 0; j < eyes[i].videos.size(); j++) {
					eyes[i].videos[j].image.draw(0, 0);
				}
			}

			for (int j = 0; j < eyes[i].blobSequences.size(); j++) {
				for (int k = 0; k < eyes[i].blobSequences[j].blobs.size(); k++) {
					int x = eyes[i].blobSequences[j].blobs[k].x * eyeWidth;
					int y = eyes[i].blobSequences[j].blobs[k].y * eyeHeight;

					x += originX;
					y += originY;
					int idx = eyes[i].blobSequences[j].blobs[k].index;			

					ofSetColor(eyes[i].idColor);
					ofCircle(x, y, dotSize);
					
					ofSetColor(eyes[i].bgColor);
					string msg = ofToString(idx);
					ofDrawBitmapString(msg, x - dotSize / 2, y + dotSize / 2);

					ofDrawBitmapStringHighlight(eyes[i].hostName, originX, originY + eyeHeight, ofColor::black, ofColor::yellow);
				}
			}
		}

        stringstream info;
        info << width << "x" << height << " @ " << ofGetFrameRate() << "fps" << " / " << timestamp << "\n";
        ofDrawBitmapStringHighlight(info.str(), 10, 10, ofColor::black, ofColor::yellow);
    }
}

int ofApp::checkSessionId(string _sessionId) {
    int idx = -1;
    for (int i=0; i<eyes.size(); i++) {
        if (_sessionId == eyes[i].sessionId) {
            idx = i;
            break;
        }
    }
    return idx;
}


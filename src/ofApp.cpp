#include "ofApp.h"

using namespace cv;
using namespace ofxCv;

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
	// a randomly generated id
#ifdef TARGET_LINUX_ARM
	uniqueId = "RPi";
#else
	uniqueId = "";
#endif

	file.open(ofToDataPath("unique_id.txt"), ofFile::ReadWrite, false);
    ofBuffer buff;
    if (file) { // use existing file if it's there
        buff = file.readToBuffer();
        uniqueId = buff.getText();
    } else { // otherwise make a new one
        uniqueId += "_" + ofGetTimestampString("%y%m%d%H%M%S%i");
        ofStringReplace(uniqueId, "\n", "");
        ofStringReplace(uniqueId, "\r", "");
        buff.set(uniqueId.c_str(), uniqueId.size());
        ofBufferToFile("unique_id.txt", buff);
    }

#ifdef TARGET_LINUX_ARM
    // the actual RPi hostname
    ofSystem("cp /etc/hostname " + ofToDataPath("DocumentRoot/js/"));
    hostName = ofSystem("cat /etc/hostname");
    hostName.pop_back(); // last char is \n
#else
	hostName = "unknown";
#endif

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
        string newUniqueId = msg.getArgAsString(1);
        
        int whichOne = checkUniqueId(newUniqueId);
        
        if (whichOne == -1) {
			cout << "New Eye detected: " << newHostName << " " << newUniqueId << endl;
			whichOne = eyes.size();
			Eye eye = Eye(newHostName, newUniqueId, whichOne);
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
			image.allocate(imageWidth, imageHeight, OF_IMAGE_GRAYSCALE);
			image.load(buffer);
            int msg_timestamp = msg.getArgAsInt32(3);
            
            eyes[whichOne].addVideo(image, msg_timestamp, timestamp);
        }
    }
}

//--------------------------------------------------------------
void ofApp::draw() {
	if (debug) {
		ofBackground(0);

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

void ofApp::sendOscBlobs(int index, float x, float y, float z) {
    ofxOscMessage m;
    m.setAddress("/blob");

	m.addStringArg(hostName);
	m.addStringArg(uniqueId);
    m.addIntArg(index);  
    m.addFloatArg(x);
    m.addFloatArg(y);
    m.addFloatArg(z);
    m.addIntArg(timestamp);

    sender.sendMessage(m);
}

int ofApp::checkUniqueId(string _uniqueId) {
    int idx = -1;
    for (int i=0; i<eyes.size(); i++) {
        if (_uniqueId == eyes[i].uniqueId) {
            idx = i;
            break;
        }
    }
    return idx;
}


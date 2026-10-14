#include "ofApp.h"

#include "../../common/src/Pinopticon.hpp"
#include "../../common/src/Pinopticon_Http.hpp"
#include "../../common/src/Pinopticon_Osc.hpp"

using namespace cv;
using namespace ofxCv;
using namespace Pinopticon;

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
	thumbWidth = 120;
	thumbHeight = 90;
	eyeWidth = 320;
	eyeHeight = 240;
	width = eyeWidth * numColumns; // settings.getValue("settings:width", 1280);
	height = eyeHeight * numRows; // settings.getValue("settings:height", 720);
	ofSetWindowShape(width, height);

    debug = (bool) settings.getValue("settings:debug", 1);

    oscHost = settings.getValue("settings:osc_host", "127.0.0.1");
    oscReceivePort = settings.getValue("settings:osc_receive_port", 7110);
    streamPort = settings.getValue("settings:stream_port", 7111);
    wsClientPort = settings.getValue("settings:ws_client_port", 7112);
    wsServerPort = settings.getValue("settings:ws_server_port", 7114);
    postPort = settings.getValue("settings:post_port", 7113);
    oscSendPort = settings.getValue("settings:osc_send_port", 7114);
 
    sendOsc = (bool)settings.getValue("settings:send_osc", 1);
    sendWs = (bool)settings.getValue("settings:send_ws", 1);
    sendHttp = (bool)settings.getValue("settings:send_http", 1);
    sendMjpeg = (bool)settings.getValue("settings:send_mjpeg", 1);

    debug = (bool) settings.getValue("settings:debug", 1);

	dotSize = 10;

    // ~ ~ ~   where each eye is, for triangulating blobs in 3D   ~ ~ ~
    string calibrationFile = settings.getValue("settings:calibration_file", "reconstruction.json");
    maxRayAngle = glm::radians(settings.getValue("settings:max_ray_angle", 1.0));
    maxBlobAge = 3000 / std::max(camFramerate, 1); // three camera frames
    if (ofFile::doesFileExist(calibrationFile)) calibrations = loadEyeCalibrations(calibrationFile);
    cout << "\nCalibration: " << calibrations.size() << " eyes from " << calibrationFile << endl;

    // the farthest eye goes halfway out on the grid
    float reach = 0;
    for (auto& calibration : calibrations) reach = std::max(reach, (float) glm::length(calibration.second.center));
    worldScale = (reach > 0) ? 100.0 / reach : 1.0;

	// ~ ~ ~   get a persistent name for this computer   ~ ~ ~
#ifdef TARGET_LINUX_ARM
    ofSystem("cp /etc/hostname " + ofToDataPath("DocumentRoot/js/"));
    hostName = getHostName();
#else
	hostName = "unknown";
#endif
    sessionId = getSessionId();

    // * stream video *
    int maxClientConnections = settings.getValue("settings:max_stream_connections", 5); // default 5
    int maxClientBitRate = settings.getValue("settings:max_stream_bitrate", 512); // default 1024
    int maxClientFrameRate = settings.getValue("settings:max_stream_framerate", 30); // default 30
    int maxClientQueueSize = settings.getValue("settings:max_stream_queue", 10); // default 10
    if (sendMjpeg) setupMjpeg(streamServer, streamPort, maxClientConnections, maxClientBitRate, maxClientFrameRate, maxClientQueueSize, width, height, "live_view.html");

    // * post form *
    if (sendHttp) setupHttp(this, postServer, postPort, "result.html");

    // * websockets *
    // events: connect, open, close, idle, message, broadcast
    if (sendWs) setupWsServer(this, wsServer, wsServerPort);

    if (sendOsc) setupOscSender(sender, oscHost, oscSendPort);
    
    setupOscReceiver(receiver, oscReceivePort);
}

//--------------------------------------------------------------
void ofApp::update() {
    timestamp = getTimestamp();
    bool newBlobs = false;
    
    // check for waiting messages
    while (receiver.hasWaitingMessages()) {
        // get the next message
        ofxOscMessage msg;
        receiver.getNextMessage(msg);

        string newHostName = msg.getArgAsString(0);
        string newSessionId = msg.getArgAsString(1);

        int whichOne = checkHostName(newHostName);
        //int whichOne = checkSessionId(newsessionId);

        if (whichOne == -1) {
			whichOne = eyes.size();
			Eye eye = Eye(newHostName, newSessionId, whichOne);
			eyes.push_back(eye);
            eyes[whichOne].wsClientConnect(eye.hostName, wsClientPort);

            if (calibrations.count(newHostName) > 0) {
                eyes[whichOne].calibration = calibrations[newHostName];
                eyes[whichOne].calibrated = true;
            }

            cout << "New Eye detected: " << newHostName << " " << newSessionId << (eyes[whichOne].calibrated ? " (calibrated)" : "") << endl;
        }
        
        Eye eye = eyes[whichOne];

        if (eye.sessionId != newSessionId) {
            eyes[whichOne].sessionId = newSessionId;
            eyes[whichOne].wsClientConnect(eye.hostName, wsClientPort);

            cout << "Eye reconnected: " << eye.hostName << " " << eye.sessionId << endl;
        }

        //cout << "New OSC message " << msg.getAddress() << " received from " << eye.hostName << " " << eye.sessionId << endl;

        if (msg.getAddress() == "/blob") {
            int index = msg.getArgAsInt32(2);
            float x = msg.getArgAsFloat(3);
            float y = msg.getArgAsFloat(4);
            int msg_timestamp = msg.getArgAsInt32(5);
            
            eyes[whichOne].addBlob(index, x, y, msg_timestamp, timestamp);
            newBlobs = true;
        } else if (msg.getAddress() == "/video" && videoEnabled) {
            ofBuffer buffer = msg.getArgAsBlob(2);
            ofImage image;
            bufferToImage(buffer, image, thumbWidth, thumbHeight, false);
            //cout << "Received image " << image.getWidth() << "x" << image.getHeight() << endl;
            int msg_timestamp = msg.getArgAsInt32(3);

            eyes[whichOne].addVideo(image, msg_timestamp, timestamp);
        } else if (msg.getAddress() == "/contour") {
            /*
            ofBuffer buffer = msg.getArgAsBlob(2);
            ofImage image;
            bufferToImage(buffer, image, imageWidth, imageHeight, true);
            cout << "Received image " << image.getWidth() << "x" << image.getHeight() << endl;
            int msg_timestamp = msg.getArgAsInt32(3);

            eyes[whichOne].addVideo(image, msg_timestamp, timestamp);
            */
        }
    }

    triangulateBlobs();

    // pass the 3D blobs on whenever the eyes have reported something new
    if (sendOsc && sender.isReady() && newBlobs) {
        for (int i = 0; i < blobs3d.size(); i++) {
            sendOscBlobs(blobs3d[i].index, blobs3d[i].position.x, blobs3d[i].position.y, blobs3d[i].position.z);
        }
    }
}

//--------------------------------------------------------------
void ofApp::triangulateBlobs() {
    blobRays.clear();
    blobs3d.clear();

    // a ray through the newest position of every blob still being reported
    for (int i = 0; i < eyes.size(); i++) {
        if (!eyes[i].calibrated) continue;

        for (int j = 0; j < eyes[i].blobSequences.size(); j++) {
            Eye::EyeBlob& blob = eyes[i].blobSequences[j].blobs.back();
            if (timestamp - blob.received > maxBlobAge) continue;
            if (blob.x < 0 || blob.x > 1 || blob.y < 0 || blob.y > 1) continue; // must be a fraction of the image, not pixels

            BlobRay ray;
            ray.index = blob.index;
            ray.origin = eyes[i].calibration.center;
            ray.direction = eyes[i].calibration.ray(blob.x, blob.y);
            blobRays.push_back(ray);
        }
    }

    // rays with the same blob index are taken to be the same point
    std::sort(blobRays.begin(), blobRays.end(), [](const BlobRay& a, const BlobRay& b) { return a.index < b.index; });

    int first = 0;
    while (first < blobRays.size()) {
        int last = first + 1;
        while (last < blobRays.size() && blobRays[last].index == blobRays[first].index) last++;

        glm::dvec3 position;
        int numEyes = intersectRays(&blobRays[first], last - first, maxRayAngle, position);

        if (numEyes > 0) {
            Blob3d blob;
            blob.index = blobRays[first].index;
            blob.position = glm::vec3(position);
            blob.numEyes = numEyes;
            blobs3d.push_back(blob);
        }

        first = last;
    }
}

//--------------------------------------------------------------
void ofApp::draw() {
	if (debug) {
		ofBackground(0);

        cam.begin();
        ofEnableDepthTest();
        ofDrawGrid(20, 10, true, true, true, true);
        drawWorld();
        ofDisableDepthTest();
        cam.end();

		for (int i=0; showEyes && i<eyes.size(); i++) {
			int originX = eyeWidth * (i % numColumns);
			int originY = eyeHeight * (i / numRows);

			ofSetColor(eyes[i].bgColor);
			ofRect(originX, originY, eyeWidth, eyeHeight);

			if (videoEnabled) {
				eyes[i].drawVideo(originX, originY, eyeWidth, eyeHeight);
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
				}
			}

            ofDrawBitmapStringHighlight(eyes[i].hostName, originX, originY + eyeHeight, ofColor::black, ofColor::yellow);
        }

        stringstream info;
        info << width << "x" << height << " @ " << ofGetFrameRate() << "fps" << " / " << timestamp << "\n";
        if (calibrations.size() > 0) info << blobs3d.size() << " blobs in 3D / spacebar " << (showEyes ? "hides" : "shows") << " eyes\n";
        ofDrawBitmapStringHighlight(info.str(), 10, 10, ofColor::black, ofColor::yellow);
    }
}

// the calibrated eyes and the blobs triangulated from them, inside the 3D grid
void ofApp::drawWorld() {
    for (auto& calibration : calibrations) {
        int whichOne = checkHostName(calibration.first);
        ofSetColor(whichOne == -1 ? ofColor(127) : eyes[whichOne].idColor);

        // an eye is drawn as the corners of its image
        glm::vec3 center = glm::vec3(calibration.second.center) * worldScale;
        glm::vec3 corners[4];
        for (int i = 0; i < 4; i++) {
            float x = (i == 1 || i == 2) ? 1 : 0;
            float y = (i >= 2) ? 1 : 0;
            corners[i] = center + glm::vec3(calibration.second.ray(x, y)) * (dotSize * 2.0f);
        }
        for (int i = 0; i < 4; i++) {
            ofDrawLine(center, corners[i]);
            ofDrawLine(corners[i], corners[(i + 1) % 4]);
        }

        ofDrawBitmapString(calibration.first, center);
    }

    ofSetColor(255);
    for (int i = 0; i < blobs3d.size(); i++) {
        glm::vec3 position = blobs3d[i].position * worldScale;
        ofDrawSphere(position, dotSize / 2);
        ofDrawBitmapString(ofToString(blobs3d[i].index), position + glm::vec3(dotSize));
    }
}

// a 3D blob in the units of the calibration, sent under the name of this computer
void ofApp::sendOscBlobs(int index, float x, float y, float z) {
    Pinopticon::sendOscBlobs(sender, hostName, sessionId, index, x, y, z, timestamp);
}

void ofApp::keyPressed(int key) {
    if (key == ' ') showEyes = !showEyes;
}

void ofApp::exit() {
    for (int i = 0; i < eyes.size(); i++) {
        eyes[i].wsClient -> close();
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

int ofApp::checkHostName(string _hostName) {
    int idx = -1;
    for (int i = 0; i < eyes.size(); i++) {
        if (_hostName == eyes[i].hostName) {
            idx = i;
            break;
        }
    }
    return idx;
}

void ofApp::onWebSocketOpenEvent(ofxHTTP::WebSocketEventArgs& evt) {
    cout << "Websocket connection opened." << evt.connection().clientAddress().toString() << endl;
}

void ofApp::onWebSocketCloseEvent(ofxHTTP::WebSocketCloseEventArgs& evt) {
    cout << "Websocket connection closed." << evt.connection().clientAddress().toString() << endl;
}

void ofApp::onWebSocketFrameReceivedEvent(ofxHTTP::WebSocketFrameEventArgs& evt) {
    cout << "Websocket frame was received:" << evt.connection().clientAddress().toString() << endl;
    string msg = evt.frame().getText();
    cout << msg << endl;

    if (msg == "take_photo") {
        //takePhoto();
    } else if (msg == "stream_photo") {
        //streamPhoto();
    }
}

void ofApp::onWebSocketFrameSentEvent(ofxHTTP::WebSocketFrameEventArgs& evt) {
    cout << "Websocket frame was sent." << endl;
}


void ofApp::onWebSocketErrorEvent(ofxHTTP::WebSocketErrorEventArgs& evt) {
    cout << "Websocket Error." << evt.connection().clientAddress().toString() << endl;
}

// ~ ~ ~ POST ~ ~ ~
void ofApp::onHTTPPostEvent(ofxHTTP::PostEventArgs& args) {
    ofLogNotice("ofApp::onHTTPPostEvent") << "Data: " << args.getBuffer().getText();

    //takePhoto();
}


void ofApp::onHTTPFormEvent(ofxHTTP::PostFormEventArgs& args) {
    ofLogNotice("ofApp::onHTTPFormEvent") << "";
    ofxHTTP::HTTPUtils::dumpNameValueCollection(args.getForm(), ofGetLogLevel());

    //takePhoto();
}


void ofApp::onHTTPUploadEvent(ofxHTTP::PostUploadEventArgs& args) {
    std::string stateString = "";

    switch (args.getState()) {
    case ofxHTTP::PostUploadEventArgs::UPLOAD_STARTING:
        stateString = "STARTING";
        break;
    case ofxHTTP::PostUploadEventArgs::UPLOAD_PROGRESS:
        stateString = "PROGRESS";
        break;
    case ofxHTTP::PostUploadEventArgs::UPLOAD_FINISHED:
        stateString = "FINISHED";
        break;
    }

    ofLogNotice("ofApp::onHTTPUploadEvent") << "";
    ofLogNotice("ofApp::onHTTPUploadEvent") << "         state: " << stateString;
    ofLogNotice("ofApp::onHTTPUploadEvent") << " formFieldName: " << args.getFormFieldName();
    ofLogNotice("ofApp::onHTTPUploadEvent") << "orig. filename: " << args.getOriginalFilename();
    ofLogNotice("ofApp::onHTTPUploadEvent") << "     filename: " << args.getFilename();
    ofLogNotice("ofApp::onHTTPUploadEvent") << "     fileType: " << args.getFileType().toString();
    ofLogNotice("ofApp::onHTTPUploadEvent") << "# bytes xfer'd: " << args.getNumBytesTransferred();
}

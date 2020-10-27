#pragma once

#include "ofMain.h"

class Eye {

    public:
        Eye(string _hostName, string _uniqueId);
        virtual ~Eye(){};
    
        string uniqueId;
        string hostName;
        
        struct EyeBlob {
            int index;
            float x;
            float y;
            int timestamp;
            int diff;
        };
        
        vector<EyeBlob> blobs;
        
        struct EyeVideo {
            ofBuffer buffer;
            ofImage image;
            int timestamp;
            int diff;
        };
    
        vector<EyeVideo> videos;
        
        int numSamples = 30;
    
        void addBlob(int _index, float _x, float _y, int _timestamp, int _diff_timestamp);
        void addVideo(ofBuffer _buffer, int _timestamp, int _diff_timestamp);
    
};


#pragma once

#include "ofMain.h"

class Eye {

    public:
        Eye();
        virtual ~Eye(){};
    
        string uniqueId;
        string hostName;
        int timestamp;
    
};

#pragma once 

#include <time.h>
#include <cstdint>


class Clock{
    public:
        virtual uint64_t nowMs() const = 0;
        virtual ~Clock() = default;
};

class MonotonicClock : public Clock{
    public:
        uint64_t nowMs() const override{
            struct timespec tv = {0, 0};
            clock_gettime(CLOCK_MONOTONIC, &tv);
            return uint64_t(tv.tv_sec) * 1000 + tv.tv_nsec / 1000 / 1000;
        }
    
};

class FakeClock : public Clock{
    private:
        uint64_t current_ms;
    public: 
        FakeClock(){
            current_ms = 0;
        }

        FakeClock(uint64_t time){
            current_ms = time;
        }

        void advance(uint64_t t){
            current_ms += t;
        }

        uint64_t nowMs() const override{
            return current_ms;
        }
};
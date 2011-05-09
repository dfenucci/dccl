#include <cstdlib> // for rand

#include "goby/core.h"

#include "config.pb.h"
#include "depth_reading.pb.h"

using goby::core::operator<<;

class DepthSimulator : public goby::core::Application
{
public:
    DepthSimulator()
        : goby::core::Application(&cfg_)
        { }

    void loop()
        {
            DepthReading reading;
            // just post the depth given in the configuration file plus a
            // small random offset
            reading.set_depth(cfg_.depth() + (rand() % 10) / 10.0);
            
            goby::glog << reading << std::flush;
            publish(reading);    
        }

    static DepthSimulatorConfig cfg_;
};

DepthSimulatorConfig DepthSimulator::cfg_;

int main(int argc, char* argv[])
{
    return goby::run<DepthSimulator>(argc, argv);
}


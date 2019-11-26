/******************************************************************************
 * Copyright (c) 2018, Texas Instruments Incorporated - http://www.ti.com/
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions are met:
 *       * Redistributions of source code must retain the above copyright
 *         notice, this list of conditions and the following disclaimer.
 *       * Redistributions in binary form must reproduce the above copyright
 *         notice, this list of conditions and the following disclaimer in the
 *         documentation and/or other materials provided with the distribution.
 *       * Neither the name of Texas Instruments Incorporated nor the
 *         names of its contributors may be used to endorse or promote products
 *         derived from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *   ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 *   LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *   ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 *   THE POSSIBILITY OF SUCH DAMAGE.
 *****************************************************************************/
#include <signal.h>
#include <getopt.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <cassert>
#include <string>
#include <functional>
#include <queue>
#include <algorithm>
#include <time.h>
#include <memory.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

#include "executor.h"
#include "execution_object.h"
#include "execution_object_pipeline.h"
#include "configuration.h"
#include "imgutil.h"

#include "opencv2/opencv.hpp"
#include "opencv2/core.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/videoio.hpp"

#define MAX_EOPS 8
#define MAX_CLASSES 1100

using namespace tidl;
using namespace cv;
using namespace std;

int current_eop = 0;
int num_eops = 0;
int top_candidates = 3;
int size = 0;
int selected_items_size;
int * selected_items;
std::string * labels_classes[MAX_CLASSES];
Configuration configuration;
Executor *e_eve = nullptr;
Executor *e_dsp = nullptr;
std::vector<ExecutionObjectPipeline *> eops;
int last_rpt_id = -1;


float the_distance=3;
int rot=90;

bool CreateExecutionObjectPipelines();
void AllocateMemory(const std::vector<ExecutionObjectPipeline*>& eops);
bool ProcessFrame(ExecutionObjectPipeline* eop, Mat &src);
void DisplayFrame(const ExecutionObjectPipeline* eop, Mat& dst);
int tf_postprocess(uchar *in);
bool tf_expected_id(int id);
void populate_labels(const char* filename);

// exports for the filter
extern "C" {
    bool filter_init(const char* args, void** filter_ctx);
    void filter_process(void* filter_ctx, Mat& src, Mat& dst);
    void filter_free(void* filter_ctx);
}

bool verbose = false;

void init_tmpfile() {
    //first check if there is a tmp file running.  If not create one and set it to -1.0 to let the user know that they forgot to run the other service tfmini.c
	
	struct stat buf;
    if (stat("/home/debian/ramdisk/bbaibackupcam_distance", &buf) == -1)
    {
        std::cout<<("Launching t.\n")<<std::endl;
        system ("/var/lib/cloud9/BeagleBone/AI/backupcamera/s &");
        
        system ("/var/lib/cloud9/BeagleBone/AI/backupcamera/t &");
        
    } else
    
    {   //addresses file locks
        system ("sudo rm /home/debian/ramdisk/bbaibackupcam_distance");
        system ("sudo rm /home/debian/ramdisk/bbaibackupcam_rotation");
    }
	return;
}


/**
    Initializes the filter. If you return something, it will be passed to the
    filter_process function, and should be freed by the filter_free function
*/
bool filter_init(const char* args, void** filter_ctx) {
    std::cout << "Initializing filter" << std::endl;

    populate_labels("/usr/share/ti/examples/tidl/classification/imagenet.txt");

    selected_items_size = 9;
    selected_items = (int *)malloc(selected_items_size*sizeof(int));
    if (!selected_items) {
        std::cout << "selected_items malloc failed" << std::endl;
        return false;
    }
    selected_items[0] = 609; /* jeep */
    selected_items[1] = 627; /* limousine */
    selected_items[2] = 654; /* minibus */
    selected_items[3] = 656; /* minivan */
    selected_items[4] = 703; /* park_bench */
    selected_items[5] = 705; /* passenger_car */
    selected_items[6] = 779; /* school_bus */
    selected_items[7] = 829; /* streetcar */
    selected_items[8] = 176; /* Saluki */

    std::cout << "loading configuration" << std::endl;
    configuration.numFrames = 0;
    configuration.inData = 
        "/usr/share/ti/examples/tidl/test/testvecs/input/preproc_0_224x224.y";
    configuration.outData = 
        "/usr/share/ti/examples/tidl/classification/stats_tool_out.bin";
    configuration.netBinFile = 
        "/usr/share/ti/examples/tidl/test/testvecs/config/tidl_models/tidl_net_imagenet_jacintonet11v2.bin";
    configuration.paramsBinFile = 
        "/usr/share/ti/examples/tidl/test/testvecs/config/tidl_models/tidl_param_imagenet_jacintonet11v2.bin";
    configuration.preProcType = 0;
    configuration.inWidth = 224;
    configuration.inHeight = 224;
    configuration.inNumChannels = 3;
    configuration.layerIndex2LayerGroupId = { {12, 2}, {13, 2}, {14, 2} };
    configuration.enableApiTrace = false;
    configuration.runFullNet = true;

    try
    {
        std::cout << "allocating execution object pipelines (EOP)" << std::endl;
        
        // Create ExecutionObjectPipelines
        if (! CreateExecutionObjectPipelines())
            return false;

        // Allocate input/output memory for each EOP
        std::cout << "allocating I/O memory for each EOP" << std::endl;
        AllocateMemory(eops);
        num_eops = eops.size();
        std::cout << "num_eops=" << num_eops << std::endl;
        std::cout << "About to start ProcessFrame loop!!" << std::endl;
        std::cout << "http://localhost:8080/?action=stream" << std::endl;
    }
    catch (tidl::Exception &e)
    {
        std::cerr << e.what() << std::endl;
        return false;
    }

    init_tmpfile();

    return true;
}

/**
    Called by the OpenCV plugin upon each frame
*/
void filter_process(void* filter_ctx, Mat& src, Mat& dst) {
    int doDisplay = 0;
    dst = src;

    try
    {
        // Process frames with available EOPs in a pipelined manner
        // additional num_eops iterations to flush the pipeline (epilogue)
        ExecutionObjectPipeline* eop = eops[current_eop];

        // Wait for previous frame on the same eo to finish processing
        if(eop->ProcessFrameWait()) doDisplay = 1;

        ProcessFrame(eop, src);
        if(doDisplay) DisplayFrame(eop, dst);

        current_eop++;
        if(current_eop >= num_eops)
            current_eop = 0;
    }
    catch (tidl::Exception &e)
    {
        std::cerr << e.what() << std::endl;
    }

    return;
}

/**
    Called when the input plugin is cleaning up
*/
void filter_free(void* filter_ctx) {
    try
    {
        // Cleanup
        for (auto eop : eops)
        {
            free(eop->GetInputBufferPtr());
            free(eop->GetOutputBufferPtr());
            delete eop;
        }
        if (e_dsp) delete e_dsp;
        if (e_eve) delete e_eve;
    }
    catch (tidl::Exception& e)
    {
        std::cerr << e.what() << std::endl;
    }

    return;
}

bool CreateExecutionObjectPipelines()
{
    const uint32_t num_eves = 4;
    const uint32_t num_dsps = 0;
    const uint32_t buffer_factor = 1;

    DeviceIds ids_eve, ids_dsp;
    for (uint32_t i = 0; i < num_eves; i++)
        ids_eve.insert(static_cast<DeviceId>(i));
    for (uint32_t i = 0; i < num_dsps; i++)
        ids_dsp.insert(static_cast<DeviceId>(i));

#if 0
    // Create Executors with the approriate core type, number of cores
    // and configuration specified
    // EVE will run layersGroupId 1 in the network, while
    // DSP will run layersGroupId 2 in the network
    std::cout << "allocating executors" << std::endl;
    e_eve = num_eves == 0 ? nullptr :
            new Executor(DeviceType::EVE, ids_eve, configuration, 1);
    e_dsp = num_dsps == 0 ? nullptr :
            new Executor(DeviceType::DSP, ids_dsp, configuration, 2);

    // Construct ExecutionObjectPipeline that utilizes multiple
    // ExecutionObjects to process a single frame, each ExecutionObject
    // processes one layerGroup of the network
    // If buffer_factor == 2, duplicating EOPs for pipelining at
    // EO level rather than at EOP level, in addition to double buffering
    // and overlapping host pre/post-processing with device processing
    std::cout << "allocating individual EOPs" << std::endl;
    for (uint32_t j = 0; j < buffer_factor; j++)
    {
        for (uint32_t i = 0; i < std::max(num_eves, num_dsps); i++)
            eops.push_back(new ExecutionObjectPipeline(
                            {(*e_eve)[i%num_eves], (*e_dsp)[i%num_dsps]}));
    }
#else
    e_eve = num_eves == 0 ? nullptr :
            new Executor(DeviceType::EVE, ids_eve, configuration);
    e_dsp = num_dsps == 0 ? nullptr :
            new Executor(DeviceType::DSP, ids_dsp, configuration);

    // Construct ExecutionObjectPipeline with single Execution Object to
    // process each frame. This is parallel processing of frames with
    // as many DSP and EVE cores that we have on hand.
    // If buffer_factor == 2, duplicating EOPs for double buffering
    // and overlapping host pre/post-processing with device processing
    for (uint32_t j = 0; j < buffer_factor; j++)
    {
        for (uint32_t i = 0; i < num_eves; i++)
            eops.push_back(new ExecutionObjectPipeline({(*e_eve)[i]}));
        for (uint32_t i = 0; i < num_dsps; i++)
            eops.push_back(new ExecutionObjectPipeline({(*e_dsp)[i]}));
    }
#endif

    return true;
}


void AllocateMemory(const std::vector<ExecutionObjectPipeline*>& eops)
{
    for (auto eop : eops)
    {
        size_t in_size  = eop->GetInputBufferSizeInBytes();
        size_t out_size = eop->GetOutputBufferSizeInBytes();
        std::cout << "Allocating input and output buffers" << std::endl;
        void*  in_ptr   = malloc(in_size);
        void*  out_ptr  = malloc(out_size);
        assert(in_ptr != nullptr && out_ptr != nullptr);
        
        ArgInfo in(in_ptr,   in_size);
        ArgInfo out(out_ptr, out_size);
        eop->SetInputOutputBuffer(in, out);
    }
}


bool ProcessFrame(ExecutionObjectPipeline* eop, Mat &src)
{
    if(configuration.enableApiTrace)
        std::cout << "preprocess()" << std::endl;
    imgutil::PreprocessImage(src, 
                             eop->GetInputBufferPtr(), configuration);
    eop->ProcessFrameStartAsync();
        
    return false;
}

char *distance_message() {
    static char buf[20];
    static char suffix[4]=" Ft";
    static time_t timer=time(NULL);
    
    if (time(NULL)>timer) {
        int fd = open("/home/debian/ramdisk/bbaibackupcam_distance", O_RDONLY );
        if (fd>-1) {
            int result=read(fd,buf,sizeof(buf));
            if (result>-1){
                close(fd);
                memcpy(buf+3,suffix,4);
            }
            
            sscanf(buf, "%f", &the_distance);
        }
        timer=time(NULL)+.5;
    }
    return (char *)buf;
}

int rotation() {
    static char buf[20];
    
    static time_t timer=time(NULL)+.02;
    
    if (time(NULL)>timer) {
        int fd = open("/home/debian/ramdisk/bbaibackupcam_rotation", O_RDONLY );
        if (fd>-1){
            int result=read(fd,buf,sizeof(buf));
            if (result!=-1) {
                close(fd);
    
                int i;
                sscanf(buf, "%d", &i);
                rot=i;
            } 
        }
        timer=time(NULL)+.02;
    }
    return (rot);
}

void DisplayFrame(const ExecutionObjectPipeline* eop, Mat& dst)
{   static time_t timer=time(NULL);
    static string my_message;
    static std::string * static_message=new string("");
    static float the_temp_distance=10;
    if(configuration.enableApiTrace)
        std::cout << "postprocess()" << std::endl;
    int is_object = tf_postprocess((uchar*) eop->GetOutputBufferPtr());
    if (the_distance>the_temp_distance+.5) the_temp_distance=the_distance;
    if (the_distance<8&&the_distance<(the_temp_distance-.5)&&time(NULL)>timer) {
        system("sudo -u debian aplay /var/lib/cloud9/BeagleBone/AI/backupcamera/woa.wav");
        the_temp_distance=the_distance;
        timer=time(NULL) + 1;
    }
    if(is_object >= 0)
    {   my_message=*labels_classes[is_object];
        if (time(NULL)>(timer)) {
            if (rot<30) {
                system("sudo -u debian aplay /var/lib/cloud9/BeagleBone/AI/backupcamera/car_on_right.wav");
            } else if (rot>58){
               system("sudo -u debian aplay /var/lib/cloud9/BeagleBone/AI/backupcamera/car_on_left.wav");
            } else {
                system("sudo -u debian aplay /var/lib/cloud9/BeagleBone/AI/backupcamera/car_behind.wav");
            }
            timer=time(NULL) + 4;
        }
        
    }else {
        my_message=*static_message;
        if (time(NULL)>timer&&the_distance<1){
            
            if (rot<30) {
                system("sudo -u debian aplay /var/lib/cloud9/BeagleBone/AI/backupcamera/something_on_right.wav");
                timer=time(NULL) + 4;
            } else if (rot>58){
                system("sudo -u debian aplay /var/lib/cloud9/BeagleBone/AI/backupcamera/something_on_left.wav");
                timer=time(NULL) + 4;
            } 
            
        }
    }
    
    cv::putText(
            dst,
            my_message.c_str(),
            cv::Point(220, 420),
            cv::FONT_HERSHEY_SIMPLEX,
            1.5,
            cv::Scalar(0,0,255),
            3,  /* thickness */
            8
        );
    //Header
         cv::rectangle(
             dst,
             cv::Point(0,0),
             cv::Point(640,130),
             cv::Scalar(255,255,255),
             CV_FILLED,8,0
         );
         
         cv::rectangle(
             dst,
             cv::Point(0,130),
             cv::Point(640,170),
             cv::Scalar(0,0,0),
             CV_FILLED,8,0
         );
        
         cv::putText(
            dst,
            "BACKUP ASSISTANCE",
            cv::Point(60, 165), //origin of bottom left horizontal, vertical
            cv::FONT_HERSHEY_TRIPLEX, //fontface
            1.5, //fontscale
            cv::Scalar(255,255,255), //color
            2,  /* thickness */
            8
        );

    //backup distance meter left side
        cv::line(
            dst,
            cv::Point(104, 420),
            cv::Point(50, 480),
            cv::Scalar(0,0,255), //color blue
            5, //thickness
            8, //connected line type
            0 //fractional bits
        );
        cv::line(//inward line
            dst,
            cv::Point(104, 420),
            cv::Point(134, 420),
            cv::Scalar(0,0,255), //color blue
            5, //thickness
            8, //connected line type
            0 //fractional bits
        );
        cv::line(
            dst,
            cv::Point(158, 360),
            cv::Point(114, 411),
            cv::Scalar(0,255,255), //color yellow
            4, //thickness
            8, //connected line type
            0 //fractional bits
        );
        cv::line(//inward line
            dst,
            cv::Point(158, 360),
            cv::Point(178, 360),
            cv::Scalar(0,255,255), //color yellow
            4, //thickness
            8, //connected line type
            0 //fractional bits
        );
        cv::line(
            dst,
            cv::Point(212, 300),
            cv::Point(168, 351),
            cv::Scalar(0,255,0), //color green
            2, //thickness
            8, //connected line type
            0 //fractional bits
        );
        cv::line(//inward line
            dst,
            cv::Point(212, 300),
            cv::Point(222, 300),
            cv::Scalar(0,255,0), //color green
            2, //thickness
            8, //connected line type
            0 //fractional bits
        );
        
    //backup distance meter right side
        cv::line(
            dst,
            cv::Point(536, 420),
            cv::Point(590, 480),
            cv::Scalar(0,0,255), //color blue
            5, //thickness
            8, //connected line type
            0 //fractional bits
        );
        cv::line(//inward line
            dst,
            cv::Point(536, 420),
            cv::Point(506, 420),
            cv::Scalar(0,0,255), //color blue
            5, //thickness
            8, //connected line type
            0 //fractional bits
        );
        cv::line(
            dst,
            cv::Point(482, 360),
            cv::Point(526, 411),
            cv::Scalar(0,255,255), //color yellow
            4, //thickness
            8, //connected line type
            0 //fractional bits
        );
        cv::line(//inward line
            dst,
            cv::Point(482, 360),
            cv::Point(462, 360),
            cv::Scalar(0,255,255), //color yellow
            4, //thickness
            8, //connected line type
            0 //fractional bits
        );
        cv::line(
            dst,
            cv::Point(428, 300),
            cv::Point(472, 350),
            cv::Scalar(0,255,0), //color green
            2, //thickness
            8, //connected line type
            0 //fractional bits
        );
        cv::line( //inward line
            dst,
            cv::Point(428, 300),
            cv::Point(418, 300),
            cv::Scalar(0,255,0), //color green
            2, //thickness
            8, //connected line type
            0 //fractional bits
        );
        //Footer
        cv::rectangle(
             dst,
             cv::Point(220,480),
             cv::Point(420,440),
             cv::Scalar(0,0,0),
             CV_FILLED,8,0
         );
        cv::putText(
            dst,
            (distance_message()),
            cv::Point(255, 475), //origin of bottom left horizontal, vertical
            cv::FONT_HERSHEY_TRIPLEX, //fontface
            1.5, //fontscale
            cv::Scalar(255,255,255), //color
            2,  /* thickness */
            8
        );
        int rot=rotation();
        if (rot>44){
            cv::drawMarker( 
                dst,
                cv::Point(320+((44-rot)*5), 250),
                cv::Scalar(0,0,0), //color green
                MARKER_CROSS, 
                20,
                5,
                8 
            );
            cv::drawMarker( 
                dst,
                cv::Point(320+((44-rot)*5), 250),
                cv::Scalar(255,255,255), //color green
                MARKER_CROSS, 
                20,
                1,
                8 
            );
        } else if (rot<44){
            cv::drawMarker( 
                dst,
                cv::Point(320-((rot-44)*5), 250),
                cv::Scalar(0,0,0), 
                MARKER_CROSS, 
                20,
                5,
                8
            );
            cv::drawMarker( 
                dst,
                cv::Point(320-((rot-44)*5), 250),
                cv::Scalar(255,255,255),
                MARKER_CROSS, 
                20,
                1,
                8
            );
        } else {
            cv::drawMarker( 
                dst,
                cv::Point(320, 250),
                cv::Scalar(0,255,0), //color green
                MARKER_DIAMOND ,
                20,
                1,
                8 
            );
        }
    
    if(last_rpt_id != is_object) {
        if(is_object >= 0)
        {
            std::cout << "(" << is_object << ")="
                      << (*(labels_classes[is_object])).c_str() << std::endl;
        }
        last_rpt_id = is_object;
    }
}

// Function to filter all the reported decisions
bool tf_expected_id(int id)
{
   // Filter out unexpected IDs
   for (int i = 0; i < selected_items_size; i ++)
   {
       if(id == selected_items[i]) return true;
   }
   return false;
}

int tf_postprocess(uchar *in)
{
  //prob_i = exp(TIDL_Lib_output_i) / sum(exp(TIDL_Lib_output))
  // sort and get k largest values and corresponding indices
  const int k = top_candidates;
  int rpt_id = -1;

  typedef std::pair<uchar, int> val_index;
  auto constexpr cmp = [](val_index &left, val_index &right) { return left.first > right.first; };
  std::priority_queue<val_index, std::vector<val_index>, decltype(cmp)> queue(cmp);
  // initialize priority queue with smallest value on top
  for (int i = 0; i < k; i++) {
    if(configuration.enableApiTrace)
        std::cout << "push(" << i << "):"
                  << in[i] << std::endl;
    queue.push(val_index(in[i], i));
  }
  // for rest input, if larger than current minimum, pop mininum, push new val
  for (int i = k; i < size; i++)
  {
    if (in[i] > queue.top().first)
    {
      queue.pop();
      queue.push(val_index(in[i], i));
    }
  }

  // output top k values in reverse order: largest val first
  std::vector<val_index> sorted;
  while (! queue.empty())
   {
    sorted.push_back(queue.top());
    queue.pop();
  }

  for (int i = 0; i < k; i++)
  {
      int id = sorted[i].second;

      if (tf_expected_id(id))
      {
        rpt_id = id;
      }
  }
  return rpt_id;
}

void populate_labels(const char* filename)
{
  ifstream file(filename);
  if(file.is_open())
  {
    string inputLine;

    while (getline(file, inputLine) )                 //while the end of file is NOT reached
    {
      //labels_classes[size++] = new string(inputLine);  original line
      labels_classes[size++] = new string("VEHICLE");
    }
    file.close();
  }
#if 0
  std::cout << "==Total of " << size << " items!" << std::endl;
  for (int i = 0; i < size; i ++)
    std::cout << i << ") " << *(labels_classes[i]) << std::endl;
#endif
}


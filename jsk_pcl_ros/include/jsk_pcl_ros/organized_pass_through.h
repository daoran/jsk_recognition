// -*- mode: c++ -*-
/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2014, JSK Lab
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of the JSK Lab nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *********************************************************************/


#ifndef JSK_PCL_ROS_ORGANIZED_PASS_THROUGH_H_
#define JSK_PCL_ROS_ORGANIZED_PASS_THROUGH_H_

#include <pcl_ros/pcl_nodelet.h>
#include <jsk_topic_tools/diagnostic_nodelet.h>
#include <jsk_topic_tools/counter.h>
#include <dynamic_reconfigure/server.h>
#include "jsk_pcl_ros/OrganizedPassThroughConfig.h"

namespace jsk_pcl_ros
{
  class OrganizedPassThrough: public jsk_topic_tools::DiagnosticNodelet
  {
  public:
    typedef jsk_pcl_ros::OrganizedPassThroughConfig Config;
    typedef pcl::PointXYZRGB PointT;
    OrganizedPassThrough();
  protected:
    ////////////////////////////////////////////////////////
    // methods
    ////////////////////////////////////////////////////////
    virtual void onInit();
    
    virtual void subscribe();
    
    virtual void unsubscribe();
    
    virtual void configCallback (Config &config, uint32_t level);
    
    virtual void filter(const sensor_msgs::PointCloud2::ConstPtr& msg);

    virtual pcl::PointIndices::Ptr filterIndices(const pcl::PointCloud<PointT>::Ptr& pc);
    
    virtual void updateDiagnostic(
      diagnostic_updater::DiagnosticStatusWrapper &stat);

      // pcl removed the method by 1.13, no harm in defining it ourselves to use below
#if __cplusplus >= 201103L
#define pcl_isfinite(x) std::isfinite(x)
#endif

    bool isPointNaN(const PointT& p) {
      return (!pcl_isfinite(p.x) || !pcl_isfinite(p.y) || !pcl_isfinite(p.z));
    }

    ////////////////////////////////////////////////////////
    // ROS variables
    ////////////////////////////////////////////////////////
    ros::Subscriber sub_;
    ros::Publisher pub_;
    boost::shared_ptr <dynamic_reconfigure::Server<Config> > srv_;
    boost::mutex mutex_;

    ////////////////////////////////////////////////////////
    // Diagnostics variables
    ////////////////////////////////////////////////////////
    jsk_topic_tools::Counter filtered_points_counter_;
    
    enum FilterField
    {
      FIELD_X, FIELD_Y
    };
    
    ////////////////////////////////////////////////////////
    // Parameters
    ////////////////////////////////////////////////////////
    FilterField filter_field_;
    int min_index_;
    int max_index_;
    bool filter_limit_negative_;
    bool keep_organized_;
    bool remove_nan_;
  private:
    
  };
}

#endif

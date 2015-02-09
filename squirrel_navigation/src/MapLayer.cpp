// MapLayer.cpp --- 
// 
// Filename: MapLayer.cpp
// Description: Costmap layer related to static obstacles
// Author: Federico Boniardi
// Maintainer: boniardi@cs.uni-freiburg.de
// Created: Tue Feb 3 10:49:46 2015 (+0100)
// Version: 0.1.0
// Last-Updated: Tue Feb 3 15:38:12 2015 (+0100)
//           By: Federico Boniardi
//     Update #: 1
// URL: 
// Keywords: 
// Compatibility: 
//   ROS Hydro, ROS Indigo
// 

// Commentary: 
//   The code therein is an integration of costmap_2d::InflationLayer into
//   costmap_2d::StaticLayer. Both source codes are distributed by the authors
//   under BSD license, that is below reported
//   
//     /*********************************************************************
//      *
//      * Software License Agreement (BSD License)
//      *
//      *  Copyright (c) 2008, 2013, Willow Garage, Inc.
//      *  All rights reserved.
//      *
//      *  Redistribution and use in source and binary forms, with or without
//      *  modification, are permitted provided that the following conditions
//      *  are met:
//      *
//      *   * Redistributions of source code must retain the above copyright
//      *     notice, this list of conditions and the following disclaimer.
//      *   * Redistributions in binary form must reproduce the above
//      *     copyright notice, this list of conditions and the following
//      *     disclaimer in the documentation and/or other materials provided
//      *     with the distribution.
//      *   * Neither the name of Willow Garage, Inc. nor the names of its
//      *     contributors may be used to endorse or promote products derived
//      *     from this software without specific prior written permission.
//      *
//      *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
//      *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
//      *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
//      *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
//      *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
//      *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
//      *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
//      *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
//      *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
//      *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
//      *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
//      *  POSSIBILITY OF SUCH DAMAGE.
//      *
//      * Author: Eitan Marder-Eppstein
//      *         David V. Lu!!
//      *********************************************************************/
//
//   Tested on: - ROS Hydro on Ubuntu 12.04
//              - ROS Indigo on Ubuntu 14.04
//      
//      

// Code:

#include "squirrel_navigation/MapLayer.h"

#include <costmap_2d/static_layer.h>
#include <costmap_2d/costmap_math.h>

#include <pluginlib/class_list_macros.h>

#include <cmath>

PLUGINLIB_EXPORT_CLASS(squirrel_navigation::MapLayer, costmap_2d::Layer)

namespace squirrel_navigation {

MapLayer::MapLayer( void ) :
    dsrv_(NULL)
{
  // Nothing to do
}

MapLayer::~MapLayer( void )
{
  if( dsrv_ ) {
    delete dsrv_;
  }
}

void MapLayer::onInitialize( void )
{
  ros::NodeHandle nh("~/" + name_), g_nh;
  current_ = true;

  global_frame_ = layered_costmap_->getGlobalFrameID();

  std::string map_topic;
  nh.param("map_topic", map_topic, std::string("map"));
  nh.param("subscribe_to_updates", subscribe_to_updates_, false);
  
  nh.param("track_unknown_space", track_unknown_space_, true);
  nh.param("use_maximum", use_maximum_, false);

  int temp_lethal_threshold, temp_unknown_cost_value;
  nh.param("lethal_cost_threshold", temp_lethal_threshold, int(100));
  nh.param("unknown_cost_value", temp_unknown_cost_value, int(-1));
  nh.param("trinary_costmap", trinary_costmap_, true);

  lethal_threshold_ = std::max(std::min(temp_lethal_threshold, 100), 0);
  unknown_cost_value_ = temp_unknown_cost_value;

  ROS_INFO("%s/%s: Requesting the map...", ros::this_node::getNamespace().c_str(), ros::this_node::getName().c_str());
  map_sub_ = g_nh.subscribe(map_topic, 1, &MapLayer::incomingMap, this);
  map_received_ = false;
  has_updated_data_ = false;

  ros::Rate r(10);
  while ( !map_received_ && g_nh.ok() ) {
    ros::spinOnce();
    r.sleep();
  }

  ROS_INFO("%s/%s: Received a %d X %d map at %f m/pix", ros::this_node::getNamespace().c_str(), ros::this_node::getName().c_str(),
           getSizeInCellsX(), getSizeInCellsY(), getResolution());
  
  if( subscribe_to_updates_ ) {
    ROS_INFO("%s/%s: Subscribing to updates", ros::this_node::getNamespace().c_str(), ros::this_node::getName().c_str());
    map_update_sub_ = g_nh.subscribe(map_topic + "_updates", 10, &MapLayer::incomingUpdate, this);
  }

  {
    boost::unique_lock < boost::shared_mutex > lock(*access_);
    current_ = true;
    seen_ = NULL;
    need_reinflation_ = false;

    dynamic_reconfigure::Server<MapLayerPluginConfig>::CallbackType cb = boost::bind(
      &MapLayer::reconfigureCB, this, _1, _2);

    if(dsrv_ != NULL){
      dsrv_->clearCallback();
      dsrv_->setCallback(cb);
    }
    else
    {
      dsrv_ = new dynamic_reconfigure::Server<MapLayerPluginConfig>(ros::NodeHandle("~/" + name_));
      dsrv_->setCallback(cb);
    }
  }
  
  matchInflatedSize();
}

void MapLayer::reconfigureCB( MapLayerPluginConfig &config, uint32_t level )
{
  if ( weight_ != config.cost_scaling_factor || inflation_radius_ != config.inflation_radius ) {
    inflation_radius_ = config.inflation_radius;
    cell_inflation_radius_ = cellDistance(inflation_radius_);
    weight_ = config.cost_scaling_factor;
    need_reinflation_ = true;
    computeCaches();
  }

  inscribed_radius_ = config.robot_link_radius;
  
  if ( config.enabled != enabled_ ) {
    enabled_ = config.enabled;
    has_updated_data_ = true;
    x_ = y_ = 0;
    width_ = size_x_;
    height_ = size_y_;
    need_reinflation_ = true;
  }
}

void MapLayer::matchSize( void )
{
  Costmap2D* master = layered_costmap_->getCostmap();
  resizeMap(master->getSizeInCellsX(), master->getSizeInCellsY(), master->getResolution(),
            master->getOriginX(), master->getOriginY());
}

unsigned char MapLayer::interpretValue( unsigned char value )
{
  if ( track_unknown_space_ && value == unknown_cost_value_ ) {
    return costmap_2d::NO_INFORMATION;
  } else if ( value >= lethal_threshold_ ) {
    return costmap_2d::LETHAL_OBSTACLE;
  } else if ( trinary_costmap_ ) {
    return costmap_2d::FREE_SPACE;
  }
  
  double scale = (double) value / lethal_threshold_;
  return scale * costmap_2d::LETHAL_OBSTACLE;
}

void MapLayer::incomingMap( const nav_msgs::OccupancyGridConstPtr& new_map )
{
  unsigned int size_x = new_map->info.width, size_y = new_map->info.height;

  ROS_DEBUG("Received a %d X %d map at %f m/pix", size_x, size_y, new_map->info.resolution);

  Costmap2D* master = layered_costmap_->getCostmap();
  if ( master->getSizeInCellsX() != size_x || master->getSizeInCellsY() != size_y ||
       master->getResolution() != new_map->info.resolution || master->getOriginX() != new_map->info.origin.position.x ||
       master->getOriginY() != new_map->info.origin.position.y || !layered_costmap_->isSizeLocked() ){
    ROS_INFO("%s/%s: Resizing costmap to %d X %d at %f m/pix", ros::this_node::getNamespace().c_str(), ros::this_node::getName().c_str(),
             size_x, size_y, new_map->info.resolution);
    layered_costmap_->resizeMap(size_x, size_y, new_map->info.resolution, new_map->info.origin.position.x, new_map->info.origin.position.y, true);
  } else if ( size_x_ != size_x || size_y_ != size_y ||
              resolution_ != new_map->info.resolution ||
              origin_x_ != new_map->info.origin.position.x ||
              origin_y_ != new_map->info.origin.position.y ) {
    matchSize();
  }

  unsigned int index = 0;

  for (unsigned int i = 0; i < size_y; ++i) {
    for (unsigned int j = 0; j < size_x; ++j) {
      unsigned char value = new_map->data[index];
      unsigned char costmap_value = interpretValue(value);
      if ( costmap_value == costmap_2d::LETHAL_OBSTACLE ) {
        obstacles_.insert(index);
      }
      costmap_[index] = costmap_value;
      ++index;
    }
  }
  
  x_ = y_ = 0;
  width_ = size_x_;
  height_ = size_y_;
  map_received_ = true;
  has_updated_data_ = true;
}

void MapLayer::incomingUpdate( const map_msgs::OccupancyGridUpdateConstPtr& update ) {
  unsigned int di = 0;
  for (unsigned int y = 0; y < update->height ; y++) {
    unsigned int index_base = (update->y + y) * update->width;
    for (unsigned int x = 0; x < update->width ; x++) {
      unsigned int index = index_base + x + update->x;
      costmap_[index] = interpretValue( update->data[di++] );
    }
  }
  
  x_ = update->x;
  y_ = update->y;
  width_ = update->width;
  height_ = update->height;
  has_updated_data_ = true;
}

void MapLayer::activate( void )
{
  onInitialize();
}

void MapLayer::deactivate( void )
{
  map_sub_.shutdown();
  if (subscribe_to_updates_)
    map_update_sub_.shutdown();
}

void MapLayer::reset( void )
{
  deactivate();
  activate();
}

void MapLayer::updateBounds( double robot_x, double robot_y, double robot_yaw,
                             double* min_x, double* min_y, double* max_x, double* max_y )
{
  if ( !map_received_ || !(has_updated_data_ || has_extra_bounds_) ) {
    return;
  }

  // This function does not compile for older version of ROS-navigation (just comment it out)
  useExtraBounds(min_x, min_y, max_x, max_y);

  double mx, my;
  
  mapToWorld(x_, y_, mx, my);
  *min_x = std::min(mx, *min_x);
  *min_y = std::min(my, *min_y);
  
  mapToWorld(x_ + width_, y_ + height_, mx, my);
  *max_x = std::max(mx, *max_x);
  *max_y = std::max(my, *max_y);
  
  has_updated_data_ = false;

  updateInflatedBounds(robot_x, robot_y, robot_yaw, min_x, min_y, max_x, max_y);
}

void MapLayer::updateCosts( costmap_2d::Costmap2D& master_grid, int min_i, int min_j, int max_i, int max_j )
{
  if (!map_received_) {
    return;
  }

  if (!use_maximum_) {
    updateWithTrueOverwrite(master_grid, min_i, min_j, max_i, max_j);
  } else {
    updateWithMax(master_grid, min_i, min_j, max_i, max_j);
  }

  updateInflatedCosts(master_grid, min_i, min_j, max_i, max_j);
}

}  // namespace squirrel_navigation

// 
// MapLayer.cpp ends here

cmake_minimum_required(VERSION 2.8)
project(squirrel_3d_mapping)

## Package dependencies
set(squirrel_3d_mapping_DEPENDENCIES
  roscpp
  visualization_msgs
  sensor_msgs
  pcl_ros
  pcl_conversions
  nav_msgs
  std_msgs
  std_srvs
  octomap_ros
  octomap_msgs
  dynamic_reconfigure
  nodelet
  geometry_msgs
  message_runtime
  message_generation
)

############
## catkin ##
############

find_package(catkin REQUIRED 
  COMPONENTS 
  ${squirrel_3d_mapping_DEPENDENCIES}
)

#############
##   PCL   ##
#############

find_package(PCL REQUIRED 
  QUIET COMPONENTS 
  common 
  sample_consensus 
  io 
  segmentation 
  filters
)

#############
## OctoMap ##
#############

find_package(octomap REQUIRED)
add_definitions(-DOCTOMAP_NODEBUGOUT)

###########
## Build ##
###########

include_directories(
  include
  ${catkin_INCLUDE_DIRS}
  ${PCL_INCLUDE_DIRS}
  ${OCTOMAP_INCLUDE_DIRS}
  ${DYNAMIC_EDT_3D_DIRS}
)

generate_dynamic_reconfigure_options(cfg/OctomapServer.cfg)

add_service_files(
  FILES
  CheckCollision.srv
)

generate_messages(
  DEPENDENCIES
  geometry_msgs
  sensor_msgs
)

catkin_package(
  INCLUDE_DIRS include
  LIBRARIES ${PROJECT_NAME}
  CATKIN_DEPENDS ${squirrel_3d_mapping_DEPENDENCIES}
  DEPENDS octomap PCL
)

set(LINK_LIBS
  ${OCTOMAP_LIBRARIES}
  ${catkin_LIBRARIES}
  ${PCL_LIBRARIES}
)

add_library(dynamic_edt src/DynamicEDT3D.cpp src/DynamicEDTOctomap.cpp)
target_link_libraries(dynamic_edt ${LINK_LIBS})

add_library(${PROJECT_NAME} src/OctomapServer.cpp src/OctomapServerMultilayer.cpp src/TrackingOctomapServer.cpp)
target_link_libraries(${PROJECT_NAME} ${LINK_LIBS})
add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}_gencfg)

add_executable(octomap_server_node src/octomap_server_node.cpp)
target_link_libraries(octomap_server_node ${PROJECT_NAME} ${LINK_LIBS} dynamic_edt)

add_executable(octomap_server_static src/octomap_server_static.cpp)
target_link_libraries(octomap_server_static ${PROJECT_NAME} ${LINK_LIBS} dynamic_edt)

add_executable(octomap_server_multilayer src/octomap_server_multilayer.cpp)
target_link_libraries(octomap_server_multilayer ${PROJECT_NAME} ${LINK_LIBS} dynamic_edt)

add_executable(octomap_saver src/octomap_saver.cpp)
target_link_libraries(octomap_saver ${PROJECT_NAME} ${LINK_LIBS} dynamic_edt)

add_executable(octomap_tracking_server_node src/octomap_tracking_server_node.cpp)
target_link_libraries(octomap_tracking_server_node ${PROJECT_NAME} ${LINK_LIBS} dynamic_edt)

## Nodelet
add_library(octomap_server_nodelet src/octomap_server_nodelet.cpp)
target_link_libraries(octomap_server_nodelet ${PROJECT_NAME} ${LINK_LIBS} dynamic_edt)

#############
## Install ##
#############

install(TARGETS ${PROJECT_NAME}
  dynamic_edt
  octomap_server_node
  octomap_server_static
  octomap_server_multilayer
  octomap_saver
  octomap_tracking_server_node
  octomap_server_nodelet
  ARCHIVE DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
  LIBRARY DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
  RUNTIME DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
)

install(DIRECTORY include/${PROJECT_NAME}/
  DESTINATION ${CATKIN_PACKAGE_INCLUDE_DESTINATION}
  FILES_MATCHING PATTERN "*.h"
  PATTERN ".git" EXCLUDE
)

install(DIRECTORY launch/
  DESTINATION ${CATKIN_PACKAGE_SHARE_DESTINATION}/launch
  PATTERN ".git" EXCLUDE
)

install(FILES nodelet_plugins.xml
  DESTINATION ${CATKIN_PACKAGE_SHARE_DESTINATION})
cmake_minimum_required(VERSION 3.8)
project(turtle_control)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(std_msgs REQUIRED)

add_executable(draw_shape src/draw_shape.cpp)
ament_target_dependencies(draw_shape rclcpp geometry_msgs)

add_executable(obstacle_avoider src/obstacle_avoider.cpp)
ament_target_dependencies(obstacle_avoider rclcpp geometry_msgs std_msgs)

install(TARGETS
  draw_shape
  obstacle_avoider
  DESTINATION lib/${PROJECT_NAME})

ament_package()

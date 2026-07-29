#include <chrono>
#include <cmath>
#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/twist.hpp"

using namespace std::chrono_literals;

// Faehrt ein Dreieck/Viereck ab, per Timer getaktete Zustandsmaschine
// (fahren/drehen). Open-Loop, also ohne Odometrie - reicht fuer turtlesim.
class DrawShape : public rclcpp::Node
{
public:
  DrawShape() : Node("draw_shape")
  {
    // Ecken, Speed und Topic sind per Parameter ueberschreibbar
    n_sides_    = this->declare_parameter<int>("sides", 3);
    lin_speed_  = this->declare_parameter<double>("lin_speed", 1.0);   // m/s
    ang_speed_  = this->declare_parameter<double>("ang_speed", 0.8);   // rad/s
    side_len_   = this->declare_parameter<double>("side_len", 2.0);    // m
    std::string topic = this->declare_parameter<std::string>("topic", "/turtle1/cmd_vel");

    pub_ = this->create_publisher<geometry_msgs::msg::Twist>(topic, 10);

    // Dauer je Phase aus Weg/Geschwindigkeit berechnen
    drive_time_ = side_len_ / lin_speed_;                       // s pro Seite
    double turn_angle = 2.0 * M_PI / n_sides_;                  // Aussenwinkel
    turn_time_ = turn_angle / ang_speed_;                       // s pro Drehung

    RCLCPP_INFO(this->get_logger(),
      "Fahre %d-Eck auf Topic '%s' (Seite %.1f s, Drehung %.1f s)",
      n_sides_, topic.c_str(), drive_time_, turn_time_);

    start_ = this->now();
    timer_ = this->create_wall_timer(50ms, std::bind(&DrawShape::tick, this));
  }

private:
  void tick()
  {
    geometry_msgs::msg::Twist cmd;   // default: alles 0

    if (sides_done_ >= n_sides_) {
      pub_->publish(cmd);            // Stop
      RCLCPP_INFO_ONCE(this->get_logger(), "Form fertig gefahren.");
      return;
    }

    double elapsed = (this->now() - start_).seconds();

    if (driving_) {
      if (elapsed < drive_time_) {
        cmd.linear.x = lin_speed_;
      } else {
        driving_ = false;           // Phase wechseln: jetzt drehen
        start_ = this->now();
      }
    } else {
      if (elapsed < turn_time_) {
        cmd.angular.z = ang_speed_;
      } else {
        driving_ = true;            // naechste Seite
        sides_done_++;
        start_ = this->now();
      }
    }
    pub_->publish(cmd);
  }

  rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr pub_;
  rclcpp::TimerBase::SharedPtr timer_;
  rclcpp::Time start_;

  int    n_sides_, sides_done_ = 0;
  double lin_speed_, ang_speed_, side_len_, drive_time_, turn_time_;
  bool   driving_ = true;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<DrawShape>());
  rclcpp::shutdown();
  return 0;
}
// Hindernisvermeidung: wertet die Ultraschalldaten vom ESP32 aus
// und erzeugt daraus Fahrbefehle. Laeuft auf dem Host-PC.

#include <chrono>
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/float32.hpp"
#include "geometry_msgs/msg/twist.hpp"

using namespace std::chrono_literals;

class ObstacleAvoider : public rclcpp::Node
{
public:
  ObstacleAvoider() : Node("obstacle_avoider")
  {
    schwelle_  = this->declare_parameter<double>("schwelle", 0.25);   // m
    v_fahrt_   = this->declare_parameter<double>("v_fahrt", 0.15);
    v_dreh_    = this->declare_parameter<double>("v_dreh", 0.8);
    dreh_dauer_= this->declare_parameter<double>("dreh_dauer", 1.5);  // s

    pub_ = this->create_publisher<geometry_msgs::msg::Twist>("cmd_vel", 10);
    sub_ = this->create_subscription<std_msgs::msg::Float32>(
      "ultrasonic_range", 10,
      std::bind(&ObstacleAvoider::abstand_cb, this, std::placeholders::_1));

    timer_ = this->create_wall_timer(100ms, std::bind(&ObstacleAvoider::tick, this));
    dreh_start_ = this->now();

    RCLCPP_INFO(this->get_logger(), "Hindernisvermeidung aktiv, Schwelle %.2f m", schwelle_);
  }

private:
  void abstand_cb(const std_msgs::msg::Float32::SharedPtr msg)
  {
    abstand_ = msg->data;
  }

  void tick()
  {
    geometry_msgs::msg::Twist cmd;

    // -1 heisst: kein Echo empfangen, also freie Bahn annehmen
    bool hindernis = (abstand_ > 0.0f && abstand_ < schwelle_);

    if (!dreht_ && hindernis) {
      dreht_ = true;
      dreh_start_ = this->now();
      RCLCPP_WARN(this->get_logger(), "Hindernis bei %.2f m - weiche aus", abstand_);
    }

    if (dreht_) {
      if ((this->now() - dreh_start_).seconds() < dreh_dauer_) {
        cmd.angular.z = v_dreh_;
      } else {
        dreht_ = false;
      }
    } else {
      cmd.linear.x = v_fahrt_;
    }

    pub_->publish(cmd);
  }

  rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr pub_;
  rclcpp::Subscription<std_msgs::msg::Float32>::SharedPtr sub_;
  rclcpp::TimerBase::SharedPtr timer_;
  rclcpp::Time dreh_start_;

  float  abstand_ = -1.0f;
  bool   dreht_ = false;
  double schwelle_, v_fahrt_, v_dreh_, dreh_dauer_;
};

int main(int argc, char **argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<ObstacleAvoider>());
  rclcpp::shutdown();
  return 0;
}
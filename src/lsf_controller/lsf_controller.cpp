#include <ros/ros.h>
#include <ros/package.h>

#include <dynamic_reconfigure/server.h>
#include <mrs_msgs/AttitudeCommand.h>
#include <nav_msgs/Odometry.h>
#include <tf/transform_datatypes.h>

#include <math.h>

#include <mrs_msgs/ControllerStatus.h>
#include <mrs_mav_manager/Controller.h>

#include <mrs_controllers/lsf_gainsConfig.h>

#include <mrs_lib/Profiler.h>

namespace mrs_controllers
{

//{ class LSF

class Lsf {

public:
  Lsf(std::string name, double kp, double kv, double ka, double ki, double integral_saturation, double saturation, double g);
  double update(double position_error, double speed_error, double desired_acceleration, double mass, double pitch, double roll, double dt, double hover_thrust);
  void   reset(void);
  void   setParams(double kp, double kv, double ka, double ki, double integral_saturation);
  bool   isSaturated(void);

private:
  double integral;

  // gains
  double kp;
  double kv;
  double ka;
  double ki;

  double integral_saturation;
  double saturation;

  double g;

  double saturated;

  std::string name;
};

void Lsf::setParams(double kp, double kv, double ka, double ki, double integral_saturation) {

  this->kp                  = kp;
  this->kv                  = kv;
  this->ka                  = ka;
  this->ki                  = ki;
  this->integral_saturation = integral_saturation;
}

Lsf::Lsf(std::string name, double kp, double kv, double ka, double ki, double integral_saturation, double saturation, double g) {

  this->name = name;

  this->kp                  = kp;
  this->kv                  = kv;
  this->ka                  = ka;
  this->ki                  = ki;
  this->integral_saturation = integral_saturation;
  this->saturation          = saturation;

  this->g = g;

  this->saturated = false;

  this->integral = 0;
}

double Lsf::update(double position_error, double speed_error, double desired_acceleration, double mass, double pitch, double roll, double dt,
                   double hover_thrust) {

  double p_component = kp * position_error;
  double v_component = kv * speed_error;
  double i_component = ki * integral;
  double a_component;

  if (name.compare(std::string("x")) == 0 || name.compare(std::string("y")) == 0) {
    a_component = ka * asin((desired_acceleration * cos(pitch) * cos(roll)) / g);
  } else {
    a_component = ka * desired_acceleration * (hover_thrust / g);
  }

  // calculate the lsf action
  double control_output = p_component + v_component + a_component + i_component;

  // saturate the control output
  if (!std::isfinite(control_output)) {
    control_output = 0;
    ROS_INFO("[LsfController]: p_component=%f", p_component);
    ROS_INFO("[LsfController]: v_component=%f", v_component);
    ROS_INFO("[LsfController]: i_component=%f", i_component);
    ROS_INFO("[LsfController]: a_component=%f", a_component);
    ROS_ERROR_THROTTLE(1.0, "[LsfController]: NaN detected in variable \"control_output\", setting it to 0!!!");
  } else if (control_output > saturation) {
    control_output = saturation;
    saturated      = true;
  } else if (control_output < -saturation) {
    control_output = -saturation;
    saturated      = true;
  }

  if (saturated) {

    ROS_WARN_THROTTLE(1.0, "[LsfController]: The \"%s\" LSF is being saturated!", name.c_str());

    // integrate only in the direction oposite to the saturation (antiwindup)
    if (control_output > 0 && position_error < 0) {
      integral += position_error * dt;
    } else if (control_output < 0 && position_error > 0) {
      integral += position_error * dt;
    }
  } else {
    // if the output is not saturated, we do not care in which direction do we integrate
    integral += position_error * dt;
  }

  // saturate the integral
  double integral_saturated = false;
  if (!std::isfinite(integral)) {
    integral = 0;
    ROS_ERROR_THROTTLE(1.0, "[LsfController]: NaN detected in variable \"integral\", setting it to 0!!!");
  } else if (integral > integral_saturation) {
    integral           = integral_saturation;
    integral_saturated = true;
  } else if (integral < -integral_saturation) {
    integral           = -integral_saturation;
    integral_saturated = true;
  }

  if (integral_saturation > 0 && integral_saturated) {
    ROS_WARN_THROTTLE(1.0, "[LsfController]: The \"%s\" LSF's integral is being saturated!", name.c_str());
  }

  return control_output;
}

void Lsf::reset(void) {

  this->integral = 0;
}

bool Lsf::isSaturated(void) {

  return saturated;
}

//}

//{ class LsfController

class LsfController : public mrs_mav_manager::Controller {

public:
  LsfController(void);

  void initialize(const ros::NodeHandle &parent_nh, mrs_mav_manager::MotorParams motor_params);
  bool activate(const mrs_msgs::AttitudeCommand::ConstPtr &cmd);
  void deactivate(void);

  const mrs_msgs::AttitudeCommand::ConstPtr update(const nav_msgs::Odometry::ConstPtr &odometry, const mrs_msgs::PositionCommand::ConstPtr &reference);
  const mrs_msgs::ControllerStatus::Ptr     status();

  void dynamicReconfigureCallback(mrs_controllers::lsf_gainsConfig &config, uint32_t level);

private:
  // --------------------------------------------------------------
  // |                     dynamic reconfigure                    |
  // --------------------------------------------------------------

  boost::recursive_mutex                      config_mutex_;
  typedef mrs_controllers::lsf_gainsConfig    Config;
  typedef dynamic_reconfigure::Server<Config> ReconfigureServer;
  boost::shared_ptr<ReconfigureServer>        reconfigure_server_;
  void                                        drs_callback(mrs_controllers::lsf_gainsConfig &config, uint32_t level);
  mrs_controllers::lsf_gainsConfig            last_drs_config;

private:
  Lsf *lsf_pitch;
  Lsf *lsf_roll;
  Lsf *lsf_z;

  double uav_mass_;
  double uav_mass_difference;
  double g_;
  mrs_mav_manager::MotorParams motor_params_;
  double hover_thrust;

  double roll, pitch, yaw;

  double yaw_offset;

  // gains
  double kpxy_, kixy_, kvxy_, kaxy_;
  double kpz_, kvz_, kaz_;
  double kixy_lim_;
  double km_, km_lim_;

  double max_tilt_angle_;

  mrs_msgs::AttitudeCommand::ConstPtr last_output_command;
  mrs_msgs::AttitudeCommand           activation_control_command_;

  ros::Time last_update;
  bool      first_iteration = true;

private:
  mrs_lib::Profiler *profiler;
  mrs_lib::Routine * routine_update;
};

LsfController::LsfController(void) {
}

//}

// --------------------------------------------------------------
// |                   controller's interface                   |
// --------------------------------------------------------------

//{ initialize()

void LsfController::initialize(const ros::NodeHandle &parent_nh, mrs_mav_manager::MotorParams motor_params) {

  ros::NodeHandle nh_(parent_nh, "lsf_controller");

  ros::Time::waitForValid();

  this->motor_params_ = motor_params;

  // --------------------------------------------------------------
  // |                       load parameters                      |
  // --------------------------------------------------------------

  nh_.param("kpxy", kpxy_, -1.0);
  nh_.param("kvxy", kvxy_, -1.0);
  nh_.param("kaxy", kaxy_, -1.0);
  nh_.param("kixy", kixy_, -1.0);
  nh_.param("kpz", kpz_, -1.0);
  nh_.param("kvz", kvz_, -1.0);
  nh_.param("kaz", kaz_, -1.0);
  nh_.param("km", km_, -1.0);
  nh_.param("kixy_lim", kixy_lim_, -1.0);
  nh_.param("km_lim", km_lim_, -1.0);
  nh_.param("uav_mass", uav_mass_, -1.0);
  nh_.param("g", g_, -1.0);
  nh_.param("max_tilt_angle", max_tilt_angle_, -1.0);
  nh_.param("yaw_offset", yaw_offset, -1000.0);

  if (kpxy_ < 0) {
    ROS_ERROR("[LsfController]: kpxy is not specified!");
    ros::shutdown();
  }

  if (kvxy_ < 0) {
    ROS_ERROR("[LsfController]: kvxy is not specified!");
    ros::shutdown();
  }

  if (kaxy_ < 0) {
    ROS_ERROR("[LsfController]: kaxy is not specified!");
    ros::shutdown();
  }

  if (kixy_ < 0) {
    ROS_ERROR("[LsfController]: kixy is not specified!");
    ros::shutdown();
  }

  if (kpz_ < 0) {
    ROS_ERROR("[LsfController]: kpz is not specified!");
    ros::shutdown();
  }

  if (kvz_ < 0) {
    ROS_ERROR("[LsfController]: kvz is not specified!");
    ros::shutdown();
  }

  if (kaz_ < 0) {
    ROS_ERROR("[LsfController]: kaz is not specified!");
    ros::shutdown();
  }

  if (km_ < 0) {
    ROS_ERROR("[LsfController]: km is not specified!");
    ros::shutdown();
  }

  if (kixy_lim_ < 0) {
    ROS_ERROR("[LsfController]: kixy_lim is not specified!");
    ros::shutdown();
  }

  if (km_lim_ < 0) {
    ROS_ERROR("[LsfController]: km_lim is not specified!");
    ros::shutdown();
  }

  if (uav_mass_ < 0) {
    ROS_ERROR("[LsfController]: uav_mass is not specified!");
    ros::shutdown();
  }

  if (g_ < 0) {
    ROS_ERROR("[LsfController]: g is not specified!");
    ros::shutdown();
  }

  if (max_tilt_angle_ < 0) {
    ROS_ERROR("[LsfController]: max_tilt_angle is not specified!");
    ros::shutdown();
  }

  if (yaw_offset < -999.0) {
    ROS_ERROR("[LsfController]: yaw_offset is not specified!");
    ros::shutdown();
  }

  // convert to radians
  max_tilt_angle_ = (max_tilt_angle_ / 180) * 3.141592;
  yaw_offset      = (yaw_offset / 180.0) * 3.141592;

  ROS_INFO("[LsfController]: LsfController was launched with gains:");
  ROS_INFO("[LsfController]: horizontal: kpxy: %3.5f, kvxy: %3.5f, kaxy: %3.5f, kixy: %3.5f, kixy_lim: %3.5f", kpxy_, kvxy_, kaxy_, kixy_, kixy_lim_);
  ROS_INFO("[LsfController]: vertical:   kpz: %3.5f, kvz: %3.5f, kaz: %3.3f", kpz_, kvz_, kaz_);
  ROS_INFO("[LsfController]: mass:       km: %3.5f, km_lim: %3.5f", km_, km_lim_);

  uav_mass_difference = 0;

  // --------------------------------------------------------------
  // |                 calculate the hover thrust                 |
  // --------------------------------------------------------------

  hover_thrust = sqrt(uav_mass_ * g_) * motor_params_.hover_thrust_a + motor_params_.hover_thrust_b;

  // --------------------------------------------------------------
  // |                       initialize lsfs                      |
  // --------------------------------------------------------------

  lsf_pitch = new Lsf("x", kpxy_, kvxy_, kaxy_, kixy_, kixy_lim_, max_tilt_angle_, g_);
  lsf_roll  = new Lsf("y", kpxy_, kvxy_, kaxy_, kixy_, kixy_lim_, max_tilt_angle_, g_);
  lsf_z     = new Lsf("z", kpz_, kvz_, kaz_, 0, 0, 1.0, g_);

  // --------------------------------------------------------------
  // |                     dynamic reconfigure                    |
  // --------------------------------------------------------------

  last_drs_config.kpxy       = kpxy_;
  last_drs_config.kvxy       = kvxy_;
  last_drs_config.kaxy       = kaxy_;
  last_drs_config.kixy       = kixy_;
  last_drs_config.kpz        = kpz_;
  last_drs_config.kvz        = kvz_;
  last_drs_config.kaz        = kaz_;
  last_drs_config.kixy_lim   = kixy_lim_;
  last_drs_config.km         = km_;
  last_drs_config.km_lim     = km_lim_;
  last_drs_config.yaw_offset = (yaw_offset / 3.1415) * 180;

  reconfigure_server_.reset(new ReconfigureServer(config_mutex_, nh_));
  reconfigure_server_->updateConfig(last_drs_config);
  ReconfigureServer::CallbackType f = boost::bind(&LsfController::dynamicReconfigureCallback, this, _1, _2);
  reconfigure_server_->setCallback(f);

  // --------------------------------------------------------------
  // |                          profiler                          |
  // --------------------------------------------------------------

  profiler       = new mrs_lib::Profiler(nh_, "LsfController");
  routine_update = profiler->registerRoutine("update");
}

//}

//{ activate()

bool LsfController::activate(const mrs_msgs::AttitudeCommand::ConstPtr &cmd) {

  if (cmd == mrs_msgs::AttitudeCommand::Ptr()) {
    activation_control_command_ = mrs_msgs::AttitudeCommand();
    uav_mass_difference         = 0;
    ROS_WARN("[LsfController]: activated without getting the last tracker's command.");
  } else {
    activation_control_command_ = *cmd;
    uav_mass_difference         = cmd->mass_difference;
    ROS_INFO("[LsfController]: activated with a last trackers command.");
  }

  first_iteration = true;

  ROS_INFO("[LsfController]: activated");

  return true;
}

//}

//{ deactivate()

void LsfController::deactivate(void) {

  first_iteration     = false;
  uav_mass_difference = 0;

  ROS_INFO("[LsfController]: deactivated");
}

//}

//{ update()

const mrs_msgs::AttitudeCommand::ConstPtr LsfController::update(const nav_msgs::Odometry::ConstPtr &       odometry,
                                                                const mrs_msgs::PositionCommand::ConstPtr &reference) {

  routine_update->start();

  // --------------------------------------------------------------
  // |                  calculate control errors                  |
  // --------------------------------------------------------------

  double position_error_x = reference->position.x - odometry->pose.pose.position.x;
  double position_error_y = reference->position.y - odometry->pose.pose.position.y;
  double position_error_z = reference->position.z - odometry->pose.pose.position.z;

  double speed_error_x = reference->velocity.x - odometry->twist.twist.linear.x;
  double speed_error_y = reference->velocity.y - odometry->twist.twist.linear.y;
  double speed_error_z = reference->velocity.z - odometry->twist.twist.linear.z;

  // --------------------------------------------------------------
  // |                      calculate the dt                      |
  // --------------------------------------------------------------

  double dt;

  if (first_iteration) {

    lsf_pitch->reset();
    lsf_roll->reset();
    lsf_z->reset();
    last_update = ros::Time::now();

    first_iteration = false;

    routine_update->end();
    return mrs_msgs::AttitudeCommand::ConstPtr(new mrs_msgs::AttitudeCommand(activation_control_command_));

  } else {

    dt = (ros::Time::now() - last_update).toSec();
  }

  if (dt <= 0.001) {

    ROS_WARN("[LsfController]: the update was called with too small dt!");
    if (last_output_command != mrs_msgs::AttitudeCommand::Ptr()) {

      routine_update->end();
      return last_output_command;

    } else {

      routine_update->end();
      return mrs_msgs::AttitudeCommand::ConstPtr(new mrs_msgs::AttitudeCommand(activation_control_command_));
    }
  }

  last_update = ros::Time::now();

  // --------------------------------------------------------------
  // |                 calculate the euler angles                 |
  // --------------------------------------------------------------

  double         yaw, pitch, roll;
  tf::Quaternion quaternion_odometry;
  quaternionMsgToTF(odometry->pose.pose.orientation, quaternion_odometry);
  tf::Matrix3x3 m(quaternion_odometry);
  m.getRPY(roll, pitch, yaw);

  // --------------------------------------------------------------
  // |                recalculate the hover thrust                |
  // --------------------------------------------------------------

  hover_thrust = sqrt((uav_mass_ + uav_mass_difference) * g_) * motor_params_.hover_thrust_a + motor_params_.hover_thrust_b;

  // --------------------------------------------------------------
  // |                integrate the mass difference               |
  // --------------------------------------------------------------

  if (!lsf_z->isSaturated()) {

    uav_mass_difference += km_ * position_error_z * dt;
  }

  // saturate the integral
  bool uav_mass_saturated = false;
  if (!std::isfinite(uav_mass_difference)) {
    uav_mass_difference = 0;
    ROS_WARN_THROTTLE(1.0, "[LsfController]: NaN detected in variable \"uav_mass_difference\", setting it to 0 and returning!!!");
  } else if (uav_mass_difference > km_lim_) {
    uav_mass_difference = km_lim_;
    uav_mass_saturated  = true;
  } else if (uav_mass_difference < -km_lim_) {
    uav_mass_difference = -km_lim_;
    uav_mass_saturated  = true;
  }

  if (uav_mass_saturated) {
    ROS_WARN_THROTTLE(1.0, "[LsfController]: The uav_mass_difference is being saturated to %1.3f!", uav_mass_difference);
  }

  // --------------------------------------------------------------
  // |                      update parameters                     |
  // --------------------------------------------------------------

  if (reference->disable_position_gains) {
    lsf_pitch->setParams(kpxy_ / 10.0, kvxy_, kaxy_, kixy_, kixy_lim_);
    lsf_roll->setParams(kpxy_ / 10.0, kvxy_, kaxy_, kixy_, kixy_lim_);
    ROS_WARN_THROTTLE(1.0, "[LsfController]: position gains are disabled");
  } else {
    lsf_pitch->setParams(kpxy_, kvxy_, kaxy_, kixy_, kixy_lim_);
    lsf_roll->setParams(kpxy_, kvxy_, kaxy_, kixy_, kixy_lim_);
  }

  // --------------------------------------------------------------
  // |                     calculate the LSFs                     |
  // --------------------------------------------------------------

  double action_pitch =
      lsf_pitch->update(position_error_x, speed_error_x, reference->acceleration.x, uav_mass_ + uav_mass_difference, pitch, roll, dt, hover_thrust);
  double action_roll =
      lsf_roll->update(-position_error_y, -speed_error_y, -reference->acceleration.y, uav_mass_ + uav_mass_difference, pitch, roll, dt, hover_thrust);
  double action_z = (lsf_z->update(position_error_z, speed_error_z, reference->acceleration.z, uav_mass_ + uav_mass_difference, pitch, roll, dt, hover_thrust) +
                     hover_thrust) *
                    (1 / (cos(roll) * cos(pitch)));

  mrs_msgs::AttitudeCommand::Ptr output_command(new mrs_msgs::AttitudeCommand);
  output_command->header.stamp = ros::Time::now();

  output_command->pitch  = action_pitch * cos(yaw + yaw_offset) - action_roll * sin(yaw + yaw_offset);
  output_command->roll   = action_roll * cos(yaw + yaw_offset) + action_pitch * sin(yaw + yaw_offset);
  output_command->yaw    = reference->yaw;
  output_command->thrust = action_z;

  output_command->mass_difference = uav_mass_difference;

  last_output_command = output_command;

  routine_update->end();
  return output_command;
}

//}

//{ status()

const mrs_msgs::ControllerStatus::Ptr LsfController::status() {

  return mrs_msgs::ControllerStatus::Ptr();
}

//}

// --------------------------------------------------------------
// |                          callbacks                         |
// --------------------------------------------------------------

//{ dynamicReconfigureCallback()

void LsfController::dynamicReconfigureCallback(mrs_controllers::lsf_gainsConfig &config, uint32_t level) {

  kpxy_      = config.kpxy;
  kvxy_      = config.kvxy;
  kaxy_      = config.kaxy;
  kixy_      = config.kixy;
  kpz_       = config.kpz;
  kvz_       = config.kvz;
  kaz_       = config.kaz;
  km_        = config.km;
  kixy_lim_  = config.kixy_lim;
  km_lim_    = config.km_lim;
  yaw_offset = (config.yaw_offset / 180) * 3.141592;

  lsf_pitch->setParams(kpxy_, kvxy_, kaxy_, kixy_, kixy_lim_);
  lsf_roll->setParams(kpxy_, kvxy_, kaxy_, kixy_, kixy_lim_);
  lsf_z->setParams(kpz_, kvz_, kaz_, 0, 0);
}

//}
}  // namespace mrs_controllers

#include <pluginlib/class_list_macros.h>
PLUGINLIB_EXPORT_CLASS(mrs_controllers::LsfController, mrs_mav_manager::Controller)

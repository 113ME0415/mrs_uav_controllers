^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Changelog for package mrs_controllers
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

0.0.5 (2020-02-26)
------------------
* allowed emergency ctrl to have non-zero integrals
* Contributors: Tomas Baca

0.0.4 (2020-02-18)
------------------
* update darpa green motors params
* fixed fcu acceleration output
* added version checks
* removed the Baca thrust constant
* acceleration published in fcu
* updated accel publishers
* refactored odometry switch with transformer
* increased timeout for profiler routine
* decreased mpc integral gains
* added mpc initial condition validation
* decreased mpc and emergency max tilt to 30 deg
* updated t650 attiude controller's gains
* updated atttitude controller's outputs
* added rampup into attitude command
* refactoring uninitialized variables
* added verbosity to rampup
* fixed mass initialization in rampup
* fixed wrong mpc gains for hardware
* Add f450 motor params for grey/green darpa t-motors
* updated failsafe activation
* PartialLanding: fixed prints in activate()
* increased t650 mpc integral gains
* set emergency controller's integral gains to 0
* updated flip mitigation
* added rampup to mpc and se3
* updated the flip mitigation
* updated nsf gains
* added the option for speed controller integrals to se3 and mpc
* updated controller's interface
* updated to new attitude cmd flags
* Contributors: Tomas Baca

0.0.3 (2019-10-25)
------------------
* beautified the print for mitigating flip
* fixed application of mass factor
* fixed kq vertical for most drones to 0.1
* added partial landing controller
* added the constraints override feature
* added yaw rate saturation to MpcController
* added emergancy controller
* added service for enabling integral terms into MpcController
* improved printing and documentation
* added world conversion of integral feedback to attitude target
* added disturbances to attitude command
* fixed the body integrator yaw rotation bug
* removed unsued drs from mpc controller
* updated prints of integral terms
* refactored controllers' dynamic reconfigure
* added missing mutex for MPC
* updated mpc solver
* fixed deactivation bug in controllers
* added integrals of MPC controller to DRS
* added loadReference method in mpc solver
* added odometry switch routine to SE3, MPC and NSF controllers
* re-setting Q and S in mpc tracker before every iteration
* added acceleration controller
* moved mpc solver from mpc_controller to its own namespace and folder
* Contributors: Tomas Baca

0.0.2 (2019-07-01)
------------------
* added feedback disablation to mpc controller during takeoff
* added desired acceleration to outputs
* increased failsafe thrust constants
* fixed the mass antiwindup in MPC
* constraints are passed to controllers
* mpc now controls the z axis
* added negative z-force detection and flip mitigation
* fixed body integral in se3, cleaned configs
* upgraded se3's max tilt saturation and failsafe (Naki's accident)
* fixed body rate orientation for new Mavros
* + Mpc controller
* fixed attitude rate reference frame
* Contributors: Tomas Baca

0.0.1 (2019-05-20)
------------------

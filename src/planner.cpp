#include "planner.h"

constexpr double pi() { return M_PI; }
double deg2rad(double x) { return x * pi() / 180; }
double rad2deg(double x) { return x * 180 / pi(); }

// Checks if the SocketIO event has JSON data.
// If there is data the JSON object in string format will be returned,
// else the empty string "" will be returned.
string hasData(string s) {
  auto found_null = s.find("null");
  auto b1 = s.find_first_of("[");
  auto b2 = s.find_first_of("}");
  if (found_null != string::npos) {
    return "";
  } else if (b1 != string::npos && b2 != string::npos) {
    return s.substr(b1, b2 - b1 + 2);
  }
  return "";
}

// returns the eucledian distance between two points
double distance(double x1, double y1, double x2, double y2)
{
	return sqrt((x2-x1)*(x2-x1)+(y2-y1)*(y2-y1));
}

// returns the closest waypoint in the map to the current waypoint
int ClosestWaypoint(double x, double y, const vector<double> &maps_x, const vector<double> &maps_y)
{

	double closestLen = 100000; //large number
	int closestWaypoint = 0;

	for(int i = 0; i < maps_x.size(); i++)
	{
		double map_x = maps_x[i];
		double map_y = maps_y[i];
		double dist = distance(x,y,map_x,map_y);
		if(dist < closestLen)
		{
			closestLen = dist;
			closestWaypoint = i;
		}

	}

	return closestWaypoint;

}

// returns the next waypoint given current position and heading
int NextWaypoint(double x, double y, double theta, const vector<double> &maps_x, const vector<double> &maps_y)
{

	int closestWaypoint = ClosestWaypoint(x,y,maps_x,maps_y);

	double map_x = maps_x[closestWaypoint];
	double map_y = maps_y[closestWaypoint];

	double heading = atan2((map_y-y),(map_x-x));

	double angle = fabs(theta-heading);
  angle = min(2*pi() - angle, angle);

  if(angle > pi()/4)
  {
    closestWaypoint++;
  if (closestWaypoint == maps_x.size())
  {
    closestWaypoint = 0;
  }
  }

  return closestWaypoint;
}

// Transform from Cartesian x,y coordinates to Frenet s,d coordinates
vector<double> getFrenet(double x, double y, double theta, const vector<double> &maps_x, const vector<double> &maps_y)
{
	int next_wp = NextWaypoint(x,y, theta, maps_x,maps_y);

	int prev_wp;
	prev_wp = next_wp-1;
	if(next_wp == 0)
	{
		prev_wp  = maps_x.size()-1;
	}

	double n_x = maps_x[next_wp]-maps_x[prev_wp];
	double n_y = maps_y[next_wp]-maps_y[prev_wp];
	double x_x = x - maps_x[prev_wp];
	double x_y = y - maps_y[prev_wp];

	// find the projection of x onto n
	double proj_norm = (x_x*n_x+x_y*n_y)/(n_x*n_x+n_y*n_y);
	double proj_x = proj_norm*n_x;
	double proj_y = proj_norm*n_y;

	double frenet_d = distance(x_x,x_y,proj_x,proj_y);

	//see if d value is positive or negative by comparing it to a center point

	double center_x = 1000-maps_x[prev_wp];
	double center_y = 2000-maps_y[prev_wp];
	double centerToPos = distance(center_x,center_y,x_x,x_y);
	double centerToRef = distance(center_x,center_y,proj_x,proj_y);

	if(centerToPos <= centerToRef)
	{
		frenet_d *= -1;
	}

	// calculate s value
	double frenet_s = 0;
	for(int i = 0; i < prev_wp; i++)
	{
		frenet_s += distance(maps_x[i],maps_y[i],maps_x[i+1],maps_y[i+1]);
	}

	frenet_s += distance(0,0,proj_x,proj_y);

	return {frenet_s,frenet_d};

}

// Transform from Frenet s,d coordinates to Cartesian x,y
vector<double> getXY(double s, double d, const vector<double> &maps_s, const vector<double> &maps_x, const vector<double> &maps_y)
{
	int prev_wp = -1;

	while(s > maps_s[prev_wp+1] && (prev_wp < (int)(maps_s.size()-1) ))
	{
		prev_wp++;
	}

	int wp2 = (prev_wp+1)%maps_x.size();

	double heading = atan2((maps_y[wp2]-maps_y[prev_wp]),(maps_x[wp2]-maps_x[prev_wp]));
	// the x,y,s along the segment
	double seg_s = (s-maps_s[prev_wp]);

	double seg_x = maps_x[prev_wp]+seg_s*cos(heading);
	double seg_y = maps_y[prev_wp]+seg_s*sin(heading);

	double perp_heading = heading-pi()/2;

	double x = seg_x + d*cos(perp_heading);
	double y = seg_y + d*sin(perp_heading);

	return {x,y};

}

// Function to read the map file and populate the map_wayoint vectors
bool readMapFile(string map_file, vector<double> &map_waypoints_x, vector<double> &map_waypoints_y, 
	             vector<double> &map_waypoints_s, vector<double> &map_waypoints_dx, vector<double> &map_waypoints_dy) {
	// The max s value before wrapping around the track back to 0
  double max_s = 6945.554;

  ifstream in_map_(map_file.c_str(), ifstream::in);

  string line;
  while (getline(in_map_, line)) {
  	istringstream iss(line);
  	double x;
  	double y;
  	float s;
  	float d_x;
  	float d_y;
  	iss >> x;
  	iss >> y;
  	iss >> s;
  	iss >> d_x;
  	iss >> d_y;
  	map_waypoints_x.push_back(x);
  	map_waypoints_y.push_back(y);
  	map_waypoints_s.push_back(s);
  	map_waypoints_dx.push_back(d_x);
  	map_waypoints_dy.push_back(d_y);
  }

  return true;
}

/*
  Function to make the lane change given the current lane and distance to closest car in the adjacent lanes 
  
  returns : The lane to which the lane change should be made. Returns the same lane if lane change is not 
            possible given the traffic around
*/
int makeLaneChange(int lane, double distance_front_left, double distance_rear_left, double distance_front_right, double distance_rear_right) {
	double rear_thresh = 5;
	double for_thresh = 35;

	if (lane == 0) {
		if (distance_rear_right > rear_thresh && distance_front_right > for_thresh) {
			return 1;
		}
	}

	if (lane == 2) {
		if (distance_rear_left > rear_thresh && distance_front_left > for_thresh) {
			return 1;
		}
	}

	if (lane == 1) {
		if (distance_rear_left > rear_thresh && distance_front_left > for_thresh && distance_rear_right > rear_thresh && distance_front_right > for_thresh) {
			if (distance_front_left > distance_front_right) {return 0;}
			else {return 2;}
		} 
		else if (distance_rear_left > rear_thresh && distance_front_left > for_thresh) {return 0;}
		else if (distance_rear_right > rear_thresh && distance_front_right > for_thresh) {return 2;}
	}

	return lane;
}

/* 
	Function goes over the sensor fusion data and determines the behavior of the car
	This involves determining if there is a car in the front and determining distance
	to cars around

	returns : vector<double> of 4 closest cars (1 front left, 1 rear left, 1 front right, and 1 rear right) 
			      around the ego vehicle in the lane to the left and right of the ego lane.
*/
vector<double> behaviourPlanner(int lane, int prev_size, double car_s, vector<vector<double>> &sensor_fusion, 
								                bool &too_close, bool &lane_change) {
	double distance_front_left = 1000;	// closest car in the front in the lane to the left
	double distance_rear_left = 1000; 	// closest car in the rear in the lane to the left
	double distance_front_right = 1000; // closest car in the front in the lane to the right
	double distance_rear_right = 1000;	// closest car in the rear in the lane to the right

	// find ref_v to use
	// Iterate over all the sensor fusion ouputs
	for (int i = 0; i < sensor_fusion.size(); i++) {
		float d = sensor_fusion[i][6];
		double vx = sensor_fusion[i][3];
	  double vy = sensor_fusion[i][4];
	  double check_speed = sqrt(vx*vx + vy*vy);
	  double check_car_s = sensor_fusion[i][5];
		
	  //if using previous points, can project s value out
	 	check_car_s+=(double)prev_size * .02 * check_speed;

	 	// if lane is 1 or 2, then calculate the closest car in the front and rear 
	 	// in the lane to the left
	 	if (lane == 1 || lane == 2) {
	 		// check if the car is in lane to the left
	 		if (d < (2 + 4*(lane-1) + 2) && d > (2 + 4*(lane-1) - 2)) { 
	 			// check if the car is in the rear
	 			if (check_car_s < car_s) {
	 				double temp = car_s - check_car_s;
	 				if (temp < distance_rear_left) distance_rear_left = temp;
	 			}
	 			// check if the car is in front
	 			if(check_car_s > car_s) {
	 				double temp = check_car_s - car_s;
	 				if (temp < distance_front_left) distance_front_left = temp;
	 			}
	 		}
	 	}

	 	if (lane == 0 || lane == 1) {
	 		// check if the car is in the lane to the right
	 		if (d < (2 + 4*(lane+1) + 2) && d > (2 + 4*(lane+1) - 2)) {
	 			// check if the car is in the rear
	 			if(check_car_s < car_s) {
	 				double temp = car_s - check_car_s;
	 				if (temp < distance_rear_right) {
	 					distance_rear_right = temp;
	 				}
	 			}

	 			// check if the car is in the front
	 			if(check_car_s > car_s) {
	 				double temp = check_car_s - car_s;
	 				if (temp < distance_front_right) {
	 					distance_front_right = temp;
	 				}
	 			}
	 		}
	 	}
		
		if (d < (2 + 4*lane + 2) && d > (2 + 4*lane - 2)) {	
			//check s values greater than mine and s gap
			if ((check_car_s > car_s) && (check_car_s - car_s < 30)) {
				too_close = true;

				// Start checking for the lane to change to!
				lane_change = true;
			}
		}
	}

  return {distance_front_left,distance_rear_left,distance_front_right,distance_rear_right};
}

/*
  Plans the trajectory of the car to smoothly navigatet the map. Uses spline to create a smooth path.

  returns : vector<vector<double>> - a list of of x and y points that the car should traverse.
*/

vector<vector<double>> trajectorPlanner(int lane, double ref_vel, int prev_size, double car_x, double car_y, double car_s, double car_yaw, 
										vector<double> &map_waypoints_s, vector<double> &map_waypoints_x, vector<double> &map_waypoints_y,
										vector<double> &previous_path_x, vector<double> &previous_path_y) {
	// We will fit a spline to the points to generate a smooth path
	// ptsx and ptsy is a list of point that will be fit using a spline
	vector<double> ptsx;
    vector<double> ptsy;

	// reference x, y, yaw states
  	// either we will reference the starting point as where the car is or at the previous paths end point
  	double ref_x = car_x;
  	double ref_y = car_y;
  	double ref_yaw = deg2rad(car_yaw);

  	// if previous size is almost empty, use the car as starting reference
  	if (prev_size < 2)
  	{
  		//Use two points that make the path tangent to the car
  		double prev_car_x = car_x - cos(car_yaw);
  		double prev_car_y = car_y - sin(car_yaw);

  		ptsx.push_back(prev_car_x);
  		ptsx.push_back(car_x);

  		ptsy.push_back(prev_car_y);
  		ptsy.push_back(car_y);
  	}
  	// use the previous path's end point as starting reference
  	else
  	{
  		// redefine reference state as previous path end point
  		ref_x = previous_path_x[prev_size-1];
  		ref_y = previous_path_y[prev_size-1];

  		double ref_x_prev = previous_path_x[prev_size-2];
  		double ref_y_prev = previous_path_y[prev_size-2];
  		ref_yaw = atan2(ref_y-ref_y_prev, ref_x-ref_x_prev);

			// Use two points that make the path tangent to the previous path's end point
			ptsx.push_back(ref_x_prev);
			ptsx.push_back(ref_x);

			ptsy.push_back(ref_y_prev);
			ptsy.push_back(ref_y);
  	}

  	//In Frenet add evenly 30m spaced points ahead of the starting reference
  	vector<double> next_wp0 = getXY(car_s+30,(2+4*lane),map_waypoints_s,map_waypoints_x,map_waypoints_y);
  	vector<double> next_wp1 = getXY(car_s+60,(2+4*lane),map_waypoints_s,map_waypoints_x,map_waypoints_y);
  	vector<double> next_wp2 = getXY(car_s+90,(2+4*lane),map_waypoints_s,map_waypoints_x,map_waypoints_y);

  	ptsx.push_back(next_wp0[0]);
  	ptsx.push_back(next_wp1[0]);
  	ptsx.push_back(next_wp2[0]);

  	ptsy.push_back(next_wp0[1]);
  	ptsy.push_back(next_wp1[1]);
  	ptsy.push_back(next_wp2[1]);

  	for (int i = 0; i < ptsx.size(); i++)
  	{
  		//shift car reference angle to 0 degrees
  		double shift_x = ptsx[i] - ref_x;
  		double shift_y = ptsy[i] - ref_y;

  		// rotate the coordinates to car's local frame
  		ptsx[i] = (shift_x * cos(0 - ref_yaw) - shift_y * sin(0 - ref_yaw));
  		ptsy[i] = (shift_x * sin(0 - ref_yaw) + shift_y * cos(0 - ref_yaw));
  	}

  	//create a spline
  	tk::spline s;

  	// set (x,y) points to the spline
  	s.set_points(ptsx, ptsy);

  	// define actual vector for the planner points we will use
  	vector<double> next_x_vals;
  	vector<double> next_y_vals;

    for (int i = 0; i<previous_path_x.size(); i++) {
    	next_x_vals.push_back(previous_path_x[i]);
    	next_y_vals.push_back(previous_path_y[i]);
    }

    double target_x = 30;
    double target_y = s(target_x);
    double target_dist = sqrt((target_x)*(target_x)+(target_y)*(target_y));

    double x_add_on = 0;
    double N = (target_dist/(0.02 * ref_vel/2.24));
    double inc = target_x/N;

    for (int i = 0; i <= 50 - previous_path_x.size(); i++) {
    	double x_point = x_add_on + inc;
    	double y_point = s(x_point);

    	x_add_on = x_point;
    	double x_ref = x_point;
    	double y_ref = y_point;

    	// rotate back to normal(global frame) after rotating to local frame earlier
    	x_point = (x_ref * cos(ref_yaw) - y_ref * sin(ref_yaw));
    	y_point = (x_ref * sin(ref_yaw) + y_ref * cos(ref_yaw));

    	x_point += ref_x;
    	y_point += ref_y;

    	next_x_vals.push_back(x_point);
    	next_y_vals.push_back(y_point);
    }

    vector<vector<double>> v;
    v.push_back(next_x_vals); v.push_back(next_y_vals);
    return v;
}

/*
  Run loop 

  retuns : void 
*/
void run(){
	uWS::Hub h;

  // Load up map values for waypoint's x,y,s and d normalized normal vectors
  vector<double> map_waypoints_x;
  vector<double> map_waypoints_y;
  vector<double> map_waypoints_s;
  vector<double> map_waypoints_dx;
  vector<double> map_waypoints_dy;

  // Waypoint map to read from
  string map_file_ = "../data/highway_map.csv";
  readMapFile(map_file_, map_waypoints_x, map_waypoints_y, map_waypoints_s, map_waypoints_dx, map_waypoints_dy);

  int lane = 1;

  double ref_vel = 0;

  // Simulator Message Functions!

  h.onMessage([&map_waypoints_x,&map_waypoints_y,&map_waypoints_s,&map_waypoints_dx,&map_waypoints_dy,&lane,&ref_vel](uWS::WebSocket<uWS::SERVER> ws, char *data, size_t length,
                     uWS::OpCode opCode) {
    // "42" at the start of the message means there's a websocket message event.
    // The 4 signifies a websocket message
    // The 2 signifies a websocket event
    //auto sdata = string(data).substr(0, length);
    //cout << sdata << endl;
    if (length && length > 2 && data[0] == '4' && data[1] == '2') {

      auto s = hasData(data);

      if (s != "") {
        auto j = json::parse(s);
        
        string event = j[0].get<string>();
        
        if (event == "telemetry") {
			      // j[1] is the data JSON object
        	  // Main car's localization Data
          	double car_x = j[1]["x"];
          	double car_y = j[1]["y"];
          	double car_s = j[1]["s"];
          	double car_d = j[1]["d"];
          	double car_yaw = j[1]["yaw"];
          	double car_speed = j[1]["speed"];

          	// Previous path data given to the Planner
          	vector<double> previous_path_x = j[1]["previous_path_x"];
          	vector<double> previous_path_y = j[1]["previous_path_y"];
          	// Previous path's end s and d values 
          	double end_path_s = j[1]["end_path_s"];
          	double end_path_d = j[1]["end_path_d"];

          	// Sensor Fusion Data, a list of all other cars on the same side of the road.
          	vector<vector<double>> sensor_fusion = j[1]["sensor_fusion"];
          	int prev_size = previous_path_x.size();

          	if(prev_size > 0) {
          		car_s = end_path_s;
          	}

          	// -----------------------------Behaviour Planning ----------------------------------
          	bool too_close = false;
          	bool lane_change = false;
  
       		  vector<double> carsAround = behaviourPlanner(lane, prev_size, car_s, sensor_fusion, too_close, lane_change);

          	// make lane change if a car in front
          	if(lane_change) {
          		lane = makeLaneChange(lane, carsAround[0], carsAround[1], carsAround[2], carsAround[3]);
          	}

          	// slow down if following too close to a car
          	if (too_close) {
          		ref_vel -= .224;
          	}
          	else if (ref_vel < 49.5) {
          		ref_vel += .224;
          	}


          	// -----------------------------Trajectory Planning -----------------------------------
          	vector<vector<double>> next_path_vals = trajectorPlanner(lane, ref_vel, prev_size, car_x, car_y, car_s, car_yaw, map_waypoints_s, 
          															 map_waypoints_x, map_waypoints_y, previous_path_x, previous_path_y);

   	        json msgJson;
          	// TODO: define a path made up of (x,y) points that the car will visit sequentially every .02 seconds
          	msgJson["next_x"] = next_path_vals[0];
          	msgJson["next_y"] = next_path_vals[1];

          	auto msg = "42[\"control\","+ msgJson.dump()+"]";

          	//this_thread::sleep_for(chrono::milliseconds(1000));
          	ws.send(msg.data(), msg.length(), uWS::OpCode::TEXT);
          
        }
      } else {
        // Manual driving
        std::string msg = "42[\"manual\",{}]";
        ws.send(msg.data(), msg.length(), uWS::OpCode::TEXT);
      }
    }
  });

  // We don't need this since we're not using HTTP but if it's removed the
  // program doesn't compile :-(
  h.onHttpRequest([](uWS::HttpResponse *res, uWS::HttpRequest req, char *data,
                     size_t, size_t) {
    const std::string s = "<h1>Hello world!</h1>";
    if (req.getUrl().valueLength == 1) {
      res->end(s.data(), s.length());
    } else {
      // i guess this should be done more gracefully?
      res->end(nullptr, 0);
    }
  });

  h.onConnection([&h](uWS::WebSocket<uWS::SERVER> ws, uWS::HttpRequest req) {
    std::cout << "Connected!!!" << std::endl;
  });

  h.onDisconnection([&h](uWS::WebSocket<uWS::SERVER> ws, int code,
                         char *message, size_t length) {
    ws.close();
    std::cout << "Disconnected" << std::endl;
  });

  int port = 4567;
  if (h.listen(port)) {
    std::cout << "Listening to port " << port << std::endl;
  } else {
    std::cerr << "Failed to listen to port" << std::endl;
    return;
  }
  h.run();	

}
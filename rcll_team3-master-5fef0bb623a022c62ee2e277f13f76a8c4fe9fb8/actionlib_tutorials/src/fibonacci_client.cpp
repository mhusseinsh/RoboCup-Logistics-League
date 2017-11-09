#include <ros/ros.h>
#include <actionlib/client/simple_action_client.h>
#include <actionlib/client/terminal_state.h>
#include <fawkes_msgs/ExecSkillAction.h>
#include <rcll_ros_msgs/GameState.h>
#include <rcll_ros_msgs/MachineInfo.h>
#include <rcll_ros_msgs/RingInfo.h>
#include <rcll_ros_msgs/OrderInfo.h>
#include <rcll_ros_msgs/SendPrepareMachine.h>
#include <rcll_ros_msgs/ProductColor.h>
#include <list>

bool getNextOrder;
bool executingSkill;
bool isMachineReady(std::string skillstring);
void prepareMachine(std::string goal);
void prepareBaseMachine(int base_color);
void prepareCapMachineRetrieve();
void prepareCapMachineMount();
void prepareDeliveryMachine(int delivery_gate);
void prepareRingMachine(int ring_colors);
int count_cap1;
int count_cap2;
int cs1_shelf;
int cs2_shelf;
std::string ring_machine(int order_ring_color);
std::string getCapColor(int cap_color);
std::list<std::string> skills;
std::string cap_shelf;
fawkes_msgs::ExecSkillGoal goal;
rcll_ros_msgs::OrderInfo::ConstPtr latestOrderInfo;
rcll_ros_msgs::MachineInfo::ConstPtr latestMachineInfo;
rcll_ros_msgs::RingInfo::ConstPtr latestRingInfo;
rcll_ros_msgs::GameState::ConstPtr latestGameState;
ros::ServiceClient prep_machine_client;
rcll_ros_msgs::Order o;
actionlib::SimpleActionClient<fawkes_msgs::ExecSkillAction> *ac;

void gameStateCallback(const rcll_ros_msgs::GameState::ConstPtr& msg)
{
	latestGameState = msg;
  //ROS_INFO_STREAM("I heard: "<< *msg);
}

void machineInfoCallback(const rcll_ros_msgs::MachineInfo::ConstPtr& msg)
{
	latestMachineInfo = msg;
  //ROS_INFO_STREAM("I heard: "<< *msg);
}

void ringInfoCallback(const rcll_ros_msgs::RingInfo::ConstPtr& msg)
{
	latestRingInfo = msg;
  //ROS_INFO_STREAM("I heard: "<< *msg);
}

void orderInfoCallback(const rcll_ros_msgs::OrderInfo::ConstPtr& msg)
{
  //ROS_INFO_STREAM("I heard: "<< *msg);
  latestOrderInfo = msg;
  /*
  for(int i = 0; i < msg->orders.size(); i++)
  {
  	const rcll_ros_msgs::Order& order = msg->orders[i];
  	ROS_INFO_STREAM("id: "<<order.id);
  	ROS_INFO_STREAM("complexity: "<<std::to_string(order.complexity));
  	ROS_INFO_STREAM("base_color: "<<std::to_string(order.base_color));
  	ROS_INFO_STREAM("cap_color: "<<std::to_string(order.cap_color));
  	
  	for (int j = 0; j < order.ring_colors.size(); j++)
  	{
  		ROS_INFO_STREAM("ring_colors" <<j <<": " <<std::to_string(order.ring_colors[j]));
  	}
  }
  */
}

int countBase(int order_ring_color)
{
	for (int j = 0; j < latestRingInfo->rings.size(); j++)
  	{
  		if (latestRingInfo->rings[j].ring_color == order_ring_color)
  			return latestRingInfo->rings[j].raw_material;
  	}
}

std::string ringMachine(int order_ring_color)
{
	for (int j = 0; j < latestMachineInfo->machines.size(); j++)
  {
		if (latestMachineInfo->machines[j].name.find("RS") != std::string::npos)
		{
			for (int i = 0; i < latestMachineInfo->machines[j].rs_ring_colors.size(); i++)
			{
				if (order_ring_color == latestMachineInfo->machines[j].rs_ring_colors[i])
					return latestMachineInfo->machines[j].name;
			}
		}
  }
  return "";
}

bool defineNextOrder()
{
	if (latestOrderInfo == NULL)
		return false;
		
	bool flag = false;
	for(int i = 0; i < latestOrderInfo->orders.size(); i++)
  {
  if (latestOrderInfo->orders[i].quantity_requested > latestOrderInfo->orders[i].quantity_delivered_cyan)
  {
			if (latestOrderInfo->orders[i].complexity == 2)
			{
				o = latestOrderInfo->orders[i];
				flag = true;
			}
		}
  }
  return flag;
}

void implementOrder()
{
		
	if (defineNextOrder() == true)
	{		
		std::string cap_machine = getCapColor(o.cap_color);
		if (cap_machine == "C-CS1")
		{
			cs1_shelf ++;
			if (cs1_shelf == 1)
				cap_shelf = "LEFT";
			else if (cs1_shelf == 2)
				cap_shelf = "MIDDLE";
			else if (cs1_shelf == 3)
			{
				cap_shelf = "RIGHT";
				cs1_shelf = 0;
			}
		}
		else if (cap_machine == "C-CS2")
		{
			cs2_shelf ++;
			if (cs2_shelf == 1)
				cap_shelf = "LEFT";
			else if (cs2_shelf == 2)
				cap_shelf = "MIDDLE";
			else if (cs2_shelf == 3)
			{
				cap_shelf = "RIGHT";
				cs2_shelf = 0;
			}
		}
			skills.push_back("get_product_from{place='" + cap_machine + "', shelf='" + cap_shelf +"'}");
			skills.push_back("prepareCapMachineRetrieve");
			skills.push_back("bring_product_to{place='" + cap_machine + "', side='input'}");
			skills.push_back("get_product_from{place='" + cap_machine + "', side='output'}");
			skills.push_back("ax12gripper{command='OPEN'}");
			
		if (o.complexity == 0)
		{
			skills.push_back("prepareBaseMachine");
			skills.push_back("get_product_from{place='C-BS', side='output'}");
			getNextOrder = false;
		}
		else if (o.complexity == 1)
		{
			getNextOrder = false;
			std::string ring_machine_name = ringMachine(o.ring_colors[0]);
			for (int i = 1; i <= countBase(o.ring_colors[0]); i++)
			{
				skills.push_back("prepareBaseMachine");
				skills.push_back("get_product_from{place='C-BS', side='output'}");
				skills.push_back("bring_product_to{place='" + ring_machine_name + "', side='input', slide='true'}");
			}
			skills.push_back("prepareBaseMachine");
			skills.push_back("get_product_from{place='C-BS', side='output'}");
			skills.push_back("prepareRingMachine" + std::to_string(o.ring_colors[0]));
			skills.push_back("bring_product_to{place='" + ring_machine_name + "', side='input'}");
			skills.push_back("get_product_from{place='" + ring_machine_name + "', side='output'}");
			
		}
		else if (o.complexity == 2)
		{
			getNextOrder = false;
			std::string ring_machine_name_1 = ringMachine(o.ring_colors[0]);
			for (int i = 1; i <= countBase(o.ring_colors[0]); i++)
			{
				skills.push_back("prepareBaseMachine");
				skills.push_back("get_product_from{place='C-BS', side='output'}");
				skills.push_back("bring_product_to{place='" + ring_machine_name_1 + "', side='input', slide='true'}");
			}
			skills.push_back("prepareBaseMachine");
			skills.push_back("get_product_from{place='C-BS', side='output'}");
			skills.push_back("prepareRingMachine" + std::to_string(o.ring_colors[0]));
			skills.push_back("bring_product_to{place='" + ring_machine_name_1 + "', side='input'}");
			
			std::string ring_machine_name_2 = ringMachine(o.ring_colors[1]);
			for (int i = 1; i <= countBase(o.ring_colors[1]); i++)
			{
				skills.push_back("prepareBaseMachine");
				skills.push_back("get_product_from{place='C-BS', side='output'}");
				skills.push_back("bring_product_to{place='" + ring_machine_name_2 + "', side='input', slide='true'}");
			}
						
			skills.push_back("get_product_from{place='" + ring_machine_name_1 + "', side='output'}");
			skills.push_back("prepareRingMachine" + std::to_string(o.ring_colors[1]));
			skills.push_back("bring_product_to{place='" + ring_machine_name_2 + "', side='input'}");
			skills.push_back("get_product_from{place='" + ring_machine_name_2 + "', side='output'}");
		}
		else if (o.complexity == 3)
		{
			getNextOrder = false;
			std::string ring_machine_name_1 = ringMachine(o.ring_colors[0]);
			for (int i = 1; i <= countBase(o.ring_colors[0]); i++)
			{
				skills.push_back("prepareBaseMachine");
				skills.push_back("get_product_from{place='C-BS', side='output'}");
				skills.push_back("bring_product_to{place='" + ring_machine_name_1 + "', side='input', slide='true'}");
			}
			skills.push_back("prepareBaseMachine");
			skills.push_back("get_product_from{place='C-BS', side='output'}");
			skills.push_back("prepareRingMachine" + std::to_string(o.ring_colors[0]));
			skills.push_back("bring_product_to{place='" + ring_machine_name_1 + "', side='input'}");
			
			std::string ring_machine_name_2 = ringMachine(o.ring_colors[1]);
			for (int i = 1; i <= countBase(o.ring_colors[1]); i++)
			{
				skills.push_back("prepareBaseMachine");
				skills.push_back("get_product_from{place='C-BS', side='output'}");
				skills.push_back("bring_product_to{place='" + ring_machine_name_2 + "', side='input', slide='true'}");
			}
						
			skills.push_back("get_product_from{place='" + ring_machine_name_1 + "', side='output'}");
			skills.push_back("prepareRingMachine" + std::to_string(o.ring_colors[1]));
			skills.push_back("bring_product_to{place='" + ring_machine_name_2 + "', side='input'}");
			
			std::string ring_machine_name_3 = ringMachine(o.ring_colors[2]);
			for (int i = 1; i <= countBase(o.ring_colors[2]); i++)
			{
				skills.push_back("prepareBaseMachine");
				skills.push_back("get_product_from{place='C-BS', side='output'}");
				skills.push_back("bring_product_to{place='" + ring_machine_name_3 + "', side='input', slide='true'}");
			}
			
			skills.push_back("get_product_from{place='" + ring_machine_name_2 + "', side='output'}");
			skills.push_back("prepareRingMachine" + std::to_string(o.ring_colors[2]));
			skills.push_back("bring_product_to{place='" + ring_machine_name_3 + "', side='input'}");
			
			skills.push_back("get_product_from{place='" + ring_machine_name_3 + "', side='output'}");
		}
			skills.push_back("prepareCapMachineMount");
			skills.push_back("bring_product_to{place='" + cap_machine + "', side='input'}");
			skills.push_back("get_product_from{place='" + cap_machine + "', side='output'}");
			skills.push_back("prepareDeliveryMachine");
			skills.push_back("bring_product_to{place='C-DS', side='input'}");
	}
}

void prepareMachine(std::string prepare)
{
	if (prepare == "prepareBaseMachine")
		prepareBaseMachine(o.base_color);
	else if (prepare == "prepareCapMachineRetrieve")
		prepareCapMachineRetrieve();
	else if (prepare == "prepareCapMachineMount")
		prepareCapMachineMount();
	else if (prepare == "prepareDeliveryMachine")
		prepareDeliveryMachine(o.delivery_gate);
	else if (prepare.find("prepareRingMachine") != std::string::npos)
		prepareRingMachine(std::stoi(prepare.substr(prepare.length()-1, 1)));
}

void executeGoal()
{	
	if (skills.empty())
	{
		getNextOrder = true;
		return;
	}
	
	if (executingSkill == false)
	{
		executingSkill = true;
		goal.skillstring = skills.front();
		ROS_INFO_STREAM("Sending Action "<<goal.skillstring);
		skills.pop_front();
  	ac->sendGoal(goal);
	}
	else
	{
		actionlib::SimpleClientGoalState goalState = ac->getState();
		if ((goalState != actionlib::SimpleClientGoalState::ACTIVE) && (goalState != actionlib::SimpleClientGoalState::PENDING))
		{
		  actionlib::SimpleClientGoalState state = ac->getState();
		  ROS_INFO("Action finished: %s",state.toString().c_str());
		  // decide what to do
	 		goal.skillstring = skills.front();
	 		ROS_INFO_STREAM("Sending Action "<<goal.skillstring);
	 		if (goal.skillstring.find("prepare") == 0)
	 		{
	 			if (goal.skillstring.find("Delivery") != std::string::npos)
	 			{
	 				if(latestGameState->game_time.sec <= o.delivery_period_begin)
	 					return;	 		
			 	}
	 			skills.pop_front();
	 			prepareMachine(goal.skillstring);
	 		}
	 		else
	 		{
	 			if (goal.skillstring.find("get") == 0 && goal.skillstring.find("output") != std::string::npos && ((goal.skillstring.find("C-CS") != std::string::npos) || (goal.skillstring.find("C-RS") != std::string::npos)))
	 			{
	 				bool machineReady = false;
	 				ros::Rate rate(10);
	 				while (machineReady == false && ros::ok()) 
	 				{
	 					ros::spinOnce();
		 				if (isMachineReady(goal.skillstring) == true)
		 				{
							machineReady = true;
							ROS_INFO_STREAM(skills.front());
							goal.skillstring = skills.front();
			 				skills.pop_front();
							ac->sendGoal(goal);
						}
						else
						{
							ROS_INFO_STREAM("Waiting for machine to be ready "<<goal.skillstring);
							rate.sleep();
						}					
					}
				}
				else
				{
					ROS_INFO_STREAM("else case: " <<skills.front());
					skills.pop_front();
					ac->sendGoal(goal);
				}
			}
		}
	//	else
		  // ROS_INFO("Action did not finish before the time out.");
	}
}

bool isMachineReady(std::string skillstring)
{
	if (skillstring.find("C-CS1") != std::string::npos)
	{
		for (int j = 0; j < latestMachineInfo->machines.size(); j++)
  	{
  		if (latestMachineInfo->machines[j].name == "C-CS1")
  		{
  			if (latestMachineInfo->machines[j].state == "READY-AT-OUTPUT")
  				return true;
  			else
  			{
  				ROS_INFO_STREAM("The current state of CS1: " + latestMachineInfo->machines[j].state);
  				return false;
  			}
  		}
  	}
  	return false;
	}
	else if (skillstring.find("C-CS2") != std::string::npos)
	{
		for (int j = 0; j < latestMachineInfo->machines.size(); j++)
  	{
  		if (latestMachineInfo->machines[j].name == "C-CS2")
  		{
  			if (latestMachineInfo->machines[j].state == "READY-AT-OUTPUT")
  				return true;
  			else
  			{
  				ROS_INFO_STREAM("The current state of CS2: " + latestMachineInfo->machines[j].state);
  				return false;	
				}  		
  		}
  	}
  	return false;
	}
	else if (skillstring.find("C-RS1") != std::string::npos)
	{
		for (int j = 0; j < latestMachineInfo->machines.size(); j++)
  	{
  		if (latestMachineInfo->machines[j].name == "C-RS1")
  		{
  			if (latestMachineInfo->machines[j].state == "READY-AT-OUTPUT")
  				return true;
  			else
  			{
  				ROS_INFO_STREAM("The current state of RS1: " + latestMachineInfo->machines[j].state);
  				return false;	
				}  		
  		}
  	}
  	return false;
	}
	else if (skillstring.find("C-RS2") != std::string::npos)
	{
		for (int j = 0; j < latestMachineInfo->machines.size(); j++)
  	{
  		if (latestMachineInfo->machines[j].name == "C-RS2")
  		{
  			if (latestMachineInfo->machines[j].state == "READY-AT-OUTPUT")
  				return true;
  			else
  			{
  				ROS_INFO_STREAM("The current state of RS2: " + latestMachineInfo->machines[j].state);
  				return false;	
				}  		
  		}
  	}
  	return false;
	}
	return false;
}


void prepareBaseMachine(int base_color)
{
	rcll_ros_msgs::SendPrepareMachine srv;
	if (base_color == 1)
	{
		srv.request.bs_base_color = rcll_ros_msgs::ProductColor::BASE_RED;
	}
	else if (base_color == 2)
	{
		srv.request.bs_base_color = rcll_ros_msgs::ProductColor::BASE_BLACK;
	}
	else if (base_color == 3)
	{
		srv.request.bs_base_color = rcll_ros_msgs::ProductColor::BASE_SILVER;
	}
	srv.request.machine = "C-BS";
	srv.request.bs_side = srv.request.BS_SIDE_OUTPUT;
	
	if (prep_machine_client.call(srv))
  {
    ROS_INFO("OK");
  }
  else
  {
    ROS_ERROR("Failed to call service add_two_ints");
  }
}

void prepareCapMachineRetrieve()
{
	rcll_ros_msgs::SendPrepareMachine srv;
	srv.request.cs_operation = srv.request.CS_OP_RETRIEVE_CAP;
	srv.request.machine = getCapColor(o.cap_color);
	
	if (prep_machine_client.call(srv))
  {
    ROS_INFO("OK");
  }
  else
  {
    ROS_ERROR("Failed to call service add_two_ints");
  }
}

void prepareCapMachineMount()
{
	rcll_ros_msgs::SendPrepareMachine srv;
	srv.request.cs_operation = srv.request.CS_OP_MOUNT_CAP;
	srv.request.machine = getCapColor(o.cap_color);
	
	if (prep_machine_client.call(srv))
  {
    ROS_INFO("OK");
  }
  else
  {
    ROS_ERROR("Failed to call service add_two_ints");
  }
}

void prepareDeliveryMachine(int delivery_gate)
{
	rcll_ros_msgs::SendPrepareMachine srv;
	srv.request.machine = "C-DS";
	srv.request.ds_gate = delivery_gate;
	
	if (prep_machine_client.call(srv))
  {
    ROS_INFO("OK");
  }
  else
  {
    ROS_ERROR("Failed to call service add_two_ints");
  }
}

void prepareRingMachine(int ring_colors)
{
	rcll_ros_msgs::SendPrepareMachine srv;
	srv.request.machine = ringMachine(ring_colors);
	srv.request.rs_ring_color = ring_colors;
	
	if (prep_machine_client.call(srv))
  {
    ROS_INFO("OK");
  }
  else
  {
    ROS_ERROR("Failed to call service add_two_ints");
  }
}

std::string getCapColor(int cap_color)
{
	if (cap_color == 1)
	{
		return "C-CS2";
	}
	else if (cap_color == 2)
	{
		return "C-CS1";
	}
}

int main (int argc, char **argv)
{
  ros::init(argc, argv, "test_fibonacci");
	ros::NodeHandle n;
	
	ac = new actionlib::SimpleActionClient<fawkes_msgs::ExecSkillAction>("robot1/skiller", true);
	
	prep_machine_client = n.serviceClient<rcll_ros_msgs::SendPrepareMachine>("/robot1/rcll/send_prepare_machine");
	
	ros::Subscriber subGameState = n.subscribe("/robot1/rcll/game_state", 1000, gameStateCallback);
	ros::Subscriber subMachineInfo = n.subscribe("/robot1/rcll/machine_info", 1000, machineInfoCallback);
	ros::Subscriber subRingInfo = n.subscribe("/robot1/rcll/ring_info", 1000, ringInfoCallback);
	ros::Subscriber subOrderInfo = n.subscribe("/robot1/rcll/order_info", 1000, orderInfoCallback);
	
  // create the action client
  // true causes the client to spin its own thread

  ROS_INFO("Waiting for action server to start.");
  // wait for the action server to start
  ac->waitForServer(); //will wait for infinite time

  ROS_INFO("Action server started, sending goal.");
  // send a goal to the action
	
	skills.push_back("drive_into_field{team='CYAN', wait=0}");
	executingSkill = false;

	ros::Rate rate(10);
	
	getNextOrder = true;
	
	cs1_shelf = 0;
	cs2_shelf = 0;
	
  while(ros::ok())
{
  ros::spinOnce();
	
	if (getNextOrder == true)
	{
		implementOrder();
	}
	
	executeGoal();
  //wait for the action to return
  //bool finished_before_timeout = ac.waitForResult(ros::Duration(30.0));
    
    
  rate.sleep();
}

  //exit
  return 0;
}



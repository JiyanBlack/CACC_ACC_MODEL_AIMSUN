/****************************************************************

     To modify the demand at the beginning of the simulation run
	 It will not work this way!!!

****************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <vector>
#include <numeric>
#include <map>
#include <fstream>      // std::ifstream
#include <algorithm>    // std::find
#include <string>

#define Car_Type  53  
#define HOVveh_Type  17859
#define HIAveh_Type  21350  // 811
#define Truck_Type  56  

#define ACCveh_Type  344  // 812
#define CACCveh_Type 346  // 813
#define RAMP_ID 23554
#define MAINLANE 23552
#define ACC_LENGTH 820 //in feet
#define FREE_FLOW_SPEED 30.0 //[MPH]

std::map<int,std::vector<int>> time_next; //the time for the next vehicle from an origin 
std::map<int,std::vector<double>> avg_headway_origin;
std::map<int,std::vector<double>> min_headway_origin;
std::map<int,std::vector<std::vector<double>>> flows;
std::map<int,std::vector<std::vector<double>>> truck_portions;
std::map<int,std::vector<double>> turning_portions; //turning proportions
std::map<int,std::map<int, double>> dest; // the dest of a origin and its flow proportion
std::map<int,int> orgin_section; // the section that connects the origin
double global_acc = 0;
double global_cacc = 0;
int global_interval = 0; //minutes
int interval_shift = 7;//in hours

int dmd_modify(double T)
{
	//int start_up_slice, start_dwn_slice;
	int i, j, current_slice=0;
	double input_dmd[NumOnRamp][max_onramp_ln]={{2000.0,2000.0,2000.0}};
	double new_dmd[NumOnRamp][max_onramp_ln]={{2000.0,2000.0,2000.0}};
	static int init_sw=1, dmd_up_sw=0, dmd_dwn_sw=0;
	
	if (init_sw == 1)
	{
		for (i=0; i<NumOnRamp;i++)
		{
			for(j=0;j<max_onramp_ln;j++)
			{
				input_dmd[i][j]=0.0;
				new_dmd[i][j]=0.0;
			}
		}
		init_sw=0;
	}
	
	current_slice=(int)floor(T/dmd_slice_len);   // 300[s] per spice

	for (i=0;i<NumOnRamp;i++)
	{		
		for(j=0;j<max_onramp_ln;j++)
			{
				if (OnRamp_Ln_Id[i][j]>0)
				{
					if (DEMAND_CHANGE==0)
					{
						input_dmd[i][j]=AKIStateDemandGetDemandSection(OnRamp_Ln_Id[i][j], ALLvehType, current_slice);  	
						new_dmd[i][j]=(input_dmd[i][j]);
					}
					else if (DEMAND_CHANGE==1)
					{
						if (i==0 || i==5 || i==6 || i==7 || i==8)
						{
							input_dmd[i][j]=AKIStateDemandGetDemandSection(OnRamp_Ln_Id[i][j], ALLvehType, current_slice);  	
							new_dmd[i][j]=(input_dmd[i][j])*(1.0+dmd_change);
						}
						else  //if (DEMAND_CHANGE==2)
						{
							input_dmd[i][j]=AKIStateDemandGetDemandSection(OnRamp_Ln_Id[i][j], ALLvehType, current_slice);  	
							new_dmd[i][j]=(input_dmd[i][j]);
						}

					}
					else
					{
						input_dmd[i][j]=AKIStateDemandGetDemandSection(OnRamp_Ln_Id[i][j], ALLvehType, current_slice);  	
						new_dmd[i][j]=(input_dmd[i][j])*(1.0+dmd_change);
					}

					if (AKIStateDemandSetDemandSection(OnRamp_Ln_Id[i][j], Car_Type, current_slice, new_dmd[i][j]) < 0)  
						AKIPrintString("Demand change failed!");
					if (AKIStateDemandSetDemandSection(OnRamp_Ln_Id[i][j], HIAveh_Type, current_slice, new_dmd[i][j]) < 0)  
						AKIPrintString("Demand change failed!");
					if (AKIStateDemandSetDemandSection(OnRamp_Ln_Id[i][j], ACCveh_Type, current_slice, new_dmd[i][j]) < 0)  
						AKIPrintString("Demand change failed!");
					if (AKIStateDemandSetDemandSection(OnRamp_Ln_Id[i][j], CACCveh_Type, current_slice, new_dmd[i][j]) < 0)  
						AKIPrintString("Demand change failed!");
			
					if ((i==0) && (j==0))
						fprintf(dmd_f, "%lf\t%d\t%lf\t%lf\t ", T, current_slice, input_dmd[0], new_dmd[0]);
					else
						fprintf(dmd_f, "%lf\t%lf\t", input_dmd[i][j], new_dmd[i][j]);
					if((i==NumOnRamp-1) && (j==max_onramp_ln-1))
						fprintf(dmd_f, "%lf\t%lf\n", input_dmd[i][j], new_dmd[i][j]);
				}
			} //j-loop end
	} //i-loop end

	return 1;
}

void read_precentage(double &acc_percent, double &cacc_percent)
{
	int expriment_id = ANGConnGetExperimentId();
	const unsigned short *CACC_PercentString = AKIConvertFromAsciiString( 
		"GKExperiment::CACC_Percent");
	cacc_percent = ANGConnGetAttributeValueDouble( ANGConnGetAttribute( CACC_PercentString ), expriment_id );
	//delete[] CACC_PercentString;

	const unsigned short *ACC_PercentString = AKIConvertFromAsciiString( 
		"GKExperiment::ACC_Percent");
	acc_percent = ANGConnGetAttributeValueDouble( ANGConnGetAttribute( ACC_PercentString ), expriment_id );
	//delete[] ACC_PercentString;

	const unsigned short *simStepAtt = AKIConvertFromAsciiString( 
		"GKExperiment::simStepAtt");
	ANGConnSetAttributeValueDouble( ANGConnGetAttribute( simStepAtt ), expriment_id ,0.1);
	//delete[] simStepAtt;

}

//read ramp and main-lane volume
void read_volumn(int &mainlane, int &on_ramp, int &off_ramp)
{
	int exp_id = ANGConnGetExperimentId();
	const unsigned short *MAINLANE_PercentString = 
		AKIConvertFromAsciiString( 
		"through_flow");
	mainlane = (int)ANGConnGetAttributeValueDouble
		( ANGConnGetAttribute( MAINLANE_PercentString ), exp_id );
	//delete[] MAINLANE_PercentString;

	const unsigned short *on_RAMP_PercentString = 
		AKIConvertFromAsciiString( 
		"on_ramp_flow");
	on_ramp = (int)ANGConnGetAttributeValueDouble
		( ANGConnGetAttribute(on_RAMP_PercentString ), exp_id );
	//delete[] on_RAMP_PercentString;

	const unsigned short *off_RAMP_PercentString = 
		AKIConvertFromAsciiString( 
		"off_ramp_flow");
	off_ramp = (int)ANGConnGetAttributeValueDouble
		( ANGConnGetAttribute(off_RAMP_PercentString ), exp_id );
	//delete[] off_RAMP_PercentString;

	/*char config_file[len_str];
	sprintf_s(config_file,len_str, "C:\\CACC_Simu_Data\\config.txt",replication);	
	try
	{
		fopen_s(&cfg_file,config_file,"r");		
		char mystring [100];
		fgets (mystring , 100 , cfg_file);
		mainlane =0;
		ramp =0;
		int index=0;
		while(isdigit(mystring[index]))
		{
			mainlane = mainlane*10+ (mystring[index] - '0');
			index++;
		}
		index++;
		while(isdigit(mystring[index]))
		{
			ramp = ramp*10+(mystring[index] - '0');
			index++;
		}

		fclose(cfg_file);
	}
	catch(int e)
	{
		mainlane=0;
		ramp=0;
	}*/
}

void modify_section_cooperation(double coop)
{
	for(int i=0;i<sectionNum;i++)
	{
		int id = AKIInfNetGetSectionANGId(i);	
		int report=0;
		A2KSectionBehaviourParam para = 
			AKIInfNetGetSectionBehaviouralParam( id,&report);
		para.cooperation_OnRamp = coop;
		//this api may have a bug. Change the cooperation percent does not work.
		int error = AKIInfNetSetSectionBehaviouralParam(id, para, false);
		if(error<0)
			error=error;
	}	
}

//////////////////////////////////////////////////////////////////////////
// lane capacity per hour
//////////////////////////////////////////////////////////////////////////
double Lane_Capacity()
{
	//this only serve as a rough estimation
	const unsigned short *avg_headway_String = 
		AKIConvertFromAsciiString( "headway_mean" );
	double avg_headway_time = 
		ANGConnGetAttributeValueDouble(
		ANGConnGetAttribute( avg_headway_String ), Car_Type);
	//delete[] avg_headway_String;

	const unsigned short *meanJamString = AKIConvertFromAsciiString( 
		"GDrivingSimPluging::GKVehicle::Jam Gap Mean" );
	double meanJam = ANGConnGetAttributeValueDouble( ANGConnGetAttribute( meanJamString ), Car_Type );
	//delete[] meanJamString;

	return FREE_FLOW_SPEED/(meanJam+4.0+FREE_FLOW_SPEED*avg_headway_time)*3600;
}

std::vector<double> getHwyDist(int onramp_flow, double acc_length, double avg_headway)
{
	std::vector<double> hwys;
	double total_flow = 3600.0/avg_headway; //total flow
	//then check if assigning proportional flow to the inside lane 
	//will exceed capacity

	double prop = (0.2178-0.000125*onramp_flow+0.01115*acc_length/50);
	double flow_inside_lane = total_flow*(1-prop)*0.5;
	
	//exceed capacity
	double capacity = Lane_Capacity();
	if(flow_inside_lane>Lane_Capacity())
	{
		double flow_outer = (total_flow-capacity*2)*0.6;
		if(flow_outer>capacity)
		{
			hwys.push_back(3600.0/capacity);
			hwys.push_back(3600.0/capacity);
		}
		else
		{
			hwys.push_back(3600.0/flow_outer/0.6*0.5);
			hwys.push_back(3600.0/flow_outer);			
		}
		hwys.push_back(3600.0/capacity);
		hwys.push_back(3600.0/capacity);		
	}
	else
	{
		hwys.push_back(avg_headway/(prop*0.4));
		hwys.push_back(avg_headway/(prop*0.6));
		hwys.push_back(avg_headway/((1-prop)*0.5));
		hwys.push_back(avg_headway/((1-prop)*0.5));
	}
	return hwys;
}

//////////////////////////////////////////////////////////////////////////
// tell is a string is a number
//////////////////////////////////////////////////////////////////////////
bool is_number(std::string& s)
{
	std::string::iterator it = s.begin();
	while (it != s.end() && isdigit(*it)) ++it;
	return !s.empty() && it == s.end();
}

//////////////////////////////////////////////////////////////////////////
// read volume data and set to the experiment attribute
//////////////////////////////////////////////////////////////////////////
bool ResetVolumeFromFile()
{
	char config_file[len_str];
	sprintf_s(config_file,len_str, "C:\\CACC_Simu_Data\\temp.txt");	
	try
	{
		fopen_s(&cfg_file,config_file,"r");		
		char mystring [100];
		fgets (mystring, 100 , cfg_file);
		fclose(cfg_file);
		std::string source = std::string(mystring);
		std::string delimiter = ",";
		std::vector<std::string> flows;
		for(int i=0;i<3;i++)
		{
			int index = source.find(delimiter);
			if(index >= 0)
			{
				if(is_number(source.substr(0, index)))
				{
					flows.push_back(source.substr(0, index));
					source = source.substr(index+1, source.length());
				}
				else
					return false;
			}
			else
			{
				if(is_number(source))
					flows.push_back(source);
				else
					return false;
			}
		}
		int on_ramp = std::atoi(flows.at(0).c_str());
		int through = std::atoi(flows.at(1).c_str());
		int off_ramp = std::atoi(flows.at(2).c_str());

		int exp_id = ANGConnGetExperimentId();
		const unsigned short *MAINLANE_PercentString = 
			AKIConvertFromAsciiString( 
			"through_flow");
		ANGConnSetAttributeValueDouble
			( ANGConnGetAttribute( MAINLANE_PercentString ), exp_id, through );
		//delete[] MAINLANE_PercentString;

		const unsigned short *on_RAMP_PercentString = 
			AKIConvertFromAsciiString( 
			"on_ramp_flow");
		ANGConnSetAttributeValueDouble
			( ANGConnGetAttribute(on_RAMP_PercentString ), exp_id, on_ramp );
		//delete[] on_RAMP_PercentString;

		const unsigned short *off_RAMP_PercentString = 
			AKIConvertFromAsciiString( 
			"off_ramp_flow");
		ANGConnSetAttributeValueDouble
			( ANGConnGetAttribute(off_RAMP_PercentString ), exp_id, off_ramp );
		//delete[] off_RAMP_PercentString;
		
	}
	catch(int e)
	{
		return false;
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
// based on the experimental id tell if the simulation file should be run  
// in batch mode
//////////////////////////////////////////////////////////////////////////
bool IsBatchMode()
{
	int expid = ANGConnGetExperimentId();
	const unsigned short *batch_string = 
		AKIConvertFromAsciiString( 
		"run_batch");
	return ANGConnGetAttributeValueBool
		(ANGConnGetAttribute(batch_string), expid );
}

//Modify demand by OD matrix
void ModifyMatrixDemand(double acc_percent, double cacc_percent)
{
	//all of the sections
	int slice_num = AKIODDemandGetNumSlicesOD(1);
	int vtp_num = AKIVehGetNbVehTypes();

	//from mainlane to mainlane
	int total_through = 0;
	//off_ramp from mainlane
	int off_ramp = 0; //off_ramp vehicles can only be manual driven
	//on ramp to mainlane and exit from mainlane
	int on_ramp = 0;//on_ramp vehicles can only be manual driven

	//read demand
	if(IsBatchMode())
	{
		ResetVolumeFromFile();
	}
	read_volumn(total_through,on_ramp,off_ramp);
	
	//determine the result from HCM method
	int car_through = (double)total_through*(1-acc_percent-cacc_percent);
	int cacc_through =(double) total_through*cacc_percent;
	int acc_through = (double)total_through*acc_percent;
	int car_off =(double) off_ramp*(1-acc_percent-cacc_percent);
	int cacc_off = (double)off_ramp*cacc_percent;
	int acc_off =(double) off_ramp*acc_percent;
	int car_on = (double)on_ramp*(1-acc_percent-cacc_percent);
	int cacc_on =(double) on_ramp*cacc_percent;
	int acc_on =(double) on_ramp*acc_percent;

	//double ramp_demand = 800;
	int ACC_pos = AKIVehGetVehTypeInternalPosition (ACCveh_Type);
	int CACC_pos = AKIVehGetVehTypeInternalPosition (CACCveh_Type);
	int Car_pos = AKIVehGetVehTypeInternalPosition (Car_Type);

	int nCentroid = AKIInfNetNbCentroids();

	int mainlane_centroid_origin = 0;
	int on_ramp_centroid = 0;
	int off_ramp_centroid=0;
	int mainlane_centroid_dest =0;

	int mainlane_id = 0;
	int on_ramp_id = 0;
	
	for(int i=0;i<nCentroid;i++)
	{
		int iId = AKIInfNetGetCentroidId (i);
		A2KCentroidInf centroid_info = 
			AKIInfNetGetCentroidInf(iId);
		if(centroid_info.IsOrigin == true)
		{
			for(int j=0;j<centroid_info.NumConnecTo;j++)
			{
				int section_id = 
					AKIInfNetGetIdSectionofOriginCentroidConnector(iId,j);
				if(orgin_section.find(section_id) == orgin_section.end())
				{
					orgin_section.insert(std::pair<int,int>(iId,
						section_id));
				}
				A2KSectionInf section_info = 
					AKIInfNetGetSectionANGInf(section_id);
				//find the on-ramp for the test network
				if (section_info.nbCentralLanes == 1)
				{
					on_ramp_id = section_id;
					on_ramp_centroid = iId;
				}
				else
				{
					mainlane_id = section_id;
					mainlane_centroid_origin = iId;
				}
			}
		}
		else
		{
			for(int j=0;j<centroid_info.NumConnecFrom;j++)
			{
				int section_id = 
					AKIInfNetGetIdSectionofDestinationCentroidConnector(iId,j);
				A2KSectionInf section_info = 
					AKIInfNetGetSectionANGInf(section_id);
				//find the on-ramp for the test network
				if (section_info.nbCentralLanes == 1)
				{
					off_ramp_centroid = iId;
				}
				else
				{
					mainlane_centroid_dest = iId;
				}
			}
		}
	}

	for(int j=0;j<slice_num;j++)
	{
		AKIODDemandSetDemandODPair(mainlane_centroid_origin,
			mainlane_centroid_dest,Car_pos,j,
			car_through==0?10:car_through);
		AKIODDemandSetDemandODPair(mainlane_centroid_origin,
			mainlane_centroid_dest,ACC_pos,j,
			acc_through==0?10:acc_through);
		AKIODDemandSetDemandODPair(mainlane_centroid_origin,
			mainlane_centroid_dest,CACC_pos,j,
			cacc_through==0?10:cacc_through);

		AKIODDemandSetDemandODPair(mainlane_centroid_origin,
			off_ramp_centroid,Car_pos,j,
			car_off==0?10:car_off);
		AKIODDemandSetDemandODPair(mainlane_centroid_origin,
			off_ramp_centroid,CACC_pos,j,
			cacc_off==0?10:cacc_off);
		AKIODDemandSetDemandODPair(mainlane_centroid_origin,
			off_ramp_centroid,ACC_pos,j,
			acc_off==0?10:acc_off);

		AKIODDemandSetDemandODPair(on_ramp_centroid,
			mainlane_centroid_dest,CACC_pos,j,
			(cacc_on==0?10:cacc_on));
		AKIODDemandSetDemandODPair(on_ramp_centroid,
			mainlane_centroid_dest,ACC_pos,j,
			acc_on==0?10:acc_on);
		AKIODDemandSetDemandODPair(on_ramp_centroid,
			mainlane_centroid_dest,Car_pos,j,
			car_on==0?10:car_on);

		//get total demand from the mainlane origin
		int total_mainlane = total_through
			+ off_ramp;
		
		double avg_headway = 3600.0/(double)total_mainlane;
		std::vector<double> hwy_dist = getHwyDist(on_ramp, ACC_LENGTH,avg_headway);
		avg_headway_origin.insert(
			std::pair<int,std::vector<double>>(mainlane_centroid_origin,hwy_dist));
		std::vector<double> temp_min; 
		temp_min.push_back(1.0);temp_min.push_back(1.0);temp_min.push_back(1.0);temp_min.push_back(1.0);
		min_headway_origin.insert(
			std::pair<int,std::vector<double>>(mainlane_centroid_origin,temp_min));

		//get total demand from the on-ramp
		int onramp_flow = on_ramp;
		avg_headway = 3600.0/(double)onramp_flow;
		std::vector<double> temp1; temp1.push_back(avg_headway);
		avg_headway_origin.insert(
			std::pair<int,std::vector<double>>(on_ramp_centroid,temp1));
		std::vector<double> temp2; temp2.push_back(1.0);
		min_headway_origin.insert(
			std::pair<int,std::vector<double>>(on_ramp_centroid,temp2));

		// initialize the next time vector
		if(time_next.find(mainlane_centroid_origin)==time_next.end())
		{
			std::vector<int> mainlane_vector;
			mainlane_vector.push_back(0);
			mainlane_vector.push_back(0);
			mainlane_vector.push_back(0);
			mainlane_vector.push_back(0);
			time_next.insert
				(std::pair<int,std::vector<int>>
				(mainlane_centroid_origin, mainlane_vector));

			std::map<int, double> destinations;
			if(total_through+off_ramp>0)
			{
				destinations.insert(std::pair<int,double>
					(mainlane_centroid_dest, total_through/(total_through+off_ramp)));
				destinations.insert(std::pair<int,double>
					(off_ramp_centroid, 1.0));
			}
			dest.insert(std::pair<int,std::map<int,double>>
				(mainlane_centroid_origin,destinations));
		}
		
		if(time_next.find(on_ramp_centroid)==time_next.end())
		{
			// initialize the next time vector
			std::vector<int> on_ramp;
			on_ramp.push_back(0);
			time_next.insert
				(std::pair<int,std::vector<int>>
				(on_ramp_centroid, on_ramp));

			std::map<int, double> destinations;
			destinations.insert(std::pair<int,double>
				(mainlane_centroid_dest, 1));
			dest.insert(std::pair<int,std::map<int,double>>
				(on_ramp_centroid,destinations));
		}
	}
}


//////////////////////////////////////////////////////////////////////////
// generate a random exponential distributed variable
//////////////////////////////////////////////////////////////////////////
double RandomExpHeadway(double min_hwy, double avg_hwy)
{
	if(avg_hwy>0)
	{
		if(avg_hwy < min_hwy)
			return min_hwy;
		else
		{
			double laneflow = 3600.0/avg_hwy;
			double lambda = laneflow / 3600.0 / (1 - min_hwy * laneflow / 3600.0);
			//double lambda = 1/avg_hwy;
			double theta = exp(lambda * min_hwy);
			double x = AKIGetRandomNumber();
			x = log((1 - x) / theta) / (-lambda);
			return x ;
		}
	}
	else
		return 0;
}

double round(double x)
{
	return floor(x+0.5);
}

//////////////////////////////////////////////////////////////////////////
// Generate vehicles on original sections
//////////////////////////////////////////////////////////////////////////
double dmd_generate_section(double time, 
							double timeSta, 
							double timTrans, 
							double acicle)
{

	//get simulation steps
	int current_step =(int)(round(time/acicle));
	if(global_interval == 0)
		return 0;
	// according to the current time, determine the truck percentage
	int time_index = (int)(time/60.0)/global_interval+interval_shift*60/global_interval;	

	// iterate of vector
	// this size of the time next is the number of origins
	typedef std::map<int, std::vector<int>>::iterator it_type;
	for(it_type iterator = time_next.begin(); iterator != time_next.end(); iterator++) 
	{
		//section id
		int id = iterator->first;

		const unsigned short *increase_DLC_close_ramp_str = 
			AKIConvertFromAsciiString( "section_ramp_type");
		int ramp_type = ((ANGConnGetAttributeValueInt(
			ANGConnGetAttribute(increase_DLC_close_ramp_str), id)));
		//delete [] increase_DLC_close_ramp_str;
		//int lanes = AKIInfNetGetSectionANGInf(id).nbCentralLanes;
		//delete[] increase_DLC_close_ramp_str;

		std::vector<int> times = time_next[id];
		//the lane id in pmes is the opposite of the AIMSUN
		//call in reverse order
		for(int k=0;k<times.size();k++)
		{
			//next_time at lane k
			int next_time = times.at(k);
			if(time_index>=truck_portions[id].size())
				break;
			double truck_percentage = truck_portions[id][time_index][k];
			if( next_time == current_step)
			{
				//generate a new vehicle here
				//first determine its destination
				double rand_num = AKIGetRandomNumber();
				int veh_type = 0;
				if(rand_num < ACC_percent)
					veh_type = ACCveh_Type;
				else if(rand_num < ACC_percent+
					CACC_percent)
					veh_type = CACCveh_Type;
				else if(rand_num < ACC_percent + CACC_percent + truck_percentage)
					veh_type = Truck_Type;
				else
					veh_type = Car_Type;

				int turningsection = 0;
				rand_num = AKIGetRandomNumber();
				double prob=0;
				int turningid = 0;
				for(int i=0; i<turning_portions[id].size(); i++)
				{
					prob += turning_portions[id][i];
					if(rand_num < prob)
					{
						turningsection = i;
						turningid = AKIInfNetGetIdSectionANGDestinationofTurning(id, turningsection);
						break;
					}
				}

				// if the car cannot be put there we don't put there
				// instead we put it on a queue and evaluate queue each time
				int res = 
					AKIPutVehTrafficFlow(
					id,times.size()-k,
					AKIVehGetVehTypeInternalPosition(veh_type),0,0,
					turningid,false);				
				
				if(k==0)
					k=0;

				//randomly generate the next time and replace
				//with the value in vector
				
				double avg_headway_flow = global_interval*60.0;
				if(flows[id][time_index][k] > 0)				
				{
					avg_headway_flow = min(avg_headway_flow,
						3600.0/(((double)flows[id][time_index][k])/(double)global_interval*60.0));
				}
				double min_headway = 1.2;
				if (ramp_type == 3 
					////&& ECIGetNumberofControls ()>0
					//&& lanes == 2
					&& time_index >= 72
					&& time_index <= 12*9
					) // on ramp
				{
					//continue;
					min_headway = 6;
				}
				int new_time = (int)(RandomExpHeadway(min_headway, avg_headway_flow)/acicle+0.5); //add 0.5 is because the (int) always truncate
				time_next[id][k] = new_time+current_step;
				if(new_time+current_step == 125)
					new_time = new_time;
			}
			
		}
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
// Generate vehicle
//////////////////////////////////////////////////////////////////////////
int dmd_generate_matrix(double time, 
						double timeSta, double timTrans, 
						double acicle)
{
	//get simulation steps
	int current_step =(int)(round(time/acicle));
	//iterate of vector
	for(int i=0;i<time_next.size();++i)
	{
		if(time>=AKIODDemandGetIniTimeSlice(1, i)&&
			time<AKIODDemandGetEndTimeSlice(1, i))
		{
			for(int j=0;j<AKIInfNetNbCentroids();++j)
			{
				int origin = AKIInfNetGetCentroidId(j);
				if(time_next.find(origin)!=time_next.end())
				{
					std::vector<int> times = time_next[origin];
					for(int k=0;k<times.size();++k)
					{
						int next_time = times.at(k);
						if(next_time == current_step)
						{
							//generate a new vehicle here
							//first determine its destination
							double rand_num = AKIGetRandomNumber();
							std::map<int,double>::iterator _iterator;
							//go over the destinations
							for(_iterator = dest[origin].begin(); _iterator != dest[origin].end(); ++_iterator)
							{
							//for(int n=0;n<dest[origin].size();n++)
							//{
								if(rand_num <= _iterator->second) //dest[origin][n][1] is the flow proportion 
																   // of the specific origin and destination
								{
									//find the destination
									int denstine = _iterator->first;
									//determine the type
									rand_num = AKIGetRandomNumber();
									int veh_type = 0;
									if(rand_num < ACC_percent)
										veh_type = ACCveh_Type;
									else if(rand_num < ACC_percent+
										CACC_percent)
										veh_type = CACCveh_Type;
									else
										veh_type = Car_Type;

									//put a car on the section
									if(AKIPutVehTrafficOD
											(orgin_section[origin], k+1, 
											AKIVehGetVehTypeInternalPosition(veh_type),
											origin,denstine,0,0,false) < 0)
									{
										veh_type = 0;
									}

									//randomly generate the next time and replace
									//with the value in vector
									if(avg_headway_origin.find(origin)!=avg_headway_origin.end()
										&&
										avg_headway_origin[origin].at(k)>0)
									{
										int new_time = (int)(RandomExpHeadway(min_headway_origin[origin].at(k),
											avg_headway_origin[origin].at(k))/acicle);
										time_next[origin][k] = new_time+current_step;
									}
									
									break;
								}
							}
						}
					}
				}
			}
		}
	}
	return 0;
}

int dmd_modify_cacc(double ACC_percent, double CACC_percent, double ramp_demand, double mainlane)
{
	if(ACC_percent<0 || CACC_percent<0)
		return -1;
	if(ACC_percent+CACC_percent>1)
		return -1;
	//all of the sections
	int slice_num = AKIStateDemandGetNumSlices();
	int vtp_num = AKIVehGetNbVehTypes();
	//if the total demand is larger than 0, then add CACC and ACC vehicle
	int ACC_pos = AKIVehGetVehTypeInternalPosition (ACCveh_Type);
	int CACC_pos = AKIVehGetVehTypeInternalPosition (CACCveh_Type);
	int Car_pos = AKIVehGetVehTypeInternalPosition (Car_Type);
	for(int j=0;j<slice_num;j++)
	{
		//for the last period reduce mainlane demand to 200 per hour and ramp to 50 per hour
		if(j == slice_num-1)
		{
			double car_demand = 200*(1- ACC_percent - CACC_percent);
			AKIStateDemandSetDemandSection(MAINLANE, Car_pos, j, car_demand==0?1e-8:car_demand);
			//car_demand==0?1:car_demand%\);
			double acc_demand = 200*ACC_percent;
			AKIStateDemandSetDemandSection(MAINLANE, ACC_pos, j, acc_demand==0?1e-8:acc_demand);
			double cacc_demand = 200*CACC_percent;
			AKIStateDemandSetDemandSection(MAINLANE, CACC_pos, j, cacc_demand==0?1e-8:cacc_demand);

			AKIStateDemandSetDemandSection(RAMP_ID, Car_pos, j, 50);
			AKIStateDemandSetDemandSection(RAMP_ID, ACC_pos, j, 1);
			AKIStateDemandSetDemandSection(RAMP_ID, CACC_pos, j, 1);
			break;
		}
		double car_demand = mainlane*(1- ACC_percent - CACC_percent);
		AKIStateDemandSetDemandSection(MAINLANE, Car_pos, j, car_demand==0?1e-8:car_demand);
		//car_demand==0?1:car_demand%\);
		double acc_demand = mainlane*ACC_percent;
		AKIStateDemandSetDemandSection(MAINLANE, ACC_pos, j, acc_demand==0?1e-8:acc_demand);
		double cacc_demand = mainlane*CACC_percent;
		AKIStateDemandSetDemandSection(MAINLANE, CACC_pos, j, cacc_demand==0?1e-8:cacc_demand);

		car_demand = ramp_demand;
		AKIStateDemandSetDemandSection(RAMP_ID, Car_pos, j, car_demand==0?1e-8:car_demand);//all the demand goes to car
		AKIStateDemandSetDemandSection(RAMP_ID, ACC_pos, j, 1);//ramp has no acc and cacc vehicles
		AKIStateDemandSetDemandSection(RAMP_ID, CACC_pos, j, 1);//ramp has no acc and cacc vehicles

	}

	return 1;

}

int read_pems_flow()
{
	//get the origin sections of the network
	int num_sec = AKIInfNetNbSectionsANG();
	std::map<int,A2KSectionInf> hashmap;
	std::vector<int> set;
	for(int i=0; i<num_sec; i++)
	{
		int id = AKIInfNetGetSectionANGId(i);
		A2KSectionInf inf = AKIInfNetGetSectionANGInf(id);
		hashmap.insert(std::pair<int,A2KSectionInf>(id, inf));
		for(int j=0; j<inf.nbTurnings; j++)
		{
			int next
				= AKIInfNetGetIdSectionANGDestinationofTurning(id, j);
			if(find (set.begin(), set.end(), next) ==set.end())
				set.push_back(next);		
		}
	}
	global_interval = 0; //minutes
	//for each source, we read its input flows
	for(int i=0; i<num_sec; i++)
	{
		int id = AKIInfNetGetSectionANGId(i);
		if(find(set.begin(), set.end(), id)
			!= set.end())
		{
			continue;;
		}
		//select the file
		std::string filename = "C:\\CACC_Simu_Data\\flows\\";
		char buffer[10];
		std::sprintf(buffer, "%d", id);

		filename += (std::string(buffer)
			+std::string(".txt"));
		std::ifstream myfile (filename.c_str());
		int line_id = 0;
		std::string delimiter = " ";
		if (myfile.is_open())
		{
			int num_lanes = 0;
			int cols = 1;
			std::string line;
			std::vector<std::vector<double>> flow_interval;
			while ( getline (myfile,line))
			{
				if(line_id == 0)
				{
					int index = line.find(delimiter);
					std::string str_interval = line.substr(0, index);
					int interval = atoi( str_interval.c_str());
					if(interval == 0)
						return 0;
					if(global_interval == 0)
						global_interval = interval;
					else if(global_interval!=interval)
						return 0;
					while(line.find(delimiter) != std::string::npos)
					{
						cols++;
						line = line.substr(line.find(delimiter)+1,
							line.length() - line.find(delimiter)-1);
					}
					num_lanes = (cols-7)/4;
					if(num_lanes != hashmap[id].nbCentralLanes)
					{
						return 0;
					}
				}
				else
				{
					std::vector<double> laneflow;
					//read a line of flow
					//the last three columns is not lane-based flow
					//skip th first two cols
					int index = line.find_first_of((char)(9));
					line = line.substr(index+1, line.length()
						-index-1);
					for(int k=0;k<num_lanes;k++)
					{
						index = line.find_first_of((char)(9));
						double tempflow = atof((line.substr(0,index)).c_str());
						laneflow.push_back(tempflow);
						line = line.substr(index+1, line.length()
							-index-1);
					}
					flow_interval.push_back(laneflow);										
				}
				line_id++;
			}
			flows.insert
				(std::pair
				<int,std::vector<std::vector<double>>>(id, flow_interval));
			myfile.close();
		}
	}

	return 1;
}

int read_pems_truck_percentage()
{
	//based on the flow initialize the truck percentage as zero
	typedef std::map<int, std::vector<std::vector<double>>>::iterator it_type;
	for(it_type iterator = flows.begin(); iterator != flows.end(); iterator++) 
	{
		std::vector<std::vector<double>> truck_percent;
		for(int j=0; j<flows[iterator->first].size(); j++)
		{
			std::vector<double> temptruck;
			for(int k=0; k<flows[iterator->first].at(j).size(); k++)
			{
				temptruck.push_back(0);
			}
			truck_percent.push_back(temptruck);
		}
		int id = iterator->first;
		truck_portions.insert(std::pair<int, std::vector<std::vector<double>>>(id, truck_percent));

		//select the file
		std::string filename = "C:\\CACC_Simu_Data\\flows\\";
		char buffer[10];
		std::sprintf(buffer, "%d", id);

		filename += (std::string(buffer)
			+std::string("-truck.txt"));
		std::ifstream myfile (filename.c_str());
		int line_id = 0;
		std::string delimiter = " ";
		if (myfile.is_open())
		{
			int num_lanes = 0;
			int cols = 1;
			std::string line;
			while ( getline (myfile,line))
			{
				if(line_id == 0)
				{
					
				}
				else
				{
					if(line_id > truck_portions[id].size())
						return -1;
					//read a line of flow
					//the last three columns is not lane-based flow
					//skip th first two cols
					int index = line.find_first_of((char)(9));
					line = line.substr(index+1, line.length()
						-index-1);
					for(int k=0;k<truck_portions[id][line_id-1].size();k++)
					{
						index = line.find_first_of((char)(9)); //tab's ascii is 9
						double tempflow = atof((line.substr(0,index)).c_str());
						if(k >= truck_portions[id][line_id-1].size())
							return -1;
						truck_portions[id][line_id-1][k] = tempflow*0.01;
						line = line.substr(index+1, line.length()
							-index-1);
					}									
				}
				line_id++;
			}
			myfile.close();
		}
	}

	return 1;
}

//////////////////////////////////////////////////////////////////////////
// Initialize the next time vector
//////////////////////////////////////////////////////////////////////////
int create_nexttime()
{
	typedef std::map<int, std::vector<std::vector<double>>>::iterator it_type;
	for(it_type iterator = flows.begin(); iterator != flows.end(); iterator++) 
	{
		int id = iterator->first;
		std::vector<int> temp;
		for(int j=0;j<iterator->second.size();j++)
		{
			for(int k=0;k<iterator->second[j].size(); k++)
			{
				temp.push_back(0);
			}
			time_next.insert(std::pair<int,std::vector<int>>(id, temp));
			break;
		}
	}
	return 1;
}

//////////////////////////////////////////////////////////////////////////
// read turning proportions
//////////////////////////////////////////////////////////////////////////
int read_turnings()
{
	typedef std::map<int, std::vector<std::vector<double>>>::iterator it_type;
	for(it_type iterator = flows.begin(); iterator != flows.end(); iterator++) 
	{
		int id = iterator->first;
		A2KSectionInf sec_info = 
			AKIInfNetGetSectionANGInf(id);
		std::vector<double> temp;
		for(int k=0; k<sec_info.nbTurnings;k++)
			temp.push_back(1.0);
		
		turning_portions.insert(std::pair<int, std::vector<double>>(id, temp));
	}
	return 1;
}

// read surge factor from txt file
double ReadSurgeFactor()
{
	std::ifstream infile("C:\\CACC_Simu_Data\\SurgeFactor.txt");
	std::string line;
	double surgefactor=1;
	if(infile.is_open() == false)
		return surgefactor;
	while (std::getline(infile, line))
	{
		surgefactor = atof(line.c_str());
		break; 
	}
	infile.close();
	return surgefactor;
}

//////////////////////////////////////////////////////////////////////////
// calculate the start time of drop and drop rate
//flow pattern: --------------------   //flat at the beginning
//                                  \
//                                   \ 
//	                                  \
//									   \   drop rate
//                                      \
//                                       \--------------------
//                                  |time when drop happens
//////////////////////////////////////////////////////////////////////////
double Automatic_surge(int total_count, int total_period, int end_count
					   ,double surge_flow, double& reduction_step)
{

	/*double serge_factor = (total_count - (double)(total_period)/2.0*end_count)/
		((double)(total_period)/2.0*start_count);

	reduction_step = ((serge_factor*start_count-end_count)/(double)(total_period-1));

	return serge_factor;*/

	//double surge_period = 0;
	//double count_in_surge = surge_period * surge_flow/(double)global_interval; //surge flow is per global interval
	//
	//double nf_s = total_period*surge_flow;
	double delta_fs = surge_flow - end_count;
	//double drop_period = 2*(nf_s-total_count)/delta_fs;

	//reduction_step = delta_fs/(drop_period-1);

	double n1 = 2*(total_count - end_count*total_period)/delta_fs;
	reduction_step = delta_fs/(n1-1);

	return n1;
}

//////////////////////////////////////////////////////////////////////////
// Modify demand in congested period to compensate the low flow data 
//////////////////////////////////////////////////////////////////////////
int modify_demand_congest()
{
	//read start and end time for correction period
	int id = ANGConnGetExperimentId();
	const unsigned short *demand_start_str = 
		AKIConvertFromAsciiString( "_surgedemand_start_time");
	double surge_start = ((ANGConnGetAttributeValueDouble(
		ANGConnGetAttribute(demand_start_str), id)));	

	const unsigned short *demand_end_str = 
		AKIConvertFromAsciiString( "_surgedemand_end_time");
	double surge_end = ((ANGConnGetAttributeValueDouble(
		ANGConnGetAttribute(demand_end_str), id)));	

	const unsigned short *section_str = 
		AKIConvertFromAsciiString( "_surge_section");
	int surge_section = ((ANGConnGetAttributeValueInt(
		ANGConnGetAttribute(section_str), id)));	
	
	//gather the cumulative arrivals during the period
	double surge_period = surge_end - surge_start;
	int total_period = floor(surge_period*60.0/(double)global_interval);

	int total_count = 0;
	for(int i=0; i< total_period; i++)
	{
		int period = i + (int)(surge_start*60.0/global_interval);
		total_count += std::accumulate(flows[surge_section].at(period).begin(), 
			flows[surge_section].at(period).end(), 0);
	}

	int start_interval = (int)(surge_start*60.0/global_interval);
	int start_count = std::accumulate(flows[surge_section].at(start_interval).begin(), 
		flows[surge_section].at(start_interval).end(), 0);

	int end_interval = (int)(surge_end*60.0/global_interval);
	int end_count = std::accumulate(flows[surge_section].at(end_interval).begin(), 
		flows[surge_section].at(end_interval).end(), 0);

	if(end_count >= start_count)
		return 1;

	//now read the surge factor
	double surge_factor = ReadSurgeFactor();
	double reduction_step = 0;
	double high_flow_period = Automatic_surge(total_count, total_period, end_count, surge_factor*start_count,
		reduction_step);
	//high_flow_period = 6;
	//double drop_start = end_interval - drop_period;
	double surgeflow = surge_factor*start_count + reduction_step;
	//then according to surge factor and reduction adjust the input flow
	for(int i=0; i< total_period; i++)
	{
		int period = i + (int)(surge_start*60.0/global_interval);
		std::vector<double> ratio;
		int totaltemp = std::accumulate(flows[surge_section].at(period).begin(), 
			flows[surge_section].at(period).end(), 0);
		for(int j=0; j<flows[surge_section].at(period).size();j++)
		{
			ratio.push_back((double)flows[surge_section][period][j] /(double)totaltemp);
		}
		if(period > high_flow_period + start_interval)
			surgeflow = end_count;
		else
			surgeflow -= reduction_step;

		for(int j=0; j<flows[surge_section].at(period).size();j++)
		{
			flows[surge_section].at(period)[j] = surgeflow*ratio[j];
		}
	}
	
	return 1;
}

// this method read pmes direct txt data output and insert 
// traffic states based on that file. an example data file is:
// 5 Minutes	Lane 1 Flow (Veh/5 Minutes)	Lane 2 Flow (Veh/5 Minutes)	Lane 3 Flow (Veh/5 Minutes)	Lane 4 Flow (Veh/5 Minutes)	Lane 5 Flow (Veh/5 Minutes)	Flow (Veh/5 Minutes)	# Lane Points	% Observed
// 09/03/2015 00:00	31	33	25	56	8	153	5	80
// 09/03/2015 00:05	35	46	32	57	6	176	5	80
// 09/03/2015 00:10	26	29	25	68	5	153	5	80
// 09/03/2015 00:15	20	35	36	28	5	124	5	80
// 09/03/2015 00:20	24	34	33	29	5	125	5	80
// each line will be used to create a state
// each source section will need to have a file
int dmd_create_pems(double ACC_percent, double CACC_percent)
{
	//initialize the internal shift
	int id = ANGConnGetExperimentId();
	const unsigned short *shift_str = 
		AKIConvertFromAsciiString( "_field_start_time");
	interval_shift = ((ANGConnGetAttributeValueInt(
		ANGConnGetAttribute(shift_str), id)));	
	//delete[] shift_str;
	shift_str = 0;

	if(read_pems_flow() == 1)
	{
		if(modify_demand_congest() == 1)
		{
			if(read_pems_truck_percentage()==1)
			{
				if(read_turnings() == 1)
					if(create_nexttime() == 1)
						return 1;
			}
		}
	}

	return 0;
}


//in this method, we generate linearly increasing demands for mainlane traffic from 1000 vph 
// to 1600 vph linearly, and then return to 800.
// every 15min: 1000, 1200, 1400, 1600, 1600, 1600, 1600, 1400, 1200, 1000, 800, 600 
//the mainlane traffic stay the same 400 vph.
int dmd_modify_cacc_linear(double ACC_percent, double CACC_percent)
{
	if(ACC_percent<0 || CACC_percent<0)
		return -1;
	if(ACC_percent+CACC_percent>1)
		return -1;
	//all of the sections
	int slice_num = AKIStateDemandGetNumSlices();
	int vtp_num = AKIVehGetNbVehTypes();
	//if the total demand is larger than 0, then add CACC and ACC vehicle
	double mainlane_demand_3hr[12] = {800, 1000, 1200, 1400, 1200, 1000, 800, 600, 400, 400, 400, 400};
	double mainlane_demand_2hr[8] = {1600, 1600, 1600, 1600, 400, 400, 400, 400};
	double ramp_demand = 400;
	//double ramp_demand = 800;
	int ACC_pos = AKIVehGetVehTypeInternalPosition (ACCveh_Type);
	int CACC_pos = AKIVehGetVehTypeInternalPosition (CACCveh_Type);
	int Car_pos = AKIVehGetVehTypeInternalPosition (Car_Type);
	for(int j=0;j<slice_num;j++)
	{
		double car_demand =( slice_num == 8? mainlane_demand_2hr[j]:mainlane_demand_3hr[j])
			*(1- ACC_percent - CACC_percent);
		AKIStateDemandSetDemandSection(MAINLANE, Car_pos, j, car_demand==0?1e-8:car_demand);
		//car_demand==0?1:car_demand%\);
		double acc_demand = (slice_num == 8? mainlane_demand_2hr[j]:mainlane_demand_3hr[j])
			*ACC_percent;
		AKIStateDemandSetDemandSection(MAINLANE, ACC_pos, j, acc_demand==0?1e-8:acc_demand);
		double cacc_demand = (slice_num == 8? mainlane_demand_2hr[j]:mainlane_demand_3hr[j])
			*CACC_percent;
		AKIStateDemandSetDemandSection(MAINLANE, CACC_pos, j, cacc_demand==0?1e-8:cacc_demand);

		car_demand = ramp_demand;
		AKIStateDemandSetDemandSection(RAMP_ID, Car_pos, j, car_demand==0?1e-8:car_demand);//all the demand goes to car
		AKIStateDemandSetDemandSection(RAMP_ID, ACC_pos, j, 1);//ramp has no acc and cacc vehicles
		AKIStateDemandSetDemandSection(RAMP_ID, CACC_pos, j, 1);//ramp has no acc and cacc vehicles

	}

	return 1;

}
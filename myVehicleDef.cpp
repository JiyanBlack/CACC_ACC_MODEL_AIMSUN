#include "myVehicleDef.h"
#include <stdlib.h>
#include "AKIProxie.h"
#include "ANGConProxie.h"
#include <stdio.h>
#include <math.h>
#include <queue>
#include <hash_map>
#include <algorithm>  
#include "mybehavioralModel.h"

#define MAX(a,b)    (((a)>(b)) ? (a) : (b))
#define MIN(a,b)    (((a)<(b)) ? (a) : (b))

#define CF	0
#define MCF	1
#define BCF	2
#define ECF	3
#define DCF	4
#define ACF	5
#define CCF	6
#define RCF	7
#define LC	8

#define EXIT 1
#define ELC 2
#define DLC 3
#define RAMP 4
#define TURNING 5

#define MANDATORY 4
#define  OPTIONAL 3

#define RAMP_DECISION_SLOW_DOWN -1
#define RAMP_DECISION_FOLLOW 1
#define RAMP_LANE_CHANGE_FEASIBLE 0
#define RAMP_DECISION_NORMAL_FOLLOW 2

#define DLC_DECISION_SLOW_DOWN -1
#define DLC_DECISION_FOLLOW 1
#define DLC_LANE_CHANGE_FEASIBLE 0
#define DLC_DECISION_NORMAL_FOLLOW 2

#define EXIT_DECISION_SLOW_DOWN -1
#define EXIT_DECISION_FOLLOW 1
#define EXIT_CHANGE_FEASIBLE 0
#define EXIT_DECISION_NORMAL_FOLLOW 2

#define FeederOnRamp 23554
#define END_SECTION 23551
#define NO_CHANGE_ZONE_LENGTH 1357
#define RAMP_LENGTH 1666.3
#define END_OF_RAMP 1657 //ASSUME THE EFFECTIVE RAMP LENGTH = 300 meters
#define RAMP_SPEED 25
#define CREEP_SPEED 20

#define COMF_LEVEL 0.5 //the comfortable level of deceleration
#define DANGER_GAP 0.8
#define MIN_REGAIN_AUTO_SPEED 5

#define ACC_BASED_MODEL 1
#define GAP_BASED_MODEL 0

double bound(double x,double x_high,double x_low)
{
	double b;
	b = MIN(x,x_high);
	b = MAX(b,x_low);
	return b;
}


myVehicleDef:: myVehicleDef ( void *handlerVehicle, unsigned short idhandler,bool isFictitiousVeh ): A2SimVehicle( handlerVehicle, idhandler,isFictitiousVeh ) {
	
	//set initial values
	SetInitialVal();

	if ((getVehType() == ACCveh) || (getVehType() == CACCveh))
	{
		applyACC_CACC = true; 
	}else{
		applyACC_CACC = false;
	}
}

myVehicleDef::~ myVehicleDef (){}

bool myVehicleDef::getapplyACC()
{
	return applyACC_CACC;
}

// update the vehicle in regular car following mode
void myVehicleDef::updateRegularCf()
{	
	double x, x_CF;
	x=getPosition(0);
	x_CF=PosCf(leader, 0, 1, 1, 1);
	setNewPosition(x_CF, 2*(x_CF - x) / delta_t-this->getSpeed());
}

//determine the current mode based on the mode from the last step
//This function reorganized Hwasoo's code as the basic car following function 
//In this function, lane changing conditions are tested, and the corresponding modes are set.
int myVehicleDef::determineDrivingMode()
{

	int currentMode = getMode();

	//do not do any lane changes on node
	if(this->isCurrentLaneNode() == true)
	{
		if(VehID == debug_track_id)
		{
			VehID = VehID;
		}
		switch (currentMode)
		{
			case CF:
				return getMode(); //decide if lane change /merge /cooperation is needed
			case  ACF:
				return getMode(); //decide if another lane change followers this lane change
			case CCF:
				return getMode(); //see if it is willing to yield or itself needs a change
			/*case RCF:
				return DetermineReceiveOrLc();*/ 
			case BCF:
				return setMode(CF);
			default:
				return currentMode;
		}
	}
	switch (currentMode)
	{
		case CF:
			return DetermineLcOrMergeOrCoop(); //decide if lane change /merge /cooperation is needed
		case  ACF:
			return Determine2ndLcAfterLc(); //decide if another lane change followers this lane change
		case CCF:
			return determineCoopOrLc(); //see if it is willing to yield or itself needs a change
		/*case RCF:
			return DetermineReceiveOrLc();*/ 
		case BCF:
			return determineGapOrGiveup();
		default:
			return currentMode;
	}
}

//run Yeo Model
// each update function must do three things:
// calculate the next position, call setnewposition, and set the mode for the next step
void myVehicleDef::RunNGSIM()
{              
	
	switch(determineDrivingMode())
	{
		case CF:                          
			updateRegularCf();
			break;
		//MCF mode was disabled. Try to avoid this type of behavior. And we try to make the connection one to one
		//using side lane to model merge type of behavior
		/*case MCF:                           
			UpdateMergeCf();
			break;*/
		case BCF:
			UpdateBeforeLaneChangeCf();
			break;
		//not sure if this type is necessary to be here
		/*case ECF:
			UpdateEmergentCf();
			break;*/
		//this one either
		/*case DCF:
			UpdateDuingLaneChangeCf();           
			break;*/
		case ACF:                         
			UpdateAfterLaneChangeCf();
			break;
		case CCF:
			updateCoopCf();
			break;
		/*case RCF:                         
			UpdateReceiveCf();*/
			//break;
		default:
			break;
	}
}


#pragma optimize("",off)
bool myVehicleDef::ApplyNGSIMModel() 
{
	int sectionid = getIdCurrentSection();		
	A2KSectionInf sectioninfo = AKIInfNetGetSectionANGInf(sectionid);  
	if (getapplyACC() == false //this is not a cacc or acc vehicle
		||isCurrentLaneOnRamp() //this vehicle is on ramp
		||(getIdCurrentLane() == 1 && sectioninfo.nbSideLanes >=1
		|| this->getIdCurrentSection() == FeederOnRamp
		|| this->getMode() == ACF
		|| this->source_section == FeederOnRamp) //this vehicle is on ramp
		)
	{
		return true;
	}
	return false;
}
#pragma optimize("",on)


void myVehicleDef::UpdateVehicle(double simu_step)
{
	delta_t = simu_step;
	int veh_Id= getId();
	AKIVehSetAsTracked(veh_Id);
	this->info = AKIVehTrackedGetInf(veh_Id);
	this->staticinfo = AKIVehTrackedGetStaticInf(veh_Id);
	AKIVehSetAsNoTracked(veh_Id);
	this->leader = this->getLeader();
	//we hate to do this. But, we have to do it due to the fact that we cannot really recognize the vehicle in front.
	
	if(this->ApplyNGSIMModel())
	{
		RunNGSIM();
	}
	//ACC and CACC behavior
	//they are only applied to these vehicles without lane-changing intent
	//the lane change intent is identified when the vehicle is on the rightmost lane for the specific testing scenario
	else 
	{
		RunACCCACC();
	}
}

#pragma optimize("",off)
bool myVehicleDef::LetAimsunHandle() 
{
	double buffer = 5;
	if (isCurrentLaneNode() == true)
	{
		return true;
	}
	else
	{
		int sectionid = getIdCurrentSection();
		A2KSectionInf sectioninfo = AKIInfNetGetSectionANGInf(sectionid);
		if(sectioninfo.length-getPosition(0) <= buffer
			&& sectioninfo.id != END_SECTION)
		{
			return true;	
		}
		return false;

	}
	return false;
}
#pragma optimize("",on)

void myVehicleDef::RunACCCACC()
{
	if (VehID == this->getDebugTrackID())
		VehID = VehID;

	//***********collect and record basic information
	double	x_CF=0.0;
	double  v_des=0.0;
	double a_U        = 0.0;			
	double a_L        = 0.0;
    double vf         = 0.0;
    double v          = 0.0;
    double x          = 0.0;
	double v_1          = 0.0;
    double x_1          = 0.0;
	double v_2          = 0.0;
    double x_2          = 0.0;
	double l_leader=0.0;
	double a_L_leader=0.0;
	double d_leader=0.0;
    double v_leader=0.0; // note: leader has already been updated  
	double x_leader=0.0; 
    double v_1_leader=0.0;
	double x_1_leader=0.0; 
    double v_2_leader=0.0;
	double x_2_leader=0.0; 
    double headway=0.0;
	double dist_safety=0.0;
	
	double desired_time_gap=0;
	double speed_error=0.0;
	double acceleration=0.0;
	double desired_range = 0.0;
	double gap=0.0;
	double range_error = 0.0;
	double range_rate = 0.0;	
	int Has_Leader=0;

	double  e=0.0, a_des=0.0;	
	double e_pre_step=0.0, e_pre_step_2=0.0;
	
	desired_time_gap=this->staticinfo.headwayMin;

	a_U        = getMAXacc();			//a_U>0    
	a_L        = getMAXdec();			//a_L<0
	vf         = getFreeFlowSpeed();
	v          = getSpeed(0);
	x          = getPosition(0);
	v_1        = getSpeed(1);
	x_1        = getPosition(1);
	v_2        = getSpeed(2);
	x_2        = getPosition(2);

	//do have leader
	if (leader 
		&& (leader->getId() > 0) 
		&& leader->getPositionReferenceVeh
		(leader->isUpdated()?1:0,this,0)>0.0 
		&& (leader->getIdCurrentLane() == getIdCurrentLane() ) )
	{
		headway=leader->getPositionReferenceVeh
			(leader->isUpdated()?1:0,this,0);

		if (leader->isUpdated())
		{
			v_leader    = leader->getSpeed(1); // note: leader has already been updated
			x_leader	= leader->getPosition(1); 
			v_1_leader	=leader->getSpeed(2);
			x_1_leader	=leader->getPosition(2); 
			v_2_leader	=leader->getSpeed(3);
			x_2_leader	=leader->getPosition(3); 
		}
		else
		{
			v_leader = leader->getSpeed(0); 
			x_leader = leader->getPosition(0); 
			v_1_leader = leader->getSpeed(1);
			x_1_leader = leader->getPosition(1); 			
			v_2_leader = leader->getSpeed(2);
			x_2_leader = leader->getPosition(2); 
		}

		a_L_leader  = getMAXdec();
		l_leader=leader->getLength();
		gap=headway-l_leader;

		if(ResumeMannual() == true)
		{
			RunNGSIM();
			return;
		}

		d_leader = -(v_leader * v_leader) / (2*a_L_leader);

		//the leading and the current vehicles are both CACC vehicles
		//then apply the CACC model
		if ((getVehType()==CACCveh) && (leader->getVehType()==CACCveh))			
		{		
			desired_range = desired_time_gap * v;	
			// e is the distance difference to the desired distance gap
			e = gap-(desired_time_gap)*v; 
			e_pre_step=(x_1_leader-x_1- l_leader)-(desired_time_gap)*v_1; 				
			v_des=v+0.45*e+0.2*e_pre_step;			
			x_CF=x+v_des*delta_t;
		} 
		// otherwise apply ACC model
		else
		{		
			double asca=0.0, asca_pre_step=0.0, aa=0.0, aa_pre_step=0.0;
			desired_range = desired_time_gap * v;
			aa = -0.07*(v_1-v_1_leader)+0.23*(gap-desired_time_gap*v_1);			
			v_des=v+aa*delta_t;
		} 
	}
	// no leader
	else
	{
		// to free flow
		speed_error = v - vf;
		acceleration = -0.4 * speed_error;
		v_des=v+acceleration*delta_t;			
		x_CF = x+(v_des+v)*delta_t/2;
	}

	//bound free flow
	v_des = MIN(v_des, getFreeFlowSpeed());

	//bound acceleration and deceleration
	acceleration = (v_des - v)/delta_t;
	acceleration = bound(acceleration, a_U, a_L);
	v_des = v + acceleration*delta_t;

	//negative speed avoidance
	if(v_des<0)
	{
		v_des = 0;
		x_CF = x;
	}
	else
	{
		x_CF = x+(v_des+v)*delta_t/2;
	}

	setNewPosition(x_CF,v_des);
}

bool myVehicleDef::ResumeAutomatic()
{
	if(this->Turn2Mannual==true)
	{
		//the current speed is safe
		if(this->getSpeed()<Safe_Speed())
		{
			double lead_speed = ((myVehicleDef*)(leader))->getSpeed();
			//the current speed is less than the leader's speed
			if(this->getSpeed()<lead_speed && this->getSpeed()
			   >MIN_REGAIN_AUTO_SPEED)
			{
				this->Turn2Mannual = false;
				return true;
			}
		}
	}
	return false;
}

bool myVehicleDef::ResumeMannual()
{
	if(this->leader == NULL)
	{
		this->Turn2Mannual = false;
		return false;
	}
	try
	{
		if(this->Turn2Mannual == true )
		{
			if (ResumeAutomatic())
			{
				Turn2Mannual = false;
				return false;
			}
			else
				return true;
		}
		else
		/*if (((myVehicleDef*)this->leader)->getSpeed() > this->getPosition())
			return true;
		else*/
		{
			if(this->getSpeed()>this->Safe_Speed())
			{
				this->Turn2Mannual = true;
				return true;
			}
			else
			{
				this->Turn2Mannual = false;
				return false;
			}
		}
	}
	catch (int errorr)
	{
		this->Turn2Mannual = false;
		return false;
	}
}

double myVehicleDef::Safe_Speed()
{
	double adjust_coeff = 0.9; //prevent collision. could use COMF_LEVEL. BUT THAT SEEMS TOO SOFT
	double lead_speed = ((myVehicleDef*)this->leader)->getSpeed();
	double lead_max_dec = ((myVehicleDef*)this->leader)->getMAXdec();
	double lead_pos =((myVehicleDef*)this->leader)->getPosition();
	double lead_len =((myVehicleDef*)this->leader)->getLength();
	//double buffer =MAX(this->staticinfo.headwayMin*this->getSpeed()*DANGER_GAP,this->getJamGap());
	//buffer = this->getJamGap();
	//safe speed is obtained 
	double low_pos = lead_pos-lead_speed *lead_speed/(2*lead_max_dec);
	//double low_pos = lead_pos + delta_t*(lead_speed + lowspeed)/2;
	low_pos = low_pos - lead_len - this->getJamGap();
	//assuming this vehicle decelerates with a comfortable level
	double comf_pos = this->getPosition()-this->getSpeed() *this->getSpeed()
		/(2*this->getMAXdec()*adjust_coeff);
	/*double comfortable_stop = this->getPosition()+this->getSpeed()*delta_t+
		0.5*this->getMAXdec()*COMF_LEVEL*delta_t*delta_t;*/
	if(comf_pos > low_pos)
		return this->getSpeed()-1;
	else 
		return this->getSpeed()+1;
	double travel_dis = low_pos - this->getPosition();
	return  travel_dis/delta_t;
}

double myVehicleDef::getMAXacc()
{
	double MaxDec = 0.0;
	StaticInfVeh vInfo;

	vInfo = AKIVehGetStaticInf (this->getId());
	MaxDec = vInfo.maxAcceleration;
	return MaxDec;
}

double myVehicleDef::getMAXdec()
{
	double MaxDec = 0.0;
	StaticInfVeh vInfo;

	vInfo = AKIVehGetStaticInf (this->getId());
	// MaxDec = vInfo.maxDeceleration;    // default sign
	MaxDec = (vInfo.maxDeceleration);    // sign changed by XYLu 11_07_14
	return MaxDec;
}

// This gap acceptance method judges the front gap first. If the gap is unacceptable, the driver
// will try to sync its speed and spacing regarding the front vehicle on the target lane by decelerating. 
// After the front gap is satisfied, the driver will judge the lag gap by estimating the possible deceleration 
// the new follower would apply. If the maximum intensity leads to collision, the driver gives up this lane changing
// and slow down to pass this gap.
int myVehicleDef::GapAcceptDecision_Sync_First()
{
	if (VehID == this->debug_track_id)
		VehID = debug_track_id;
	//the intensity of the 
	//desire determines some parameters 
	//in the gap acceptance process
	double desire = this->getLaneChangeDesire();
	A2SimVehicle *vehUp=NULL;
	A2SimVehicle *vehDown=NULL;	
	getUpDown((const A2SimVehicle *&)vehUp, 
		(const A2SimVehicle *&)vehDown, getTargetLane(), 0);
	//no up and down vehicles, gap accepted
	if ((vehDown == NULL||this==vehDown)&&
		(vehUp == this || vehUp == NULL))
		return 0;
	//this gap ac threshold defines when the driver feels the gap is acceptable
	//this should be the comfortable deceleration
	double Gap_AC_Thrd = 0;
	//acceleration for the follower on the target lane after inserted
	//the vehicle assumes that the follower is a manual driving car
	double tau = this->getReactionTime();
	double headway=0;
	double d_leader = 0;
	double min_headway = staticinfo.headwayMin*beta;

	//headway w.r.s to the upstream vehicle
	double up_headway=0;

	//relaxation defines how much deceleration would be applied to calculate the safe distance in NGSIM model
	//1 is the neutral one meaning no affect. 

	double acc_self = 0;
	double acc_up = 0;

	Gap_AC_Thrd = this->getMAXdec()*0.9;

	// boolean indicators to see if upstream and downstream gaps are satisfied
	bool Ok_upstream_gap = true;
	bool Ok_downstream_gap = true;
	if(vehDown!=NULL
		&& this!=vehDown)
	{
		// use the subject vehicle's max dec
		
		d_leader  = - 
			(((myVehicleDef *)vehDown)->getSpeed() 
			*((myVehicleDef *)vehDown)->getSpeed()) / 
			(2*getMAXdec())
			*this->Relaxation;
		//get the headway using the current vehicle as the reference car
		headway = ((myVehicleDef *)vehDown)
			->getPositionReferenceVeh(this);
		// to ensure consistence for fictitious vehicle
		// the position of the leader is replaced with current my pos
		// + current headway
		double current_pos_down = 
			GetPositionRelativeFake(((myVehicleDef *)vehDown),0,true);
		double past_pos_down = 
			GetPositionRelativeFake(((myVehicleDef *)vehDown),tau*beta,true);
			
		if(headway - ((myVehicleDef *)vehDown)->getLength() 
			//- getJamGap()*alpha
			<=0)
		{
			Ok_downstream_gap = false;
		}
		else
		{
			if(this->getGapAcceptanceModel() == ACC_BASED_MODEL)
			{
				if(this->AccGapAccepted(
					getMAXdec(), 
					getMAXacc(), 
					tau*beta, 
					headway, 
					this->alpha*getJamGap(), 
					d_leader, 
					((myVehicleDef *)vehDown)->getLength(), 
					getFreeFlowSpeed(), 
					getSpeed(), 
					getPosition(),
					current_pos_down,
					past_pos_down,
					((myVehicleDef *)vehDown)->getSpeed(), 
					min_headway,
					Gap_AC_Thrd,
					desire) 
					== false)
				{
					Ok_downstream_gap = false;
				}
			}
			else
			{
				if(this->DisGapAccepted(getMAXdec(),
					getMAXacc(), 
					tau*beta, 
					headway, 
					this->alpha*getJamGap(), 
					d_leader, 
					((myVehicleDef *)vehDown)->getLength(), 
					getFreeFlowSpeed(), 
					getSpeed(), 
					getPosition(),
					current_pos_down,
					past_pos_down,
					((myVehicleDef *)vehDown)->getSpeed(), 
					min_headway,
					Gap_AC_Thrd,
					desire) == false)
				{
					Ok_downstream_gap = false;
				}
			}
		}
	}

	if(vehUp!=NULL && this!=vehUp)
	{
		d_leader  = - 
			(this->getSpeed() *this->getSpeed()) 
			/ (2*this->getMAXdec())*this->Relaxation;
		
		up_headway = this->getPositionReferenceVeh(((myVehicleDef *)vehUp));
		//the lane changer has no information about the min-headway of 
		//the follower so assume it is the same
		min_headway = staticinfo.headwayMin;
		/*if(vehUp->getIdCurrentSection() == this->getIdCurrentSection())
		{*/
		if(up_headway - getLength()
			//- getJamGap()*alpha
			<=0)
		{
			Ok_upstream_gap = false;
		}
		else if(((myVehicleDef *)vehUp)->isFictitious()==false
			&& ((myVehicleDef *)vehUp)->getSpeed()<=0
			)
		{
			Ok_upstream_gap = true;
		}
		else
		{
			//use shorter gap mode
			//note: use the follower's free flow speed on the target lane
			double early_pos = this->GetPastPos(tau*beta);
			//the subject car is the leader
			//the other car is the follower
			double current_pos_up = 
				GetPositionRelativeFake(((myVehicleDef *)vehUp),0,false);

			if(this->getGapAcceptanceModel() == ACC_BASED_MODEL)
			{
				if(this->AccGapAccepted(
					getMAXdec(),
					getMAXacc(), 
					getReactionTime()*beta, 
					up_headway, 
					getJamGap()*alpha, 
					d_leader, 
					this->getLength(), 
					((myVehicleDef *)vehUp)->getFreeFlowSpeed(), 
					((myVehicleDef *)vehUp)->getSpeed(), 
					current_pos_up, 
					this->getPosition(), 
					early_pos, 
					this->getSpeed(), 
					min_headway,
					Gap_AC_Thrd,
					desire) == false)
				{
					/*if (VehID == this->getDebugTrackID())
					{
						VehID = VehID;
					}
					double minimun_gap = AnticipatedMinGap(acc_self,
						Gap_AC_Thrd, desire, 
						((myVehicleDef*)vehUp)->getSpeed(),
						up_headway);

					if (minimun_gap < getJamGap())*/
						Ok_upstream_gap = false;
				}		
				else
				{
					Ok_upstream_gap = true;
				}
			}
			else
			{
				if(this->DisGapAccepted(
					getMAXdec(),
					getMAXacc(), 
					getReactionTime()*beta, 
					up_headway, 
					getJamGap()*alpha, 
					d_leader, 
					this->getLength(), 
					((myVehicleDef *)vehUp)->getFreeFlowSpeed(), 
					((myVehicleDef *)vehUp)->getSpeed(), 
					current_pos_up, 
					this->getPosition(), 
					early_pos, 
					this->getSpeed(), 
					min_headway,
					Gap_AC_Thrd,
					desire) == false)
				{
					Ok_downstream_gap = false;
				}
			}
		}
	}

	if(Ok_upstream_gap == true)
	{
		if(Ok_downstream_gap == true)
		{
			return RAMP_LANE_CHANGE_FEASIBLE;
		}
		else
		{
			return RAMP_DECISION_FOLLOW;
		}
	}
	else
	{
		return RAMP_DECISION_SLOW_DOWN;
	}
}

int myVehicleDef::gapAccepted
	(const A2SimVehicle *vehUp, const A2SimVehicle *vehDown, double desire)
{
	//this gap ac threshold defines when the driver feels the gap is acceptable
	//this should be the comfortable deceleration
	double Gap_AC_Thrd = 0;
	//acceleration for the follower on the target lane after inserted
	//the vehicle assumes that the follower is a manual driving car
	double tau = this->getReactionTime();
	double headway=0;
	double d_leader = 0;
	//relaxation defines how much deceleration would be applied to calculate the safe distance in NGSIM model
	//1 is the neutral one meaning no affect. 
	if(vehUp!=NULL && vehUp->isFictitious() == false && this!=vehUp)
	{
		Gap_AC_Thrd = ((myVehicleDef *)vehUp)->getMAXdec();
		d_leader  = - (this->getSpeed() *this->getSpeed()) / (2*this->getMAXdec())*this->Relaxation;
		headway = this->getPosition()-((myVehicleDef *)vehUp)->getPosition();	
		double min_headway = ((myVehicleDef *)vehUp)->staticinfo.headwayMin;
		if(vehUp->getIdCurrentSection() == this->getIdCurrentSection())
		{
			if(headway - getLength()<=0)
				return -1;
			//not in the same section but since we have hard coded that between 50 meter cannot change so it is fine
			//use shorter gap mode
			//note: use the follower's free flow speed on the target lane
			double early_pos = 
				this->GetPastPos(tau*beta);
			double new_pos = BaseCfModel(getMAXdec(), 
				getMAXacc(), 
				tau*beta, 
				headway, 
				((myVehicleDef *)vehUp)->getJamGap()*alpha, 
				d_leader, 
				this->getLength(), 
				((myVehicleDef *)vehUp)->getFreeFlowSpeed(), 
				((myVehicleDef *)vehUp)->getSpeed(), 
				((myVehicleDef *)vehUp)->getPosition(), 
				this->getPosition(), 
				early_pos, 
				this->getSpeed(), min_headway);

			double acc_up = 
				(((new_pos - ((myVehicleDef *)vehUp)->getPosition())
					/this->delta_t)
				-((myVehicleDef *)vehUp)->getSpeed())*2/delta_t;

			if (acc_up < Gap_AC_Thrd*desire*0.99)
			{
				//if this criterion cannot be satisfied 
				//then adopt the gap based criterion
				//actually the deceleration gap can hardly be satisfied
				if(headway-((myVehicleDef*)vehUp)->getJamGap()-this->getLength()<0)
				{
					return -1;
				}
			}
		}
	}
	
	if(vehDown!=NULL && vehDown->isFictitious() == false && this!=vehDown)
	{
		Gap_AC_Thrd = this->getMAXdec();
		d_leader  = - 
			(((myVehicleDef *)vehDown)->getSpeed() *((myVehicleDef *)vehDown)->getSpeed()) / 
			(2*((myVehicleDef *)vehDown)->getMAXdec())*this->Relaxation;
		headway = -this->getPosition()+((myVehicleDef *)vehDown)->getPosition();
		double min_headway = staticinfo.headwayMin*beta;
		if(vehDown->getIdCurrentSection() == this->getIdCurrentSection())
		{		
			if(headway - ((myVehicleDef *)vehDown)->getLength()<=0)
				return 1;
			//use shorter gap mode
			double early_pos = 
				((myVehicleDef *)vehDown)->GetPastPos(tau*beta);
			double new_pos_self = BaseCfModel(getMAXdec(), getMAXacc(), tau*beta, 
				headway, this->alpha*getJamGap(), d_leader, 
				((myVehicleDef *)vehDown)->getLength(),
				getFreeFlowSpeed(), getSpeed(), getPosition(),
				((myVehicleDef *)vehDown)->getPosition(), 
				early_pos,
				((myVehicleDef *)vehDown)->getSpeed(), min_headway);

			double acc_self = 
				(((new_pos_self - getPosition())/delta_t)-getSpeed())
				*2/delta_t;

			if (acc_self < Gap_AC_Thrd*desire*0.99)
				return 1;
		}
	}

	return 0;
}

double myVehicleDef::PosCfSkipGap(const A2SimVehicle* potential_leader, 
								  bool apply_creep_speed)
{
	if(apply_creep_speed)
	{
		PosCfSkipGap(potential_leader);
	}
	else
	{
		if(potential_leader!=NULL)
		{
			double a_L = this->getMAXdec()*COMF_LEVEL;		//comfortable dec
			double speed = MAX(0,this->getSpeed()+a_L*delta_t);

			return this->getPosition() + 
				(this->getSpeed()+speed)*delta_t/2;
		}
		return this->getPosition();
	}
}

//this function returns the position of the vehicle if it slow down with a comfortable deceleration
double myVehicleDef::PosCfSkipGap(const A2SimVehicle* potential_leader)
{
	//apply a comfortable deceleration to a speed that is slightly lower than the the potential leader
	if(potential_leader!=NULL)
	{
		double a_L = this->getMAXdec()*COMF_LEVEL;		//comfortable dec
		double speed = this->getSpeed()+a_L*delta_t;

		//double target_speed = MAX(0, ((myVehicleDef*)potential_leader)->getSpeed()-5);
		//speed = MAX(target_speed, speed);
		
		speed = MAX(CREEP_SPEED, speed);

		return this->getPosition() + (this->getSpeed()+speed)*delta_t/2;
	}
	return this->getPosition();
}

/***************************************************************************************/
//	cntract() Used in PosCf
/***************************************************************************************/

void cntract(double sample_t, double mag_rate, double e_rate, double a_val, double b_val, 
            double *int_val, double *int_val_d)
{
  double temp=0.0, temp1=0.0, rate=0.0;

  double SIGN1(double);

  temp=a_val-b_val;
  if (mag_rate <= fabs(temp))
     rate = mag_rate;
  else
     rate = (double)fabs(temp);
  temp1=SIGN1(temp);
  *int_val = a_val - temp1*rate*sample_t*
             (exp(e_rate*temp)-exp(-e_rate*temp))/(exp(e_rate*temp)+exp(-e_rate*temp));
  *int_val_d =  - temp1*rate*sample_t*4.0*
             e_rate/((exp(e_rate*temp)+exp(-e_rate*temp))*(exp(e_rate*temp)+exp(-e_rate*temp)));                                 
}


double SIGN1(double x)
  {
   static double tmp=0.0000001;
   if(x>=tmp) 
     return 1.0;
   else if (x<tmp && x>-tmp) 
     return 0.0;
   else
     return -1.0;
   }         

// base car following model
double myVehicleDef::BaseCfModel
	(double a_L, double a_U, double tau, 
	 double headway, double jamGap, double d_leader, 
	 double l_leader, double vf, double v, double x, 
	 double x_leader, double x_leader_steps_early,
	 double lead_v, double min_headway
	 )
{
	double target_pos  =0;
	return this->BaseCfModel
		( a_L,  a_U,  tau, 
		 headway,  jamGap,  d_leader, 
		 l_leader,  vf,  v,  x, 
		 x_leader,  x_leader_steps_early,
		 lead_v,  min_headway,  target_pos
		);
}

// base car following model
double myVehicleDef::BaseCfModel
			(double a_L, double a_U, double tau, 
			 double headway, double jamGap, double d_leader, 
			 double l_leader, double vf, double v, double x, 
			 double x_leader, double x_leader_steps_early,
			 double lead_v, double min_headway, double &target_pos
			)
{

	if(VehID == 29)
		VehID = VehID;
	double x_CF = 0;
	double ths = 5;
	target_pos = 0;
	//Hwasoo model is a discrete time model
	if(false)  //Hwasoo's model will not be used
	{
		
		//eq(11) - Safety constraints
		//modification by Dali Wei
		//Hwasoo model's safety constraint is too strict especially with the delay factor tau
		//which would easily add extra 30-60 meters to the safety buffer
		double tmp=a_L * tau*a_L * tau;
		double dist_safety  = 0;		
		if ((tmp) >= 2*a_L*( headway - l_leader - jamGap + d_leader))
		{			
			dist_safety = this->delta_t * (
								a_L * tau + sqrt(
												 MAX(
													  0.0001, 
													  tmp - 
														2*a_L*
														(headway-l_leader-
														jamGap + d_leader)
													)
												) 
									);		//eq(11) - safety constraint
			
			dist_safety=MAX(0.0,dist_safety);  // added on 12_21_14
			
		}

		//Eq. (6) Modified by DW with numUpdate. 
		//Also, instead of using reference position, should use position
		double dist_CF = 
			x_leader_steps_early - 
			l_leader - jamGap; // this specifies the basic rule of Newell model. 

		// Added back by XYLu 11_07_14
		//Coasting distance
		double x_CS=v*delta_t;
				
		if(lead_v>v+ths && headway > v*tau+jamGap+l_leader && headway <2*(v*tau+jamGap+l_leader))
			dist_CF=MAX(x_CS, dist_CF);	
		
		//eq(8) - Free-flow distance
		double dist_Freeflow = vf * delta_t;
		
		//eq(7) - Max acceleration distance
		double dist_MaxAcc = 
			v * delta_t + 0.5*a_U * delta_t * delta_t;
		//dist_MaxAcc = v * delta_t + a_U * delta_t * delta_t;
		
		//eq(10) - Lower bound for driving distance
		double x_L = x + MAX(v * delta_t + 0.5*a_L*delta_t*delta_t, 0);  // a_l is max-decel of the subject veh
			
		//Finding minimum upper bound		
		double x_U = x + MIN(dist_Freeflow, MIN(dist_MaxAcc, dist_safety));  // default;   XYLu 11_08_14

		x_U = MIN(x_U, dist_CF);

		// Modified by XYLu 11_07_14
		x_CF = MAX(x_U, x_L);  // The original
		//due to the leader problem at connectors , we have to specify the following hard break
		//although this is not realistic
		//x_CF = MIN(x_CF, x_leader-l_leader-jamGap);
		x_CF = MAX(x,x_CF);       // changed on 02_25_15
	}
	else if(ModelApplied.compare("IDM")==0)
	{
		//acceleration term
		double acc_term = pow(v/vf,this->delta);
		//this jam gap has already timed coefficient of alpha
		double s_0 = jamGap;
		double T = min_headway;

		double comfort_b = this->getMAXdec()*COMF_LEVEL;
		double a = this->getMAXacc();//*1.5; // times * 1.5 because it will never reach max_acc if do not times 

		double safe_term = s_0+
			MAX(0, 
				v*T+
				v*(v-lead_v)
				/(2*sqrt(-a*comfort_b))								 
				);
		
		safe_term = pow(safe_term/(headway-l_leader),2);

		double acc = a
			*(1-acc_term-safe_term);

		acc = bound(acc, a_U, a_L);

		if ((v+acc*delta_t)<0)
			x_CF = x-0.5*pow(v, 2)/acc;
		else
			x_CF = x+v*delta_t+0.5*pow(delta_t, 2)*acc;

		target_pos = x_CF;
	}
	else if(this->ModelApplied.compare("SNGSIM")==0
		||ModelApplied.compare("NGSIM")==0
		|| true)   //ngsim model is the default if users input error code
	{	
		double maxDec = a_L;
		double maxAcc = a_U;
		double reaction_time = tau;
		//this coefficient is suggested to be larger than 
		//the maximum dec of the current vehicle for stability purpose
		double b_estimate = 
			getEstimateLeaderDecCoeff()
			*maxDec;
		double desired_headway = min_headway;
		double theta = this->getGippsTheta()*reaction_time;
		
		double v_after_tau = GippsDecelerationTerm(
			maxDec,reaction_time,theta,x_leader,x,jamGap,
			l_leader,v,lead_v,b_estimate); 
		
		// The maximum speed based on Gipps safety criterion
		double max_a = MIN(maxAcc, 
		MAX(maxDec, (v_after_tau-v)/reaction_time));

		// The acceleration based on Newell rule
		double newell_a = MIN(maxAcc, 
		MAX(maxDec, 
			((headway - l_leader - jamGap)/desired_headway-v)
			/(desired_headway/2)));   //Newell original paper Eq. (9)

		// free acc
		double min_a = maxAcc*(1-v/vf);

		double current_acc = (getSpeed(0)-getSpeed(1))/delta_t;
		double acc_target = MIN(MIN(min_a, newell_a), max_a);
		double acc = 
			current_acc + (acc_target-current_acc)
			/this->getAccSmoothCoef();
		double vel = v+acc*delta_t;
		x_CF = x+ (vel+v)/2*delta_t;

		double target_v = v+acc_target*delta_t;
		target_pos = x+ (vel+target_v)/2*delta_t;
	}
	return x_CF;
}

//----------------------------------------------------------------------------------------------------------
// Car-following functions
//----------------------------------------------------------------------------------------------------------

// Function PosCf
// returns base car-following distance
//
double myVehicleDef::PosCf
	(const A2SimVehicle* leader, int shortGap, double beta, double alpha, double relaxation)
{
    double  x_L=0.0;
	double	x_U=0.0;
	double	x_CF=0.0; //x_CFMax, x_CS,;	
	double a_U   = 0.0;			//a_U>0 
	double a_U_tmp = 0.0;
	double a_L        = 0.0;			//a_L<0
    double jamGap     = 0.0;
    double vf         = 0.0;
    double v          = 0.0;
    double x          = 0.0;
	double v_1          = 0.0;
    double x_1          = 0.0;
	double v_2          = 0.0;
    double x_2          = 0.0;
    double   reaction    = 0.0;
    double tau        = 0.0;
	const double ths=5;
//	double ths=5;
	int numUpdate=0; //		= int(reaction/delta_t);
	double l_leader=0.0;
	double a_L_leader=0.0;
	double d_leader=0.0;
    double v_leader=0.0; // note: leader has already been updated  
	double x_leader=0.0; 
    double v_1_leader=0.0;
	double x_1_leader=0.0; 
    double v_2_leader=0.0;
	double x_2_leader=0.0; 
    double headway=0.0;
	double dist_CF=0.0;
	double dist_Freeflow=0.0;
	double dist_MaxAcc=0.0;
	double x_CS=0.0;
	
	double  e=0.0, a_des=0.0;	
	double e_pre_step=0.0, e_pre_step_2=0.0;
	double gap=0.0, v_des=0.0;

	//for ACC & CACC
	double desired_time_gap=1.1;
	int delaystep=0;
	int lookat=552;
	double ref_pos_front=0.0;
	double speed_error=0.0;
	double acceleration=0.0;
	double sc_accel = 0.0;
	double desired_range = 0.0;
	double range_error = 0.0;
	double range_rate = 0.0;	
	int Has_Leader=0;

	// parameter change for short gap mode

    //a_U        = getAcceleration();		//a_U>0
	a_U        = getMAXacc();			//a_U>0
    //a_L        = getDeceleration();		//a_L<0
	a_L        = getMAXdec();			//a_L<0
    jamGap     = getJamGap();
    vf         = getFreeFlowSpeed();
    v          = getSpeed(0);
    x          = getPosition(0);

    reaction   = getReactionTime();

    tau        = reaction;
	numUpdate	= int(reaction/delta_t);
	
	if (leader!=NULL && leader!=this)
	{
		Has_Leader=1;	

		if(VehID == debug_track_id)
			VehID=VehID;

		//if the leader has updated, then use its previous step
		if (leader->isUpdated())
			ref_pos_front= leader->getPositionReferenceVeh(1,this,0);	
		else
			ref_pos_front= leader->getPositionReferenceVeh(0,this,0);	

		l_leader = leader->getLength();
		headway = ref_pos_front;
		gap=headway-l_leader;
	
		// the update judgment has been put input the reloaded getspeed
		v_leader = ((myVehicleDef*)leader)->getSpeed(); 
		x_leader = ((myVehicleDef*)leader)->getPosition(); 
		
		double leader_past_pos
			= ((myVehicleDef*)leader)->GetPastPos(getReactionTime()*beta);

		//if the leader is not on the same section, the x_leader
		//does not equal (x+headway)
		if(abs(x_leader - (ref_pos_front+x)) > 0.1
			|| leader->getIdCurrentSection()!=this->getIdCurrentSection())
		{
			x_leader = ref_pos_front + x;
			//the past pos the leader changes to:
			leader_past_pos = x + 
				 ((myVehicleDef*)leader)->
				 getPastPositionReferenceVehs
				 (getReactionTime()*beta,this,0);
		}

		a_L_leader  = ((myVehicleDef *)leader)->getMAXdec();
		d_leader    = - (v_leader * v_leader) 
			/ (2*a_L_leader)*(shortGap==1?relaxation:1);
		
		x_CF = BaseCfModel(a_L, a_U, reaction*beta, 
			headway, jamGap*alpha, d_leader, 
			l_leader, vf, v, x, x_leader, 
			leader_past_pos,
			((myVehicleDef*)leader)->getSpeed(),
			staticinfo.headwayMin*beta);
		
		/*if (leader->getIdCurrentLane() == this->getIdCurrentLane() &&
			leader->getIdCurrentSection() == this->getIdCurrentSection())
		{
			x_CF = MIN(x_CF, -this->getJamGap()+ ((myVehicleDef *)leader)->getPosition()-leader->getLength());
		}*/
		
	}
	else  
	{
		// No leader and accelerates to the free flow speed mildly
		x_CF = x + delta_t*v + 
			0.5*a_U*(1-pow(v/vf, delta))*pow(delta_t,2);
		//Has_Leader=0;
		//
		////double v_des=0.0;
		//dist_MaxAcc= v * delta_t + a_U * delta_t * delta_t;
		//dist_Freeflow = vf * delta_t;
		//x_CF = x + MIN(dist_Freeflow, dist_MaxAcc);
		
	}

    return x_CF;
}

//this function determines if the driver decides to do a lane-changing
//what is the target lane, and how strong is the desire.
//both discretionary and mandatory are combined together
//using a weight coefficient
bool myVehicleDef::NeedLC()
{

	ResetDesires();
	//discretionary 
	NeedDlc();
	//mandatory options are exclusive
	if(NeedRampLc()==false)
		if(NeedLc4Turning()==false)
			NeedLcExit();
	//combine all options
	//within combine desires, we set lc type
	return CombineLCDesires();
}

double myVehicleDef::getPosition(int state)
{
	double x = 0.0;
	x  = A2SimVehicle::getPosition(state);
	//this is probably because the Aimsum did not save that many states for use
	//so we must use our own list
	if (state>1 && this->ModelApplied.compare("NGSIM")==0)
	{
		if(this->position_queue.size()>=state)
			return this->position_queue.
				at(this->position_queue.size()-state);
		else
		{
			if(this->position_queue.size()>0)
				return this->position_queue.front();
			else
				return this->getPosition();
		}
	}
	return x;
}

void myVehicleDef::setNewPosition(double pos,double velocity)
{	
	if(this->ModelApplied.compare("NGSIM")==0)
	{
		position_queue.push_back(this->getPosition());
		if(this->position_queue.size()
			>this->getReactionTime()/delta_t+10)
			this->position_queue.pop_front();
	}
	A2SimVehicle::setNewPosition(pos,MAX(velocity,0));
}


double myVehicleDef::getPosition()
{
	int state= 0;
	double v = 0.0;
	state     = isUpdated() ? 1 : 0;
	v       = A2SimVehicle::getPosition(state);
	return v;
}
// get updated speed for a 
double myVehicleDef::getSpeed()
{
	int state= 0;
    double v = 0.0;
	state     = isUpdated() ? 1 : 0;
	v       = A2SimVehicle::getSpeed(state);
    return v;
}

double myVehicleDef::getSpeed(int state)
{
	return A2SimVehicle::getSpeed(state);
}

const A2SimVehicle* myVehicleDef::getLeader()
{
	const A2SimVehicle* leader = (A2SimVehicle::getLeader());

	if (leader)
	{
		if(leader->getId() > 0.0
		&&leader->isFictitious() == false
		/*&&(leader->getPositionReferenceVeh(0,this,0)>0.0)*/
		/*&& (leader->getPositionReferenceVeh(1,this,1)>0.0)*/
		&& (leader->getIdCurrentLane() == getIdCurrentLane() )
		&&  leader->getIdCurrentSection()== getIdCurrentSection())
		{
			return leader;
		}
		goto 
			GET_REAL_LEADER;
	}

GET_REAL_LEADER:
	
	const A2SimVehicle* real_leader = 
		(A2SimVehicle::getRealLeader());
	if (real_leader)
	{
		if (((myVehicleDef*)real_leader)
			->getPositionReferenceVeh(this)<=0)
			return NULL;
		return real_leader;
	}
	else
	{
		return NULL;
	}
	
	//else
	//{
	//	const A2SimVehicle* real_leader = (A2SimVehicle::getRealLeader());
	//	if (real_leader && real_leader->isFictitious() == false &&
	//		real_leader->getId() > 0.0)
	//	{
	//		return real_leader;
	//	}
	//	else
	//	{
	//		int sectionid = this->getIdCurrentSection();
	//		int junctionid = this->getIdNextJunction();
	//		if(this->isCurrentLaneNode())
	//		{
	//			
	//		}
	//		else
	//		{	
	//			int nextsection = AKIVehInfPathGetNextSection(VehID,sectionid);
	//			A2KSectionInf section_info = AKIInfNetGetSectionANGInf(sectionid);
	//			int numveh_next_section = AKIVehStateGetNbVehiclesSection(nextsection,true);
	//			int nb_turnings = section_info.nbTurnings;
	//			int turning_id = -1;
	//			//find the turning that is used for the vehicle to get onto the next section
	//			for(int i=0;i<nb_turnings;i++)
	//			{
	//				if (AKIInfNetGetIdSectionANGDestinationofTurning(sectionid,i) == nextsection)
	//				{
	//					turning_id = i;
	//					break;
	//				}
	//			}
	//			if(turning_id>=0)
	//			{
	//				int des_fromlane =  AKIInfNetGetDestinationFromLaneofTurning(turning_id, turning_id);
	//				int des_tolane = AKIInfNetGetDestinationToLaneofTurning(turning_id, turning_id);
	//				int or_fromlane = AKIInfNetGetOriginFromLaneofTurning(turning_id, turning_id);
	//				int or_tolane = AKIInfNetGetOriginToLaneofTurning(turning_id, turning_id);
	//				
	//				//get the vehicle with the smallest position on the des_from to des_to lane, and use that vehicle as the leader
	//				double min_pos = 5000000000;
	//				for(int j=0;j<numveh_next_section;j++)
	//				{
	//					InfVeh temp_info = AKIVehStateGetVehicleInfSection(nextsection,j);
	//					double position = temp_info.CurrentPos;
	//					if (temp_info.numberLane<=des_fromlane && temp_info.numberLane>=des_tolane)
	//					{
	//						if(position<min_pos)
	//							leader_info = temp_info;
	//					}
	//				}				
	//			}
	//		}
	//		return NULL;
	//	}
	//}
}

double myVehicleDef::getFreeFlowSpeed()
{
	//these vehicles on the ramp
	/*if(this->isCurrentLaneOnRamp()
		|| this->getIdCurrentSection() == FeederOnRamp
		|| (this->getIdCurrentSection() == END_SECTION && this->getIdCurrentLane()==1 
			&& this->getPosition()<NO_CHANGE_ZONE_LENGTH)
	  )
		return RAMP_SPEED;
	else
		return this->staticinfo.maxDesiredSpeed/3.6;*/
	return A2SimVehicle::getFreeFlowSpeed();
}

int myVehicleDef::DetermineLcOrMergeOrCoop()
{
	//first decide if lane change is necessary
	if (NeedLC())
	{
		return this->setMode(BCF);
	}
	/*else if (NeedMCF()!=0)
	{
		return this->setMode(MCF) ;
	}*/
	else if(NeedCoop())
	{
		return this->setMode(CCF);
	}
	else
		return this->getMode();

}

int myVehicleDef::Determine2ndLcAfterLc()
{
	//first decide if lane change is necessary
	if (NeedLC())
	{
		return this->setMode(BCF) ;
	}
	else
	{
		if(isAfterLaneChangeFinish())
			return this->setMode(CF);
		else
			return getMode();
	}
}

int myVehicleDef::determineCoopOrLc()
{
	if(NeedLC())
	{
		return setMode(BCF);
	}
	else if(!NeedCoop()) //do not need cooperation any longer
	{
		return setMode(CF);
	}
	else
	{
		return getMode();
	}
}

int myVehicleDef::DetermineReceiveOrLc()
{
	if(NeedLC())
	{
		return setMode(BCF);
	}
	else if(!NeedReceive()) 
	{
		return setMode(CF);
	}
	else
	{
		return getMode();
	}
}

// the vehicle tries to merge into the main-lane. We try to avoid this case
// by either making side lanes
// so this function is not implemented here
void myVehicleDef::UpdateMergeCf()
{
	return;
}

// Behavior function about before a lane change after the decision is made
// 1) either try to sync its speed with respect to the leader on the target lane
//    or slow down to skip the current gap
void myVehicleDef::UpdateBeforeLaneChangeCf()
{
	if(VehID == debug_track_id)
		VehID = VehID;
	
	if(this->LCType == EXIT) //type 1: exit or turning
	{
		int decision = ExitCfDecision(); 
		//vehicles choose to sync with the leader on the target lane or slow to skip this gap
		if(decision == EXIT_DECISION_SLOW_DOWN)
		{
			this->BeforeExitorTurningLcSlowDown();
		}
		else if(decision == EXIT_DECISION_FOLLOW)
		{
			this->BeforeExitorTurningLcSync();
		}
		else
		{
			this->UpdateLc();
		}
	}
	else if(this->LCType == TURNING) // turning use the same flow as EXIT
	{
		int decision = ExitCfDecision(); 
		//vehicles choose to sync with the leader on the target lane or slow to skip this gap
		if(decision == EXIT_DECISION_SLOW_DOWN)
		{
			this->BeforeExitorTurningLcSlowDown();
		}
		else if(decision == EXIT_DECISION_FOLLOW)
		{
			this->BeforeExitorTurningLcSync();
		}
		else
		{
			this->UpdateLc();
		}
	}
	//else if(this->LCType == ELC) //type 2: emergent
	//{

	//}
	else if(this->LCType == DLC) //type 3: optional 4 improving speed
	{
		//vehicles choose to sync with the leader on the target lane or slow to skip this gap
		int decision = DLCCFDecision(); //get CF options before lane changing
		if(decision == DLC_DECISION_SLOW_DOWN)
		{
			this->BeforeDLcSlowDown();
		}
		else if (decision == DLC_DECISION_FOLLOW)
		{
			this->BeforeDLcSync();
		}
		else
		{
			this->UpdateLc();
		}
	}
	else if(this->LCType == RAMP) //type 4: on-ramp merging
	{
		//vehicles choose to sync with the leader on the target lane or slow to skip this gap
		int ramp_lc_decision = RampCfDecision();
		if(ramp_lc_decision == RAMP_DECISION_SLOW_DOWN)
		{
			this->BeforeOnRampLcSlowDown();
		}
		else if(ramp_lc_decision == RAMP_DECISION_FOLLOW)
		{
			this->BeforeOnRampLcSync();
		}
		else if(ramp_lc_decision == RAMP_DECISION_NORMAL_FOLLOW)
		{
			this->updateRegularCf();
		}
		else
		{
			this->UpdateLc();
		}
	}
}

void myVehicleDef::UpdateEmergentCf()
{
	throw std::logic_error("The method or operation is not implemented.");
}

void myVehicleDef::UpdateDuingLaneChangeCf()
{
	throw std::logic_error("The method or operation is not implemented.");
}

// Update the position after lane changing
void myVehicleDef::UpdateAfterLaneChangeCf()
{
	double new_beta = 
		(1-beta)/(double)ACF_Steps*(double)ACF_Step+beta;
	double new_alpha = 	
		(1-alpha)/(double)ACF_Steps*(double)ACF_Step+alpha;
	double new_relaxation = 
		(1-Relaxation) /(double)ACF_Steps*(double)ACF_Step+Relaxation;
	double x_CF = PosCf
		(this->leader, 1, new_alpha, new_beta, new_relaxation);
	ACF_Step++;	
	setNewPosition(x_CF, 
		2*(x_CF - this->getPosition()) / delta_t-this->getSpeed());
	if(ACF_Step >= ACF_Steps)
	{
		ACF_Step = 0;
		setMode(CF);
	}
}

// Apply cooperative car following model to give gap for lane changers
// as if it has two leaders
// but it only applies a comfortable deceleration
void myVehicleDef::updateCoopCf()
{
	//follow the current leader in a normal way
	double nextPos = this->PosCf
		(leader,0,1,1,1);
	//CoopRequester is one of the vehicle in the 
	if(this->CoopRequester != NULL)
	{
		double nextPosCoopVeh = this->PosCf
			(CoopRequester,
			 1,this->alpha, this->beta, this->Relaxation);
		nextPos = std::min(nextPos,nextPosCoopVeh);
		//but it will apply a deceleration that is beyond the comfortable
		double v = 2*(nextPos - this->getPosition()) / delta_t-this->getSpeed();
		double acc  = (v-this->getSpeed())/delta_t;
		if( acc < this->getMAXdec()*COMF_LEVEL)
		{
			v = MAX(0,this->getSpeed()+delta_t*acc);
		}
		nextPos = this->getPosition()+(v+this->getSpeed())*delta_t/2;
	}
	setNewPosition(nextPos, 
		2*(nextPos - this->getPosition()) / delta_t-this->getSpeed());
	
}

// A vehicle cuts in and this function defines how the new follower follows this new leader

void myVehicleDef::UpdateReceiveCf()
{
	double nextPos = this->PosCf(leader,1,this->alpha, this->beta, this->Relaxation);
	setNewPosition(nextPos, 
		2*(nextPos - this->getPosition()) / delta_t-this->getSpeed());
}

// This is Hwasoo's original code; not fully understand the logic behind it
// Get conflict lane    
// find merging lane, +1: left lane, -1: right lane, 0:  no conflict lane
// Always set the rightmost lane up to merging lane
//--------------------------------------------------------------------------
int myVehicleDef::GetConflictLane(myVehicleDef *vehicle)
{
	A2KSectionInf sinfo;
	int aid=0;
	int nbturnings=0;
	int origfromlane[2]={0,0};
	int origtolane[2]={0,0};
	int destfromlane[2]={0,0};
	int desttolane[2]={0,0};
	int orig_nlanes[2]={0,0};
	int dest_nlanes[2]={0,0};
	int result=0;

	int currentLane = vehicle->getIdCurrentLane();
    aid = getIdCurrentSection();
    if (aid > 0)
	{
		sinfo=AKIInfNetGetSectionANGInf(aid);
        nbturnings=sinfo.nbTurnings;

        for (int i=0; i<nbturnings; i++)
		{
			destfromlane[i]=AKIInfNetGetDestinationFromLaneofTurning(aid, i);
			desttolane[i]=AKIInfNetGetDestinationToLaneofTurning(aid, i);
			origfromlane[i]=AKIInfNetGetOriginFromLaneofTurning(aid, i);
			origtolane[i]=AKIInfNetGetOriginToLaneofTurning(aid, i);
			dest_nlanes[i]=desttolane[i]-destfromlane[i]+1;
			orig_nlanes[i]=origtolane[i]-origfromlane[i]+1;

			//if number of destination lane is less than the original lanes 
			//then a merge happens
			if (dest_nlanes[i] < orig_nlanes[i])
			{
				if (currentLane == 1) {result = -1; break;}	//rightmost lane : merging lane
			}
		}
		
		/*
		for (int i=0; i<nbturnings; i++)
		{
			if (dest_nlanes[i] < orig_nlanes[i])	//Rightmost lane Merging case
			{
				if (lane==origfromlane[i]+1) {result=-1; break;}
				else if (lane==origfromlane[i]) {result=1; break;}
			}
			
			if (dest_nlanes[i] < orig_nlanes[i])	//Merging case
			{
				if (lane==origtolane[i]) {result=-1; break;}
				else if (lane<origtolane[i]) {result=1; break;}
			}
			
		}*/
	}

	return result;
}

bool myVehicleDef::isAfterLaneChangeFinish()
{
	if(this->leader == NULL || this->leader->isFictitious())
	{
		this->ACF_Step = ACF_Steps;
		return true;
	}
	if(this->ACF_Steps == this->ACF_Step)
	{
		return true;
	}
	return false;
}

bool myVehicleDef::NeedReceive()
{
	throw std::logic_error("The method or operation is not implemented.");
}

//determine if the car still wants to lane change or return to car following
int myVehicleDef::determineGapOrGiveup()
{
	if(NeedLC() == true)
	{	
		return setMode(BCF);
	}
	else
	{
		return setMode(CF);
	}
}


int myVehicleDef::RampCfDecision()
{
	//if this vehicle has a direct leader than 
	// just let it follow the leader instead of seeking gaps
	if (this->leader!=NULL
		&& this->leader->isFictitious() == false)
	{
		if(this->AllowUnsequentialMerging() == false)
			return RAMP_DECISION_NORMAL_FOLLOW;
	}
	return GapAcceptDecision_Sync_First();
}

//before lane change on Ramp slow down to skip the current gap 
void myVehicleDef::BeforeOnRampLcSlowDown()
{
	//run synchronization based lane change
	A2SimVehicle *vehUp=NULL;
	A2SimVehicle *vehDown=NULL;	
	getUpDown((const A2SimVehicle *&)vehUp, (const A2SimVehicle *&)vehDown, 
		this->getTargetLane(), 0);
	//now slow down to catch the next gap with a mild deceleration
	double posSlow = PosCfSkipGap(vehUp);
	//the current leader
	double posFollowCurrentLeader = 0;	
	if(this->leader == NULL) //if no leader, then pay attention to the end of the ramp
	{
		posFollowCurrentLeader = PosCf2EndofRamp();
	}
	else //with leader follow leader no leader
	{
		posFollowCurrentLeader = PosCf(this->leader,1,beta,alpha, Relaxation);
	}
	double x = getPosition(0);
	setNewPosition(MIN(posSlow,posFollowCurrentLeader),
		(MIN(posSlow,posFollowCurrentLeader) - x) / delta_t*2-this->getSpeed());
}

//before lane change on ramp sync the speed with respect to the leader on the target lane
void myVehicleDef::BeforeOnRampLcSync()
{
	//run synchronization based lane change
	A2SimVehicle *vehUp=NULL;
	A2SimVehicle *vehDown=NULL;	
	getUpDown((const A2SimVehicle *&)vehUp, 
		(const A2SimVehicle *&)vehDown, this->getTargetLane(), 0);
	double x_CF_Sync = PosCf(vehDown, 1, beta, alpha, Relaxation);
	double x_CF_NoSync = 0;
	if(this->leader == NULL)
	{
		x_CF_NoSync = PosCf2EndofRamp();
	}
	else
	{
		x_CF_NoSync = PosCf(this->leader,1,beta,alpha, Relaxation);
	}
	double x=getPosition(0);	
	setNewPosition(MIN(x_CF_NoSync,x_CF_Sync),
		(MIN(x_CF_NoSync,x_CF_Sync) - x) / delta_t*2-this->getSpeed());
}

// define slow down behavior before a mandatory exit /turning LC.
void myVehicleDef::BeforeExitorTurningLcSlowDown()
{
	//run synchronization based lane change
	A2SimVehicle *vehUp=NULL;
	A2SimVehicle *vehDown=NULL;	
	getUpDown((const A2SimVehicle *&)vehUp, 
		(const A2SimVehicle *&)vehDown, 
		this->getTargetLane(), 0);
	//now slow down to catch the next gap with a mild deceleration
	double posSlow = PosCfSkipGap(vehUp, true);
	//the current leader
	double posFollowCurrentLeader = 0;	
	double posFollowEnd = PosCf2EndofExitTurning();
	if(this->leader != NULL) //if no leader, then pay attention to the end of the ramp
	{
		posFollowCurrentLeader = 
			PosCf(this->leader,1,beta,alpha, Relaxation);
	}
	
	double x = getPosition(0);
	posFollowCurrentLeader = MIN(posFollowCurrentLeader,posFollowEnd);
	setNewPosition(MIN(posSlow,posFollowCurrentLeader),
		(MIN(posSlow,posFollowCurrentLeader) - x) / delta_t*2-this->getSpeed());
}

// define SYNC behavior before a mandatory exit /turning LC.
void myVehicleDef::BeforeExitorTurningLcSync()
{
	//run synchronization based lane change
	A2SimVehicle *vehUp=NULL;
	A2SimVehicle *vehDown=NULL;	
	getUpDown((const A2SimVehicle *&)vehUp, 
		(const A2SimVehicle *&)vehDown, this->getTargetLane(), 0);
	double x_CF_Sync = PosCf(vehDown, 1, beta, alpha, Relaxation);
	//the current leader
	double x_CF_NoSync = 0;	
	double posFollowEnd = PosCf2EndofExitTurning();
	if(this->leader != NULL) //if no leader, then pay attention to the end of the ramp
	{
		x_CF_NoSync = PosCf(this->leader,1,beta,alpha, Relaxation);
	}
	double x=getPosition(0);	
	x_CF_NoSync = MIN(x_CF_NoSync,posFollowEnd);
	setNewPosition(MIN(x_CF_NoSync,x_CF_Sync),
		(MIN(x_CF_NoSync,x_CF_Sync) - x) / delta_t*2-this->getSpeed());
}

//Car following behavior when approaching the end of an on-ramp
double myVehicleDef::PosCf2EndofRamp()
{
	//simplify assume that there is a car at the end of the ramp
	return BaseCfModel(this->getMAXdec(),
					   this->getMAXacc(),
					   this->getReactionTime()*beta,
					   this->distance2EndAccLane(), //the distance to the end of the ramp
					   this->getJamGap()*alpha,0,0,
					   this->getFreeFlowSpeed(),this->getSpeed(),this->getPosition(),
					   this->posEndAccLane(),//assume there is a car at the end of the ramp with speed of zero
					   this->posEndAccLane(), //the position of the car steps earlier
					   0,
					   staticinfo.headwayMin);
}

//get the distance to the end of the on ramp
double myVehicleDef::distance2EndAccLane()
{
	return this->distance2EndOnRamp();
}

//get the length of the acceleration lane
double myVehicleDef::posEndAccLane()
{
	int secId = this->getIdCurrentSection();
	A2KSectionInf sectionInfo = AKIInfNetGetSectionANGInf(secId);
	return sectionInfo.distance_OnRamp;
}

double myVehicleDef::getLaneChangeDesire()
{
	return this->desireLC;
}


// lane change is feasible
void myVehicleDef::UpdateLc()
{
	double nextPos = 0;
	A2SimVehicle *vehUp=NULL;
	A2SimVehicle *vehDown=NULL;	
	getUpDown((const A2SimVehicle *&)vehUp, (const A2SimVehicle *&)vehDown, 1, 0);
	nextPos = this->PosCf(vehDown, 1, this->beta, this->alpha, this->Relaxation);
	if((nextPos-getPosition())/delta_t*2-this->getSpeed()>30)
	{
		nextPos = nextPos;
	}
	applyLaneChanging(vehDown!=this?vehDown:NULL,
		this->getTargetLane(),nextPos,
		(nextPos-getPosition())/delta_t*2-this->getSpeed());
	this->setMode(ACF);
}

//decide if the car needs a lane change on ramp
//in our model, the on-ramp is modeled as a side lane on the main section
//which is closer to the real case
//so this function tells if the vehicle is on the side lane
bool myVehicleDef::NeedRampLc()
{
	int secId = this->getIdCurrentSection();
	A2KSectionInf sectionInfo = AKIInfNetGetSectionANGInf(secId);
	if(sectionInfo.nbSideLanes<=0)
		return false;

	if(this->isCurrentLaneOnRamp() == true )
	{
		double incentive = 
			CalculateDesireForce(1, distance2EndAccLane(),
			this->getSpeed(),true);

		this->setLaneChangeDesireForce(incentive, 0);
		this->setMandatoryType(RAMP);
		return true;
	}
	
	return false;
}


double myVehicleDef::GetAdditionalDlcDesire(int target_lane)
{
	double vf= getFreeFlowSpeed();
	double d_scan = getDLCScanRange();   // scan maximum 50m ahead
	int n_scan = getDLCScanNoCars();   // scan maximum 5 vehicles ahead
	double v_target = isLaneChangingPossible(target_lane) ?
		getAverageSpeedAHead(target_lane, d_scan, n_scan) : 0;
	double v_ahead = getAverageSpeedAHead(0, d_scan, n_scan);
	return MAX(0,(v_target - v_ahead) / vf);
}

bool myVehicleDef::NeedDlc()
{
	//if(getLCTime()<=20) return false;

    int target_lane = 0;
    double vf= 0.0;
    double lc_prob=0.0;
	double lc_prob_right=0.0;
    double d_scan    = getDLCScanRange();   // scan maximum 50m ahead
	//double n_scan    = 5;   // scan maximum 5 vehicles ahead// change by XYLu on 07_22_14 to avoid type problem
    int n_scan = getDLCScanNoCars();   // scan maximum 5 vehicles ahead
    double phi = 2.17F;
    bool result=false;
	double v_left=0.0;
	double v_ahead=0.0;
	double v_right=0.0;

	//int ind_lc = 0;	//index for LC (0: no LC, 1: left LC, -1:right LC)
	
    vf= getFreeFlowSpeed();
    v_left = isLaneChangingPossible(1) ?
        getAverageSpeedAHead(1, d_scan, n_scan) : 0;
    v_ahead = getAverageSpeedAHead(0, d_scan, n_scan);
    v_right = isLaneChangingPossible(-1) ?
        getAverageSpeedAHead(-1, d_scan, n_scan) : 0;

	if(VehID == this->getDebugTrackID())
		VehID = VehID;

	// if no vehicle is inside the scan range set the speed as free flow speed
	if(v_left<0	 || v_left>vf) v_left=vf;
	if(v_right<0 || v_right>vf) v_right=vf;
	if(v_ahead<0 || v_ahead>vf) v_ahead=vf;

	// change lane to left lane(1)
    if ( v_left > v_right 
		&& v_left > v_ahead
		&& isLaneChangingPossible(1)
		&& !IsOnramp(LEFT)
		)
	{
		//MyPrintString("Try to left LC (%i)", getId());
		//ind_lc = 1;
		//lc_prob = (v_left - v_ahead) * delta_t / (vf * phi);
		lc_prob = (v_left - v_ahead) / (vf)/** delta_t*/ ;

		/*if (mybehavioralModel::sampleUniformDist() < lc_prob)
        {*/
			result=true;
			setnLC(1);
			setTargetLane(LEFT);  // set left lane as target lane
        //}
    }
    else if (v_right > v_left 
		&& v_right > v_ahead 
		&& isLaneChangingPossible(-1)
		&& !IsOnramp(RIGHT))
	{
		//MyPrintString("Try to right LC (%i)", getId());
		//ind_lc = -1;
		//lc_prob_right = (v_right - v_ahead) * delta_t / (vf * phi);
		lc_prob_right = (v_right - v_ahead) / vf*this->getRightDLCCoeff()/** delta_t*/;

		/*if (mybehavioralModel::sampleUniformDist() < lc_prob)
        {*/
			result=true;
			setnLC(-1);
			setTargetLane(RIGHT);  // set right lane as target lane
        //}
    }
	else 
	{
		setTargetLane(NOCHANGE);
		return false;
	}

    if(result)
    {
		/*
		int exit_lane_from,exit_lane_to;
		getIdTargetLanes4NextTurning(0, exit_lane_from, exit_lane_to);
		int lane=getIdCurrentLane();

		if (lane < exit_lane_from || lane > exit_lane_to)
		{
			double d_exit  = getPositionNextTurning() - getPosition(0);

			if((d_exit < 100) && lc_prob < 0.8)
			//if (lc_prob<0.7)
			{
				 setTargetLane(0); // do not change lane
				 result=false;
			}*/
		/*
		double d_exit = getPositionNextTurning() - getPosition(0);
		if (d_exit < 100 && lc_prob < 0.7)
		{
			setTargetLane(0);
			result = false;
		}*/
		int exit_lane_from, exit_lane_to;
		getIdTargetLanes4NextTurning(0, exit_lane_from, exit_lane_to);
		int curLane = getIdCurrentLane();

		double d_exit = getPositionNextTurning() - getPosition(0);
		if (d_exit < this->getDLCForbidZoneBeforeExit())
		{
			goto NO_LANE_CHANGE;
		}

		if (curLane + getTargetLane() < exit_lane_from 
			|| curLane + getTargetLane() > exit_lane_to) 
		{
			lc_prob *= this->getPenaltyDLCNoExitLane();
			lc_prob_right *= this->getPenaltyDLCNoExitLane();
		}

		//DLC is also not allowed 
		//when the vehicle is at the original section
		//with the same zone distance
		if(this->getIdCurrentSection() ==
			this->getSourceSection()&&
			this->getPosition()<this->getDLCForbidZoneBeforeExit())
		{
			goto NO_LANE_CHANGE;
		}

		this->setLaneChangeDesireOption(lc_prob, lc_prob_right);
	}
    return result;

NO_LANE_CHANGE:
	setTargetLane(0); 
	result = false;
	return result;
}

bool myVehicleDef::NeedLcExit()
{
	bool result;
	double a=2.0F;
//    int exit_lane_from, exit_lane_to;
	int n_lc;
    int lane = getIdCurrentLane();

	/*
	StaticInfVeh vehinfo;
	int exitcenid=vehinfo.centroidDest;
	if (exitcenid==323) {return true;}	//Exit centroid id = 323
	else {return false;}*/

	double d_exit  = getPositionNextExit() - getPosition(0);
	n_lc    = getNbLaneChanges2ReachNextExit();
	
	/*
	if(getId() == 1)
	{
		MyPrintString("Need LC number = (%i)", n_lc);
		MyPrintString("Left Distance = (%f)", d_exit);
	}*/

	
	if(n_lc!=0)
	{
		//result = (d_exit <= (20+getE() + a*(abs(n_lc) - 1)*getT()));
		result = (d_exit <= (20+getE() + a*abs(n_lc)*getT()));
		setxExit(getPositionNextExit()-
			(20+getE() + a*(abs(n_lc) - 1)*getT()));
		if(n_lc>0) 
		{
			setTargetLane(LEFT);}
		else 
		{setTargetLane(RIGHT);}
		/*
		if(getId() == 1)
		{
			MyPrintString("LC to Exit (%i)", getId());
		}*/
	}

	else
	{
		result=false;
	}

	//Check if current Section is on-ramp
	//MyPrintString("CurrentSection : (%i)", getIdCurrentSection());
	//if (getIdCurrentSection() == 314) {result=false;}
	
	if(result) 
	{
		setnLC(n_lc);
		this->setMandatoryType(MANDATORY);
	}

	//if(result) MyPrintString("Current step: needs_lc_to_exit (%i)", getId());
	//if(result && getId() == 8)	MyPrintString("Current step: needs_lc_to_exit (%i)", getId());

    return result;
}

//check if lane change is needed for turning
bool myVehicleDef::NeedLc4Turning()
{
	if (VehID == debug_track_id)
		VehID = debug_track_id;

	bool result = false;
	static double a=2.0F;
	int n_lc=0;  // number of lane changes required to have turning
	int lane = 0;
	int conflictLane=0;
	int index = 0;
	int exit_lane_from=0;
	int exit_lane_to=0;	
	double d_exit =0.0;
	double req_exit =0.0;

	lane = getIdCurrentLane();
	conflictLane = GetConflictLane(this);
	getIdTargetLanes4NextTurning(index, exit_lane_from, exit_lane_to);

	// check if left lane changing is needed
	if (lane < exit_lane_from)
	{
		d_exit  = getPositionNextTurning() 
			- getPosition(0);
		n_lc = exit_lane_from-lane;
		//use the same criterion as the ramp lane change for this one
		setLaneChangeDesireForce
			(CalculateDesireForce(n_lc, d_exit, this->getSpeed(),false)
			,0);
		result = true;
		/*req_exit = 20 + getE() 
			+ a * abs(n_lc) * getT();
		result = (d_exit <= req_exit);*/
		//if (result)	
		//	setTargetLane(1); // set left lane as target lane
		//setxExit(getPositionNextTurning()-req_exit);
	}
	// check  if right lane changing is needed
	else if (lane > exit_lane_to)
	{
		d_exit  = getPositionNextTurning() - getPosition(0);
		n_lc    = exit_lane_to-lane;
		setLaneChangeDesireForce
			(0,
			CalculateDesireForce(n_lc, d_exit, this->getSpeed(),false));
		result = true;
		//req_exit = 20 + getE() + a * abs(n_lc) * getT();
		//result = (d_exit <= req_exit);
		//if (result)	
		//	setTargetLane(-1); // set right lane as target lane
		//setxExit(getPositionNextTurning()-req_exit);
	}
	else {
		// no lane changing is needed
		result = false;
	}

	if(result) 
	{
		setnLC(n_lc);
		setMandatoryType(TURNING);
	}
	return result;
}


int myVehicleDef::NeedMCF()
{
	return 0;
}

//see if the other vehicles need cooperation
bool myVehicleDef::NeedCoop()
{
	this->CoopRequester = NULL;
	//if both lanes have cooperation requesters
	//select the one that is the closest
	A2SimVehicle *vehUp=NULL;
	A2SimVehicle *vehDown=NULL;	
	getUpDown((const A2SimVehicle *&)vehUp, 
		(const A2SimVehicle *&)vehDown, LEFT, 0);
	if(vehDown!=NULL
		&& ((myVehicleDef*)vehDown)->getMode() == BCF
		&& ((myVehicleDef*)vehDown)->getTargetLane() == RIGHT)
	{
		if(this->Willing2Coop(((myVehicleDef*)vehDown)))
		{
			this->CoopRequester = (myVehicleDef*)vehDown;
		}
	}

	getUpDown((const A2SimVehicle *&)vehUp, 
		(const A2SimVehicle *&)vehDown, RIGHT, 0);
	if(vehDown!=NULL
		&& ((myVehicleDef*)vehDown)->getMode() == BCF
		&& ((myVehicleDef*)vehDown)->getTargetLane() == LEFT)
	{
		//find the leader 
		double dis = ((myVehicleDef*)vehDown)->getPositionReferenceVeh
			(this);
		if(this->Willing2Coop( (myVehicleDef*)vehDown))
		{
			if(this->CoopRequester!=NULL)
			{
				if(dis < CoopRequester->getPositionReferenceVeh(this))
				{
					this->CoopRequester = (myVehicleDef*)vehDown;
				}
			}
			else
			{
				this->CoopRequester = (myVehicleDef*)vehDown;
			}
		}
	}
	
	if(this->CoopRequester!=NULL)
	{
		return true;
	}
	else
		return false;
}

// wiling2Coop defines if the driver is willing to cooperate given the 
// status of the coop-requester
bool myVehicleDef::Willing2Coop(myVehicleDef *coop_veh)
{
	if(this->getPoliteness() 
		> this->getRandomPoliteness())
	{
		if(!this->IsCoopEffectMuch(coop_veh))
			return true;
	}
	return false;
}

// this function made a decision as to slow down of sync to get lane change 
// when exiting
int myVehicleDef::ExitCfDecision()
{
	return GapAcceptDecision_Sync_First();
}

void myVehicleDef::setLaneChangeDesire(double incentive)
{
	this->desireLC = bound(incentive,1,0);
}

//set the desire of the lane change on ramp
void myVehicleDef::setLaneChangeDesireForce(double incentive_left,
											double incentive_right)
{
	this->desireLC_force_left = incentive_left>=0?incentive_left:0;
	this->desireLC_force_right = incentive_right>=0?incentive_right:0;
}

//set the desire of the optional lane-change
void myVehicleDef::setLaneChangeDesireOption(double incentive_left,
											 double incentive_right)
{
	desireLC_option_left = bound(incentive_left,1.0,0.0);
	desireLC_option_right = bound(incentive_right,1.0,0.0);
}

//combine all desires of lane change and determine the final decision
bool myVehicleDef::CombineLCDesires()
{
	if(isCurrentLaneNode() == true) 
		goto No_Lane_Change;

	//four options:
	//force left right
	//option left right
	if(desireLC_force_left+
		desireLC_force_right+
		desireLC_option_left+
		desireLC_option_right<=0.0001)
	{
		goto No_Lane_Change;
	}
	else
	{
		double left_desire
			= desireLC_force_left+
			this->discretionary_LC_weight*desireLC_option_left;
		double right_desire
			= desireLC_force_right+
			this->discretionary_LC_weight*desireLC_option_right;

		//prioritize the mandatory lane change
		if(desireLC_force_left>0)
		{
			right_desire = 0;
			if (desireLC_option_left == 0)
			{
				desireLC_option_left = 
					GetAdditionalDlcDesire(LEFT);
				left_desire = desireLC_force_left+
					desireLC_option_left*discretionary_LC_weight;
			}
		}
		else if(desireLC_force_right>0)
		{
			left_desire = 0;
			if(desireLC_option_right == 0)
			{
				desireLC_option_right = 
					GetAdditionalDlcDesire(RIGHT);
				right_desire = desireLC_force_right+
					desireLC_option_right*discretionary_LC_weight;
			}
		}

		double desire = std::_cpp_max(left_desire,right_desire);
		int target_lane = 
			left_desire>right_desire?LEFT:RIGHT;

		//if lane change is impossible return 0
		if(this->isLaneChangingPossible(target_lane>0?LEFT:RIGHT)==false)
			goto No_Lane_Change;

		//if this desire if larger than the driver's normal desire
		if(desire>this->getLaneChangeDesireThrd())
		{
			int type = MANDATORY;
			if(left_desire>right_desire)
				type = desireLC_force_left>0?this->getMandatoryType():OPTIONAL;
			else
				type = desireLC_force_right>0?this->getMandatoryType():OPTIONAL;

			//desire is less than meaning it is not that urgent
			// then the time between lc changes must be satisfied
			double punish_time_factor_4_swaping = 3.0;
			if((AKIGetCurrentSimulationTime()
				- this->getLastLCTime()<=this->getMinTimeBtwLcs()
				&& (this->getLastLCTarget() == target_lane||
					this->getLastLCTarget() == NOCHANGE))		
				||
				(AKIGetCurrentSimulationTime()
				- this->getLastLCTime()<=
				punish_time_factor_4_swaping*this->getMinTimeBtwLcs()
				&& this->getLastLCTarget() == -target_lane)	
				//lane change again to the original lane is not encouraged
				&& desire<1)
			{
				goto No_Lane_Change;
			}

			setLCType(type);
			setTargetLane(target_lane);
			this->setLaneChangeDesire(desire);
			//no need to set number lane changes here
			//it has already been set in NeedLc
			return true;
		}
		else
		{
			goto No_Lane_Change;
		}
	}

No_Lane_Change:
	setLCType(0);
	setTargetLane(NOCHANGE);
	this->setLaneChangeDesire(0);
	setnLC(0);
	return false;

}

void myVehicleDef::ResetDesires()
{
	this->desireLC_force_left = 0.0;	
	this->desireLC_force_right = 0.0;	
	this->desireLC_option_left = 0.0;	
	this->desireLC_option_right = 0.0;	

}

//////////////////////////////////////////////////////////////////////////
//calculate the desire of mandatory lane change based on the distance left
//the speed, and the number lanes to be crossed
//two set of methods for ramp and off-ramp/turning
//////////////////////////////////////////////////////////////////////////
double myVehicleDef::CalculateDesireForce(int n_lc, double d_exit, 
										double speed, bool is_for_on_ramp)
{

	//determine the urgency
	double dis2End = d_exit;
	double time2End = d_exit/speed;
	double para1 = (dis2End)/this->E/abs(n_lc);
	double para2 = (time2End)/this->T/abs(n_lc);

	// for exiting the main-lane
	if(!is_for_on_ramp)
	{
		//determine the urgency
		dis2End = d_exit;
		time2End = d_exit/speed;
		para1 = (dis2End)/this->getOffRampE()/abs(n_lc);
		para2 = (time2End)/this->getOffRampT()/abs(n_lc);
	}
	
	return Bound_Function(MAX(1-para1, 1-para2));
	//return bound(MAX(1-para1, 1-para2), 1,0);
}

void myVehicleDef::SetInitialVal()
{
	this->setLastLCTime(0);
}

//////////////////////////////////////////////////////////////////////////
//override the default one to add the time of lane change
void myVehicleDef::applyLaneChanging
	(const A2SimVehicle *vehDown, 
	const int targetLane, const double newpos, const double newspeed)
{
	this->setLastLCTarget(targetLane);
	A2SimVehicle::applyLaneChanging
		(vehDown, targetLane, newpos,newspeed);
	this->setLastLCTime(AKIGetCurrentSimulationTime());
}

//Newtonian equation to position
double myVehicleDef::NewtonianPos(double v, double a_f, double x)
{
	return x+v*delta_t+0.5*a_f*pow(delta_t,2);
}

double myVehicleDef::GetPastPos(double reaction_time)
{
	double leader_past_pos
		= getPosition
			((int)(reaction_time/this->delta_t) - 1);
	if(isUpdated() == true)
	{
		leader_past_pos
			= getPosition((int)(reaction_time/this->delta_t));
	}
	return leader_past_pos;
}

// this function get the position of the fictitious vehicle
// using the headway of the ref vehicle 
double myVehicleDef::GetPositionRelativeFake(myVehicleDef* fake_veh, 
							   double reaction_time_fake,
							   bool downstream)
{
	// if the fictitious vehicle is downstream
	if (downstream == true)
	{
		double past_headway = fake_veh->getPastPositionReferenceVehs
			(0,this,reaction_time_fake);
		return past_headway+this->getPosition();
	}
	else //the fake vehicle is upstream and this vehicle is the leader
	{
		double past_headway = getPastPositionReferenceVehs
			(reaction_time_fake,fake_veh,0);
		return this->getPosition()-past_headway;
	}
}

// this function get the relative distance before the specified reaction times
// taking the reference vehicle's position is zero
// that means the reference vehicle must be downstream
double myVehicleDef::getPastPositionReferenceVehs
	(double reaction_time_ref, myVehicleDef* ref_veh, 
	 double reaction_time_this)
{
	int step_ref = (int)(reaction_time_ref/this->delta_t);
	if(ref_veh->isUpdated() == true)
	{
		step_ref
			=  (int)(reaction_time_ref/this->delta_t)+1;
	}

	int step_this = (int)(reaction_time_this/this->delta_t);
	if(isUpdated() == true)
	{
		step_this
			=  (int)(reaction_time_this/this->delta_t)+1;
	}
	return A2SimVehicle::getPositionReferenceVeh(step_this,
		ref_veh, step_ref);	
}

// override the base class to automatically include state
// this can be used only for the current states
double myVehicleDef::getPositionReferenceVeh(myVehicleDef* ref_veh)
{
	int ref_state = ref_veh->isUpdated()?1:0;
	int this_state = isUpdated()?1:0;
	
	return A2SimVehicle::getPositionReferenceVeh(this_state,ref_veh,
		ref_state);
}

// get minimum gap if the follower uses the desired deceleration and 
// the lane changer uses the anticipated acceleration after lc
double myVehicleDef::AnticipatedMinGap(double acc_self,
									   double Gap_AC_Thrd, double desire, 
									   double up_speed,
									   double headway)
{
	double expected_dec = acc_self;//ideally this would be the case
	//but we apply a more conservative condition that 
	//assumes the lane changer 
	//would not apply 
	//acceleration
	expected_dec = MIN(0,expected_dec);
	double desired_dec_for_follower = Gap_AC_Thrd*desire;
	double delta_v = this->getSpeed() - 
		up_speed;
	double delta_a = expected_dec - 
		desired_dec_for_follower;
	double minimun_time = 0;
	double minimun_gap = 0;
	if(delta_a>0)
	{
		minimun_time = MAX(0, -delta_v/delta_a);
		minimun_gap = 
			headway - this->getLength() + 
			delta_v*minimun_time + 
			0.5*delta_a*minimun_time*minimun_time;
	}
	else
	{
		//the time when the shortest gap occurs is either the time zero
		//or the time when the follower stops
		double time_follower_stop = -up_speed
			/desired_dec_for_follower;
		minimun_gap = 
			headway - this->getLength() + 
			MIN(0,
			delta_v*time_follower_stop + 
			0.5*delta_a*time_follower_stop*time_follower_stop
			);
	}
	return minimun_gap;
}

//Tell if the distance is too close so cooperation does not make much sense
bool myVehicleDef::IsCoopEffectMuch(myVehicleDef *coop_veh)
{
	double dis 
		= coop_veh->getPositionReferenceVeh(this)-coop_veh->getLength();
	//if(dis < this->jam_gap * this->alpha)
	if(dis < 0)
	{
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
// Decide what cf rules should be applied before after a DLC desire is 
// generated. 
//////////////////////////////////////////////////////////////////////////
int myVehicleDef::DLCCFDecision()
{
	return GapAcceptDecision_Sync_First();
}

//////////////////////////////////////////////////////////////////////////
//slow down to give up the current gap in Discretionary LC
//this case is rare because the reason for DLC is to seek higher speed
//////////////////////////////////////////////////////////////////////////
void myVehicleDef::BeforeDLcSlowDown()
{
	this->BeforeOnRampLcSlowDown();
}

//////////////////////////////////////////////////////////////////////////
//Sync the current speed with the target leader for Discretionary LC
//////////////////////////////////////////////////////////////////////////
void myVehicleDef::BeforeDLcSync()
{
	this->BeforeOnRampLcSync();
}

void myVehicleDef::setLaneChangeDesireThrd(double val)
{
	this->lane_change_prob = bound(val,1,0);
}

void myVehicleDef::setRightDLCCoeff(double val)
{
	this->right_dlc_coeff_=bound(val,1,0);
}

void myVehicleDef::setReactionTime(double val)
{
	this->reaction_time_ = MAX(0.01, val);
}

//////////////////////////////////////////////////////////////////////////
// this is the implementation of Hwasoo's 
bool myVehicleDef::DisGapAccepted(double a_L, double a_U, double tau, 
								  double headway, double jamGap, 
								  double d_leader, 
								  double l_leader, double vf, double v, 
								  double x, 
								  double x_leader, 
								  double x_leader_steps_early,
								  double lead_v, double min_headway,
								  double Gap_AC_Thrd,
								  double desire)
{
	if(lead_v > v)
	{
		if (x_leader-x-l_leader-jamGap>=0)
		{
			return true;
		}	
	}
	double theta = tau*this->getGippsTheta();
	double b_estimate = a_L*this->getEstimateLeaderDecCoeff();
	return GippsGap(a_L,tau,theta,x_leader,
		x,jamGap,l_leader,v,lead_v,b_estimate);
}

bool myVehicleDef::AccGapAccepted(double a_L, double a_U, double tau, 
								  double headway, double jamGap, 
								  double d_leader, 
								  double l_leader, double vf, double v, 
								  double x, 
								  double x_leader, 
								  double x_leader_steps_early,
								  double lead_v, double min_headway,
								  double Gap_AC_Thrd,
								  double desire)
{
	double new_pos=0;
	BaseCfModel(
		a_L, 
		a_U, 
		tau, 
		headway, 
		jamGap, 
		d_leader, 
		l_leader, 
		vf, 
		v, 
		x,
		x_leader,
		x_leader_steps_early,
		lead_v, 
		min_headway,
		new_pos); //assuming the downstream vehicle has the same reaction time

	double new_v = ((new_pos - x)*2/delta_t)-v;
	double acc = (new_v - v)/delta_t;

	if(acc < Gap_AC_Thrd*desire)
	{
		return false; //downstream gap is not satisfied
	}	
	return true;
}

//////////////////////////////////////////////////////////////////////////
/////Ciuffo, Biagio, Vincenzo Punzo, 
//and Marcello Montanino. 
//"Thirty Years of Gipps' Car-Following Model: 
//Applications, Developments, and New Features." 
//Transportation Research Record: 
//Journal of the Transportation Research Board 
//2315 (2012): 89-99. (Eq. 3)
//////////////////////////////////////////////////////////////////////////
double myVehicleDef::GippsDecelerationTerm
	(double maxDec,double reaction_time,double theta, 
	double x_leader,double x,double jamGap, double l_leader,
	double v,double lead_v,double b_estimate)
{
	double value_in_sqrt = 
		pow(maxDec*(reaction_time+theta),2)-
		maxDec*(2*(x_leader-x-jamGap-l_leader)-
		v*reaction_time-lead_v*lead_v/b_estimate);

	double v_after_tau = 0;
	if(value_in_sqrt>0)
	{
		v_after_tau =maxDec
			*(reaction_time+theta)+
			sqrt(value_in_sqrt);
	}
	return v_after_tau;
}

//////////////////////////////////////////////////////////////////////////
//gipps safety gaps
//Gipps, Peter G. "A behavioural car-following model for 
//computer simulation." 
//Transportation Research Part B: Methodological 15, no. 2 (1981): 105-111.
//Eq. and (5)
//////////////////////////////////////////////////////////////////////////
bool myVehicleDef::GippsGap(double maxDec,double reaction_time,double theta, 
							  double x_leader,double x,double jamGap, double l_leader,
							  double v,double lead_v,double b_estimate)
{
	double leader_stop_x 
		= x_leader-pow(lead_v,2)/2/b_estimate;

	//assuming no action was taken for reaction time +theta
	double follower_stop_x
		= x-pow(v,2)/2/maxDec+v*(reaction_time+theta); 

	if (leader_stop_x-follower_stop_x>l_leader+jamGap)
	{
		return true;
	}
	else
		return false;
}

bool myVehicleDef::HwasooGap(double maxDec,double reaction_time,double theta, 
							  double x_leader,double x,double jamGap, double l_leader,
							  double v,double lead_v,double b_estimate)
{
	double leader_stop_x 
		= x_leader-pow(lead_v,2)/2/b_estimate;

	double follower_stop_x
		= x-pow(v,2)/2/maxDec;

	if (leader_stop_x-follower_stop_x>l_leader+jamGap)
	{
		return true;
	}
	else
		return false;
}

double myVehicleDef::PosCf2EndofExitTurning()
{
	double distance2End = 0;
	double posEnd = 0;
	if(this->getMandatoryType() == EXIT)
	{
		posEnd = this->getPositionNextExit();
	}
	else
	{
		posEnd = this->getPositionNextTurning();
	}
	distance2End = posEnd - this->getPosition();
	//simplify assume that there is a car at the edge of exiting or turning
	return BaseCfModel(this->getMAXdec(),
		this->getMAXacc(),
		this->getReactionTime()*beta,
		distance2End, //the distance to the end of the ramp
		this->getJamGap()*alpha,0,0,
		this->getFreeFlowSpeed(),this->getSpeed(),this->getPosition(),
		posEnd,//assume there is a car at the end of the ramp with speed of zero
		posEnd, //the position of the car steps earlier
		0,
		staticinfo.headwayMin);
}

double myVehicleDef::Bound_Function(double param1)
{
	return exp(param1-0.5)/(1+exp(param1-0.5));
}
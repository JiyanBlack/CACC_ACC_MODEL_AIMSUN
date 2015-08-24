/****************************************************************

     To modify the demand at the beginning of the simulation run
	 It will not work this way!!!

****************************************************************/


#define Car_Type  53  
#define HOVveh_Type  17859
#define HIAveh_Type  21350  // 811
#define ACCveh_Type  23053  // 812
#define CACCveh_Type 23057  // 813
#define RAMP_ID 23554
#define MAINLANE 23552


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

	const unsigned short *ACC_PercentString = AKIConvertFromAsciiString( 
		"GKExperiment::ACC_Percent");
	acc_percent = ANGConnGetAttributeValueDouble( ANGConnGetAttribute( ACC_PercentString ), expriment_id );

	const unsigned short *simStepAtt = AKIConvertFromAsciiString( 
		"GKExperiment::simStepAtt");
	ANGConnSetAttributeValueDouble( ANGConnGetAttribute( simStepAtt ), expriment_id ,0.1);

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

	const unsigned short *on_RAMP_PercentString = 
		AKIConvertFromAsciiString( 
		"on_ramp_flow");
	on_ramp = (int)ANGConnGetAttributeValueDouble
		( ANGConnGetAttribute(on_RAMP_PercentString ), exp_id );

	const unsigned short *off_RAMP_PercentString = 
		AKIConvertFromAsciiString( 
		"off_ramp_flow");
	off_ramp = (int)ANGConnGetAttributeValueDouble
		( ANGConnGetAttribute(off_RAMP_PercentString ), exp_id );

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

//Modify demand by OD matrix
void ModifyMatrixDemand(double acc_percent, double cacc_percent)
{
	//all of the sections
	int slice_num = AKIODDemandGetNumSlicesOD(1);
	int vtp_num = AKIVehGetNbVehTypes();

	//from mainlane to mainlane
	int total_through = 8000;
	//off_ramp from mainlane
	int off_ramp = 0; //off_ramp vehicles can only be manual driven
	//on ramp to mainlane and exit from mainlane
	int on_ramp = 1000;//on_ramp vehicles can only be manual driven

	//read demand
	read_volumn(total_through,on_ramp,off_ramp);

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
				A2KSectionInf section_info = 
					AKIInfNetGetSectionANGInf(section_id);
				//find the on-ramp for the test network
				if (section_info.nbCentralLanes == 1)
				{
					on_ramp_centroid = iId;
				}
				else
				{
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
	}

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
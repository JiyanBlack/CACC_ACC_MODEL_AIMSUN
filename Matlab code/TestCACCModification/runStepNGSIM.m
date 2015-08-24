%% ngsim model

function [pos, vel, error, acc, rule, dist_safety_out]...
        = runStepNGSIM(pos_follower, ...
                       speed_follower, ....
                       pos_leader, ...
                       speed_leader,...
                       vehLen, ...
                       leader_past_pos, ...
                       deltaT,maxAcc,maxDec,...
                       reaction_time,...
                       jamGap,...
                       vf)
maxDec = -maxDec;
reaction_time = 1;
tmp=maxDec * reaction_time*maxDec * reaction_time;
dist_safety  = 0;		
headway = pos_leader-pos_follower;
d_leader = speed_leader*speed_leader/2/maxDec;
dist_safety_out = 0;
if ((tmp) >= 2*maxDec*( headway - vehLen - jamGap + d_leader))	
    dist_safety = deltaT * (...
							maxDec * reaction_time + sqrt(...
                                                    	 max(...
                                                         0.0001, ...
                                                         tmp - ...
                                                            2*maxDec*...
														(headway-vehLen-...
														jamGap + d_leader)...
													)...
												) ...
									);	
	dist_safety_out = dist_safety;		
	dist_safety=max(0.0,dist_safety);  
end
dist_CF = leader_past_pos - vehLen - jamGap; 
dist_Freeflow = vf * deltaT;
dist_MaxAcc = ...
			speed_follower * deltaT + 0.5*maxAcc * deltaT * deltaT;
x_L = pos_follower + ...
            max(speed_follower * deltaT + 0.5*maxDec*deltaT*deltaT, 0);  				
x_U = pos_follower + min(dist_Freeflow, min(dist_MaxAcc, dist_MaxAcc));  
x_U = min(x_U, dist_CF);
pos = max(x_U, x_L);  
pos = max(pos_follower,pos);    
vel = (pos - pos_follower)/deltaT*2-speed_follower;
acc = (vel - speed_follower)/deltaT;

if pos==x_L % max dec mode
    rule = 0;
else
    if pos == pos_follower+dist_MaxAcc %max acc mode
        rule = 1;
    elseif pos == pos_follower+dist_safety %safety mode        
        rule = 2;
    elseif pos == dist_CF     % Newell rule         
        rule = 3;
    else 
        rule = 4; %free flow mode
    end
end

error=0;
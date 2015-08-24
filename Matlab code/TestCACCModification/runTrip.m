function [speedMat,timePointList,...
    disHeadwayMat, headwayMat, posMat,accMat, ruleMat, dissafetyMat] ...
            = runTrip(realTraj, nFollower, modelCode)

    desireHeadway = 1.6; %initial parameter
    vehLen = 6;
    jamGap = 2;
    maxAcc=2;
    maxDec=2;
    freeFlowSpeed = 40;
    reaction_Time = desireHeadway;

    colSpeed = 2; % Column 1: time in [s]
    colTime = 1; % Column 2: speed in  [m/s].
    
    %deltaT interval
    deltaT = 0.1;    
    
    % use linear interpolation to change the sampling interval to 0.1 second
    [timePointList,leadSpd] = nearestSampling(deltaT, ...
        realTraj(:,1), realTraj(:,2));        
    timeLen = length(leadSpd);
    
    % create matrix to store the status of the following vehicles
    speedMat = zeros(timeLen, nFollower+1);
    posMat = zeros(timeLen, nFollower+1);
    errorMat = zeros(timeLen, nFollower+1);
    disHeadwayMat = zeros(timeLen, nFollower+1);
    headwayMat = zeros(timeLen, nFollower+1);
    accMat = zeros(timeLen, nFollower+1);
    ruleMat = zeros(timeLen, nFollower+1);
    dissafetyMat = zeros(timeLen, nFollower+1);
    
    %copy the leader profile to speedMat
    speedMat(:,1) = leadSpd(:,1);
    % determine the position of the leader using blastic numerical scheme
    posMat(1,1)=0;
    for i = 2:timeLen
        posMat = posFromSpd(1, i, deltaT, posMat, speedMat);
        accMat = accFromSpd(1, i, deltaT, accMat, speedMat);
    end
    
    % at the begining, every car is in its equilibiurm position
    % seperated by a jam gap
    speedMat(1,:) = leadSpd(1,1);
    leadSpeed = speedMat(1,1);
    if(modelCode~=3)
        equDisHeadway = desireHeadway*leadSpeed + jamGap+vehLen; 
    else
        equDisHeadway = desireHeadway*leadSpeed + jamGap+vehLen; 
    end
    for i=2:nFollower+1
        posMat(1, i) = posMat(1, i-1)-equDisHeadway;
    end
    errorMat(1,:)=0;
    
    %% start run
    for iTime = 1:timeLen-1
        for iFollower = 2:nFollower+1
            % deal with error in previous steps
            if iTime == 1
                errorPre = 0;
                errorPrePre = 0;
                current_acc = 0;
            elseif iTime == 2
                errorPre = errorMat(iTime-1, iFollower);
                errorPrePre=0;
                current_acc = accMat(iTime-1, iFollower);
            else
                errorPre = errorMat(iTime-1, iFollower);
                errorPrePre = errorMat(iTime-2, iFollower);
                current_acc = accMat(iTime-1, iFollower);
            end
            if modelCode == 3 || modelCode == 4 || ...
                    modelCode == 5 % prepare for previous state up to reaction time
                if iTime<reaction_Time/deltaT+1
                    leader_past_pos = ...
                        -leadSpeed*(reaction_Time/deltaT+1-iTime);
                else                    
                    leader_past_pos = ...
                            posMat(iTime-reaction_Time/deltaT,iFollower-1);
                 end
            end
            if modelCode == 0 % Original
                [tempPos, tempSpd, error,acc] = ...
                    runStepCacc(posMat(iTime,iFollower), ...
                                speedMat(iTime,iFollower), ....
                                posMat(iTime,iFollower-1), ...
                                vehLen, desireHeadway, ...
                                errorPre, ...
                                deltaT,maxAcc,maxDec);
            elseif modelCode == 1    %XY LU       
                [tempPos, tempSpd,error,acc] = ...
                     runStepCaccXiaoyun(posMat(iTime,iFollower), ...
                                speedMat(iTime,iFollower), ....
                                posMat(iTime,iFollower-1), ...
                                vehLen, desireHeadway, ...
                                errorPre, ...
                                errorPrePre, ...
                                deltaT,maxAcc,maxDec);
            elseif modelCode == 2 % No acceleration constraint  
                [tempPos, tempSpd,error,acc] = ...
                     runStepCaccXiaoyunNoHardConstraint(posMat(iTime,iFollower), ...
                                speedMat(iTime,iFollower), ....
                                posMat(iTime,iFollower-1), ...
                                vehLen, desireHeadway, ...
                                errorPre, ...
                                errorPrePre, ...
                                deltaT,maxAcc,maxDec);
            elseif modelCode == 3 % NGSIM
                [tempPos, tempSpd, error, acc, rule,dist_safety_out] = ...
                     runStepNGSIM(posMat(iTime,iFollower), ...
                                speedMat(iTime,iFollower), ....
                                posMat(iTime,iFollower-1), ...
                                speedMat(iTime, iFollower-1),...
                                vehLen, ...
                                leader_past_pos, ...
                                deltaT,maxAcc,maxDec,...
                                reaction_Time,...
                                jamGap,...
                                freeFlowSpeed);    
                 ruleMat(iTime, iFollower) = rule;     
                 dissafetyMat(iTime, iFollower) = dist_safety_out;     
             elseif modelCode == 4 % NGSIM with patch
                [tempPos, tempSpd, error, acc, rule] = ...
                     runStepNGSIM_Patch(posMat(iTime,iFollower), ...
                                speedMat(iTime,iFollower), ....
                                posMat(iTime,iFollower-1), ...
                                speedMat(iTime, iFollower-1),...
                                vehLen, ...
                                desireHeadway, ...
                                deltaT,maxAcc,maxDec,...
                                reaction_Time,...
                                jamGap,...
                                freeFlowSpeed,...
                                current_acc);    
                 ruleMat(iTime, iFollower) = rule;      
             
            elseif modelCode == 5 % NGSIM with patch with IDM
                [tempPos, tempSpd, error, acc, rule] = ...
                     runStepNGSIM_Patch_IDM(posMat(iTime,iFollower), ...
                                speedMat(iTime,iFollower), ....
                                posMat(iTime,iFollower-1), ...
                                speedMat(iTime, iFollower-1),...
                                vehLen, ...
                                leader_past_pos, ...
                                deltaT,maxAcc,maxDec,...
                                reaction_Time,...
                                jamGap,...
                                freeFlowSpeed);    
                 ruleMat(iTime, iFollower) = rule; 
                 
            elseif modelCode == 6 % IDM
                [tempPos, tempSpd, error, acc] = ...
                     runIDM(posMat(iTime,iFollower), ...
                                speedMat(iTime,iFollower), ....
                                posMat(iTime,iFollower-1), ...
                                speedMat(iTime, iFollower-1),...
                                vehLen, ...
                                desireHeadway, ...
                                deltaT,maxAcc,maxDec,...
                                jamGap,...
                                freeFlowSpeed);    
            end
            
            errorMat(iTime,iFollower) = error; %fill the error fo the current step for uses in next steps
            posMat(iTime+1,iFollower) = tempPos;
            speedMat(iTime+1,iFollower) = tempSpd; 
            accMat(iTime, iFollower) = acc;
            disHeadwayMat(iTime, iFollower) = posMat(iTime, iFollower-1)...
                                - posMat(iTime,iFollower);
            if tempSpd < 1
                headwayMat(iTime, iFollower) = 0;
            else
                headwayMat(iTime, iFollower) = disHeadwayMat(iTime, iFollower)...
                                    / speedMat(iTime, iFollower);
            end
        end
    end
end
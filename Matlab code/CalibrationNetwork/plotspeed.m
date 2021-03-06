function [] = plotspeed(temp, temp1, detid, figureid, ...
    figure_title)

if nargin<5
    figure_title = '';
end

figure(figureid);
plot(temp, 'r', 'LineWidth', 2); hold on;
ylim([0, 100]);
plot(temp1, 'b', 'LineWidth', 2);               
legend('simulation data', 'field data');
title(strcat(figure_title, int2str(detid), ' speed [mph]; RMSE: ', ...
    num2str(RMSE(temp1, temp))));hold off;  
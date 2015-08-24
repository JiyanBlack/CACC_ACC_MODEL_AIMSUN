function draw_surf(xdata1, ydata1, zdata1)
%CREATEFIGURE(XDATA1, YDATA1, ZDATA1)
%  XDATA1:  surface xdata
%  YDATA1:  surface ydata
%  ZDATA1:  surface zdata

%  Auto-generated by MATLAB on 23-Jul-2015 15:14:08

% Create figure
figure1 = figure;

% Create axes
axes1 = axes('Parent',figure1,...
    'Position',[0.114375 0.265632984901277 0.57078125 0.532276422764228],...
    'FontSize',16);
%% Uncomment the following line to preserve the Z-limits of the axes
% zlim(axes1,[0 140]);
view(axes1,[32.5 70]);
grid(axes1,'on');
hold(axes1,'all');

% Create surf
surf(xdata1,ydata1,zdata1,'Parent',axes1,'LineStyle','none');

% Create xlabel
xlabel('Time (minutes)','HorizontalAlignment','right','FontSize',16);

% Create ylabel
ylabel('Location (m)','FontSize',16,'HorizontalAlignment','left');

% Create zlabel
zlabel('Speed (km per hour)','FontSize',16);

% Create colorbar
colorbar('peer',axes1,...
    [0.740677083333335 0.362369337979094 0.0208333333333333 0.348925667828107]);

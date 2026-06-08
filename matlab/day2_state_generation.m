clc;clear;close all;
%时间离散
t=linspace(0,10,20);
%1.直线轨迹
x1=t;
y1=t;

vx1=diff(x1);
vy1=diff(y1);

figure;
plot(x1,y1,'o-');
hold on;
quiver(x1(1:end-1),y1(1:end-1),vx1,vy1);

axis equal;
grid on;

state1=[x1(1:end-1)',y1(1:end-1)',vx1',vy1'];

disp('Line States:');
disp(state1);

%% -----------------------------
%% 2. Sin轨迹
x2 = t;
y2 = sin(t);

vx2 = diff(x2);
vy2 = diff(y2);

figure;
plot(x2,y2,'o-');
hold on;
quiver(x2(1:end-1), y2(1:end-1), vx2, vy2);

axis equal;
grid on;

states2 = [x2(1:end-1)', y2(1:end-1)', vx2', vy2'];

disp('Sin States:');
disp(states2);

%% -----------------------------
%% 3. 圆轨迹
x3 = cos(t);
y3 = sin(t);

vx3 = diff(x3);
vy3 = diff(y3);

figure;
plot(x3,y3,'o-');
hold on;
quiver(x3(1:end-1), y3(1:end-1), vx3, vy3);

axis equal;
grid on;

states3 = [x3(1:end-1)', y3(1:end-1)', vx3', vy3'];

disp('Circle States:');
disp(states3);
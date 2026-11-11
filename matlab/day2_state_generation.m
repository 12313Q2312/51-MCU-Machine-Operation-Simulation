clc;clear;close all;
%% 输出文件
outFile = "trajectory.txt";
fid = fopen(outFile, "w");
if fid == -1
    error("无法创建输出文件 %s", outFile);
end

%时间离散
t=linspace(0,10,20);
%1.直线轨迹
x1=t;
y1=t;

vx1=diff(x1);
vy1=diff(y1);

figure;
plot(x1,y1,"o-");
hold on;
quiver(x1(1:end-1),y1(1:end-1),vx1,vy1);

axis equal;
grid on;

state1= [x1(1:end-1).',y1(1:end-1).',vx1.',vy1.'];

disp("Line States:");
disp(state1);

% 写入文件: x y vx vy (每行一个状态点)
for i = 1:size(state1, 1)
    fprintf(fid, "%.6f %.6f %.6f %.6f\n", state1(i,1), state1(i,2), state1(i,3), state1(i,4));
end

%% -----------------------------
%% 2. Sin轨迹
x2 = t;
y2 = sin(t);

vx2 = diff(x2);
vy2 = diff(y2);

figure;
plot(x2,y2,"o-");
hold on;
quiver(x2(1:end-1), y2(1:end-1), vx2, vy2, 0.5);

axis equal;
grid on;

states2 = [x2(1:end-1).', y2(1:end-1).', vx2.', vy2.'];

disp("Sin States:");
disp(states2);

% 写入文件
for i = 1:size(states2, 1)
    fprintf(fid, "%.6f %.6f %.6f %.6f\n", states2(i,1), states2(i,2), states2(i,3), states2(i,4));
end

%% -----------------------------
%% 3. 圆轨迹
x3 = cos(t);
y3 = sin(t);

vx3 = diff(x3);
vy3 = diff(y3);

figure;
plot(x3,y3,"o-");
hold on;
quiver(x3(1:end-1), y3(1:end-1), vx3, vy3, 0.5);

axis equal;
grid on;

states3 = [x3(1:end-1).', y3(1:end-1).', vx3.', vy3.'];

disp("Circle States:");
disp(states3);

% 写入文件
for i = 1:size(states3, 1)
    fprintf(fid, "%.6f %.6f %.6f %.6f\n", states3(i,1), states3(i,2), states3(i,3), states3(i,4));
end

%% 关闭文件
fclose(fid);
fprintf("状态向量已写入 %s (%d 行)\n", outFile, ...
    size(state1,1) + size(states2,1) + size(states3,1));

disp("完成！");

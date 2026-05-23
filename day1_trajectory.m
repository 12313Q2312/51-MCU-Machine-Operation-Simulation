t=linspace(0,10,100);
%直线
x1=t;y1=t;
plot(x1,y1,'r');
hold on;
%正弦
x2=t;
y2=sin(t);
plot(x2,y2,'g');
%圆
x3=cos(t);
y3=sin(t);
plot(x3,y3,'b');

axis equal;
xlabel('x');
ylabel('y');
legend('line','sin','Circle');

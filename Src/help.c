/*
 * help.c
 *
 *  Created on: 29 ���. 2020 �.
 *      Author: Arh
 */


const char help_text[] =  "\r\
S - set fix CAN bit rate. 'S6' - 500k \r \
s - set custum CAN bit rate via 's5000000' - 500k\r \
O - open CAN channel \r \
C - close CAN channel \r \
t - send CAN message with 11bit ID \r \
T - send CAN message with 29bit ID \r \
r - send CAN remote message with 11bit ID \r \
R - send CAN remote message with 29bit ID \r \
F - read status flag byte \r \
M - set Acceptance ID Register. Stand./Ext. + Data/Rem. + ID 'MSD3B9'\r \
m - set Acceptance Mask/ID2 Register. 'm01FFF' \r \
f - set filter number (0-13) and type (M - mask, I - ID) 'f1I'\r \
V - get hardware and software version \r \
v - get software version only \r \
N - get device serial number \r \
Z - toggle time stamp setting, 'Z1' - ON, 'Z0' - OFF \r \
L - open CAN in listen only mode \r \
l - LIN mode: lM - master, lS - slave \r \
u - set custom USART bitrate 'u115200' \r \
U - set USART bitrate 'U1' - 115200\r \
B - select channel CAN/LIN 'B1' - CAN1\r \
=== Script === \r\
P - Programm script: start = 'P1', stop = 'P0', print = 'P2' \r \
J - marker for jump script after end \r \
D - delay before next script command 'D1000' - 1sec\r \
x - stop script \r \
X - start script \r \
=== Logging === \r\
H - start(format) logging. If H1 - Binary, H2 - GVRET, H3 - CRTD \r \
h - stop logging \r \
=== Misc === \r\
? - list of supported commands \r \
!RST - reset\r \
!SD1 / !SD0 - startup SD connect \0";

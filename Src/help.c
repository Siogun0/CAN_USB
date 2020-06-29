/*
 * help.c
 *
 *  Created on: 29 θών. 2020 γ.
 *      Author: Arh
 */


const char help_text[] =  "\r\
S - set CAN bit rate \r \
s - set CAN bit rate via \r \
O - open CAN channel \r \
C - close CAN channel \r \
t - send CAN message with 11bit ID \r \
T - send CAN message with 29bit ID \r \
r - send CAN remote message with 11bit ID \r \
R - send CAN remote message with 29bit ID \r \
F - read status flag byte \r \
M - set Acceptance Code Register \r \
m - set Acceptance Mask Register \r \
V - get hardware and software version \r \
v - get software version only \r \
N - get device serial number \r \
Z - toggle time stamp setting, 'Z1' - ON, 'Z0' - OFF \r \
L - switch to listen only mode \r \
u - set custom USART bitrate 'u115200' \r \
U - set USART bitrate \r \
B - select channel CAN/LIN 'B1' - CAN1\r \
P - Programm script: start = 'P1', stop = 'P0', print = 'P2' \r \
J - marker for jump script after end \r \
D - delay before next script command 'D1000' - 1sec\r \
x - stop script \r \
X - start script \r \
H - start logging \r \
h - stop logging \r \
? - list of supported commands \r \
! - '!RST' - reset \0";

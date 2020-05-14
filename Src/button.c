/*
 * button.c
 *
 *  Created on: 10 ���. 2020 �.
 *      Author: Arh
 */
#include "button.h"

t_button_state Button_rutine (t_button_var * var, int pressed)
{
	t_button_state button_state = RELISED;

	switch (var->state) {
	case 0:	// ��������
		if (pressed){
			var->time_stamp = GetTimeMs();
			var->state = 1;
		}
		break;
	case 1: // ���� �������
		if (pressed){
			if(GetTimeMs() - var->time_stamp > BUT_DEBOUNSE){
				button_state = PRESSED;
				var->time_stamp = GetTimeMs();
				var->state = 2;
			}
		}
		else {
			var->state = 0;
		}
		break;
	case 2: // ���� ������ (�����������)
		if (pressed){
			if(GetTimeMs() - var->time_stamp > BUT_LONG){
				button_state = LONG_PRESSED;
				var->time_stamp = GetTimeMs();
				var->state = 3;
			}
			else{
				button_state = DOWN;
			}
		}
		else{
			var->time_stamp = GetTimeMs();
			var->state = 4;
		}
		break;
	case 3: // ������������ �����
		if (pressed){
			if(GetTimeMs() - var->time_stamp > BUT_REPEAT){
				button_state = REPEAT_PRESS;
				var->time_stamp = GetTimeMs();
			}
			else{
				button_state = DOWN;
			}
		}
		else{
			button_state = RELISED;
			var->state = 0;
		}
		break;
	case 4: // ��� ��������
		if(pressed){
			button_state = 2;
		}
		else{
			if(GetTimeMs() - var->time_stamp > BUT_DEBOUNSE){
				button_state = CLICKED;
				var->time_stamp = GetTimeMs();
				var->state = 5;
			}
		}
		break;
	case 5: // ���� �������� ����� ��������� ������� (����� state == 0)
		if(pressed){
			var->time_stamp = GetTimeMs();
			var->state = 6;
		}
		else{
			if(GetTimeMs() - var->time_stamp > BUT_DOUBLE){
				var->state = 0;
			}
		}
		break;
	case 6: // ���� ������� double
		if (pressed){
			if(GetTimeMs() - var->time_stamp > BUT_DEBOUNSE){
				button_state = PRESSED;
				var->time_stamp = GetTimeMs();
				var->state = 7;
			}
		}
		else {
			var->state = 5;
		}
		break;

	case 7: // ���� ������ (�����������)
		if (pressed){
			if(GetTimeMs() - var->time_stamp > BUT_LONG){
				button_state = LONG_PRESSED;
				var->time_stamp = GetTimeMs();
				var->state = 3;
			}
			else{
				button_state = DOWN;
			}
		}
		else{
			var->time_stamp = GetTimeMs();
			var->state = 8;
		}
		break;
	case 8: // ��� ��������
		if(pressed){
			var->state = 7;
		}
		else{
			if(GetTimeMs() - var->time_stamp > BUT_DEBOUNSE){
				button_state = DOUBLE_CLICKED;
				var->time_stamp = GetTimeMs();
				var->state = 0;
			}
		}
		break;
	default:
		var->state = 0;
		break;
	}
	return button_state;
}

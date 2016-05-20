/*
 * main.c
 *
 *  Created on: Apr 11, 2016
 *      Author: Sean Harrington
 */

int main(void)
{
	int * modeAddr = (int *) 0x7fc;
	*modeAddr++ = 0x4142;
	*modeAddr++ = 0x4b53;
	*modeAddr++ = 0x5543;
	*modeAddr++ = 0x4b53;
	asm("   B #0xE59, UNC");
	   while(1);

}


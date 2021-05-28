#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

struct Byte {	//stored by bit
	unsigned char bit0 : 1, bit1 : 1, bit2 : 1, bit3 : 1,
				  bit4 : 1, bit5 : 1, bit6 : 1, bit7 : 1;
};

union Memory {
	unsigned char data;
	struct Byte b;
};

union Register {
	short data;
	struct {
		struct Byte b0;
		struct Byte b1;
	}b;
};

const int START = 16384;			//dataSegment starts from 16384

union Memory memory[32 * 1024];		//32Kb Memory
unsigned int PC = 0;				//ProcessCounter
union Register IR;					//InstructionRegister, stores only 2 bytes
union Register ax[9];				//GeneralRegisters, store only 2 bytes. (don't use ax[0]).
int FR = 0;							//FlagsRegister

void ReadToMemory();
void process();
void move1(int dest, int src);
void move2(int dest, int src);
void cal(int dest, int src, int mode);
void AND(int dest, int src);
void OR(int dest, int src);
void NOT(int dest, int mode);
void cmp(int dest, int src);
void show();

int main() {

	//Load all data into memory.
	ReadToMemory();

	//Analyze and run instructions.
	process();

	return 0;
}


void move1(int dest, int src) {		//immed/memory->ax
	short data = (memory[src].data << 8) | memory[src + 1].data;
	ax[dest].data = data;
}
void move2(int dest, int src) {		//ax->memory
	memory[dest].data = ax[src].data >> 8;
	memory[dest + 1].data = ax[src].data & 255;
}
void cal(int dest, int src, int mode) {		//calculate, mode: 0->add, 1->sub, 2->mul, 3->div
	short data = (memory[src].data << 8) | memory[src + 1].data;
	if (mode == 0) ax[dest].data += data;
	else if (mode == 1) ax[dest].data -= data;
	else if (mode == 2) ax[dest].data *= data;
	else if (mode == 3) ax[dest].data /= data;
}
void AND(int dest, int src) {	//logic and
	short data = (memory[src].data << 8) | memory[src + 1].data;
	(ax[dest].data & data) ? (ax[dest].data = 1) : (ax[dest].data = 0);
}
void OR(int dest, int src) {	//logic or
	short data = (memory[src].data << 8) | memory[src + 1].data;
	(ax[dest].data | data) ? (ax[dest].data = 1) : (ax[dest].data = 0);
}
void NOT(int dest, int mode) {		//logic not
	if (mode == 0) ax[dest].data = !ax[dest].data;
	else {
		if (memory[dest].data || memory[dest + 1].data) memory[dest].data = memory[dest + 1].data = 0;
		else memory[dest + 1].data = 1;
	}
}
void cmp(int dest, int src) {		//compare
	short data = (memory[src].data << 8) | memory[src + 1].data;
	if (ax[dest].data == data) FR = 0;
	else if (ax[dest].data > data) FR = 1;
	else FR = -1;
}

void show() {	//show you all the information
	printf("ip = %hd\nflag = %d\nir = %hd\n", PC, FR, IR.data);
	printf("ax1 = %hd ax2 = %hd ax3 = %hd ax4 = %hd\n", ax[1].data, ax[2].data, ax[3].data, ax[4].data);
	printf("ax5 = %hd ax6 = %hd ax7 = %hd ax8 = %hd\n", ax[5].data, ax[6].data, ax[7].data, ax[8].data);
}

void ReadToMemory() {	//read data and store in memory
	FILE* fp = fopen("dict.dic", "r");
	unsigned int cnt = 0;
	char line[33] = { 0 };
	while (fscanf(fp, "%s", line) != EOF) {
			if (line[0] < '0' || line[0] > '1') break;	//unavailable data
			for (int i = 0; i < 4; i++) {
				unsigned char _data = 0;
				for (int j = 0; j < 8; j++) _data = _data * 2 + line[i * 8 + j] - '0';
				memory[cnt++].data = _data;
			}
	}
	fclose(fp);
}

void process() {	//main function, to judge and operate
	while (1) {
		IR.data = (memory[PC].data << 8) | memory[PC + 1].data;
		int cmd = IR.data >> 8;
		int from = IR.data & 15, to = (IR.data & 240) >> 4;
		bool noJump = 1;
		if (cmd == 0) {		//shut down
			PC += 4;
			show();
			printf("\ncodeSegment :\n");
			for (int i = 0; i < 16; i++) {
				for (int j = 0; j < 8; j++) {
					if (j) putchar(' ');
					printf("%d", (memory[i * 32 + j * 4].data << 24) | (memory[i * 32 + j * 4 + 1].data << 16) |
						(memory[i * 32 + j * 4 + 2].data << 8) | memory[i * 32 + j * 4 + 3].data);
				}
				putchar('\n');
			}
			printf("\ndataSegment :\n");
			for (int i = 0; i < 16; i++) {
				for (int j = 0; j < 16; j++) {
					if (j) putchar(' ');
					printf("%hd", (memory[START + i * 32 + j * 2].data << 8) | memory[START + i * 32 + j * 2 + 1].data);
				}
				putchar('\n');
			}
			break;
		}
		if (cmd == 1) {		//move
			if (from == 0) move1(to, PC + 2);	//immed->ax
			else if (from >= 5) move1(to, ax[from].data);	//memory->ax
			else move2(ax[to].data, from);	//ax->memory
		}
		else if (cmd == 2) {	//add
			from == 0 ? cal(to, PC + 2, 0) : cal(to, ax[from].data, 0);
		}
		else if (cmd == 3) {	//subtract
			from == 0 ? cal(to, PC + 2, 1) : cal(to, ax[from].data, 1);
		}
		else if (cmd == 4) {	//multiply
			from == 0 ? cal(to, PC + 2, 2) : cal(to, ax[from].data, 2);
		}
		else if (cmd == 5) {	//divide
			from == 0 ? cal(to, PC + 2, 3) : cal(to, ax[from].data, 3);
		}
		else if (cmd == 6) {	//logic and
			from == 0 ? AND(to, PC + 2) : AND(to, ax[from].data);
		}
		else if (cmd == 7) {	//logic or
			from == 0 ? OR(to, PC + 2) : OR(to, ax[from].data);
		}
		else if (cmd == 8) {	//logic not
			from == 0 ? NOT(to, 0) : NOT(ax[from].data, 1);
		}
		else if (cmd == 9) {	//compare
			from == 0 ? cmp(to, PC + 2) : cmp(to, ax[from].data);
		}
		else if (cmd == 10) {	//jump
			short data = (memory[PC + 2].data << 8) | memory[PC + 3].data;
			if (from == 0 || from == 1 && FR == 0 || from == 2 && FR == 1 || from == 3 && FR == -1) {
				PC += data;
				noJump = 0;
				show();
			}
		}
		else if (cmd == 11) {	//input
			printf("in:\n");
			scanf("%hd", &ax[to].data);
		}
		else if (cmd == 12) {	//output
			printf("out: %hd\n", ax[to].data);
		}

		if (noJump) {
			PC += 4;
			show();
		}
	}
}
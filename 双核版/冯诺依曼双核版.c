#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <process.h>
#include <windows.h>

typedef struct Byte {	//stored by bit
	unsigned char bit0 : 1, bit1 : 1, bit2 : 1, bit3 : 1,
				  bit4 : 1, bit5 : 1, bit6 : 1, bit7 : 1;
}Byte;

typedef union Memory {
	unsigned char data;
	Byte b;
}Memory;

typedef union Register {
	short data;
	struct {
		Byte b0;
		Byte b1;
	}b;
}Register;

const int START = 16384;			//dataSegment starts from 16384
const int CODESTART[2] = { 0,256 };	//beginning of codeSegment of core 1 and core 2

Memory memory[32 * 1024];		//32Kb Memory
bool memLock[2][32 * 1024];		//Memory lock
bool threadLock[2];				//Thread lock. Threads take turns to get tickets
unsigned int PC[2];				//ProcessCounter
Register IR[2];					//InstructionRegister, stores only 2 bytes
Register ax[2][9];				//GeneralRegisters, store only 2 bytes (don't use ax[][0])
int FR[2];						//FlagsRegister

HANDLE hThread1, hThread2, IOMutex;

void ReadToMemory(FILE* fp, int begin);
unsigned __stdcall process(void* p);
void move1(int dest, int src, int id);
void move2(int dest, int src, int id);
void cal(int dest, int src, int mode, int id);
void AND(int dest, int src, int id);
void OR(int dest, int src, int id);
void NOT(int dest, int mode, int id);
void cmp(int dest, int src, int id);
void show(int id);
void checkLock(int pos, int id);

int main() {

	FILE* fp1 = fopen("dict1.dic", "r"), * fp2 = fopen("dict2.dic", "r");
	ReadToMemory(fp1, CODESTART[0]);
	ReadToMemory(fp2, CODESTART[1]);
	fclose(fp1);
	fclose(fp2);
	memory[16385].data = 100;
	int id1 = 0, id2 = 1;
	void* p1 = &id1, * p2 = &id2;

	IOMutex = CreateMutex(NULL, FALSE, NULL);
	hThread1 = (HANDLE)_beginthreadex(NULL, 0, process, p1, 0, NULL);
	hThread2 = (HANDLE)_beginthreadex(NULL, 0, process, p2, 0, NULL);
	WaitForSingleObject(hThread1, INFINITE);
	CloseHandle(hThread1);
	WaitForSingleObject(hThread2, INFINITE);
	CloseHandle(hThread2);

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

	return 0;
}


void move1(int dest, int src, int id) {		//immed/memory->ax
	checkLock(src, id);
	short data = (memory[src].data << 8) | memory[src + 1].data;
	ax[id][dest].data = data;
}
void move2(int dest, int src, int id) {		//ax->memory
	checkLock(dest, id);
	memory[dest].data = ax[id][src].data >> 8;
	memory[dest + 1].data = ax[id][src].data & 255;
}
void cal(int dest, int src, int mode, int id) {		//calculate, mode: 0->add, 1->sub, 2->mul, 3->div
	checkLock(src, id);
	short data = (memory[src].data << 8) | memory[src + 1].data;
	if (mode == 0) ax[id][dest].data += data;
	else if (mode == 1) ax[id][dest].data -= data;
	else if (mode == 2) ax[id][dest].data *= data;
	else if (mode == 3) ax[id][dest].data /= data;
}
void AND(int dest, int src, int id) {	//logic and
	checkLock(src, id);
	short data = (memory[src].data << 8) | memory[src + 1].data;
	(ax[id][dest].data & data) ? (ax[id][dest].data = 1) : (ax[id][dest].data = 0);
}
void OR(int dest, int src, int id) {	//logic or
	checkLock(src, id);
	short data = (memory[src].data << 8) | memory[src + 1].data;
	(ax[id][dest].data | data) ? (ax[id][dest].data = 1) : (ax[id][dest].data = 0);
}
void NOT(int dest, int mode, int id) {		//logic not
	checkLock(dest, id);
	if (mode == 0) ax[id][dest].data = !ax[id][dest].data;
	else {
		if (memory[dest].data || memory[dest + 1].data) memory[dest].data = memory[dest + 1].data = 0;
		else memory[dest + 1].data = 1;
	}
}
void cmp(int dest, int src, int id) {		//compare
	checkLock(src, id);
	short data = (memory[src].data << 8) | memory[src + 1].data;
	if (ax[id][dest].data == data) FR[id] = 0;
	else if (ax[id][dest].data > data) FR[id] = 1;
	else FR[id] = -1;
}

void show(int id) {		//show you all the information
	WaitForSingleObject(IOMutex, INFINITE);
	printf("id = %d\nip = %hd\nflag = %d\nir = %hd\n", id + 1, PC[id] + CODESTART[id], FR[id], IR[id].data);
	printf("ax1 = %hd ax2 = %hd ax3 = %hd ax4 = %hd\n", ax[id][1].data, ax[id][2].data, ax[id][3].data, ax[id][4].data);
	printf("ax5 = %hd ax6 = %hd ax7 = %hd ax8 = %hd\n", ax[id][5].data, ax[id][6].data, ax[id][7].data, ax[id][8].data);
	ReleaseMutex(IOMutex);
}

void ReadToMemory(FILE* fp, int begin) {	//read data and store in memory
	unsigned int cnt = begin;
	while (1) {
		char line[33] = { 0 };
		if (fscanf(fp, "%s", line) != EOF) {
			if (line[0] < '0' || line[0] > '1') break;	//unavailable data
			for (int i = 0; i < 4; i++) {
				unsigned char _data = 0;
				for (int j = 0; j < 8; j++) _data = _data * 2 + line[i * 8 + j] - '0';
				memory[cnt++].data = _data;
			}
		}
		else break;
	}
}

unsigned __stdcall process(void* p) {	//main function, to judge and operate
	int id = *((int*)p);
	while (1) {
		IR[id].data = (memory[PC[id]].data << 8) | memory[PC[id] + 1].data;
		int cmd = IR[id].data >> 8;
		int from = IR[id].data & 15, to = (IR[id].data & 240) >> 4;
		bool noJump = 1;
		if (cmd == 0) {		//shut down
			PC[id] += 4;
			show(id);
			break;
		}
		if (cmd == 1) {		//move
			if (from == 0) move1(to, PC[id] + 2 + CODESTART[id], id);	//immed->ax[id]
			else if (from >= 5) move1(to, ax[id][from].data, id);	//memory->ax[id]
			else move2(ax[id][to].data, from, id);	//ax[id]->memory
		}
		else if (cmd == 2) {	//add
			from == 0 ? cal(to, PC[id] + 2 + CODESTART[id], 0, id) : cal(to, ax[id][from].data, 0, id);
		}
		else if (cmd == 3) {	//subtract
			from == 0 ? cal(to, PC[id] + 2 + CODESTART[id], 1, id) : cal(to, ax[id][from].data, 1, id);
		}
		else if (cmd == 4) {	//multiply
			from == 0 ? cal(to, PC[id] + 2 + CODESTART[id], 2, id) : cal(to, ax[id][from].data, 2, id);
		}
		else if (cmd == 5) {	//divide
			from == 0 ? cal(to, PC[id] + 2 + CODESTART[id], 3, id) : cal(to, ax[id][from].data, 3, id);
		}
		else if (cmd == 6) {	//logic and
			from == 0 ? AND(to, PC[id] + 2 + CODESTART[id], id) : AND(to, ax[id][from].data, id);
		}
		else if (cmd == 7) {	//logic or
			from == 0 ? OR(to, PC[id] + 2 + CODESTART[id], id) : OR(to, ax[id][from].data, id);
		}
		else if (cmd == 8) {	//logic not
			from == 0 ? NOT(to, 0, id) : NOT(ax[id][from].data, 1, id);
		}
		else if (cmd == 9) {	//compare
			from == 0 ? cmp(to, PC[id] + 2 + CODESTART[id], id) : cmp(to, ax[id][from].data, id);
		}
		else if (cmd == 10) {	//jump
			short data = (memory[PC[id] + 2 + CODESTART[id]].data << 8) | memory[PC[id] + 3 + CODESTART[id]].data;
			if (from == 0 || from == 1 && FR[id] == 0 || from == 2 && FR[id] == 1 || from == 3 && FR[id] == -1) {
				PC[id] += data;
				noJump = 0;
				show(id);
			}
		}
		else if (cmd == 11) {	//input
			WaitForSingleObject(IOMutex, INFINITE);
			printf("in:\n");
			ReleaseMutex(IOMutex);
			scanf("%hd", &ax[id][to].data);
		}
		else if (cmd == 12) {	//output
			WaitForSingleObject(IOMutex, INFINITE);
			printf("id = %d    out: %hd\n", id + 1, ax[id][to].data);
			ReleaseMutex(IOMutex);
		}
		else if (cmd == 13) {	//lock
			short pos = (memory[PC[id] + 2 + CODESTART[id]].data << 8) + memory[PC[id] + 3 + CODESTART[id]].data;
			while (threadLock[id]) Sleep(10);
			checkLock(pos, id);
			id == 0 ? (memLock[1][pos] = 1) : (memLock[0][pos] = 1);
		}
		else if (cmd == 14) {	//release
			short pos = (memory[PC[id] + 2 + CODESTART[id]].data << 8) + memory[PC[id] + 3 + CODESTART[id]].data;
			if (id == 0) {
				memLock[1][pos] = 0;
				threadLock[0] = 1;
				threadLock[1] = 0;
			}
			else {
				memLock[0][pos] = 0;
				threadLock[0] = 0;
				threadLock[1] = 1;
			}
		}
		else if (cmd == 15) {	//sleep
			short sleepTime = (memory[PC[id] + 2 + CODESTART[id]].data << 8) + memory[PC[id] + 3 + CODESTART[id]].data;
			Sleep(sleepTime);
		}

		if (noJump) {
			PC[id] += 4;
			show(id);
		}
	}
	_endthreadex(0);
	return 0;
}

void checkLock(int pos, int id) {
	while (memLock[id][pos]) Sleep(10);
}
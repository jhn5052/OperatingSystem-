#include "pipeline.h"

int main(int argc, char *argv[]) {
	char line[MAXLINELENGTH];
	stateType state, newState;
	FILE *filePtr;
	int end = 0;
	int cnt_instr = 0;

	if (argc != 2) {
		printf("error: usage: %s <machine-code file>\n", argv[0]);
		exit(1);
	}

	filePtr = fopen(argv[1], "r");
	if (filePtr == NULL) {
		printf("error: can't open file %s", argv[1]);
		perror("fopen");
		exit(1);
	}

	/* read in the entire machine-code file into memory */
	for (state.numMemory = 0; fgets(line, MAXLINELENGTH, filePtr) != NULL;
		state.numMemory++) {

		if (sscanf(line, "%d", state.instrMem + state.numMemory) != 1) {
			printf("error in reading address %d\n", state.numMemory);
			exit(1);
		}

		state.dataMem[state.numMemory] = state.instrMem[state.numMemory];
	}

	/* Initialize PC and registers */
	state.pc = 0;
	memset(state.reg, 0, NUMREGS * sizeof(int));
	state.IFID.instr = NOOPINSTRUCTION;
	state.IDEX.instr = NOOPINSTRUCTION;
	state.EXMEM.instr = NOOPINSTRUCTION;
	state.MEMWB.instr = NOOPINSTRUCTION;
	state.WBEND.instr = NOOPINSTRUCTION;

	while (1) {

		printState(&state);

		/* check for halt */
		if (opcode(state.MEMWB.instr) == HALT) {
			printf("machine halted\n");
			printf("total of %d cycles executed\n", state.cycles);
			exit(0);
		}

		newState = state;
		newState.cycles++;

		/* --------------------- IF stage --------------------- */
		newState.pc = state.pc + 1;
		newState.IFID.instr = state.instrMem[state.pc];
		newState.IFID.pcPlus1 = state.pc + 1;

		/* --------------------- ID stage --------------------- */
		int ID_RegA = field0(state.IFID.instr);
		int ID_RegB = field1(state.IFID.instr);
		int ID_offset = (field2(state.IFID.instr)<<16)>>16;

		newState.IDEX.instr = state.IFID.instr;
		newState.IDEX.readRegA = state.reg[ID_RegA];
		newState.IDEX.readRegB = state.reg[ID_RegB];
		newState.IDEX.offset = ID_offset;
		newState.IDEX.pcPlus1 = state.IFID.pcPlus1;

		/* --------------------- EX stage --------------------- */
		int aluResult;
		int EX_op = opcode(state.IDEX.instr);
		aluResult = ALU_OP(EX_op, state.IDEX.readRegA, state.IDEX.readRegB, state.IDEX.offset);
		
		newState.EXMEM.instr = state.IDEX.instr;
		newState.EXMEM.branchTarget = state.IDEX.offset + state.IDEX.pcPlus1;
		newState.EXMEM.aluResult = aluResult;
		newState.EXMEM.readRegB = state.IDEX.readRegB;

		/* --------------------- MEM stage --------------------- */
		int writeData = 0;
		int Mem_op = opcode(state.EXMEM.instr);

		switch (Mem_op){
			case LW:
				writeData = state.dataMem[state.EXMEM.aluResult];
				break;

			case SW:
				state.dataMem[state.EXMEM.aluResult] = state.EXMEM.readRegB;
				break;

			case BEQ:
				if (state.EXMEM.aluResult == 0) {
					newState.pc = state.EXMEM.branchTarget;
				}
				break;

			default:
				writeData = state.EXMEM.aluResult;
				break;
		}

		newState.MEMWB.instr = state.EXMEM.instr;
		newState.MEMWB.writeData = writeData;

        //Ex 1st (add add)
        //1. IFID, IDEX (opcode)  2. (IFID)field0,1 / (IDEX)field2
        int IFID_OP = opcode(state.IFID.instr);
        int IDEX_OP = opcode(state.IDEX.instr);
        int EXMEM_OP = opcode(state.EXMEM.instr);
        
        int IFID_RegA = field0(state.IFID.instr);  
        int IFID_RegB = field1(state.IFID.instr);
        int IDEX_Dest = field2(state.IDEX.instr);
        int EXMEM_Dest = field2(state.EXMEM.instr);

        if ((IDEX_OP == ADD || IDEX_OP == NOR) && IFID_OP < HALT) {
            if (IDEX_Dest == IFID_RegA) {
                newState.IDEX.readRegA = aluResult;
            }
            if (IDEX_Dest == IFID_RegB && IFID_OP != LW) {
                newState.IDEX.readRegB = aluResult;
            }
        }

        //EX 2nd add noop add
        if ((EXMEM_OP == ADD || EXMEM_OP == NOR) && IFID_OP < HALT) {
            if (EXMEM_Dest == IFID_RegA) {
                newState.IDEX.readRegA = writeData;
            }
            if (EXMEM_Dest == IFID_RegB && IFID_OP != LW) {
                newState.IDEX.readRegB = writeData;
            }
        }


        //MEM lw noop add
        EXMEM_Dest = field1(state.EXMEM.instr);
        if (EXMEM_OP == LW && IFID_OP < HALT) {
            if (EXMEM_Dest == IFID_RegA) {
                newState.IDEX.readRegA = writeData;
            } 
            if (EXMEM_Dest == IFID_RegB && IFID_OP != LW) {
                newState.IDEX.readRegB = writeData;
            }
        }

        
        //MEM lw add
        IDEX_Dest = field1(state.IDEX.instr);
        if (IDEX_OP == LW &&  IFID_OP < HALT) {
           if ((IDEX_Dest == IFID_RegA) || (IDEX_Dest == IFID_RegB && IFID_OP != LW)) {
                newState.IDEX.instr = NOOPINSTRUCTION;
                newState.IDEX.pcPlus1 = 0;
                newState.IDEX.readRegA = 0;
                newState.IDEX.readRegB = 0;
                newState.IDEX.offset = 0;

                newState.pc = state.pc;

                newState.IFID.instr = state.IFID.instr;
                newState.IFID.pcPlus1 = state.IFID.pcPlus1;
           }

        }

        /* Control hazard */
        if (EXMEM_OP == BEQ && (state.EXMEM.aluResult == 0)) {
            newState.EXMEM.instr = NOOPINSTRUCTION;
            newState.EXMEM.branchTarget= 0;
            newState.EXMEM.aluResult = 0;
            newState.EXMEM.readRegB= 0;

            newState.IDEX.instr = NOOPINSTRUCTION;
            newState.IDEX.pcPlus1 = 0;
            newState.IDEX.readRegA = 0;
            newState.IDEX.readRegB = 0;
            newState.IDEX.offset = 0;

            newState.IFID.instr = NOOPINSTRUCTION;
            newState.IFID.pcPlus1 = 0;
        }

		/* --------------------- WB stage --------------------- */
		int Wb_op = opcode(state.MEMWB.instr);
		int WB_RD = -1;

		switch (Wb_op){
			case ADD:
			case NOR:
				WB_RD = field2(state.MEMWB.instr); //rd
				newState.reg[WB_RD] = state.MEMWB.writeData;
				break;

			case LW:
				WB_RD = field1(state.MEMWB.instr); // rt
				newState.reg[WB_RD] = state.MEMWB.writeData;
				break;
		}

		newState.WBEND.instr = state.MEMWB.instr;
		newState.WBEND.writeData = state.MEMWB.writeData;

        int MEMWB_OP = opcode(state.MEMWB.instr);
        int MEMWB_Dest = field2(state.MEMWB.instr);

        //add noop noop add
        if ((MEMWB_OP == ADD || MEMWB_OP == NOR) && IFID_OP < HALT) {
            if (MEMWB_Dest == IFID_RegA) {
                newState.IDEX.readRegA = state.MEMWB.writeData;
            }
            if (MEMWB_Dest == IFID_RegB && IFID_OP != LW) {
                newState.IDEX.readRegB = state.MEMWB.writeData;
            }
        }

        MEMWB_Dest = field1(state.MEMWB.instr);
        if (MEMWB_OP == LW && IFID_OP < HALT) {
            if (MEMWB_Dest == IFID_RegA) {
                newState.IDEX.readRegA = state.MEMWB.writeData;
            }
            if (MEMWB_Dest == IFID_RegB && IFID_OP != LW) {
                newState.IDEX.readRegB = state.MEMWB.writeData;
            }
        }

		state = newState; /* this is the last statement before end of the loop.
						  It marks the end of the cycle and updates the
						  current state with the values calculated in this
						  cycle */
	}

	while (1);
}

void
printState(stateType *statePtr)
{
	int i;
	printf("\n@@@\nstate before cycle %d starts\n", statePtr->cycles);
	printf("\tpc %d\n", statePtr->pc);

	printf("\tdata memory:\n");
	for (i = 0; i<statePtr->numMemory; i++) {
		printf("\t\tdataMem[ %d ] %d\n", i, statePtr->dataMem[i]);
	}
	printf("\tregisters:\n");
	for (i = 0; i<NUMREGS; i++) {
		printf("\t\treg[ %d ] %d\n", i, statePtr->reg[i]);
	}
	printf("\tIFID:\n");
	printf("\t\tinstruction ");
	printInstruction(statePtr->IFID.instr);
	printf("\t\tpcPlus1 %d\n", statePtr->IFID.pcPlus1);
	printf("\tIDEX:\n");
	printf("\t\tinstruction ");
	printInstruction(statePtr->IDEX.instr);
	printf("\t\tpcPlus1 %d\n", statePtr->IDEX.pcPlus1);
	printf("\t\treadRegA %d\n", statePtr->IDEX.readRegA);
	printf("\t\treadRegB %d\n", statePtr->IDEX.readRegB);
	printf("\t\toffset %d\n", statePtr->IDEX.offset);
	printf("\tEXMEM:\n");
	printf("\t\tinstruction ");
	printInstruction(statePtr->EXMEM.instr);
	printf("\t\tbranchTarget %d\n", statePtr->EXMEM.branchTarget);
	printf("\t\taluResult %d\n", statePtr->EXMEM.aluResult);
	printf("\t\treadRegB %d\n", statePtr->EXMEM.readRegB);
	printf("\tMEMWB:\n");
	printf("\t\tinstruction ");
	printInstruction(statePtr->MEMWB.instr);
	printf("\t\twriteData %d\n", statePtr->MEMWB.writeData);
	printf("\tWBEND:\n");
	printf("\t\tinstruction ");
	printInstruction(statePtr->WBEND.instr);
	printf("\t\twriteData %d\n", statePtr->WBEND.writeData);
}

int
field0(int instruction)
{
	return((instruction >> 19) & 0x7);
}

int
field1(int instruction)
{
	return((instruction >> 16) & 0x7);
}

int
field2(int instruction)
{
	return(instruction & 0xFFFF);
}

int
opcode(int instruction)
{
	return(instruction >> 22);
}

void
printInstruction(int instr)
{

	char opcodeString[10];

	if (opcode(instr) == ADD) {
		strcpy(opcodeString, "add");
	}
	else if (opcode(instr) == NOR) {
		strcpy(opcodeString, "nor");
	}
	else if (opcode(instr) == LW) {
		strcpy(opcodeString, "lw");
	}
	else if (opcode(instr) == SW) {
		strcpy(opcodeString, "sw");
	}
	else if (opcode(instr) == BEQ) {
		strcpy(opcodeString, "beq");
	}
	else if (opcode(instr) == JALR) {
		strcpy(opcodeString, "jalr");
	}
	else if (opcode(instr) == HALT) {
		strcpy(opcodeString, "halt");
	}
	else if (opcode(instr) == NOOP) {
		strcpy(opcodeString, "noop");
	}
	else {
		strcpy(opcodeString, "data");
	}
	printf("%s %d %d %d\n", opcodeString, field0(instr), field1(instr),
		field2(instr));
}

int ALU_OP(int op, int a, int b, int offset) {
	switch (op){
		case ADD: 
			return a + b;

		case NOR:
			return ~(a | b);
		
		case LW:
		case SW:
			return a + offset;
		
		case BEQ:
			return a - b;
		
		default:
			return 0;
	}
}

#include <dbt/dbtPlateform.h>
#include <dbt/insertions.h>
#include <dbt/profiling.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include <lib/endianness.h>
#include <lib/tools.h>
#include <simulator/vexSimulator.h>
#include <simulator/vexTraceSimulator.h>
#include <simulator/riscvSimulator.h>

#include <transformation/firstPassTranslator.h>
#include <transformation/irGenerator.h>
#include <transformation/optimizeBasicBlock.h>
#include <transformation/buildControlFlow.h>
#include <transformation/reconfigureVLIW.h>
#include <transformation/buildTraces.h>
#include <transformation/rescheduleProcedure.h>
#include <lib/debugFunctions.h>

#include <isa/vexISA.h>
#include <isa/riscvISA.h>

#include <lib/config.h>
#include <lib/log.h>
#include <lib/traceQueue.h>
#include <lib/threadedDebug.h>


#ifndef __NIOS
#include <lib/elfFile.h>
#else
#include <system.h>
#endif


void printStats(unsigned int size, short* blockBoundaries){

	float numberBlocks = 0;

	for (int oneInstruction = 0; oneInstruction < size; oneInstruction++){
		if (blockBoundaries[oneInstruction] == 1)
			numberBlocks++;
	}

  Log::printf(0, "\n* Statistics on used binaries:\n");
  Log::printf(0, "* \tThere is %d instructions.\n", size);
  Log::printf(0, "* \tThere is %d blocks.\n", (int) numberBlocks);
  Log::printf(0, "* \tBlocks mean size is %f.\n\n", size/numberBlocks);

}


int translateOneSection(DBTPlateform &dbtPlateform, uint32 placeCode, int sourceStartAddress, int sectionStartAddress, int sectionEndAddress){
	int previousPlaceCode = placeCode;
	uint32 size = (sectionEndAddress - sectionStartAddress)>>2;
	placeCode = firstPassTranslator_RISCV(&dbtPlateform,
			size,
			sourceStartAddress,
			sectionStartAddress,
			placeCode);



		return placeCode;
}


void readSourceBinaries(char* path, unsigned char *&code, unsigned int &addressStart, uint32 &size, uint32 &pcStart, DBTPlateform *platform){

#ifndef __NIOS
	//We open the elf file and search for the section that is of interest for us
	ElfFile elfFile(path);


	for (unsigned int sectionCounter = 0; sectionCounter<elfFile.sectionTable->size(); sectionCounter++){
		ElfSection *section = elfFile.sectionTable->at(sectionCounter);

		//The section we are looking for is called .text
		if (!section->getName().compare(".text")){

			code = section->getSectionCode();//0x3c
			addressStart = section->address + 0;
			size = section->size/4 - 0;

			if (size > MEMORY_SIZE){
				Log::fprintf(0, stderr, "Error: binary file has %d instructions, we currently handle a maximum size of %d\n", size, MEMORY_SIZE);
				exit(-1);
			}


		}
	}

	unsigned int localHeapAddress = 0;
	for (unsigned int sectionCounter = 0; sectionCounter<elfFile.sectionTable->size(); sectionCounter++){
		ElfSection *section = elfFile.sectionTable->at(sectionCounter);

		if (section->address != 0){

			unsigned char* data = section->getSectionCode();
			platform->vexSimulator->initializeDataMemory(data, section->size, section->address);
			free(data);

			if (section->address + section->size > localHeapAddress)
				localHeapAddress = section->address + section->size;
		}
	}
	platform->vexSimulator->heapAddress = localHeapAddress;

	for (int oneSymbol = 0; oneSymbol < elfFile.symbols->size(); oneSymbol++){
		ElfSymbol *symbol = elfFile.symbols->at(oneSymbol);
		const char* name = (const char*) &(elfFile.sectionTable->at(elfFile.indexOfSymbolNameSection)->getSectionCode()[symbol->name]);

		if (strcmp(name, "_start") == 0){
			pcStart = symbol->offset;

		}
	}



#else
//	read(0, &addressStart, sizeof(int));
//	read(0, &size, sizeof(int));
	addressStart = tmp_addressStart;
	size = tmp_size;
	code = tmp_code;
//	code = (unsigned char*) malloc(size*sizeof(unsigned char));
//
//	for (int oneByte = 0; oneByte<size; oneByte++){
//		read(0, &code[oneByte], sizeof(char));
//	}

	size = size/4;
#endif
}
int cnt = 0;
int run(DBTPlateform *platform, int nbCycle){
#ifndef __NIOS


	return platform->vexSimulator->doStep(nbCycle);

#else
  Log::printf(0, "starting run\n");

//#define ALT_CI_COMPONENT_RUN_0(A,B) __builtin_custom_inii(ALT_CI_COMPONENT_RUN_0_N,(A),(B))
//#define ALT_CI_COMPONENT_RUN_0_1(A,B) __builtin_custom_inii(ALT_CI_COMPONENT_RUN_0_1_N,(A),(B))
//#define ALT_CI_COMPONENT_RUN_0_1_N 0x4
//#define ALT_CI_COMPONENT_RUN_0_2(A,B) __builtin_custom_inii(ALT_CI_COMPONENT_RUN_0_2_N,(A),(B))
//#define ALT_CI_COMPONENT_RUN_0_2_N 0x5
//#define ALT_CI_COMPONENT_RUN_0_3(A,B) __builtin_custom_inii(ALT_CI_COMPONENT_RUN_0_3_N,(A),(B))

	int start = 0;
	 ALT_CI_COMPONENT_RUN_0(0,0);
	 int status = ALT_CI_COMPONENT_RUN_0_2(0,0);
	 if ((status & 0x3) == 1)
		 return 0;
	 ALT_CI_COMPONENT_RUN_0_1(0,0);
	 ALT_CI_COMPONENT_RUN_0(0,0);
	 return (ALT_CI_COMPONENT_RUN_0_3(0,0))>>2;


#endif

}



int main(int argc, char *argv[])
{

	/*Parsing arguments of the commant
	 *
	 */

	static std::map<std::string, FILE*> ios =
	{
	{ "stdin", stdin },
	{ "stdout", stdout },
	{ "stderr", stderr }
	};

	Config cfg(argc, argv);

	int CONFIGURATION = cfg.has("c") ? std::stoi(cfg["c"]) : 2;

	int VERBOSE = cfg.has("v") ? std::stoi(cfg["v"]) : 0;

	Log::Init(VERBOSE);

	int OPTLEVEL = cfg.has("O") ? std::stoi(cfg["O"]) : 1;
	int HELP = cfg.has("h");
	char* binaryFile = cfg.has("f") && !cfg["f"].empty() ? (char*)cfg["f"].c_str() : NULL;
/*
	char* ARGUMENTS = cfg.has("-") && !cfg["-"].empty() ? (char*)1 : (char*)0;
  std::string args_tmp;

  if (ARGUMENTS)
  {
    bool first = 1;
    for (std::string arg : cfg.argsOf("-"))
    {
      args_tmp += (first ? (first = 0, "") : " ") + arg;
    }
    ARGUMENTS = (char*)args_tmp.c_str();
  }
*/
	FILE** inStreams = (FILE**) malloc(10*sizeof(FILE*));
	FILE** outStreams = (FILE**) malloc(10*sizeof(FILE*));

	int nbInStreams = 0;
	int nbOutStreams = 0;

	int MAX_SCHEDULE_COUNT = cfg.has("m") ? std::stoi(cfg["m"]) : -1;

	if (cfg.has("i"))
	{
		for (auto filename : cfg.argsOf("i"))
		{
			FILE* fp = ios[filename];
			if (!fp)
				fp = fopen(filename.c_str(), "r");
			inStreams[nbInStreams++] = fp;
		}
	}

	if (cfg.has("o"))
	{
		for (auto filename : cfg.argsOf("o"))
		{
			FILE* fp = ios[filename];
			if (!fp)
				fp = fopen(filename.c_str(), "w");
			outStreams[nbOutStreams++] = fp;
		}
	}

//	while ((c = getopt (argc, argv, "m:vO:ha:o:i:f:c:")) != -1)
//	switch (c)
//	  {
//		case 'm':
//			MAX_SCHEDULE_COUNT = atoi(optarg);
//		break;
//	  case 'v':
//		VERBOSE = 1;
//		break;
//	  case 'h':
//		HELP = 1;
//		break;
//	  case 'a':
//		  ARGUMENTS = optarg;
//	  break;
//	  case 'O':
//		  OPTLEVEL = atoi(optarg);
//		break;
//	  case 'c':
//		  CONFIGURATION = atoi(optarg);
//		break;
//	  case 'i':
//		  if (strcmp(optarg, "stdin") == 0)
//			  inStreams[nbInStreams] = stdin;
//		  else
//			  inStreams[nbInStreams] = fopen(optarg, "r");
//		  nbInStreams++;
//	  break;
//	  case 'o':
//		  if (strcmp(optarg, "stdout") == 0)
//			  outStreams[nbOutStreams] = stdout;
//		  else if (strcmp(optarg, "stderr") == 0)
//			  outStreams[nbOutStreams] = stderr;
//		  else
//			  outStreams[nbOutStreams] = fopen(optarg, "w");
//		  nbOutStreams++;
//	  break;
//	  case 'f':
//		  binaryFile = optarg;
//	  break;
//	  default:
//		abort ();
//	  }

  int localArgc = 1;
  char ** localArgv;

  // the application has an argc equal to the number of options + 1 for its path
  // cfg stores all application arguments in the "--" option
  //
  // as neither the C nor the POSIX standard ensures that the arguments are stored
  // in contiguous space, we can use cfg's c_str() as pointers
  if (cfg.has("-"))
  {
    // we reserve localArgv's elements
    localArgc += cfg.argsOf("-").size();
    localArgv = new char*[localArgc];

    // the first argument is always the path
    localArgv[0] = binaryFile;

    int argId = 1;
    // we set localArgv values
    for (const std::string& arg : cfg.argsOf("-"))
    {
      localArgv[argId] = (char*)arg.c_str();
      argId++;
    }
  }
  else
  {
    localArgv = new char*[localArgc];
    localArgv[0] = binaryFile;
  }


  for (int i = 0; i < localArgc; ++i)
  {
    Log::printf(0, "localArgv[%d] = %s;\n", i, localArgv[i]);
  }
/*
	char** localArgv = ;

	if (ARGUMENTS == NULL){
		localArgc = argc - optind;
		localArgv =  &(argv[optind]);
	}
	else{
		//We find the size of the string representing arguments and replace spaces by 0
		int index = 0;
		int count = 1;
		while (ARGUMENTS[index] != 0){
			if (ARGUMENTS[index] == ' '){
				ARGUMENTS[index] = 0;
				count++;
				Log::printf(0, "Arg was %s\n", ARGUMENTS);
			}
			index++;
		}
		index++; //so that index is equal to the size of the char*


		//We find size of filename
		int charFileIndex = 0;
		while (binaryFile[charFileIndex] != 0)
			charFileIndex++;

		charFileIndex++; //So that charFileIndex is equal to the size of the FILE name

		//we build a char* containing all args and the file name
		char* tempArg = (char*) malloc((index + charFileIndex)*sizeof(char));
		memcpy(tempArg, binaryFile, charFileIndex*sizeof(char));
		memcpy(tempArg+charFileIndex, ARGUMENTS, index * sizeof(char));

		//We build the char** localArgv
		localArgv = (char**) malloc((count+1)*sizeof(char*));
		index = 0;
		for (int oneArg = 0; oneArg<count+1; oneArg++){
			localArgv[oneArg] = &(tempArg[index]);
			while (tempArg[index] != 0){
				index++;
			}
			index++;

		}

		//Value of localArgc is number of argument + the one from the file name
		localArgc = count + 1;
	}
*/

	if (HELP || binaryFile == NULL){
    Log::printf(0, "Usage is %s [-v] [-On] file\n\t-v\tVerbose mode, prints all execution information\n\t-On\t Optimization level from zero to two\n", argv[0]);
		return 1;
	}

	// We translate OPTLEVEL

	/***********************************
	 *  Initialization of the DBT platform
	 ***********************************
	 * In the linux implementation, this is done by reading an elf file and copying binary instructions
	 * in the corresponding memory.
	 * In a real platform, this may require no memory initialization as the binaries would already be stored in the
	 * system memory.
	 ************************************/

	//Definition of objects used for DBT process
	DBTPlateform dbtPlateform;

//	dbtPlateform.vliwInitialConfiguration = 0x24481284;
	dbtPlateform.vliwInitialConfiguration = CONFIGURATION;
	dbtPlateform.vliwInitialIssueWidth = getIssueWidth(dbtPlateform.vliwInitialConfiguration);


	#ifndef __NIOS

  ThreadedDebug * dbg = nullptr;
  TraceQueue * tracer = nullptr;
  if (cfg.has("trace")) {
    TraceQueue * tracer = new TraceQueue();

    dbg = new ThreadedDebug(tracer);
    dbg->run();

  	dbtPlateform.vexSimulator = new VexTraceSimulator(dbtPlateform.vliwBinaries, tracer);
  }
  else
  	dbtPlateform.vexSimulator = new VexSimulator(dbtPlateform.vliwBinaries);

	dbtPlateform.vexSimulator->inStreams = inStreams;
	dbtPlateform.vexSimulator->nbInStreams = nbInStreams;
	dbtPlateform.vexSimulator->outStreams = outStreams;
	dbtPlateform.vexSimulator->nbOutStreams = nbOutStreams;

	setVLIWConfiguration(dbtPlateform.vexSimulator, dbtPlateform.vliwInitialConfiguration);

	#endif

	unsigned char* code;
	unsigned int addressStart;
	uint32 size;
	uint32 pcStart;


	//We read the binaries
	readSourceBinaries(binaryFile, code, addressStart, size, pcStart, &dbtPlateform);

	if (size > MEMORY_SIZE){
		Log::fprintf(0, stderr, "ERROR: Size of source binaries is %d. Current implementation only accept size lower then %d\n", size, MEMORY_SIZE);
		exit(-1);
	}


	int numberOfSections = 1 + (size>>10);
	IRApplication application = IRApplication(numberOfSections);
	Profiler profiler = Profiler(&dbtPlateform);




	//we copy the binaries in the corresponding memory
	for (int oneInstruction = 0; oneInstruction<size; oneInstruction++)
		dbtPlateform.mipsBinaries[oneInstruction] = ((unsigned int*) code)[oneInstruction];


	//We declare the variable in charge of keeping a track of where we are writing
	uint32 placeCode = 0; //As 4 instruction bundle

	//We add initialization code to the vliw binaries
	placeCode = getInitCode(&dbtPlateform, placeCode, addressStart);
	placeCode = insertCodeForInsertions(&dbtPlateform, placeCode, addressStart);

	//We modify the initialization call
	writeInt(dbtPlateform.vliwBinaries, 0*16, assembleIInstruction(VEX_CALL, placeCode, 63));

	initializeInsertionsMemory(size*4);

	for (int oneInsertion=0; oneInsertion<placeCode; oneInsertion++){
		if (VERBOSE)
			Log::fprintf(0, stderr, "insert;%d\n", oneInsertion);
	}


	/********************************************************
	 * First part of DBT: generating the first pass translation of binaries
	 *******************************************************
	 * TODO: description
	 *
	 *
	 ********************************************************/


	for (int oneSection=0; oneSection<(size>>10)+1; oneSection++){

		int startAddressSource = addressStart + oneSection*1024*4;
		int endAddressSource = startAddressSource + 1024*4;
		if (endAddressSource > addressStart + size*4)
			endAddressSource = addressStart + (size<<2);


		int effectiveSize = (endAddressSource - startAddressSource)>>2;
		for (int j = 0; j<effectiveSize; j++){
			dbtPlateform.mipsBinaries[j] = ((unsigned int*) code)[j+oneSection*1024];
		}
		int oldPlaceCode = placeCode;

		placeCode =  translateOneSection(dbtPlateform, placeCode, addressStart, startAddressSource,endAddressSource);

		buildBasicControlFlow(&dbtPlateform, oneSection,addressStart, startAddressSource, oldPlaceCode, placeCode, &application, &profiler);



		int** insertions = (int**) malloc(sizeof(int **));
		int nbIns = getInsertionList(oneSection*1024, insertions);
		for (int oneInsertion=0; oneInsertion<nbIns; oneInsertion++){
			if (VERBOSE)
				Log::fprintf(0, stderr, "insert;%d\n", (*insertions)[oneInsertion]+(*insertions)[-1]);
		}

	}

	for (int oneUnresolvedJump = 0; oneUnresolvedJump<unresolvedJumpsArray[0]; oneUnresolvedJump++){
		unsigned int source = unresolvedJumpsSourceArray[oneUnresolvedJump+1];
		unsigned int initialDestination = unresolvedJumpsArray[oneUnresolvedJump+1];
		unsigned int type = unresolvedJumpsTypeArray[oneUnresolvedJump+1];

		unsigned char isAbsolute = ((type & 0x7f) != VEX_BR) && ((type & 0x7f) != VEX_BRF);
		unsigned int destinationInVLIWFromNewMethod = solveUnresolvedJump(&dbtPlateform, initialDestination);

		if (destinationInVLIWFromNewMethod == -1){
      Log::printf(0, "A jump from %d to %x is still unresolved... (%d insertions)\n", source, initialDestination, insertionsArray[(initialDestination>>10)<<11]);
		}
		else{
			int immediateValue = (isAbsolute) ? (destinationInVLIWFromNewMethod) : ((destinationInVLIWFromNewMethod  - source));
			writeInt(dbtPlateform.vliwBinaries, 16*(source), type + ((immediateValue & 0x7ffff)<<7));

			if (immediateValue > 0x7ffff)
				Log::fprintf(0, stderr, "error in immediate size...\n");

			unsigned int instructionBeforePreviousDestination = readInt(dbtPlateform.vliwBinaries, 16*(destinationInVLIWFromNewMethod-1)+12);
			if (instructionBeforePreviousDestination != 0)
				writeInt(dbtPlateform.vliwBinaries, 16*(source+1)+12, instructionBeforePreviousDestination);
		}
	}



	//We initialize the VLIW processor with binaries and data from elf file
	#ifndef __NIOS
	if (VERBOSE){
		dbtPlateform.vexSimulator->debugLevel = 2;
		dbtPlateform.debugLevel = 2;
	}
	#endif
//dbtPlateform.debugLevel = 2;


	//We also add information on insertions
	int insertionSize = 65536;
	int areaCodeStart=1;
	int areaStartAddress = 0;

	#ifndef __NIOS
	dbtPlateform.vexSimulator->initializeDataMemory((unsigned char*) insertionsArray, 65536*4, 0x7000000);
	#endif

	#ifndef __NIOS
	dbtPlateform.vexSimulator->initializeRun(0, localArgc, localArgv);
	#endif

	//We change the init code to jump at the correct place
	uint32 translatedStartPC = solveUnresolvedJump(&dbtPlateform, (pcStart-addressStart)>>2);
	unsigned int instruction = assembleIInstruction(VEX_CALL, translatedStartPC, 63);
	writeInt(dbtPlateform.vliwBinaries, 0, instruction);


	int runStatus=0;
	int abortCounter = 0;
	int blockScheduleCounter = 0;
	int procedureOptCounter = 0;



	while (runStatus == 0){
		runStatus = run(&dbtPlateform, 1000);
		abortCounter++;
		if (abortCounter>8000000)
			break;

		if (VERBOSE)
			Log::fprintf(0, stderr, "IPC;%f\n", dbtPlateform.vexSimulator->getAverageIPC());

		if (OPTLEVEL >= 3){
			for (int oneProcedure = 0; oneProcedure < application.numberProcedures; oneProcedure++){
				IRProcedure *procedure = application.procedures[oneProcedure];
				if (procedure->state == 0){
					Log::fprintf(0, stderr, "at entry procedure conf is %d, previous is %d\n", procedure->configuration, procedure->previousConfiguration);
					char oldPrevious = procedure->previousConfiguration;
					char oldConf = procedure->configuration;

					changeConfiguration(procedure);
					if (procedure->configuration != oldConf || procedure->configurationScores[procedure->configuration] == 0){
						IRProcedure *scheduledProcedure = rescheduleProcedure_schedule(&dbtPlateform, procedure, placeCode);
						Log::fprintf(0, stderr, "test\n");
						suggestConfiguration(procedure, scheduledProcedure);

						int score = computeScore(scheduledProcedure);
						procedure->configurationScores[procedure->configuration] = score;
						if (score > procedure->configurationScores[procedure->previousConfiguration]){
							Log::fprintf(0, stderr, "Score %d is greater than %d, changing to %d\n", score, procedure->configurationScores[procedure->previousConfiguration], procedure->configuration);
							placeCode = rescheduleProcedure_commit(&dbtPlateform, procedure, placeCode, scheduledProcedure);
						}
						else{
							procedure->configuration = procedure->previousConfiguration;
							procedure->previousConfiguration = oldPrevious;
						}
					}
					Log::fprintf(0, stderr, "at exit procedure conf is %d, previous is %d\n", procedure->configuration, procedure->previousConfiguration);

					break;
				}
			}
		}

		for (int oneBlock = 0; oneBlock<profiler.getNumberProfiledBlocks(); oneBlock++){
			int profileResult = profiler.getProfilingInformation(oneBlock);
			IRBlock* block = profiler.getBlock(oneBlock);

			if (block != NULL && OPTLEVEL >= 2 && profileResult > 20 && block->blockState == IRBLOCK_STATE_SCHEDULED){

				Log::fprintf(0, stderr, "Analyzis of %x to %x (%d to %d) for procedure building   %d \n", block->sourceStartAddress, block->sourceEndAddress, block->vliwStartAddress, block->vliwEndAddress, block->blockState);
				buildAdvancedControlFlow(&dbtPlateform, block, &application);
				block->blockState = IRBLOCK_PROC;
				buildTraces(&dbtPlateform, application.procedures[application.numberProcedures-1]);
				placeCode = rescheduleProcedure(&dbtPlateform, application.procedures[application.numberProcedures-1], placeCode);
				procedureOptCounter++;
			}


		}


		//We perform aggressive level 1 optimization: if a block takes more than 8 cycle we schedule it.
		//If it has a backward loop, we also profile it.
		for (int oneSection = 0; oneSection<numberOfSections; oneSection++){
			for (int oneBlock = 0; oneBlock<application.numbersBlockInSections[oneSection]; oneBlock++){
				IRBlock* block = application.blocksInSections[oneSection][oneBlock];

				if ((MAX_SCHEDULE_COUNT==-1 || blockScheduleCounter < MAX_SCHEDULE_COUNT) && OPTLEVEL >= 1 && block->sourceEndAddress - block->sourceStartAddress > 8  && block->blockState < IRBLOCK_STATE_SCHEDULED){
					optimizeBasicBlock(block, &dbtPlateform, &application, placeCode);
					blockScheduleCounter++;

					if (block->sourceDestination != -1 && block->sourceDestination <= block->sourceStartAddress)
						profiler.profileBlock(block);
				}

			}

		}


	}

	//We clean the last performance counters
	dbtPlateform.vexSimulator->timeInConfig[dbtPlateform.vexSimulator->currentConfig] += (dbtPlateform.vexSimulator->cycle - dbtPlateform.vexSimulator->lastReconf);






	float energyConsumption = 0;
	float period = 1.4/1000000000;
	const int lineSize = 100;
	for (int oneConfig = 0; oneConfig<32; oneConfig++){
		float timeInConfig = dbtPlateform.vexSimulator->timeInConfig[oneConfig];
		timeInConfig = timeInConfig / dbtPlateform.vexSimulator->cycle;
		Log::fprintf(0, stdout, "Conf %x\t[", oneConfig);
		int convertToPercent = timeInConfig * lineSize;
		for (int oneChar = 0; oneChar < convertToPercent; oneChar++){
			Log::fprintf(0, stdout, "|");
		}
		for (int oneChar = convertToPercent; oneChar < lineSize; oneChar++){
			Log::fprintf(0, stdout, " ");
		}
		Log::fprintf(0, stdout, "] %f  Power consumption : %f\n", timeInConfig*100, getPowerConsumption(oneConfig));
		energyConsumption += dbtPlateform.vexSimulator->timeInConfig[oneConfig] * period * getPowerConsumption(oneConfig) / 1000;
	}

	//	Log::fprintf(0, stdout, "Execution is finished...\nStatistics on the execution:\n\t Number of cycles: %ld\n\t Number of instruction executed: %ld\n\t Average IPC: %f\n\t Number of block scheduled: %d\n\t Number of procedure optimized (O2): %d\n",
	//			dbtPlateform.vexSimulator->cycle, dbtPlateform.vexSimulator->nbInstr, ((double) dbtPlateform.vexSimulator->nbInstr)/((double) dbtPlateform.vexSimulator->cycle), blockScheduleCounter, procedureOptCounter);
	//	Log::fprintf(0, stdout, "\tConfiguration used: %d\n", CONFIGURATION);
	//	Log::fprintf(0, stdout, "\tEnergy consumed: %f\n", energyConsumption);

	Log::fprintf(0, stdout, "%ld;%ld;%f;%d;%d;",	dbtPlateform.vexSimulator->cycle, dbtPlateform.vexSimulator->nbInstr, ((double) dbtPlateform.vexSimulator->nbInstr)/((double) dbtPlateform.vexSimulator->cycle), blockScheduleCounter, procedureOptCounter);
	Log::fprintf(0, stdout, "%d;", CONFIGURATION);
	Log::fprintf(0, stdout, "%f\n", energyConsumption);




	//We print profiling result
	#ifndef __NIOS
	delete dbtPlateform.vexSimulator;
  
  if (dbg)
    delete dbg;

  if (tracer)
    delete tracer;

	free(code);
	#endif

  delete localArgv;

  return 0;
}


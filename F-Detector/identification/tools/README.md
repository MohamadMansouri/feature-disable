# pin-tracer.so

**Usage: pin -t pin-tracer.so -f [feature] -i [instance] -r [run] -- [program]**

	feature: the name of the feature you are executing   
	instance: name of the input for the program
	run: number of the run
	program: executable with all its arguments    

The tool will run the program and log the trace starting from the main function. 

The output is stored in ~/disk/traces/program\_feature\_instance\_run\_[0-9]{5} directory. 
The outputs are a file containing information about the loaded images and a binary data file per thread containing all the executed basic blocks.

# profiler 

**Usage: ./profiler ~/disk/traces/[ProgramName\_\*]**

It profiles each trace of a thread and creates edges. Edges are pairs of two consequent basic blocks. Only the first appearance of an edge is save. It also collect information about each edge such as the number of times it appeared, its position in the original trace and the id of the thread the edge appeared in. The results are either stored in a mysql database or in a binary data structure files. The programs deals with syncing the sequence number of loaded libraries between different executions of the program.

Note: storing in mysql database appears to be much more slower as well as less efficient for further processing

In case of using:

- mysql database:
	- The database name is **[program]**
	- The tables names are **[feature]\_[instance]\_[run]\_thread[0-9]+**

- data structure files: 
	- The profile will be stored in the directory of the name **[program]**
	- Inside that directory you find a profile per thread in the **threads** directory


# merge 

**Usage: ./merge ~/disk/edges/[program]**

It merges all the threads' profiles of each feature execution. The results are stored in a files per feature execution at **~/disk/edges/[program]/merged**

# identify

**Usage: ./identify ~/disk/edges/[program] [feature]**

It reads all the profiles in the **merged** directory and applies:

- Diffing to find the **[feature]** unique edges
- Filtering to discard edges that are out of interest
- Identifing to find the possible **[feature]** entraces. This will initialize a static engine, namely IDA Pro (a wrapper class for the static engine exist, so its easy to replace the engine with any different one). Static information are collected and combined with the previously dynamically connected ones to identify edges that represent the possible **[feature]** entraces. 
- The results are stored at **~/disk/edges/[program]/possible**

# pin-retracer.so

**Usage: pin -t pin-retracer.so -d ~/disk/edges/[program] -- [program]**

It traces the program again to chose one of the previously identified edges. The identified edges are read from a the file  **~/disk/edges/[program]/possible**.
 



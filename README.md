drtools
=======
Note this is my working branch as such is not guarantee to work all the times.

IMPORTANT
=====================
To compile be sure to have the correct mysql libraries for client, gcc and ncurses-devel.x86_64 installed

To use the first time you must:
- go to the mysql-source directory 
	run ./configure
- go one level up 
	run make


The tool
============
The tool was initially written and maintain by Aleksandr Kuzminsky, and the code can be found at:https://launchpad.net/percona-data-recovery-tool-for-innodb 
This version is not really maintain any longer, and while is the original, it is now not including the further fix, patches, improvements coming from several developers.

The new tool official version can be found at:
https://github.com/percona/drtools


A lot of documentation can be found on the mysqlperformanceblog site about how to use the tool:
http://www.mysqlperformanceblog.com/author/akuzminsky/
Latest presentation: 
http://www.slideshare.net/marcotusa/plmce-14-be-ahero16x9final

Both will give you a good overview of the tools and how to use them one by one.


The wrapper
==================
In real life using the tools by hand is just a nightmare and not really efficient, as such I have written a wrapper that allow to process the main steps.
Obviously if you need to process some specific operation or special cases, you still need to do it by hand, but the tool can help you to reduce (90%) of the noise.

**Keep in mind that the final result will be a script:** load\_*schema\_name*.sql 

THe file will be loacated in the destination directory (that should always include the schema name) 

This script can be load to a dummy instance to recreate the loast data:

**mysql -u*user* -p -D *schema* < load\_*schema\_name*.sql**


What you need to have to run it
----------------------------------
* 1 the source files, ibdata, table spaces and so on 
* 2 an EMPTY/Dummy mysql server to use for recover the data
* 3 the DRTOOLS code
* 4 a directory where to save the files while processing them (output)


The tool is conceptually split in 3 phases.

Phase 1
---------------
- Extract data from the ibdata and identify data from the catalog. That is the basic data to identify the indexes, columns, fields.
- Load that information in the MySQL instance, in the schema DRDICTIONARY


Phase 2
---------------
- Recreate table definition from the dictionary
- Recreate the schema you are recovering in the dummy installation
- Create ALL the filters required to extract the data from the tables.

Phase 3
---------------
- Extract data from each Table/ibd identifying if table is partitioned or not, provide some information during the process, and extracting data by chunk(s).
The size of the chunk can be pass as parameter.
- create a single file containing all the LOAD command to reload the data in the schema.


The tool can be run from 0 or stop and restart from the start of a phase.

A simple example is:
./run_data_extraction.sh -v -d  employees -u stress -p tool -i 127.0.0.1 -x 3306 -s /opt/source_data -o /opt/destination_data -r /opt/drtools

Valid options are 
*  -s root_directory_for_page_files 
*  -o destination_directory_for_data extract 
*  -r executable_dir 
*  -d database name to extract 
*  -u MySQL user 
*  -p MySQL password 
*  -i MySQL IP 
*  -x MySQL Port
*  -k socket 
*  -v verbose mode  
*  -A [0|1] ask for confirmation when extracting a table  
*  -U [0|1] Unattended if set to 1 will run the whole process assuming YES is the answer to all questions 	 
*  -P Phase to start from (number):
  * 1 ibdata extract;
  * 2 compile table_def;
  * 3 run only table extraction  
*  -m recovery mode [U undeleted | D deleted]  
*  -F filter by <table_name>  
*  -S Chunk size (in MB)
 	 
 	 

Note
==============
There is much more going on in the background, and much development that can be done, so please submit the bugs, and raise ideas to make it better, we are here to do so.



v5.3

#!/bin/bash 
#-x


############
# Data extraction wrapper
#
# Author Marco Tusa 
# Copyright (C) 2001-2003, 2014
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU GUNATTENDEDeneral Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
# v1.0

ARGV="$@"
VERBOSE=0
DEBUG=0

SOURCEDIR="#"
DESTDIR="#"
EXECDIR="#"
LOCALPATH=`pwd`
DATABASE="test"
USER="#"
PASSWORD="#"
IP="#"
PORT="#"
SOCKET="#"
PHASE=0
RECOVERYMODE=U
FILTERBYTABLE="#"
ASKCONFIRMATION=0
UNATTENDED=0
CHUNKSIZE=200
CONFIRMDELDIR="n"
#
# Use LSB init script functions for printing messages, if possible
#
log_success_msg=" [ \e[32mOK\e[39m ] \n"
#"/etc/redhat-lsb/lsb_log_message success"
log_failure_msg=" [ \e[91mERROR\e[39m ] \n"
#"/etc/redhat-lsb/lsb_log_message failure"
log_warning_msg=" [ \e[93mWARNING\e[39m ] \n"
#"/etc/redhat-lsb/lsb_log_message warning"


usage() {
printf "Valid options are \n";
printf " \t -s root_directory_for_page_files \n"
printf " \t -o destination_directory_for_data extract \n"
printf " \t -r executable_dir \n"
printf " \t -d database name to extract \n"
printf " \t -u MySQL user \n"
printf " \t -p MySQL password \n"
printf " \t -i MySQL IP \n"
printf " \t -x MySQL Port\n"
printf " \t -k socket \n"
printf " \t -v verbose mode  \n"
printf " \t -A [0|1] ask for confirmation when extracting a table  \n"
printf " \t -U [0|1] Unattended if set to 1 will run the whole process assuming YES is the answer to all questions  \n"
printf " \t -P Phase to start from:\n \t1 ibdata extract;\n \t2 compile table_def;\n \t3 run only table extraction  \n"
printf " \t -m recovery mode [U undeleted | D deleted]  \n"
printf " \t -F filter by <table_name>  \n"
printf "run_data_extraction.sh -v -A 1 -F salaries -d employees -u stress -p tool -i 192.168.0.35 -x 5510 -s /home/mysql/instances/my56test1_recovery_PLMC -o /home/mysql/instances/my56test1_recovery_PLMC -r /home/mysql/recoverycode"
printf "\n"
}


if [ $# -lt 6 ] ; then
	echo 'Too few arguments supplied'
	usage
	exit 1
fi

#shift 1
    echo "###################################################"
while getopts ":s:o:r:d:u:p:i:x:k:P:r:F:A:U:S:v" opt; do
  case $opt in
    v)
       VERBOSE=1
       ;;
    A)
       ASKCONFIRMATION=1;
       if [ $VERBOSE -eq 1  ] ; then
           echo "ASKCONFIRMATION active"
       fi
       ;;
    U)
       UNATTENDED=1;
       if [ $VERBOSE -eq 1  ] ; then
           echo "UNATTENDED active"
       fi
       ;;

    s)
       SOURCEDIR=$OPTARG 

       if [ $VERBOSE -eq 1  ] ; then
           echo "Source DIR is set to $SOURCEDIR"
       fi
       ;;
    o)
       DESTDIR=$OPTARG

       if [ $VERBOSE -eq 1  ] ; then
           echo "Destination DIR is set to $DESTDIR"
       fi
       ;;
    r)
       EXECDIR=$OPTARG
       if [ $VERBOSE -eq 1  ] ; then
           echo "Execution DIR is set to $EXECDIR"
       fi
       ;;
    d)
       DATABASE=$OPTARG
       if [ $VERBOSE -eq 1  ] ; then
           echo "Database to extract is set to $DATABASE"
       fi
       ;;
    u)
       USER=$OPTARG
       if [ $VERBOSE -eq 1  ] ; then
           echo "USER to extract is set to $USER"
       fi
       ;;
    p)
       PASSWORD=$OPTARG
       if [ $VERBOSE -eq 1  ] ; then
           echo "PASSWORD to extract is set to $PASSWORD"
       fi
       ;;
    i)
       IP=$OPTARG
       if [ $VERBOSE -eq 1  ] ; then
           echo "IP to extract is set to $IP"
       fi
       ;;

    x)
       PORT=$OPTARG
       if [ $VERBOSE -eq 1  ] ; then
           echo "PORT to extract is set to $PORT"
       fi
       ;;

    k)
       SOCKET=$OPTARG
       if [ $VERBOSE -eq 1  ] ; then
           echo "SOCKET to extract is set to $SOCKET"
       fi
       ;;
      
    P)
       PHASE=$OPTARG
       if [ $VERBOSE -eq 1  ] ; then
           echo "Running From phase $PHASE"
       fi
       ;;
       
    m)
       RECOVERYMODE=$OPTARG
       if [ $VERBOSE -eq 1  ] ; then
           echo "Running using recovery mode $RECOVERYMODE"
       fi
       ;;
    F)
       FILTERBYTABLE=$OPTARG
       if [ $VERBOSE -eq 1  ] ; then
           echo "Filtering the table extraction by table name $FILTERBYTABLE"
       fi
       ;;
    S)
       CHUNKSIZE=$OPTARG
       if [ $VERBOSE -eq 1  ] ; then
           echo "Chunk size is: $CHUNKSIZE"
       fi
       ;;


    \?)
       echo "Invalid option: -$OPTARG" >&2
       usage
       exit 1
       ;;
    :)
      echo "Option -$OPTARG requires an argument." >&2
      exit 1
      ;;
  esac
done

     CONNECTIONPAR=""
     CONNECTIONPAR_C=""
        
     if [ $SOCKET = "#" ]
     then
         CONNECTIONPAR=" --user=$USER --password=$PASSWORD --host=$IP --port=$PORT "
         CONNECTIONPAR_C=" -u $USER -p $PASSWORD -h $IP -P $PORT "
     else
	 echo "Must use TCP/IP connection to mysql"
	 exit 1
         CONNECTIONPAR=" --user=$USER --password=$PASSWORD --socket=$SOCKET"
     fi



    TIMENOW=`date`;
    echo "Local DIR is set to $LOCALPATH"
    echo "###################################################"
    echo "Process STARTs ${TIMENOW}"
    echo "###################################################"
    if [ ! -e $DESTDIR ]
    then
        echo "Destination directory not present I will create it";
        `mkdir -p $DESTDIR`
        echo "---------------------------"
    
    fi
    
    if [ $PHASE -lt 2 ]
    then
        echo "PHASE 1 ---------------------------"    
        echo "Clean destination directory from ANY content please confirm [yes] full word"
        echo -n "Destination directory to clean: $DESTDIR:  [yes/no] --> "
        read CONFIRMDELDIR

        if [ "x${CONFIRMDELDIR}" !=  "xyes" ] 
        then
	    echo "NON empty directory, possible data over-write!!"
           
        else
             `rm -fr $DESTDIR/*`
            
        fi
	# I set the UNATTENDED only after the first queston given it may be too dangerous and can bring to data loss
        if [ ${UNATTENDED} -eq 1 ]
        then
            CONFIRMDELDIR="y"
        fi

 
            
        echo "Processing main IBDATA file  "
      
        cd $DESTDIR;
        echo "current directory: `pwd`"
        echo "${EXECDIR}/page_parser -f ${SOURCEDIR}/ibdata1 \n"
        `time ${EXECDIR}/page_parser -f ${SOURCEDIR}/ibdata1`
        
        TOCHANGE=`ls -d page*`;
        `mv $DESTDIR/$TOCHANGE $DESTDIR/ibdata`
        TOCHANGE=""
        
        cd $EXECDIR
        if [ ! -e  "${EXECDIR}/bin/constraints_parser.SYS_TABLES" ]
        then
            printf "\nCompile before running \n"
            `time make dict_parsers 1>&2>/dev/null`
        fi
    
        printf "\nPrecessing SYS_TABLES & SYS_INDEXES\n"
        
        cd "${DESTDIR}/ibdata";
        
        echo "Current Path `pwd`"
        
        `${EXECDIR}/bin/constraints_parser.SYS_TABLES  -p${DESTDIR}/ibdata -4Uf FIL_PAGE_INDEX/0-1 > ${DESTDIR}/ibdata/SYS_TABLES 2> ${DESTDIR}/ibdata/load_dictionary.sql`
        `${EXECDIR}/bin/constraints_parser.SYS_INDEXES -p${DESTDIR}/ibdata -4Uf FIL_PAGE_INDEX/0-3 > ${DESTDIR}/ibdata/SYS_INDEXES 2>> ${DESTDIR}/ibdata/load_dictionary.sql`
	`${EXECDIR}/bin/constraints_parser.SYS_COLUMNS -p${DESTDIR}/ibdata -4Uf FIL_PAGE_INDEX/0-2 > ${DESTDIR}/ibdata/SYS_COLUMNS 2>> ${DESTDIR}/ibdata/load_dictionary.sql`
	`${EXECDIR}/bin/constraints_parser.SYS_FIELDS  -p${DESTDIR}/ibdata -4Uf FIL_PAGE_INDEX/0-4 > ${DESTDIR}/ibdata/SYS_FIELDS 2>> ${DESTDIR}/ibdata/load_dictionary.sql`
        
        echo "---------------------------"        

        echo -n "Please check if the extracted structure is correct look in: $DESTDIR [y/n]  --> "
        if [ ${UNATTENDED} -eq 0 ]
	then
	    read CONFIRMDELDIR
	fi

	if [ "x${CONFIRMDELDIR}" == "xy" ]
        then
            echo "Cool continue then"
        else
            echo "Check what is the problem and run me again"
            exit 0 
        fi 
        echo "---------------------------"
        echo -n "(Re)Create the dictionary?: $DESTDIR [y/n]  --> "
        if [ ${UNATTENDED} -eq 0 ]
        then
            read CONFIRMDELDIR
        fi
        

        if [ "x${CONFIRMDELDIR}" == "xy" ]
        then
	  echo "Loading dictionary information ...";
	  `mysql $CONNECTIONPAR -D mysql -e "DROP schema if exists DRDICTIONARY"`;
	  `mysql $CONNECTIONPAR -D mysql -e "create schema if not exists DRDICTIONARY"`;
	  `mysql $CONNECTIONPAR -D mysql -D DRDICTIONARY < ${EXECDIR}/dictionary.sql`
	  `mysql $CONNECTIONPAR -D DRDICTIONARY <  ${DESTDIR}/ibdata/load_dictionary.sql`;
	  echo "Loading dictionary information ... COMPLETED";
        else
            echo "Check what is the problem and run me again"
            exit 0 
        fi 
        echo "---------------------------"
        
    fi
    
# Creating include files
#USER="#"
#PASSWORD="#"
#IP="#"
#PORT="#"
#SOCKET="#"

    if [ $PHASE -lt 3 ]
    then
        echo "PHASE 2 ---------------------------"
        `rm -f $EXECDIR/include/*.defrecovery`;

        for schema in  $DATABASE ;
        do
	    SCHEMA_RECOVERY="${schema}"
            echo -n "Should I recreate the structure for SCHEMA = $SCHEMA_RECOVERY ?[y/n]  --> "
	    if [ ${UNATTENDED} -eq 0 ]
       	    then
           	 read CONFIRMDELDIR
       	    fi
	   
	    if [ "x${CONFIRMDELDIR}" == "xy" ]
	    then
		`mysql $CONNECTIONPAR -D mysql -e "create schema if not exists ${SCHEMA_RECOVERY}"`;
		echo "EXECUTING sys_parser.... $EXECDIR/sys_parser $CONNECTIONPAR_C -d DRDICTIONARY -r 1 $schema"

		if [ $VERBOSE -eq 1  ] ; then
		  echo "$EXECDIR/sys_parser $CONNECTIONPAR_C -d DRDICTIONARY -r 1 $schema 1> ${DESTDIR}/${schema}_definition.sql"
		  echo "mysql $CONNECTIONPAR -D $SCHEMA_RECOVERY <  ${DESTDIR}/${schema}_definition.sql"
		fi

		`$EXECDIR/sys_parser $CONNECTIONPAR_C -d DRDICTIONARY -r 1 $schema 1> ${DESTDIR}/${schema}_definition.sql`;
		 `mysql $CONNECTIONPAR -D $SCHEMA_RECOVERY <  ${DESTDIR}/${schema}_definition.sql`;

		  echo -n "Please check the status of the SCHEMA = $schema and press [y] to continue or [n] to exit ?[y/n]  --> "
	          if [ ${UNATTENDED} -eq 0 ]
        	  then
	            read CONFIRMDELDIR
        	  fi
		  


		  if [ "x${CONFIRMDELDIR}" == "xy" ]
		  then
		     echo  "Continue......... to create table_def";		 
		  else
		    exit 1;
		  fi
	    else
		echo "Ok I assume that SCHEMA = $schema is already there";
		 
	    fi 
         
        
        
            for table in `find $SOURCEDIR/$schema/ -name *.ibd  -exec basename {} \;|awk -F '.' '{print $1}' `;
            do
                TABLEDEFINITIONNAME=""
                PARTITIONINDEX=0
                PARTITIONINDEX=`expr index "$table" \#`
                #echo "PINDEX = $PARTITIONINDEX"
                
                if [ $PARTITIONINDEX -gt 0 ]
                then
                    
                    TABLEFILTERNAME=${table:0:($PARTITIONINDEX - 1)}"#_XXX_PARTITIONED__XXX_"${table:($PARTITIONINDEX - 1)}
                    TABLEDEFINITIONNAME=${table:0:($PARTITIONINDEX - 1)}
                    
                
                else
                    TABLEDEFINITIONNAME=$table
                fi
                
                if [ $VERBOSE -eq 1  ] ; then
                    echo "Creating definition for table ($table) : $SOURCEDIR/$schema/$TABLEDEFINITIONNAME"
                    
                fi
                
                DEFINITIONFILE="${EXECDIR}/include/table_defs.${schema}.${TABLEDEFINITIONNAME}.defrecovery"
                #echo $DEFINITIONFILE
    
                `$EXECDIR/create_defs.pl $CONNECTIONPAR --db=$schema --table=$TABLEDEFINITIONNAME > $DEFINITIONFILE`
                
                if [ $PARTITIONINDEX -gt 0 ]
                then
                    #echo "${EXECDIR}/include/table_defs.${schema}.${TABLEFILTERNAME}.defrecovery"
                    `echo "" > ${EXECDIR}/include/table_defs.${schema}.${TABLEFILTERNAME}.defrecovery`
                fi    
                
            done;
        done;
    
    
        echo -n "Please check if the extracted table definition is correct look in: $EXECDIR/include/*.defrecovery [y/n]  --> "
	 if [ ${UNATTENDED} -eq 0 ]
         then
                read CONFIRMDELDIR
 	 else
		CONFIRMDELDIR="y"
         fi


        if [ "x${CONFIRMDELDIR}" == "xy" ]
        then
            echo "Cool continue then"
        else
            echo "Check what is the problem and run me again"
            exit 0 
        fi
        echo "---------------------------"
        
        
    fi
# Starting the extraction process


    if [ $PHASE -lt 4 ]
    then
        echo "PHASE 3 ---------------------------"
        #for schematable in `find $EXECDIR/include/ -name *.defrecovery  -exec basename {} \\;`
        
        
        for schematable in `ls $EXECDIR/include/*.defrecovery | xargs -n1 basename`
        do
    #read the table name and schema
    
                SCHEMA=`echo $schematable|awk -F '.' '{print $2}' `;
                TABLE=`echo $schematable|awk -F '.' '{print $3}' `;
                PARTITIONINDEX=`expr index "$TABLE" \#`
                ISPARTITIONED=0
    
                PARTITIONFLAG=$(expr match "$TABLE"  '.*\(XXX_PARTITIONED__XXX\)')
                
                if [ "#$PARTITIONFLAG" = "#XXX_PARTITIONED__XXX" ]
                then
                    ISPARTITIONED=1
                fi
    
                if [ $ISPARTITIONED -gt 0 ]
                then
                    TABLEFILTERNAME=${TABLE:0:($PARTITIONINDEX -1 )}
                    schematable="table_defs.${SCHEMA}.${TABLEFILTERNAME}.defrecovery"
                    TABLE=`echo $TABLE|sed -e"s/#_XXX_PARTITIONED__XXX_//g" `
                fi
    
		if [ "#$FILTERBYTABLE" != "##" ]
		then
		    if [ "#$FILTERBYTABLE" != "#$TABLEFILTERNAME" ]
		    then
			continue;
		    fi
		
		fi
    
                `rm -f $EXECDIR/include/table_defs.h`;
                `ln -s $EXECDIR/include/$schematable $EXECDIR/include/table_defs.h`;
    
                if [ $VERBOSE -eq 1  ] ; then
                        echo "Original Name = $schematable" ;   
                        echo "Schema Name = $SCHEMA" ;
                        echo "Table Name = $TABLE" ;
                        echo "Table is Partitioned = $ISPARTITIONED" ;
                        LINK=`readlink -f ${EXECDIR}/include/table_defs.h`
                        echo "active definition link:${LINK}"
                        echo "Active Schema/Table ${schematable}"
                        
                fi
                echo "Compile for table = $TABLE [SCHEMA=${SCHEMA}]";
                
                #set dir and compile (after havign set the link            
                cd $EXECDIR
                
                #echo""
                #printf " OLD %s   NEW %s\n" "$OLDDEFINITION" "$LINK" 
                #echo""
                
                if [ "#${OLDDEFINITION}" = "#${LINK}" ]
                then
                    
                    if [ $VERBOSE -eq 1  ] ; then
                        echo "Skipping Compilation given previous table definition and current are the same"
                        echo " Old Definition = ${OLDDEFINITION}"
                        echo " Curren Definition = ${LINK}"
                    fi 
                else    
                    if [ $VERBOSE -eq 1  ] ; then
                    echo ""
                       `make`;
                    else
                       `make  1>&2>/dev/null`;             
                    fi
                fi
		
		if [ ${ASKCONFIRMATION} -eq 1 ]
		then
		  
		  while [ 1=1 ]
		  do
		    CONFIRMDELDIR="xn";
		    echo -n "Process next Table ${TABLE}? [y/n]  --> "
        	    read CONFIRMDELDIR
		    if [ "#${CONFIRMDELDIR}" == "#y" ]
		    then
			echo "Cool continue then ${CONFIRMDELDIR}"
			break;
		    else
		      echo "Waiting ..";
		    fi
		  done  
		fi
                
                OLDDEFINITION=$LINK
                
                echo "Compilation for table = $TABLE [SCHEMA=${SCHEMA}] COMPLETE";
                
                #Read table ID from SYS_TABLE
                ESCTABLE=${TABLE//_/.\\_};
                ESCSCHEMA=${SCHEMA//_/.\\_};
            
                TABLEID=`cat ${DESTDIR}/ibdata/SYS_TABLES|grep -e "SYS_TABLES.\"${ESCSCHEMA}/${ESCTABLE}\""|sed -e"s/\t/,/g"|awk -F ',' '{print $5}'|head -n 1`;
                INDEXID=`cat ${DESTDIR}/ibdata/SYS_INDEXES|grep -e "SYS_INDEXES.${TABLEID}"|grep PRIMARY|sed -e"s/\t/,/g"|awk -v r=${TABLEID} -F ',' '{if($4==r){print $5}else{print $4 $5}}'|head -n 1`;
                
#                if [ $VERBOSE -eq 1  ] ; then
#                  echo "cat ${DESTDIR}/ibdata/SYS_TABLES|grep -e \"SYS_TABLES.\"${ESCSCHEMA}/${ESCTABLE}\"\"|sed -e\"s/\t/,/g\"|awk -F ',' '{print \$5}'"
#		  echo "cat ${DESTDIR}/ibdata/SYS_INDEXES|grep -e \"SYS_INDEXES.${TABLEID}\"|grep PRIMARY|sed -e\"s/\t/,/g\"|awk -F ',' '{if(\$4 == "${TABLEID}"){print \$5}else{exit 1}}'";
#                fi
                echo "Processing TableId $TABLEID with PK ID $INDEXID";
                
                
                FILETOPARSE=${SOURCEDIR}/${SCHEMA}/${TABLE}.ibd
                if [ ! -e $FILETOPARSE ]
                then
                    printf "The file $FILETOPARSE is not found please check the path and try again\n";
                    continue
                fi
    
    #Parsing IBD file for the given table
    
                echo "Parsing file $FILETOPARSE";
                cd $DESTDIR
                echo "Current dir:`pwd`"
                
                `time ${EXECDIR}/page_parser -f ${SOURCEDIR}/${SCHEMA}/${TABLE}.ibd`
                
                TOCHANGE=`ls -d page*`;
                `mv $DESTDIR/$TOCHANGE $DESTDIR/${SCHEMA}_${TABLE}`
                
                if [ ! -e ${DESTDIR}/load_${SCHEMA}.sql ]
                then
                  echo "Create SQL create file for ${SCHEMA} \n"
		  touch ${DESTDIR}/load_${SCHEMA}.sql
                fi
                
                printf "\n -------------------------- \n"   ;
                TIMENOW=`date`;
    #extracting data
                echo "Starting data extraction ${SCHEMA}_${TABLE} ${TIMENOW}";
                
                echo "${EXECDIR}/constraints_parser -p${DESTDIR} -5${RECOVERYMODE}f $DESTDIR/${SCHEMA}_${TABLE}/FIL_PAGE_INDEX/0-${INDEXID} -b $DESTDIR/${SCHEMA}_${TABLE}/FIL_PAGE_TYPE_BLOB/ -S $CHUNKSIZE -o $DESTDIR/DATA_${SCHEMA}_${TABLE}"
                
                `time ${EXECDIR}/constraints_parser -s ${SCHEMA} -p${DESTDIR} -5${RECOVERYMODE}f $DESTDIR/${SCHEMA}_${TABLE}/FIL_PAGE_INDEX/0-${INDEXID} -b $DESTDIR/${SCHEMA}_${TABLE}/FIL_PAGE_TYPE_BLOB/ -S $CHUNKSIZE -o $DESTDIR/DATA_${SCHEMA}_${TABLE} 2>> ${DESTDIR}/load_${SCHEMA}.sql`
                
                TIMENOW=`date`;
                echo "Data extraction ENDS ${SCHEMA}_${TABLE} ${TIMENOW}";
                
            printf "\n -------------------------- \n\n" 
    
        done;
        
    fi
    echo "###################################################"
    echo "Process ENDs ${TIMENOW}"
    echo "###################################################"
    
    
    cd $LOCALPATH;
    

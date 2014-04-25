#!/bin/bash

set -e
mysql_user="`whoami`"
mysql_password=""
mysql_host="localhost"
mysql_db="sakila"
work_dir="/tmp/recovery_$RANDOM"
top_dir="`pwd`"

# Check that the script is run from source directory
if ! test -f "$top_dir/page_parser.c"
then
	echo "Script $0 must be run from a directory with Percona InnoDB Recovery Tool source code"
	exit 1
fi

echo -n "Initializing working directory... "
if test -d "$work_dir"
then
	echo "Directory $work_dir must not exist. Remove it and restart $0"
	exit 1
fi

mkdir "$work_dir"
cd "$work_dir"
trap "if [ $? -ne 0 ] ; then rm -r \"$work_dir\"; fi" EXIT
echo "OK"


echo -n "Downloading sakila database... "
wget -q "http://downloads.mysql.com/docs/sakila-db.tar.gz"
#wget -q "http://localhost/sakila-db.tar.gz"
tar zxf sakila-db.tar.gz
echo "OK"

echo -n "Testing MySQL connection... "
if test -z "$mysql_password"
then
	MYSQL="mysql -u$mysql_user -h $mysql_host"
else
	MYSQL="mysql -u$mysql_user -p$mysql_password -h $mysql_host"
fi
$MYSQL -e "SELECT COUNT(*) FROM user" mysql >/dev/null
has_innodb=`$MYSQL -e "SHOW ENGINES"| grep InnoDB| grep -e "YES" -e "DEFAULT"`
if test -z "$has_innodb"
then
	echo "InnoDB is not enabled on this MySQL server"
	exit 1
fi
echo "OK"

echo -n "Creating sakila database... "
$MYSQL -e "CREATE DATABASE IF NOT EXISTS $mysql_db"
$MYSQL -e "CREATE DATABASE IF NOT EXISTS ${mysql_db}_recovered"
$MYSQL $mysql_db < sakila-db/sakila-schema.sql
$MYSQL $mysql_db < sakila-db/sakila-data.sql
# Wait till all pages are flushed on disk
lsn=0
lsn_checkpoint=1
while [ "$lsn" != "$lsn_checkpoint" ]
do
	lsn=`$MYSQL -NB -e "SHOW ENGINE INNODB STATUS\G"| grep "Log sequence number"| awk '{ print $NF}'`
	lsn_checkpoint=`$MYSQL -NB -e "SHOW ENGINE INNODB STATUS\G"| grep "Last checkpoint at"| awk '{ print $NF}'`
	sleep 1
done
echo "OK"

echo -n "Counting sakila tables checksums... "
tables=`$MYSQL -NB -e "SELECT TABLE_NAME FROM TABLES WHERE TABLE_SCHEMA='$mysql_db' and ENGINE='InnoDB'" information_schema`
i=0
for t in $tables
do
	checksum[$i]=`$MYSQL -NB -e "CHECKSUM TABLE $t" $mysql_db| awk '{ print $2}'`
	let i=$i+1
done
echo "OK"

echo -n "Building InnoDB dictionaries parsers... "
cd "$top_dir"
make dict_parsers > "$work_dir/make.log" 2>&1
cp page_parser bin
cd "$work_dir"
echo "OK"

# Get datadir
datadir="`$MYSQL  -e "SHOW VARIABLES LIKE 'datadir'" -NB | awk '{ $1 = ""; print $0}'| sed 's/^ //'`"
innodb_file_per_table=`$MYSQL  -e "SHOW VARIABLES LIKE 'innodb_file_per_table'" -NB | awk '{ print $2}'`
innodb_data_file_path=`$MYSQL  -e "SHOW VARIABLES LIKE 'innodb_data_file_path'" -NB | awk '{ $1 = ""; print $0}'| sed 's/^ //'`

echo "Splitting InnoDB tablespace into pages... "
old_IFS="$IFS"
IFS=";"
for ibdata in $innodb_data_file_path
do
	ibdata_file=`echo $ibdata| awk -F: '{print $1}'`
	"$top_dir"/bin/page_parser -f "$datadir/$ibdata_file"
	mv pages-* "pages-$ibdata_file"
done
IFS=$old_IFS
if [ $innodb_file_per_table == "ON" ]
then
	for t in $tables
	do
		"$top_dir"/bin/page_parser -f "$datadir/$mysql_db/$t.ibd"
		mv pages-[0-9]* "pages-$t"
	done
fi
echo "OK"

echo -n "Recovering InnoDB dictionary... "
old_IFS="$IFS"
IFS=";"
for ibdata in $innodb_data_file_path
do
	ibdata_file=`echo $ibdata| awk -F: '{print $1}'`
	dir="pages-$ibdata_file"/FIL_PAGE_INDEX/0-1
	mkdir -p "dumps/${mysql_db}_recovered"
	if test -d "$dir"
	then
		"$top_dir"/bin/constraints_parser.SYS_TABLES -4f "$dir" -p "${mysql_db}_recovered" >> "dumps/${mysql_db}_recovered/SYS_TABLES" 2>SYS_TABLES.sql
	fi
	dir="pages-$ibdata_file"/FIL_PAGE_INDEX/0-2
	if test -d "$dir"
	then
		"$top_dir"/bin/constraints_parser.SYS_COLUMNS -4f "$dir" -p "${mysql_db}_recovered" >> "dumps/${mysql_db}_recovered/SYS_COLUMNS" 2>SYS_COLUMNS.sql
	fi
	dir="pages-$ibdata_file"/FIL_PAGE_INDEX/0-3
	if test -d "$dir"
	then
		"$top_dir"/bin/constraints_parser.SYS_INDEXES -4Uf "$dir" -p "${mysql_db}_recovered" >> "dumps/${mysql_db}_recovered/SYS_INDEXES" 2>SYS_INDEXES.sql
	fi
	dir="pages-$ibdata_file"/FIL_PAGE_INDEX/0-4
	if test -d "$dir"
	then
		"$top_dir"/bin/constraints_parser.SYS_FIELDS -4Uf "$dir" -p "${mysql_db}_recovered" >> "dumps/${mysql_db}_recovered/SYS_FIELDS" 2>SYS_FIELDS.sql
	fi

done
IFS=$old_IFS
$MYSQL -e "DROP TABLE IF EXISTS \`SYS_INDEXES\`; CREATE TABLE \`SYS_INDEXES\` (
	\`TABLE_ID\` bigint(20) unsigned NOT NULL default '0',
	\`ID\` bigint(20) unsigned NOT NULL default '0',
	\`NAME\` varchar(120) default NULL,
	\`N_FIELDS\` int(10) unsigned default NULL,
	\`TYPE\` int(10) unsigned default NULL,
	\`SPACE\` int(10) unsigned default NULL,
	\`PAGE_NO\` int(10) unsigned default NULL,
	PRIMARY KEY  (\`TABLE_ID\`,\`ID\`)
	) ENGINE=InnoDB DEFAULT CHARSET=latin1" ${mysql_db}_recovered
$MYSQL -e "DROP TABLE IF EXISTS \`SYS_TABLES\`; CREATE TABLE \`SYS_TABLES\` (
	\`NAME\` varchar(255) NOT NULL default '',
	\`ID\` bigint(20) unsigned NOT NULL default '0',
	\`N_COLS\` int(10) default NULL,
	\`TYPE\` int(10) unsigned default NULL,
	\`MIX_ID\` bigint(20) unsigned default NULL,
	\`MIX_LEN\` int(10) unsigned default NULL,
	\`CLUSTER_NAME\` varchar(255) default NULL,
	\`SPACE\` int(10) unsigned default NULL,
	PRIMARY KEY  (\`NAME\`)
	) ENGINE=InnoDB DEFAULT CHARSET=latin1" ${mysql_db}_recovered
$MYSQL -e "DROP TABLE IF EXISTS \`SYS_COLUMNS\`; CREATE TABLE \`SYS_COLUMNS\` (
	\`TABLE_ID\` bigint(20) unsigned NOT NULL,
	\`POS\` int(10) unsigned NOT NULL,
	\`NAME\` varchar(255) DEFAULT NULL,
	\`MTYPE\` int(10) unsigned DEFAULT NULL,
	\`PRTYPE\` int(10) unsigned DEFAULT NULL,
	\`LEN\` int(10) unsigned DEFAULT NULL,
	\`PREC\` int(10) unsigned DEFAULT NULL,
	PRIMARY KEY (\`TABLE_ID\`,\`POS\`)
	) ENGINE=InnoDB DEFAULT CHARSET=latin1" ${mysql_db}_recovered
$MYSQL -e "DROP TABLE IF EXISTS \`SYS_FIELDS\`; CREATE TABLE \`SYS_FIELDS\` (
	\`INDEX_ID\` bigint(20) unsigned NOT NULL,
	\`POS\` int(10) unsigned NOT NULL,
	\`COL_NAME\` varchar(255) DEFAULT NULL,
        PRIMARY KEY (\`INDEX_ID\`,\`POS\`)
	) ENGINE=InnoDB DEFAULT CHARSET=latin1" ${mysql_db}_recovered

$MYSQL ${mysql_db}_recovered < SYS_TABLES.sql
$MYSQL ${mysql_db}_recovered < SYS_INDEXES.sql
$MYSQL ${mysql_db}_recovered < SYS_COLUMNS.sql
$MYSQL ${mysql_db}_recovered < SYS_FIELDS.sql
echo "OK"

echo -n "Recovering tables structure from InnoDB dictionary ... "
for t in $tables
do
	$MYSQL -e "DROP TABLE IF EXISTS \`$t\`" ${mysql_db}_recovered
	"$top_dir"/sys_parser -u $mysql_user -p "$mysql_password" -d ${mysql_db}_recovered ${mysql_db}/$t | $MYSQL ${mysql_db}_recovered
done
echo "OK"

echo -n "Recovering tables... "
for t in $tables
do
	"$top_dir"/create_defs.pl --host $mysql_host --user $mysql_user --password "$mysql_password" --db ${mysql_db}_recovered --table $t > "$top_dir"/include/table_defs.h.$t
	ln -fs table_defs.h.$t "$top_dir"/include/table_defs.h
	make -C "$top_dir" clean all > /dev/null 2>&1
	# get index id
	index_id=`$MYSQL -NB -e "SELECT SYS_INDEXES.ID FROM SYS_INDEXES JOIN SYS_TABLES ON SYS_INDEXES.TABLE_ID = SYS_TABLES.ID WHERE SYS_TABLES.NAME= '${mysql_db}/$t' ORDER BY ID LIMIT 1" ${mysql_db}_recovered`
	# get row format
	Row_format=`$MYSQL -NB -e "SHOW TABLE STATUS LIKE '$t'" ${mysql_db}| awk '{print $4}'`
	if [ "$Row_format" == "Compact" ]
	then
		Row_format_arg="-5"
	else
		Row_format_arg="-4"
	fi
	if [ $innodb_file_per_table == "ON" ]
	then
		"$top_dir"/constraints_parser $Row_format_arg -Uf "pages-$t/FIL_PAGE_INDEX/0-$index_id" -p "${mysql_db}_recovered" -b "pages-$t/FIL_PAGE_TYPE_BLOB" > "dumps/${mysql_db}_recovered/$t" 2> $t.sql
	else
		old_IFS="$IFS"
		IFS=";"
		for ibdata in $innodb_data_file_path
		do
			ibdata_file=`echo $ibdata| awk -F: '{print $1}'`
			dir="pages-$ibdata_file"/FIL_PAGE_INDEX/0-$index_id
			if test -d "$dir"
			then
				"$top_dir"/constraints_parser $Row_format_arg -Uf "$dir" -p "${mysql_db}_recovered" -b "pages-$ibdata_file/FIL_PAGE_TYPE_BLOB" >> "dumps/${mysql_db}_recovered/$t" 2> $t.sql
			fi

		done
		IFS="$old_IFS"
	fi
done
echo "OK"

echo -n "Loading recovered data into MySQL... "
for t in $tables
do
	$MYSQL -e "DROP TABLE IF EXISTS \`$t\`;CREATE TABLE $t like ${mysql_db}.$t" ${mysql_db}_recovered
	$MYSQL ${mysql_db}_recovered < $t.sql
done

echo "OK"

echo "Verifying tables checksum... "
i=0
for t in $tables
do
	echo -n "$t ... "
	chksum_recovered=`$MYSQL -NB -e "CHECKSUM TABLE $t" ${mysql_db}_recovered| awk '{ print $2}'`
	if [ "$chksum_recovered" == ${checksum[$i]} ]
	then
		echo "OK"
	else
		echo "NOT OK"
	fi
        let i=$i+1

done

cd "$top_dir"
ln -fs table_defs.h.SYS_TABLES include/table_defs.h
echo "DONE"

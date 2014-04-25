#set -x
pages="pages-xxxxx"
len=0
for d in `cat dbs`
do
	dbtbl=`mysql -NBe "SELECT NAME FROM SYS_TABLES WHERE NAME LIKE '$d/%'" test`
	for dt in $dbtbl
	do
		l="`echo $dt | wc -c`"
		if [ $l -gt $len ]; then len=$l; fi
	done
done
		printf "%${len}s %8s %8s %16s %6s %6s\n" "Table" "Index" "Dir" "Recs,d/t" "Lost" "Complete"
for d in `cat dbs`
do
	dbtbl=`mysql -NBe "SELECT NAME FROM SYS_TABLES WHERE NAME LIKE '$d/%'" test`
	for dt in $dbtbl
	do
		t=`echo $dt | awk -F/ '{ print $2}'`
		index_id=""
		table_id=`mysql -NBe "SELECT ID FROM SYS_TABLES WHERE NAME = '$d/$t'" test`
		if ! test -z "$table_id"
		then
			index_id=`mysql -NBe "SELECT ID FROM SYS_INDEXES WHERE TABLE_ID = '$table_id' ORDER BY ID LIMIT 1" test`
		fi
		dir="N"
		if test -d "$pages/FIL_PAGE_INDEX/0-$index_id"
		then
			dir="Y"
		fi
		dump="N"
		recs="0"
		lost="NA"
		if test -f "dumps/$d/$t"
		then
			dump="Y"
			recs="`grep -v '^--' dumps/$d/$t | wc -l`"
			recs_a="`mysql -NBe "select count(*) from $d.$t"`"
			if test -z "`cat dumps/$d/$t | grep "Lost records: YES"| grep -v "Leaf page: NO"`"
			then
				lost="NO"
			else
				lost="YES"
			fi
		fi
		complete="NO"
		index_id_ok="NO"
		if ! test -z "$index_id"
		then
			index_id_ok="OK"
			if [ "`./index_chk -f $pages/FIL_PAGE_INDEX/0-$index_id`" = "OK" ] ; then complete="YES"; fi
		fi
		printf "%${len}s %8s %8s %16s %6s %6s\n" $d/$t $index_id_ok    $dir    $recs/$recs_a   $lost   $complete
	done
done

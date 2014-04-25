set -ex
ts=`date +%F_%T`
pages=pages-xxxxxx

for d in `cat dbs`
do
	mkdir -p dumps/$d
	for t in `mysql -NBe "SELECT TABLE_NAME FROM TABLES WHERE TABLE_SCHEMA='$d' AND ENGINE='InnoDB'" information_schema`
	do
		echo $d/$t
		table_id=`mysql -NBe "SELECT ID FROM SYS_TABLES WHERE NAME = '$d/$t'" test`
		if ! test -z "$table_id"
		then
			index_id=`mysql -NBe "SELECT ID FROM SYS_INDEXES WHERE TABLE_ID = '$table_id' ORDER BY ID LIMIT 1" test`
			if ! test -z "$index_id"
			then
				if test -d "$pages/FIL_PAGE_INDEX/0-$index_id"
				then
					ln -fs table_defs.h.$d.$t include/table_defs.h
					make clean all > /dev/null 2>&1 
					./constraints_parser -5f $pages/FIL_PAGE_INDEX/0-$index_id -b $pages/FIL_PAGE_TYPE_BLOB/ -p $d > dumps/$d/$t 2> dumps/$d/$t.sql
				else
					echo "$d/$t $index_id" >> nf_dir_id.$ts
				fi
			else
				echo "$d/$t" >> nf_i_id.$ts
			fi
		else
			echo "$d/$t" >> nf_t_id.$ts
		fi

	done
done

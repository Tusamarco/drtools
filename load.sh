set -ex
for d in `cd dumps; ls | grep -v default`
do
	dbtbl=`mysql -NBe "SELECT NAME FROM SYS_TABLES WHERE NAME LIKE '$d/%'" test`
	for dt in $dbtbl
	do
		mysql $d < dumps/$dt.sql
	done
done

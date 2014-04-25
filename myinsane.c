#include <stdio.h>
#include <unistd.h>
#include <mysql.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <locale.h>

float count_div = 10.0;		// The difference between expected and actual number of rows in %%
float auto_inc_mismatch_limit = 80.0;	// At least auto_inc_mismatch_limit % rows must satisfy auto_inc sequence
const time_t ts_lower_limit = 631141200; // '1990-01-01' Any date/datetime type value must be more than this
const time_t ts_upper_limit = 1577829600; // '2020-01-01' Any date/datetime type value must be less than this
float max2avg_limit = 5.0;
//const time_t ts_upper_limit = 1230760800; // test value
//
int warnings = 1;		// 1 - print warnings, 0 - do not.

void usage(char* p)
{
	fprintf(stderr,"Usage: %s [-h host] [-P port] [-u username] [-p password] [-d database] [-t table] [-c count] [-a] [-s] [-i] [-v][-W]\n", p);
	exit(EXIT_FAILURE);
}

unsigned long long int module(signed long long int a){
	return (a>=0)? a: -a;
}

int table_check_count(MYSQL* connection, char* table, unsigned long long int e_count){
        MYSQL_RES *result;
	MYSQL_ROW row;
	char query[1024];
	unsigned long long int a_count = 0;
	snprintf(query, sizeof(query), "SELECT COUNT(*) FROM `%s`", table);
	if(0 == mysql_query(connection, query)){
		result = mysql_store_result(connection);
		row = mysql_fetch_row(result);
		if(row != NULL){
			a_count = strtoull(row[0], NULL, 10);
			if(module(a_count - e_count)/(float)a_count > count_div/(float)100){
				fprintf(stderr, "Table %s actual number of rows(%llu) differs from expected(%llu) more than %5.2f%%\n", table, a_count, e_count, count_div);
				exit(EXIT_FAILURE);
				}
		} else{
			fprintf(stderr,"Error: Query %s returned zero rows. MySQL said: \"%s\"\n", query, mysql_error(connection));
			exit(EXIT_FAILURE);
		}
	} else{
		fprintf(stderr,"Error: %s\n", mysql_error(connection));
		exit(EXIT_FAILURE);
	}
	return 0;
}
int table_check_auto_inc(MYSQL* connection, char* table, char* column){
        MYSQL_RES *result;
	MYSQL_ROW row;
	char query[1024];
	unsigned auto_increment_increment = 1;
	unsigned long long id = 0;
	unsigned long long expected_id = 0;
	unsigned long long match = 0, not_match = 0;

	snprintf(query, sizeof(query), "SHOW VARIABLES LIKE 'auto_increment_increment'");
	if(0 == mysql_query(connection, query)){
		result = mysql_store_result(connection);
		row = mysql_fetch_row(result);
		if(row != NULL){
			auto_increment_increment = strtoul(row[1], NULL, 10);
		} else{
			fprintf(stderr,"Error: Query %s returned zero rows. MySQL said: \"%s\"\n", query, mysql_error(connection));
			exit(EXIT_FAILURE);
		}
	} else{
		fprintf(stderr,"Error: %s\n", mysql_error(connection));
		exit(EXIT_FAILURE);
	}
	
	snprintf(query, sizeof(query), "SELECT `%s` FROM `%s` ORDER BY `%s`", column, table, column);
	if(0 == mysql_query(connection, query)){
		result = mysql_store_result(connection);
		while(NULL!=(row = mysql_fetch_row(result))){
			if(id == 0){
				id = strtoull(row[0], NULL, 10);
				expected_id = id + auto_increment_increment;
			}else{
				id = strtoull(row[0], NULL, 10);
				if(expected_id != id){
					if(warnings)fprintf(stderr, "Warning: Table: `%s`, field `%s`, expected %llu, actual %llu\n", table, column, expected_id, id);
					not_match++;
				}else{
					match++;
				}
			expected_id = id + auto_increment_increment;
			}
		} 
		if(match/(float)(not_match+match) < auto_inc_mismatch_limit/100.0){
			fprintf(stderr,"Error: Table: `%s`, field `%s`: Too many gaps in AUTO_INCREMENT-ed field(%llu). Total records %llu. \n", table, column, not_match, not_match+match);
			exit(EXIT_FAILURE);
			}
	} else{
		fprintf(stderr,"Error: %s\n", mysql_error(connection));
		exit(EXIT_FAILURE);
	}
	return 0;
}
int table_check_date(MYSQL* connection, char* table, char* column){
        MYSQL_RES *result;
	MYSQL_ROW row;
	char query[1024];
	unsigned long ts = 0;
	struct tm t1, t2;
	snprintf(query, sizeof(query), "SELECT UNIX_TIMESTAMP(`%s`), `%s` FROM `%s` WHERE `%s` IS NOT NULL ", column, column, table, column);
	if(0 == mysql_query(connection, query)){
		result = mysql_store_result(connection);
		while(NULL!=(row = mysql_fetch_row(result))){
			ts = strtoul(row[0], NULL, 10);
			if((ts < ts_lower_limit) || (ts > ts_upper_limit)){
				localtime_r(&ts_lower_limit, &t1);
				localtime_r(&ts_upper_limit, &t2);
				fprintf(stderr,"Error: Table: `%s`, field `%s`: Value '%s' is out of allowed range ('%04u-%02u-%02u %02u-%02u-%02u'..'%04u-%02u-%02u %02u-%02u-%02u')\n", table, column, row[1], t1.tm_year + 1900, t1.tm_mon + 1, t1.tm_mday, t1.tm_hour, t1.tm_min, t1.tm_sec, t2.tm_year + 1900, t2.tm_mon + 1, t2.tm_mday, t2.tm_hour, t2.tm_min, t2.tm_sec);
				exit(EXIT_FAILURE);
				}
		}
	} else{
		fprintf(stderr,"Error: %s\n", mysql_error(connection));
		exit(EXIT_FAILURE);
	}
	return 0;
}
int table_check_char(MYSQL* connection, char* table, char* column){
        MYSQL_RES *result;
	MYSQL_ROW row;
	char query[1024];
	int i = 0;
	unsigned long *lengths;
	snprintf(query, sizeof(query), "SELECT `%s` FROM `%s` WHERE `%s` IS NOT NULL AND `%s` <> ''", column, table, column, column);
	if(0 == mysql_query(connection, query)){
		result = mysql_use_result(connection);
		while(NULL!=(row = mysql_fetch_row(result))){
			lengths = mysql_fetch_lengths(result);
			for(i = 0; i < lengths[0]; i++){
			//printf("[%.*s] %lu\n", (int) lengths[0], row[0] ? row[0] : "NULL", (int) lengths[0]);
			if(!isprint(row[0][i])){
				fprintf(stderr,"Error: Table: `%s`, field `%s`: String '%s' has non-printable character '%c'(0x%X)\n", table, column, row[0], row[0][i], row[0][i]);
				exit(EXIT_FAILURE);
				}
			}
		mysql_free_result(result);
		}
	} else{
		fprintf(stderr,"Error: %s\n", mysql_error(connection));
		exit(EXIT_FAILURE);
	}
	return 0;
}
int table_check_id(MYSQL* connection, char* table, char* column){
        MYSQL_RES *result;
	MYSQL_ROW row;
	char query[1024];
	long long int min, max;
	float avg;

	snprintf(query, sizeof(query), "SELECT MIN(`%s`), AVG(`%s`), MAX(`%s`) FROM `%s`", column, column, column, table);
	if(0 == mysql_query(connection, query)){
		result = mysql_store_result(connection);
		row = mysql_fetch_row(result);
		if(row != NULL){
			if(row[0] == NULL || row[1] == NULL || row[2] == NULL) return 0;
			min = strtoll(row[0], NULL, 10);
			avg = strtof(row[1], NULL);
			max = strtoll(row[2], NULL, 10);
			if(max/(float)avg > max2avg_limit){
				fprintf(stderr, "Table `%s` field `%s`: MAX(%lld) is more than %f times bigger than AVG(%f)\n", table, column, max, max2avg_limit, avg);
				exit(EXIT_FAILURE);
				}
		} else{
			fprintf(stderr,"Error: Query %s returned zero rows. MySQL said: \"%s\"\n", query, mysql_error(connection));
			exit(EXIT_FAILURE);
		}
	} else{
		fprintf(stderr,"Error: %s\n", mysql_error(connection));
		exit(EXIT_FAILURE);
	}
	return 0;
}

int main(int argc, char** argv)
{
	extern int optind;
	int c;
	char host[255] = "";
	int port = 3306;
	char user[255] = "";
	char password[255] = "";
	char db[255] = "";
	char table[255] = "";
	int check_count = 0;
	unsigned long long int e_count = 0;
	int check_auto_inc = 0;
	int check_date = 0;
	int check_char = 0;
	int check_id = 0;
	char query[1024];

	MYSQL connection;
        MYSQL_RES *result;
	MYSQL_ROW row;

	while ((c = getopt(argc, argv, "h:P:u:p:d:t:?c:asiWv")) != -1){
		switch (c) {
			case 'h': strncpy(host, optarg, sizeof(host)); break;
			case 'P': port = strtol(optarg, (char **)NULL, 10); break;
			case 'u': strncpy(user, optarg, sizeof(user)); break;
			case 'p': strncpy(password, optarg, sizeof(password)); break;
			case 'd': strncpy(db, optarg, sizeof(db)); break;
			case 't': strncpy(table, optarg, sizeof(table)); break;
			case 'c': 
				  e_count = strtoull(optarg, NULL, 10);
				  check_count = 1;
				  break;
			case 'a': check_auto_inc = 1; break;
			case 's': check_date = 1; break;
			case 'i': check_id = 1; break;
			case 'v': check_char = 1; break;
			case 'W': warnings = 0; break;
			case '?': 
			default: usage(argv[0]);
		}
	}
	mysql_init(&connection);
	if(NULL == mysql_real_connect(&connection, host, user, password, db, port, NULL, 0)){
		fprintf(stderr,"Error: %s\n", mysql_error(&connection));
		exit(EXIT_FAILURE);
	}
	if(check_count){
		table_check_count(&connection, table, e_count);
	}
	if(0 != mysql_select_db(&connection, "information_schema")){
		fprintf(stderr,"Error: %s\n", mysql_error(&connection));
		exit(EXIT_FAILURE);
	}
	snprintf(query, sizeof(query), "SELECT COLUMN_NAME, DATA_TYPE, EXTRA FROM COLUMNS WHERE TABLE_SCHEMA='%s' AND TABLE_NAME='%s'", db, table);
	if(0 == mysql_query(&connection, query)){
		result = mysql_store_result(&connection);
		if(0 != mysql_select_db(&connection, db)){
			fprintf(stderr,"Error: %s\n", mysql_error(&connection));
			exit(EXIT_FAILURE);
			}
		while(NULL!=(row = mysql_fetch_row(result))){
			//printf("%s\t%s\t%s:\n", row[0], row[1], row[2]);
			if((NULL != strstr(row[2], "auto_increment")) && check_auto_inc == 1){
				table_check_auto_inc(&connection, table, row[0]);
			}
			if((NULL != strstr(row[1], "date") || NULL != strstr(row[1], "timestamp")) && (check_date == 1)){
				table_check_date(&connection, table, row[0]);
				}
			if((NULL != strstr(row[1], "char") || NULL != strstr(row[1], "text")) && (check_char == 1)){
				table_check_char(&connection, table, row[0]);
				}
			if((NULL != strcasestr(row[0], "_id") && NULL != strcasestr(row[1], "int")) 
					&& (check_id == 1)){
				table_check_id(&connection, table, row[0]);
				}
		}
	} else{
		fprintf(stderr,"Error: %s\n", mysql_error(&connection));
		exit(EXIT_FAILURE);
	}
	exit(EXIT_SUCCESS);
}

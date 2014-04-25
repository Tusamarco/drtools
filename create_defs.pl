#!/usr/bin/env perl

use strict;
use DBI;
use Data::Dumper;
use POSIX;
use Getopt::Long;

#----------------------------------------------------------------
# Parse command line

# Defaults
my $db_name = "test";
my $db_host = "127.0.0.1";
my $db_port = 3306;
my $db_user = "root";
my $db_pass = "";
my $db_table = "";
my $db_socket = "";
my $help = 0;

# Parse options
my $res = GetOptions ("db=s"       => \$db_name,
                      "host=s"     => \$db_host,
                      "port=i"     => \$db_port,
                      "user=s"     => \$db_user,
                      "password=s" => \$db_pass,
                      "table=s"    => \$db_table,
		      "socket=s"   => \$db_socket,
                      "help|?"     => \$help
                     );

Usage() if !$res || $help;

#----------------------------------------------------------------
# Connect to mysql
my $dsn = "";
if( $db_socket == "" ){
	$dsn = "DBI:mysql:database=$db_name;host=$db_host;port=$db_port";
}
else{
	$dsn = "DBI:mysql:database=$db_name;mysql_socket=$db_socket";
}
my $dbh = DBI->connect($dsn, $db_user, $db_pass);

Usage("Can't connect to mysql!") unless $dbh;

#----------------------------------------------------------------
# Dump tables definitions
my $sth = $dbh->prepare("SHOW TABLES");
$sth->execute;

print("#ifndef table_defs_h\n#define table_defs_h\n\n");
print("// Table definitions\ntable_def_t table_definitions[] = {\n");

while (my $row = $sth->fetchrow_arrayref) {
	my $table = $row->[0];

    # Skip if not a requested specific table
    next if ($db_table ne '' && $table ne $db_table);

	# Skip if it is not an innodb table
	next unless(GetTableStorageEngine($table) =~ /innodb/i);

	# Get fields list for table
	my @fields = ();
	my $tbl_sth = $dbh->prepare("SHOW FIELDS FROM $table");
	$tbl_sth->execute;
	while (my $field = $tbl_sth->fetchrow_hashref) { push @fields, $field }

	# Get primary key fields
	my @pk_fields = ();
	$tbl_sth = $dbh->prepare("show indexes from $table");
	$tbl_sth->execute;
	while (my $field = $tbl_sth->fetchrow_hashref) { 
		push @pk_fields, $field if ($field->{Key_name} eq 'PRIMARY');
	}

	# If no primary keys defined, check unique keys and use first one as primary
	if (scalar(@pk_fields) == 0) {
		$tbl_sth = $dbh->prepare("show indexes from $table");
		$tbl_sth->execute;
		my $pk_name = undef;
		while (my $field = $tbl_sth->fetchrow_hashref) { 
			next unless ($field->{Non_unique} == 0);
			$pk_name = $field->{Key_name} unless ($pk_name);
			last if ($field->{Key_name} ne $pk_name);
			push @pk_fields, $field;
		}
	}

	DumpTableDef($table, \@fields, \@pk_fields);
}

print("};\n\n#endif\n");

exit(0);

#------------------------------------------------------------------
sub DumpTableDef($$$) {
	my $table = shift;
	my $fields = shift;
	my $pk_fields = shift;

	print("\t{\n\t\tname: \"$table\",\n\t\t{\n");
	
	# Dump all PK fields
	foreach my $pk_field (@$pk_fields) {
		DumpField($table, FindFieldByName($fields, $pk_field->{Column_name}));
	}
	
	# Dump system PK if no PK fields found
	if (scalar(@$pk_fields) == 0) {
		DumpFieldLow(Name => 'DB_ROW_ID', ParsedType => 'FT_INTERNAL', FixedLen => 6, Null => 0);
	}
	
	# Dump 2 more sys fields
	DumpFieldLow(Name => 'DB_TRX_ID', ParsedType => 'FT_INTERNAL', FixedLen => 6, Null => 0);
	DumpFieldLow(Name => 'DB_ROLL_PTR', ParsedType => 'FT_INTERNAL', FixedLen => 7, Null => 0);
	
	# Dump the rest of the fields
	foreach my $field (@$fields) {
		DumpField($table, $field) unless $field->{Key} eq 'PRI';
	}

	print("\t\t\t{ type: FT_NONE }\n");
	print("\t\t}\n\t},\n");
}

#------------------------------------------------------------------
sub FindFieldByName($$) {
	my $fields = shift;
	my $name = shift;

	foreach my $field (@$fields) {
		return $field if $field->{Field} eq $name;
	}

	return undef;
}

#------------------------------------------------------------------
sub GetUIntLimits($) {
    my $len = shift;
    
    return (0, 255) if ($len == 1);
    return (0, 65535) if ($len == 2);
    return (0, "16777215UL") if ($len == 3);
    return (0, "4294967295ULL") if ($len == 4);
    return (0, "18446744073709551615ULL") if ($len == 8);
    return (0, 30000);
}

#------------------------------------------------------------------
sub GetIntLimits($) {
    my $len = shift;
    
    return (-128, 127) if ($len == 1);
    return (-32768, 32767) if ($len == 2);
    return ("-8388608L", "8388607L") if ($len == 3);
    return ("-2147483648LL", "2147483647LL") if ($len == 4);
    return ("-9223372036854775806LL", "9223372036854775807LL") if ($len == 8);
    return (0, 30000);
}

#------------------------------------------------------------------
sub DumpFieldLow {
	my %info = @_;
	
	printf("\t\t\t{ /* %s */\n", $info{Type});
	printf("\t\t\t\tname: \"%s\",\n", $info{Name});
	printf("\t\t\t\ttype: %s,\n", $info{ParsedType});

	if ($info{FixedLen}) {
		printf("\t\t\t\tfixed_length: %d,\n", $info{FixedLen});
	} else {
		printf("\t\t\t\tmin_length: %d,\n", $info{MinLen});
		printf("\t\t\t\tmax_length: %d,\n", $info{MaxLen});
	}
	
	printf("\n");
	
	if ($info{ParsedType} eq 'FT_TEXT' || $info{ParsedType} eq 'FT_CHAR') {
		printf("\t\t\t\thas_limits: FALSE,\n");
		printf("\t\t\t\tlimits: {\n");
    	printf("\t\t\t\t\tcan_be_null: %s,\n", $info{Null} ? 'TRUE' : 'FALSE');
		printf("\t\t\t\t\tchar_min_len: 0,\n");
		printf("\t\t\t\t\tchar_max_len: %d,\n", $info{MaxLen} + $info{FixedLen});
		printf("\t\t\t\t\tchar_ascii_only: TRUE\n");
		printf("\t\t\t\t},\n\n");
	}
        if ($info{ParsedType} eq 'FT_DECIMAL' ) {
		printf("\t\t\t\tdecimal_precision: %d,\n", $info{decimal_precision});
		printf("\t\t\t\tdecimal_digits: %d,\n", $info{decimal_digits});
		}
        if ($info{ParsedType} eq 'FT_DATETIME' ) {
		printf("\t\t\t\ttime_precision: %d,\n", $info{time_precision});
		}
        if ($info{ParsedType} eq 'FT_TIMESTAMP' ) {
		printf("\t\t\t\ttime_precision: %d,\n", $info{time_precision});
		}
        if ($info{ParsedType} eq 'FT_TIME' ) {
		printf("\t\t\t\ttime_precision: %d,\n", $info{time_precision});
		}

	if ($info{ParsedType} eq 'FT_INT') {
        my ($min, $max) = GetIntLimits($info{FixedLen});
		printf("\t\t\t\thas_limits: FALSE,\n");
		printf("\t\t\t\tlimits: {\n");
    	printf("\t\t\t\t\tcan_be_null: %s,\n", $info{Null} ? 'TRUE' : 'FALSE');
		printf("\t\t\t\t\tint_min_val: %s,\n", $min);
		printf("\t\t\t\t\tint_max_val: %s\n", $max);
		printf("\t\t\t\t},\n\n");
	} 

	if ($info{ParsedType} eq 'FT_UINT') {
        my ($min, $max) = GetUIntLimits($info{FixedLen});
		printf("\t\t\t\thas_limits: FALSE,\n");
		printf("\t\t\t\tlimits: {\n");
    	printf("\t\t\t\t\tcan_be_null: %s,\n", $info{Null} ? 'TRUE' : 'FALSE');
		printf("\t\t\t\t\tuint_min_val: %s,\n", $min);
		printf("\t\t\t\t\tuint_max_val: %s\n", $max);
		printf("\t\t\t\t},\n\n");
	} 

	if ($info{ParsedType} eq 'FT_ENUM') {
		printf("\t\t\t\thas_limits: FALSE,\n");
		printf("\t\t\t\tlimits: {\n");
    	printf("\t\t\t\t\tcan_be_null: %s,\n", $info{Null} ? 'TRUE' : 'FALSE');
		printf("\t\t\t\t\tenum_values_count: %d,\n", scalar(@{$info{Values}}));
		printf("\t\t\t\t\tenum_values: { \"%s\" }\n", join('", "', @{$info{Values}}));
		printf("\t\t\t\t},\n\n");
	} 
	if ($info{ParsedType} eq 'FT_SET') {
		printf("\t\t\t\thas_limits: FALSE,\n");
		printf("\t\t\t\tlimits: {\n");
    	printf("\t\t\t\t\tcan_be_null: %s,\n", $info{Null} ? 'TRUE' : 'FALSE');
		printf("\t\t\t\t\tset_values_count: %d,\n", scalar(@{$info{Values}}));
		printf("\t\t\t\t\tset_values: { \"%s\" }\n", join('", "', @{$info{Values}}));
		printf("\t\t\t\t},\n\n");
	} 

	printf("\t\t\t\tcan_be_null: %s\n", $info{Null} ? 'TRUE' : 'FALSE');
	printf("\t\t\t},\n");
}

#------------------------------------------------------------------
sub DumpField($$) {
	my $table = shift;
	my $field = shift;
	
	my %info;
	$info{Null} = ($field->{Null} eq 'YES');
	$info{Name} = $field->{Field};
	
	my $type_info = ParseFieldType($field->{Type});
	if ($type_info->{type} eq 'FT_INT' && IsFieldUnsigned($table, $field->{Field})) {
		$type_info->{type} = 'FT_UINT';
	}
	if ($type_info->{type} eq 'FT_CHAR') {
		my $maxlen = getMaxlen($table, $field->{Field});
		if($maxlen > 1){
			if($type_info->{fixed_len} > 0){
				# If type is CHAR(x)
				$type_info->{min_len} = $type_info->{fixed_len};
				$type_info->{max_len} = $type_info->{fixed_len} * $maxlen;
				$type_info->{fixed_len} = 0;
				}
			else{
				$type_info->{max_len} *= $maxlen;
				}
			}
	}
	
	$info{Type} = $field->{Type};
	$info{Values} = $type_info->{values};
	$info{ParsedType} = $type_info->{type};
	$info{MinLen} = $type_info->{min_len};
	$info{MaxLen} = $type_info->{max_len};
	$info{FixedLen} = $type_info->{fixed_len};
	$info{decimal_precision} = $type_info->{decimal_precision};
	$info{decimal_digits} = $type_info->{decimal_digits};
	$info{time_precision} = $type_info->{time_precision};

	
	DumpFieldLow(%info);
}

#------------------------------------------------------------------
sub IsFieldUnsigned($$) {
	my ($table, $field) = @_;
	my $sth = $dbh->prepare("SHOW CREATE TABLE $table");
	$sth->execute;
	my $row = $sth->fetchrow_arrayref;
	return ($row->[1] =~ /$field[^,]*unsigned/i);
}

#------------------------------------------------------------------
sub getMaxlen($$) {
	my ($table, $field) = @_;
	my $sth = $dbh->prepare("SHOW FULL COLUMNS FROM `$table` LIKE '$field'");
	$sth->execute;
	my $row = $sth->fetchrow_arrayref;
	my $collation = $row->[2];
	$sth = $dbh->prepare("SHOW COLLATION LIKE '$collation'");
	$sth->execute;
	$row = $sth->fetchrow_arrayref;
	my $charset = $row->[1];
	$sth = $dbh->prepare("SHOW CHARSET LIKE '$charset'");
	$sth->execute;
	$row = $sth->fetchrow_arrayref;
	my $maxlen = $row->[3];
	return ($maxlen);
}

#------------------------------------------------------------------
sub GetTableStorageEngine($) {
    my $table = shift;
    my $sth = $dbh->prepare("SHOW TABLE STATUS LIKE '$table'");
    $sth->execute;
    my $row = $sth->fetchrow_hashref;
    return $row->{Engine};
}

#------------------------------------------------------------------
sub Usage {
    my $msg = shift;
    if ($msg) {
        print "Error: $msg\n";
    }

    print "Usage: $0 [options]\n" .
          "Where options are:\n" .
          "\t--host     - mysql host\n" .
          "\t--port     - mysql port\n" .
          "\t--user     - mysql username\n" .
          "\t--password - mysql password\n" .
          "\t--db       - mysql database\n" .
          "\t--table    - specific table only\n" .
          "\t--help     - show this help\n\n";
    exit(1);
}

#------------------------------------------------------------------
sub ParseFieldType($) {
	my $type = shift;
	
	if ($type =~ /DATETIME(\((\d+)\))*/i) {
		my $p = $1;
		$p =~ s/\(//;
		$p =~ s/\)//;
		if($p != ""){
			if($p == 0){
				$p = 0;
				}
			else{
				$p = ceil($p/2.0);
				}
			return { type => 'FT_DATETIME', fixed_len => 5 + $p, time_precision => $p };
			}
		else{
			return { type => 'FT_DATETIME', fixed_len => 8 , time_precision => $p };
			}
	}

	if ($type =~ /TIMESTAMP(\((\d+)\))*/i) {
		my $p = $1;
		$p =~ s/\(//;
		$p =~ s/\)//;
		if($p != ""){
			if($p == 0){
				$p = 0;
				}
			else{
				$p = ceil($p/2.0);
				}
			return { type => 'FT_TIMESTAMP', fixed_len => 4 + $p, time_precision => $p };
			}
		else{
			return { type => 'FT_TIMESTAMP', fixed_len => 4 , time_precision => $p };
			}
	}

	if ($type =~ /DATE/i) {
		return { type => 'FT_DATE', fixed_len => 3 };
	}

	if ($type =~ /TIME(\((\d+)\))*/i) {
		my $p = $1;
		$p =~ s/\(//;
		$p =~ s/\)//;
		if($p != ""){
			if($p == 0){
				$p = 0;
				}
			else{
				$p = ceil($p/2.0);
				}
			return { type => 'FT_TIME', fixed_len => 3 + $p, time_precision => $p };
			}
		else{
			return { type => 'FT_TIME', fixed_len => 3 , time_precision => $p };
			}
	}

	if ($type =~ /YEAR/i) {
		return { type => 'FT_YEAR', fixed_len => 1 };
	}

	if ($type =~ /^TINYINT/i) {
		return { type => 'FT_INT', fixed_len => 1 };
	}

	if ($type =~ /^SMALLINT/i) {
		return { type => 'FT_INT', fixed_len => 2 };
	}

	if ($type =~ /^MEDIUMINT/i) {
		return { type => 'FT_INT', fixed_len => 3 };
	}

	if ($type =~ /^INT/i) {
		return { type => 'FT_INT', fixed_len => 4 };
	}

	if ($type =~ /^BIGINT/i) {
		return { type => 'FT_INT', fixed_len => 8 };
	}

	if ($type =~ /^CHAR\((\d+)\)/i) {
		return { type => 'FT_CHAR', fixed_len => $1 };
	}

	if ($type =~ /VARCHAR\((\d+)\)/i) {
		return { type => 'FT_CHAR', min_len => 0, max_len => $1 };
	}

        if ($type =~ /^BINARY\((\d+)\)/i) {
                return { type => 'FT_BIN', fixed_len => $1 };
        }

	if ($type =~ /VARBINARY\((\d+)\)/i) {
		return { type => 'FT_BIN', min_len => 0, max_len => $1 };
	}

	if ($type =~ /^TINYTEXT$/i) {
		return { type => 'FT_TEXT', min_len => 0, max_len => 255 };
	}

	if ($type =~ /^TEXT$/i) {
		return { type => 'FT_TEXT', min_len => 0, max_len => 65535 };
	}

	if ($type =~ /^MEDIUMTEXT$/i) {
		return { type => 'FT_TEXT', min_len => 0, max_len => 16777215 };
	}

	if ($type =~ /^LONGTEXT$/i) {
		return { type => 'FT_TEXT', min_len => 0, max_len => 4294967295 };
	}
	if ($type =~ /^TINYBLOB$/i) {
		return { type => 'FT_BLOB', min_len => 0, max_len => 255 };
	}

	if ($type =~ /^BLOB$/i) {
		return { type => 'FT_BLOB', min_len => 0, max_len => 65535 };
	}

	if ($type =~ /^MEDIUMBLOB$/i) {
		return { type => 'FT_BLOB', min_len => 0, max_len => 16777215 };
	}

	if ($type =~ /^LONGBLOB$/i) {
		return { type => 'FT_BLOB', min_len => 0, max_len => 4294967295 };
	}

	if ($type =~ /^FLOAT/i) {
		return { type => 'FT_FLOAT', fixed_len => 4 };
	}

	if ($type =~ /^DOUBLE/i) {
		return { type => 'FT_DOUBLE', fixed_len => 8 };
	}

	if ($type =~ /^ENUM\(\'(.*)\'\)/i) {
		my @enum_values = split(/[\'\"]\,[\'\"]/, $1);
		my $size;
		if(scalar(@enum_values)>255){
			$size = 2;
		} else {
			$size = 1;
		}
		return { type => 'FT_ENUM', fixed_len => $size, values => \@enum_values };
	}
	if ($type =~ /^SET\(\'(.*)\'\)/i) {
		my @set_values = split(/[\'\"]\,[\'\"]/, $1);
		my $set_size;
		$set_size = (scalar(@set_values)+7)/8;
		if($set_size>4) {$set_size = 8;}
		return { type => 'FT_SET', fixed_len => $set_size, values => \@set_values };
	}

	if ($type =~ /^DECIMAL\((\d+),\s*(\d+)\)/i) {
		my $len_bytes = ceil(($1-$2) * 4 / 9) + ceil($2*4/9);
		return { type => 'FT_DECIMAL', fixed_len => $len_bytes , decimal_precision => $1, decimal_digits => $2};
	}
	
	if ($type =~ /^BIT\((\d+)\)/i) {
		my $len_bytes = floor(($1 + 7 ) / 8);
		return { type => 'FT_BIT', fixed_len => $len_bytes};
	}

	die "Unsupported type: $type!\n";
}

#ifndef table_defs_h
#define table_defs_h

// Table definitions
table_def_t table_definitions[] = {
        {
                name: "SYS_TABLES",
                {
                        { /* varchar(255) */
                                name: "NAME",
                                type: FT_CHAR,
                                min_length: 0,
                                max_length: 255,

                                has_limits: TRUE,
                                limits: {
                                        can_be_null: FALSE,
                                        char_min_len: 0,
                                        char_max_len: 255,
                                        char_ascii_only: TRUE
                                },

                                can_be_null: FALSE
                        },
                        { /*  */
                                name: "DB_TRX_ID",
                                type: FT_INTERNAL,
                                fixed_length: 6,

                                can_be_null: FALSE
                        },
                        { /*  */
                                name: "DB_ROLL_PTR",
                                type: FT_INTERNAL,
                                fixed_length: 7,

                                can_be_null: FALSE
                        },
                        { /* bigint(20) unsigned */
                                name: "ID",
                                type: FT_UINT,
                                fixed_length: 8,

                                has_limits: TRUE,
                                limits: {
                                        can_be_null: FALSE,
                                        uint_min_val: 0,
                                        uint_max_val: 18446744073709551615ULL
                                },

                                can_be_null: FALSE
                        },
                        { /* int(10) unsigned */
                                name: "N_COLS",
                                type: FT_INT,
                                fixed_length: 4,

                                can_be_null: TRUE
                        },
                        { /* int(10) unsigned */
                                name: "TYPE",
                                type: FT_UINT,
                                fixed_length: 4,

                                can_be_null: TRUE
                        },
                        { /* bigint(20) unsigned */
                                name: "MIX_ID",
                                type: FT_UINT,
                                fixed_length: 8,

                                has_limits: TRUE,
                                limits: {
                                        can_be_null: TRUE,
                                        uint_min_val: 0,
                                        uint_max_val: 18446744073709551615ULL
                                },

                                can_be_null: TRUE
                        },
                        { /* int(10) unsigned */
                                name: "MIX_LEN",
                                type: FT_UINT,
                                fixed_length: 4,

                                can_be_null: TRUE
                        },
                        { /* varchar(255) */
                                name: "CLUSTER_NAME",
                                type: FT_CHAR,
                                min_length: 0,
                                max_length: 255,

                                has_limits: TRUE,
                                limits: {
                                        can_be_null: TRUE,
                                        char_min_len: 0,
                                        char_max_len: 255,
                                        char_ascii_only: TRUE
                                },

                                can_be_null: TRUE
                        },
                        { /* int(10) unsigned */
                                name: "SPACE",
                                type: FT_UINT,
                                fixed_length: 4,

                                can_be_null: TRUE
                        },
                        { type: FT_NONE }
                }
        },
        {
                name: "SYS_INDEXES",
                {
                        { /* bigint(20) unsigned */
                                name: "TABLE_ID",
                                type: FT_UINT,
                                fixed_length: 8,

                                has_limits: TRUE,
                                limits: {
                                        can_be_null: FALSE,
                                        uint_min_val: 0,
                                        uint_max_val: 18446744073709551615ULL
                                },

                                can_be_null: FALSE
                        },
                        { /* bigint(20) unsigned */
                                name: "ID",
                                type: FT_UINT,
                                fixed_length: 8,

                                has_limits: TRUE,
                                limits: {
                                        can_be_null: FALSE,
                                        uint_min_val: 0,
                                        uint_max_val: 18446744073709551615ULL
                                },

                                can_be_null: FALSE
                        },
                        { /*  */
                                name: "DB_TRX_ID",
                                type: FT_INTERNAL,
                                fixed_length: 6,

                                can_be_null: FALSE
                        },
                        { /*  */
                                name: "DB_ROLL_PTR",
                                type: FT_INTERNAL,
                                fixed_length: 7,

                                can_be_null: FALSE
                        },
                        { /* varchar(120) */
                                name: "NAME",
                                type: FT_CHAR,
                                min_length: 0,
                                max_length: 120,

                                has_limits: TRUE,
                                limits: {
                                        can_be_null: TRUE,
                                        char_min_len: 0,
                                        char_max_len: 120,
                                        char_ascii_only: TRUE
                                },

                                can_be_null: TRUE
                        },
                        { /* int(10) unsigned */
                                name: "N_FIELDS",
                                type: FT_UINT,
                                fixed_length: 4,

                                can_be_null: TRUE
                        },
                        { /* int(10) unsigned */
                                name: "TYPE",
                                type: FT_UINT,
                                fixed_length: 4,

                                can_be_null: TRUE
                        },
                        { /* int(10) unsigned */
                                name: "SPACE",
                                type: FT_UINT,
                                fixed_length: 4,

                                can_be_null: TRUE
                        },
                        { /* int(10) unsigned */
                                name: "PAGE_NO",
                                type: FT_UINT,
                                fixed_length: 4,

                                can_be_null: TRUE
                        },
                        { type: FT_NONE }
                }
        },
};

#endif



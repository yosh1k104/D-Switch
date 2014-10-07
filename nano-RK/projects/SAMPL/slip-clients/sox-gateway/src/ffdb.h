#ifndef _ffdb_
#define _ffdb_

#include <stdlib.h>
#include <stdio.h>
#include <sqlite3.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

#define SMALL_RAND (random()/(RAND_MAX/255))

#define FF_ENV 0
#define FF_POW 1

#define BUFLEN 256

struct firefly_env{
	char *id;
	int time;
	int light;
	int temp;
	int accl;
	int voltage;
	int audio;
};

struct firefly_pow{
	char *id;
	int time;
	int state;
	int rms_current;
	int rms_voltage;
	int true_power;
	int energy;
};

/* primary db functions */
void init_db(char *dbpath);
void write_env(struct firefly_env ff);
void write_pow(struct firefly_pow ff);
void set_ff_info(char *id, char *alias, char *group_name);

/* db helper functions */
void setup_new_db();
int check_table (char *table_name);
static int callback(void *NotUsed, int argc, char **argv, char **azColName);
void print_table(char **result, int rownum, int colnum);
void err(char *msg);
void build_ffs(struct firefly_env *ff_env, struct firefly_pow *ff_pow);
void increment_ffs(struct firefly_env *ff_env, struct firefly_pow *ff_pow);

sqlite3 *db;

#endif


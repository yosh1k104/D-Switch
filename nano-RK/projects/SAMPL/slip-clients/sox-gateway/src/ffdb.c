#include "ffdb.h"


/* main used to test saga database functions. */
/*
void main(){	
	int i;
	char *dberror = NULL;
	struct firefly_env ff_env;
	struct firefly_pow ff_pow;
	build_ffs(&ff_env,&ff_pow);

	init_db("test-db");
	for(i=0;i<20;i++){
		increment_ffs(&ff_env,&ff_pow);
		write_env(ff_env);
		write_pow(ff_pow);
	}
	set_ff_info("JIGAJIGA","Prius Charger","Garage");
	
	sqlite3_exec(db,"select * from devices",callback,0,NULL);
	sqlite3_exec(db,"select * from JIGAJIGA",callback,0,NULL);

	if(dberror)
		printf("%s\n",dberror);
	sqlite3_close(db);
}
*/

/* 
 * opens db if one is present, otherwise creates
 * and prepares a new db.
 */
void init_db(char *dbpath){
	int i, rc;
	char *dberror = NULL;
	char buf[BUFLEN];
	struct stat info;

	if(stat(dbpath,&info)){		
		if(sqlite3_open(dbpath,&db))
			err("Error opening db.\n");
		setup_new_db();
	}
	else
		if(sqlite3_open(dbpath,&db))
			err("Error opening db.\n");
		else
			printf("opening existing.\n");
	return;
}


/* 
 * adds an entry for environmental firefly data,
 * creating the table if necessary.
 */
void write_env(struct firefly_env ff){
	char *dberror = NULL;
	char cmdbuf[BUFLEN];

	if(!check_table(ff.id)){
		sprintf(cmdbuf,"create table \"%s\" (time int, light int, temp int, accl int, voltage int, audio int)",ff.id);
		sqlite3_exec(db,cmdbuf,callback,0,&dberror);
		sprintf(cmdbuf,"insert into devices(id, type) values ('%s', %d)", ff.id, FF_ENV);
		sqlite3_exec(db,cmdbuf,callback,0,&dberror);
		sqlite3_exec(db,"UPDATE info SET value=value+1 WHERE var='env_num'",callback,0,&dberror);
	}
	sprintf(cmdbuf,"insert into \"%s\" values(%d,%d,%d,%d,%d,%d)",ff.id,ff.time,
			ff.light, ff.temp, ff.accl, ff.voltage, ff.audio);
	sqlite3_exec(db,cmdbuf,callback,0,&dberror);

	return;
}


/* 
 * adds an entry for power firefly (jigawatt) data,
 * creating the table if necessary.
 */
void write_pow(struct firefly_pow ff){
	char *dberror = NULL;
	char cmdbuf[BUFLEN];
	
	if(!check_table(ff.id)){
		sprintf(cmdbuf,"create table \"%s\" (time int, state int, rms_current int, rms_voltage int, true_power int, energy int)",ff.id);
		sqlite3_exec(db,cmdbuf,callback,0,&dberror);
		sprintf(cmdbuf,"insert into devices(id, type) values ('%s', %d)", ff.id, FF_POW);
		sqlite3_exec(db,cmdbuf,callback,0,&dberror);
		sqlite3_exec(db,"UPDATE info SET value=value+1 WHERE var='pow_num'",callback,0,&dberror);
	}
	sprintf(cmdbuf,"insert into \"%s\" values(%d,%d,%d,%d,%d,%d)",ff.id,ff.time,
			ff.state, ff.rms_current, ff.rms_voltage, ff.true_power, ff.energy);
	sqlite3_exec(db,cmdbuf,callback,0,&dberror);

	return;
}


/* 
 * sets the alias and group_name of the devices specified
 * by id in the devices table
 */
void set_ff_info(char *id, char *alias, char *group_name){
	char cmdbuf[BUFLEN];

	sprintf(cmdbuf,"UPDATE devices SET alias='%s',group_name='%s' WHERE id='%s'",
		alias, group_name, id);
	sqlite3_exec(db,cmdbuf,callback,0,NULL);
	return;
}


/**** HELPER FUNCTIONS ****/


/* 
 * prepares an existing empty database to receive data from devices.
 * helper function for init_db().
 */
void setup_new_db(){
	char *dberror = NULL;

	printf("setting up db\n");
	sqlite3_exec(db,"create table info (var char(64),value int)",NULL,0,&dberror);
	sqlite3_exec(db,"insert into info values('env_num', 0)",NULL,0,&dberror);
	sqlite3_exec(db,"insert into info values('pow_num', 0)",NULL,0,&dberror);
	sqlite3_exec(db,"create table devices (id char(16), alias char(64), type int, group_name char(16))",NULL,0,&dberror);
	if(dberror)
		printf("%s\n",dberror);
	return;
}



/* 
 * prints the elements returned from a call to sqlite3_get_table().
 * useful for debugging.
 */
void print_table(char **result, int rownum, int colnum){
	int i, j;
	for(i=0; i<=rownum; i++){
		for(j=0; j<colnum; j++){
			printf("%s\t",result[(i*colnum)+j]);
		}
		printf("\n");
	}
}


/* 
 * returns 1 if a table with name 'table_name' exists, 0 otherwise. 
 */
int check_table (char *table_name){
	char cmdbuf[128];
	char **result;
	int row, col;

	sprintf(cmdbuf,"select min(rowid) from \"%s\"",table_name);
	return !(sqlite3_get_table(db,cmdbuf,&result,&row,&col,NULL));
}

/*
void update_vals(int id, struct firefly_env node, sqlite3 *db){
  char buf[BUFLEN];
	sprintf(buf,"insert into ff%d values(%d,%d,%d,%d,%d,%d)",id,time(NULL),
			node.light, node.temp, node.accl, node.voltage, node.audio);
	sqlite3_exec(db,buf,callback,0,NULL);
}
*/

/* 
 * is called by sqlite3_exec() to print db tables or elements.
 * use sqlite3_get_table() as an alternative if you wish to retrieve 
 * data, as opposed to just printing it.
 */
static int callback(void *NotUsed, int argc, char **argv, char **azColName){
  int i;
  for(i=0; i<argc; i++)
    printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
  printf("\n");
  return 0;
}


void err(char *msg){
	printf("%s",msg);
	exit(1);
}


/* 
 * builds mock fireflies for testing 
 */
void build_ffs(struct firefly_env *ff_env, struct firefly_pow *ff_pow){
	ff_env->id = malloc(sizeof(char)*9);
	strcpy(ff_env->id,"FF123456");
	ff_env->time = SMALL_RAND;
	ff_env->light = SMALL_RAND;
	ff_env->temp = SMALL_RAND;
	ff_env->accl = SMALL_RAND;
	ff_env->voltage = SMALL_RAND;
	ff_env->audio = SMALL_RAND;

	ff_pow->id = malloc(sizeof(char)*9);
	strcpy(ff_pow->id,"JIGAJIGA");
	ff_pow->time = SMALL_RAND;
	ff_pow->state = SMALL_RAND;
	ff_pow->rms_current = SMALL_RAND;
	ff_pow->rms_voltage = SMALL_RAND;
	ff_pow->true_power = SMALL_RAND;
	ff_pow->energy = SMALL_RAND;

	return;
}


/* 
 * increment mock fireflies
 */
void increment_ffs(struct firefly_env *ff_env, struct firefly_pow *ff_pow){
	ff_env->time += 1;
	ff_env->light += 2;
	ff_env->temp += 4;
	ff_env->accl += 8;
	ff_env->voltage += 16;
	ff_env->audio += 32;

	ff_pow->time += 1;
	ff_pow->state += 2;
	ff_pow->rms_current += 4;
	ff_pow->rms_voltage += 8;
	ff_pow->true_power += 16;
	ff_pow->energy += 32;

	return;
}

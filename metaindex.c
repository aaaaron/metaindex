// metaindex
// Creates mySQL tables for searching web site.
//
// Copyright 1996 IQuest Internet, Inc.
// http://www.iquest.net/
//
// author(s): aaron abelard (aarona@iquest.net)
//
// when__ who______ ver what
// 961120 aarona    000 first cut 
// 961130 aarona    001 back again 
// 961210 aarona    100 added building tables
// 970116 aarona    101 changed table implementation to use only two tables
// 970307 aarona    102 changed to support mSQL 2.0
//                      - changed all table fields named index to idx
//                      - removed PRIMARY KEY notation from CREATE syntax
//                      - changed MSQL_HOST to iquest5.iquest.net
//                      - implemented search_files.title and search_files.descr as text fields half the length
//                        of the old character fields.  reduces overall database size, while retaining ability
//                        to stuff long character strings into the database. (note: text is unsearchable)
// 970311 aarona    103 text fields caused the mSQL server to crash.  Probably overflow. 
// 970430 aarona    150 convert to support mySQL instead of mSQL.

#include <stdio.h>
#include <string.h>
#include <mysql.h>
#include <xcgi.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

char  * text;  										// how we communicate between the scanner and code 
char  * title;

#include "flex_doc.fc"

#define MYSQL_HOST "mysql.iquest.net"                                        // mySQL server hostname
#define MYSQL_USER "aarona"                                                  // mySQL username
#define MYSQL_PASS ""                                                        // mySQL user password (null)
#define WORDS_TABLE "search_words"
#define FILES_TABLE "search_files"
#define DOC_ROOT "/usr13/WWW/Public/%s"

void   index_files(char *, char *, MYSQL *);
void   parse_html (char *, char *, MYSQL *);
void   wipe_database(MYSQL *);
void   add2db(char *, char *, char *, char *, char *, MYSQL *);
char * lowerstr(char *);
char * search_replace(char *, char *, char *);

int run_mode = 0;

void main(int argc, char *argv[]) {

	MYSQL         mysql;

	char        * enviro;
	char        * vds_name;
	char        * db_name;

	char          start_dir[150];
	char          pathname[150];

	xcgi        * cgi_data;

	enviro = (char *) getenv("REQUEST_METHOD");
	if(enviro != NULL) {
		run_mode = 1;                 // set CGI variable
		cgi_data = xCGI_parse();
		vds_name = xCGI_find(cgi_data, "server");
		if(vds_name == NULL) {
			xCGI_html_error(1400, "Improper useage");
		}
		db_name = xCGI_find(cgi_data, "db");
		if(db_name == NULL) {
			db_name = strdup(vds_name);
		}
		/*		printf("Content-type: text/html\n\n"); */ 		// as a nph-cgi, this is unneeded
		run_mode = 1;
		printf("<html>\n<body bgcolor=FFFFFF text=000000>\n<p>\n");
	} else {
		if(argc <= 1) {
			printf("site_index [vds name] [database name]\n");
			printf("  if [database name] left blank, [vds name] will be used.\n");
			exit(0); 
		}
		if(argc == 3) {
			db_name = (char *) malloc (sizeof(char) * strlen(argv[2])); 
			strcpy(db_name, argv[2]);
		} else {
			db_name = (char *) malloc (sizeof(char) * strlen(argv[1])); 
			strcpy(db_name, argv[1]);
		}
		vds_name = (char *) malloc (sizeof(char) * strlen(argv[1])); 
		strcpy(vds_name, argv[1]);
		run_mode = 2;
	}

	if(!(mysql_connect(&mysql, MYSQL_HOST, MYSQL_USER, MYSQL_PASS)))
		xCGI_html_error(1405, "Error connecting to database: %s", mysql_error(&mysql));

	if(mysql_select_db(&mysql, db_name))                              // select our database
		xCGI_html_error(1410, "Cannot select database %s: %s", db_name, mysql_error(&mysql));

	if(run_mode == 1) {
		printf("<FONT SIZE=+2>Reindexing search database for %s</FONT><BR>\n", vds_name);
		printf("<FONT SIZE=+1>Database name: %s</FONT><P>\n", db_name);
	} else {
		printf("Reindexing search database for %s\n", vds_name);
		printf("Database name: %s\n\n", db_name);
	}


	// Begin actual processing
	sprintf(start_dir, DOC_ROOT, vds_name);
	strcpy(pathname, "/");        							// we're starting at the begging of the site
	wipe_database(&mysql);
	index_files(start_dir, pathname, &mysql);

	mysql_close(&mysql);

}

void index_files(char *start_dir, char *g_current_dir, MYSQL *mysql) {

	DIR           * FPdir;
	struct dirent * Entry;
	struct stat     Status;
	char          * current_dir;
	char            full_dir[1024];
	char            newdir[1204];
	char            filename[1024];
	char            filepath[1536];

	current_dir = strdup(g_current_dir);
	sprintf(full_dir, "%s%s", start_dir, current_dir);

	if((FPdir = opendir(full_dir)) == NULL) {
		return;
	}

	while( (Entry = readdir( FPdir )) != NULL) {
		strcpy(filename, Entry->d_name);
		sprintf(filepath, "%s%s", full_dir, filename);
		lstat(filepath, &Status);

		if(filename[0] == '.') {                			// skip current dir, parent dir, all hidden files/dirs.  
			continue;
		}

		if((Status.st_mode & S_IFLNK) == S_IFLNK) {		// skip over symbolic links
			continue;
		}

		if((Status.st_mode & S_IFDIR) == S_IFDIR) {
			sprintf(newdir, "%s%s/", current_dir, filename);
			index_files(start_dir, newdir, mysql);
			free(newdir);
		}

		if((strstr(filename, "html"))) {
			sprintf(newdir, "%s%s", current_dir, filename);
			parse_html(start_dir, newdir, mysql);
		}

	}
	closedir(FPdir);
	return;

}

void parse_html(char *start_dir, char *path, MYSQL *mysql) {

	FILE       * fp;
	char       * filename;
	char       * temp2;
	int          y, x;
	int          mode;
	char       * description;
	char       * keywords;

	filename = (char *) malloc(sizeof(char) * 512);

	strcpy(filename, start_dir);
	strcat(filename, path);

	fp = fopen(filename, "r");
	if(fp == NULL) {
		if(run_mode == 1) {
			printf("<FONT COLOR=FF0000><BLINK>  Error: %s</BLINK></FONT><BR>\n", path);
		} else {
			printf("Error opening %s\n", path);
		}
		return;
	}
	fflush(fp);
	fflush(yyin);
	yyrestart(fp);
	mode = 0;
	y = 0;
	x = 0;
	title = NULL;
	while(x++ < 30) {
		yylex();
		if(strstr(lowerstr(text), "name")) {
			if(strstr(lowerstr(text), "keywords")) {
				mode = 1;
			} else if(strstr(lowerstr(text), "description")) {
				mode = 2;
			}
		}
		if(mode) {
			yylex();
			temp2 = strtok(text, "=");
			if(!strcasecmp(text, "content")) {
				if(mode == 1) {
					temp2 = strtok(NULL, "=");	
					keywords = strdup(temp2);
					y++;
				} else {
					temp2 = strtok(NULL, "=");
					description = strdup(temp2);
					y++;
				}
			}
			mode = 0;
		}
		if(y == 2) {
			// Clean up all values before pasing to stuffer.
			if(title != NULL) {
				title[strlen(title) - 8] = 0;
				strcpy(title, &title[7]);	
			} else {
				title = strdup("");
			}
			if(strlen(title) >= 119) {
				strcpy(&title[115], "...\0");
			}
			title = search_replace(title, "'", "&#039");
			description = search_replace(description, "'", "&#039");
			description = search_replace(description, "\n", " ");
			description = search_replace(description, "\"", "");
			description = search_replace(description, "  ", " ");
			if(strlen(description) >= 119) {
				strcpy(&description[115], "...\0");
			}
			keywords = search_replace(keywords, "'", "&#039");
			keywords = search_replace(keywords, "\n", " ");
			keywords = search_replace(keywords, "\"", "");
			keywords = search_replace(keywords, "  ", " ");
			keywords = lowerstr(keywords);	
			if(run_mode == 1) {
				printf("<FONT COLOR=00AA00>  Added: %s</FONT><BR>\n", path);
			} else {
				printf("  Added: %s\n", path);
			}
			add2db(title, description, keywords, filename, path, mysql);
			x = 500;
		} 
	}
	if(y != 2) {
		if(run_mode == 1) {
			printf("<FONT COLOR=AA0000>Skipped: %s</FONT><BR>\n", path);
		} else {
			printf("Skipped: %s\n", path);
		}
	}

	fflush(fp);
	fflush(yyin);
	fclose(fp);
	free(filename);
}

void add2db(char * title, char * description, char * keywords, char * filename, char * path, MYSQL *mysql) {

	char                   query[4096];
	int                    retvalue; 
	int                    next_avail;
	char                 * temp;

	MYSQL_RES            * res;
	MYSQL_ROW              row;

	// Insert information into search_files first.

	retvalue = mysql_query(mysql, "select idx from search_files order by idx desc limit 1");
	if(retvalue != 0)
		xCGI_html_error(1880, mysql_error(mysql));
	res = mysql_store_result(mysql);
	row = mysql_fetch_row(res);
	if(mysql_num_rows(res)) {
		next_avail = atoi(row[0])+1;
	} else {
		next_avail = 0;
	}

	sprintf(query, "insert into search_files values ( %d, '%s', '%s', '%s' )", next_avail, 
			path, title, description);

	retvalue = mysql_query(mysql, query);
	if(retvalue != 0)
		xCGI_html_error(1881, mysql_error(mysql));

	temp = strtok(keywords, ",");
	while(temp != NULL) {
		if(strlen(temp) > 60) {
			temp[59] = 0;
		}
		temp = search_replace(temp, "  ", " ");
		temp = search_replace(temp, "  ", " ");
		if(temp[0] == ' ') {
			strcpy(temp, &temp[1]);
		}
		sprintf(query, "insert into search_words values ( %d, '%s' )", next_avail, temp);
		retvalue = mysql_query(mysql, query);
		if(retvalue != 0)
			xCGI_html_error(1882, mysql_error(mysql));
		temp = strtok(NULL, ",");
	}

}

char *lowerstr(char *line) {
	int              x = 0;
	char           * line2;

	line2 = (char *) malloc(sizeof(char) * strlen(line));

	for(x = 0; x < strlen(line); x++) {
		line2[x] = tolower(line[x]);
	}
	line2[x] = 0;
	return(line2);

}


// warning: this routine does chop the shit out of *line
char * search_replace(char *line, char *search, char *replace) {

	char           * newline;
	char           * hit;
	int              x=0;
	int              y;

	// watch out that you don't add more then 20 characters to new string.. sigh
	newline = (char *) malloc(sizeof(char) * (strlen(line)+50));

	while(x != -1) {
		hit = strstr(line, search);
		if(hit != NULL) {
			hit[0] = '\0';    // temp end string at first hit
			strcat(newline, line);
			strcat(newline, replace);
			line =  hit + strlen(search);
		} else {
			strcat(newline, line);
			x = -1;
		}
	}

	return(newline);

}

void wipe_database(MYSQL *mysql) {

	int retvalue;

	retvalue = mysql_query(mysql, "drop table search_files");
	if(retvalue != 0) {
		xCGI_html_error(1413, mysql_error(mysql));
	}

	retvalue = mysql_query(mysql, "drop table search_words");
	if(retvalue != 0) {
		xCGI_html_error(1414, mysql_error(mysql));
	}
	retvalue = mysql_query(mysql, "create table search_words (idx int not null, word char(50))");
	if(retvalue != 0) {
		xCGI_html_error(1415, mysql_error(mysql));
	}
	retvalue = mysql_query(mysql, "create index idx1 on search_words (idx)");
	if(retvalue != 0) {
		xCGI_html_error(1416, mysql_error(mysql));
	}

	retvalue = mysql_query(mysql, "create table search_files (idx int not null, path char(60), title char(120), descr char(120))");
	if(retvalue == -1) {
		xCGI_html_error(1417, mysql_error(mysql));
	}

	retvalue = mysql_query(mysql, "create index idx1 on search_files (idx)");
	if(retvalue == -1) {
		xCGI_html_error(1418, mysql_error(mysql));
	}

}


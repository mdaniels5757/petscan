#include "main.h"

//________________________________________________________________________________________________

#define DB_ESCAPE_BUFFER_SIZE 10000

TWikidataDB::TWikidataDB ( TPlatform &platform , string wiki ) {
	_platform = &platform ;
	setHostDBFromWiki ( wiki ) ;
	batch_size = 1000 ;
	curl_global_init(CURL_GLOBAL_ALL);
//	_config_file = config_file ;
	doConnect(true) ;
}	

bool TWikidataDB::setHostDBFromWiki ( string wiki ) {
	if ( 0 ) { // Future special cases
	} else {
		_host = wiki+".labsdb" ;
		_database = wiki+"_p" ;
	}
}

void TWikidataDB::doConnect ( bool first ) {	
	if ( !first ) {
		int i = mysql_ping ( &mysql ) ;
		if ( i == 0 ) return ; // Connection is already up
/*		if ( i != CR_SERVER_GONE_ERROR ) {
			cerr << "MySQL PING says: " << i << endl ;
			exit ( 0 ) ;
		}*/
	}
	
	string user = _platform->config["user"] ;
	string password = _platform->config["password"] ;
	mysql_init(&mysql);
	mysql_real_connect(&mysql,_host.c_str(),user.c_str(),password.c_str(),_database.c_str(),0,NULL,0) ;
//	printf("MySQL Server Version is %s\n",mysql_get_server_info(&mysql));
}

TWikidataDB::~TWikidataDB () {
   mysql_close(&mysql);
   curl_global_cleanup();
}

string TWikidataDB::escape ( string s ) {
	unsigned long len = s.length() * 2 + 1 ;
	if ( len >= DB_ESCAPE_BUFFER_SIZE ) {
		cerr << "DB:Escape for\n" << s << "\nrequires " << len << " bytes, but limit is " << DB_ESCAPE_BUFFER_SIZE << endl ;
		exit ( 1 ) ;
	}
	char tmp[DB_ESCAPE_BUFFER_SIZE] ; // Would prefer "new" below, but that dies for some reason on "delete"
//	char *tmp = new char ( len ) ;
	long unsigned int end = mysql_real_escape_string ( &mysql , tmp , s.c_str() , s.length() ) ;
	string ret ( tmp ) ;
//	delete [] tmp ;
	return ret ;
}

void TWikidataDB::runQuery ( string sql ) {
	doConnect() ;
	if ( mysql_query ( &mysql , sql.c_str() ) ) finishWithError() ;
}

MYSQL_RES *TWikidataDB::getQueryResults ( string sql ) {
	runQuery ( sql ) ;
	MYSQL_RES *result = mysql_store_result(&mysql);
	if ( result == NULL ) finishWithError(sql+": No result") ;
	return result ;
}

void TWikidataDB::finishWithError ( string msg ) {
	if ( msg.empty() ) fprintf(stderr, "%s\n", mysql_error(&mysql));
	else fprintf(stderr, "%s\n", msg.c_str());
	mysql_close(&mysql);
	exit(1);        
}

/*
char *TWikidataDB::getTextFromURL ( string url ) {
	char *ret = NULL ;
	CURL *curl;
	curl = curl_easy_init();
	if ( !curl ) return ret ; //finishWithError ( "Could not easy_init CURL for " + url ) ;

	struct MemoryStruct chunk;
	chunk.memory = (char*) malloc(1);  // will be grown as needed by the realloc above 
	chunk.size = 0;    // no data at this point 

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); // Follow redirect; paranoia
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "wdq-agent/1.0"); // fake agent
	
	CURLcode res = curl_easy_perform(curl);
	if (res != CURLE_OK) {
		fprintf(stderr, "curl_easy_perform() failed for %s: %s\n", url.c_str() , curl_easy_strerror(res));
		return ret ;
//		finishWithError ( "Retrieval error for " + url ) ;
	}
	
	if ( chunk.size == 0 || !chunk.memory ) return ret ; //finishWithError ( "Zero data return for " + url ) ;
	ret = chunk.memory ;
//	if(chunk.memory) free(chunk.memory);
	curl_easy_cleanup(curl);
	return ret ;
}

bool TWikidataDB::updateRecentChanges ( TItemSet &target ) {
	string last_change_time = target.time_end ;
	string timestamp ; 
	for ( uint32_t a = 0 ; a < last_change_time.length() ; a++ ) {
		if ( last_change_time[a] >= '0' && last_change_time[a] <= '9' ) timestamp += last_change_time[a] ;
	}
	
	char sql[10000] ;
	map <TItemNumber,bool> items ;
	while ( 1 ) {
		sprintf ( sql , "select page_title,rev_timestamp from revision,page where page_id=rev_page and page_namespace=0 and rev_timestamp>='%s' order by rev_timestamp asc limit %d" , timestamp.c_str() , batch_size ) ;
		MYSQL_RES *result = getQueryResults ( sql ) ;
		int num_fields = mysql_num_fields(result);
		MYSQL_ROW row;
		uint32_t cnt = 0 ;
		while ((row = mysql_fetch_row(result))) {
			timestamp = row[1] ;
			items[atol(row[0]+1)] = true ;
			if ( items.size() >= batch_size ) break ;
			cnt++ ;
		}
		mysql_free_result(result);
		if ( cnt < batch_size ) break ;
		if ( items.size() >= batch_size ) break ;
	}
//	cout << "Found " << items.size() << " items with change.\n" ;
	
	TItemSet new_set ;
	for ( map <TItemNumber,bool>::iterator a = items.begin() ; a != items.end() ; a++ ) {
		char item[100] ;
		sprintf ( item , "Q%d" , a->first ) ;
//		string url = "http://www.wikidata.org/wiki/Special:EntityData/" + string(item) + ".json" ;
		string url = "http://www.wikidata.org/w/api.php?action=wbgetentities&ids=" + string(item) + "&format=json" ;
//		cout << url << endl ;
		char *json = getTextFromURL ( url ) ;
		if ( !json ) {
			cerr << "No JSON for " << url << endl ; // Can't be helped, maybe next update...
			continue ;
		}
//		cout << json << endl ;
		new_set.addItemJSON ( json ) ;
		free ( json ) ;
	}

//	new_set.prepareProps () ;
	new_set.time_end = timestamp.substr(0,4)+"-"+timestamp.substr(4,2)+"-"+timestamp.substr(6,2)+"T"+timestamp.substr(8,2)+":"+timestamp.substr(10,2)+":"+timestamp.substr(12,2)+"Z" ;
	
//	cout << "\n\n" << new_set.getStatsString() << "\n---\n\n" ;
	
	target.mergeFromUpdate ( new_set ) ;

	return items.size() >= batch_size ; // Might there be more?
}

void TWikidataDB::getRedirects ( map <TItemNumber,bool> &remove ) {
	char sql[10000] ;
	sprintf ( sql , "select epp_entity_id FROM wb_entity_per_page,page WHERE page_id=epp_page_id  AND page_is_redirect=1" ) ;
	MYSQL_RES *result = getQueryResults ( sql ) ;
	int num_fields = mysql_num_fields(result);
	MYSQL_ROW row;
	while ((row = mysql_fetch_row(result))) {
		long q = atol ( row[0] ) ;
		remove[q] = true ;
	}
	mysql_free_result(result);
}

void TWikidataDB::getDeletedItems ( map <TItemNumber,bool> &remove ) {
	char sql[10000] ;
	sprintf ( sql , "select distinct(replace(log_title,'Q','')) from logging where log_action='delete' and log_timestamp>=replace(curdate() - interval 7 day,'-','') and log_namespace=0" ) ;
	MYSQL_RES *result = getQueryResults ( sql ) ;
	int num_fields = mysql_num_fields(result);
	MYSQL_ROW row;
	while ((row = mysql_fetch_row(result))) {
		long q = atol ( row[0] ) ;
		remove[q] = true ;
	}
	mysql_free_result(result);
}
*/


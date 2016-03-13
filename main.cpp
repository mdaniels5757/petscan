#include "main.h"
#include "mongoose.h"



//________________________________________________________________________________________________



TWikidataDB::TWikidataDB ( string config_file , string host ) {
	_database = "wikidatawiki_p" ;
	batch_size = 1000 ;
	curl_global_init(CURL_GLOBAL_ALL);
	_host = host ;
	_config_file = config_file ;
	doConnect(true) ;
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
	
	string user , password ;
	char buf[10000] ;
	FILE *fp = fopen(_config_file.c_str(),"r");
	if ( !fp ) finishWithError ( "Cannot open config file" ) ;
	while (fgets(buf, sizeof buf, fp) != NULL) {
		char *c ;
		for ( c = buf ; *c && *c != '=' ; c++ ) ;
		if ( !*c ) continue ;
		*c++ = 0 ;
		string key = buf ;
		if ( key != "user" && key != "password" ) continue ;
		char *value = ++c ;
		for ( c++ ; *c && *c != 39 ; c++ ) ;
		*c = 0 ;
		if ( key == "user" ) user = value ;
		if ( key == "password" ) password = value ;
	}
	fclose(fp);
	if ( user.empty() || password.empty() ) finishWithError ( "User or password missing from config file" ) ;
	
	mysql_init(&mysql);
	mysql_real_connect(&mysql,_host.c_str(),user.c_str(),password.c_str(),_database.c_str(),0,NULL,0) ;
//	printf("MySQL Server Version is %s\n",mysql_get_server_info(&mysql));
}

TWikidataDB::~TWikidataDB () {
   mysql_close(&mysql);
   curl_global_cleanup();
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

char *TWikidataDB::getTextFromURL ( string url ) {
	char *ret = NULL ;
	CURL *curl;
	curl = curl_easy_init();
	if ( !curl ) return ret ; //finishWithError ( "Could not easy_init CURL for " + url ) ;

	struct MemoryStruct chunk;
	chunk.memory = (char*) malloc(1);  /* will be grown as needed by the realloc above */ 
	chunk.size = 0;    /* no data at this point */ 

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
/*
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



//________________________________________________________________________________________________

static const char *s_http_port = "8083";
int timeout_ms = 3000 ;

static void ev_handler(struct mg_connection *c, int ev, void *p) {
  if (ev == MG_EV_HTTP_REQUEST) {
    struct http_message *hm = (struct http_message *) p;
    char reply[100];

    /* Simulate long calculation */
//    sleep(3);

cout << hm->uri.p << endl ;

    /* Send the reply */
    snprintf(reply, sizeof(reply), "{ \"uri\": \"%.*s\" }\n",
             (int) hm->uri.len, hm->uri.p);
    mg_printf(c, "HTTP/1.1 200 OK\r\n"
              "Content-Type: application/json\r\n"
              "Content-Length: %d\r\n"
              "\r\n"
              "%s",
              (int) strlen(reply), reply);
  }
}

int main(void) {
	struct mg_mgr mgr;
	struct mg_connection *nc;

	TWikidataDB db ( "petscan.conf" , "enwiki.labsddb" ) ;

	mg_mgr_init(&mgr, NULL);
	nc = mg_bind(&mgr, s_http_port, ev_handler);
	mg_set_protocol_http_websocket(nc);

	/* For each new connection, execute ev_handler in a separate thread */
	mg_enable_multithreading(nc);

	printf("Starting multi-threaded server on port %s\n", s_http_port);
	for (;;) {
		mg_mgr_poll(&mgr, timeout_ms);
	}
	mg_mgr_free(&mgr);

	return 0;
}